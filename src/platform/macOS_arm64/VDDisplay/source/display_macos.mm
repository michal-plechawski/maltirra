// Altirra - Atari 800/800XL/5200 emulator
// Copyright (C) 2026 Avery Lee

#include <atomic>
#include <cmath>
#include <exception>
#include <memory>
#include <utility>

#import <AppKit/AppKit.h>
#import <dispatch/dispatch.h>

#include <vd2/VDDisplay/display_macos.h>
#include <vd2/VDDisplay/displayview_macos.h>
#include <vd2/VDDisplay/internal/framequeue.h>
#include <vd2/VDDisplay/internal/presentationbuffer.h>
#include <vd2/VDDisplay/internal/softwarecomposition.h>

namespace {
	// Match the synchronous cross-thread behavior of the Windows control. Keep
	// C++ exceptions inside the block until dispatch_sync has returned.
	template<class F> void OnDisplayMain(F&& fn) {
		if ([NSThread isMainThread]) {
			fn();
			return;
		}
		std::exception_ptr error;
		std::exception_ptr *errorResult = &error;
		dispatch_sync(dispatch_get_main_queue(), ^{
			try { fn(); }
			catch (...) { *errorResult = std::current_exception(); }
		});
		if (error)
			std::rethrow_exception(error);
	}

	struct DisplayState : std::enable_shared_from_this<DisplayState> {
		DisplayState(VDGUIHandle view, sint32 width, sint32 height)
			: mView(view), mHeadlessWidth(width), mHeadlessHeight(height) {
			[reinterpret_cast<NSView *>(view) retain];
		}
		~DisplayState() {
			VDDestroyDisplayViewMac(mView);
		}

		void ApplyLayout() {
			mbNeedsRefresh = true;
			mComposition.Invalidate();
			if (!RePresent())
				ScheduleRefresh();
		}

		void Initialize() {
			const std::weak_ptr<DisplayState> weak = shared_from_this();
			VDDisplayViewSetRefreshCallbackMac(mView, [weak] {
				if (const auto state = weak.lock(); state && !state->mbShutdown) {
					state->mbNeedsRefresh = true;
					state->mComposition.Invalidate();
					state->ScheduleRefresh();
				}
			});
		}

		void ScheduleRefresh() {
			if (mbShutdown || mbRefreshScheduled)
				return;
			mbRefreshScheduled = true;
			const auto state = shared_from_this();
			dispatch_async(dispatch_get_main_queue(), ^{
				state->mbRefreshScheduled = false;
				if (!state->mbShutdown && state->mbNeedsRefresh) {
					try { state->RePresent(); }
					catch (...) { /* Preserve the last output; do not unwind a dispatch block. */ }
				}
			});
		}

		bool Compose(CGImageRef sourceImage, sint32 sourceWidth, sint32 sourceHeight, bool solidColor) {
			if (mbShutdown)
				return false;
			const VDDisplayViewOutputInfoMac output = mView
				? VDDisplayViewGetOutputInfoMac(mView)
				: VDDisplayViewOutputInfoMac { mHeadlessWidth ? mHeadlessWidth : sourceWidth,
					mHeadlessHeight ? mHeadlessHeight : sourceHeight, 1, 1 };
			if (output.mWidth <= 0 || output.mHeight <= 0)
				return false;
			const bool useSourceRect = mbSourceRect && !solidColor;
			const bool useDestRect = mbDestRect;
			const vdrect32 sourceRect = mSourceRect;
			const vdrect32f destRect = mDestRect;
			const uint32 background = mBackgroundColor;
			const bool bilinear = mFilterMode != IVDVideoDisplay::kFilterPoint;
			CGImageRef image = nullptr;
			bool rendered = false;
			try {
				rendered = mComposition.Render(output.mWidth, output.mHeight,
					[&](const VDPixmap& target) {
						CGColorSpaceRef colorSpace = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
						if (!colorSpace) return false;
						const CGBitmapInfo bitmapInfo = (CGBitmapInfo)(
							(uint32)kCGBitmapByteOrder32Little | (uint32)kCGImageAlphaNoneSkipFirst);
						CGContextRef context = CGBitmapContextCreate(target.data, target.w, target.h,
							8, (size_t)target.pitch, colorSpace, bitmapInfo);
						CGColorSpaceRelease(colorSpace);
						if (!context) return false;
						CGContextTranslateCTM(context, 0, target.h);
						CGContextScaleCTM(context, output.mScaleX, -output.mScaleY);
						VDDrawDisplayImageMac(context, sourceImage,
							CGRectMake(0, 0, target.w / output.mScaleX, target.h / output.mScaleY),
							useSourceRect ? &sourceRect : nullptr, useDestRect ? &destRect : nullptr,
							background, bilinear);
						CGContextRelease(context);
						return true;
					}, [&](const VDPixmap& target) {
						image = VDCreateDisplayImageMac(target);
						return image != nullptr;
					});
			} catch (...) {
				if (image) CGImageRelease(image);
				throw;
			}
			if (rendered && !mbShutdown) {
				mbNeedsRefresh = false;
				VDDisplayViewSetLayoutMac(mView, nullptr, nullptr, background, false);
				VDDisplayViewSetImageMac(mView, image);
			}
			if (image) CGImageRelease(image);
			return rendered && !mbShutdown;
		}

		bool RePresent() {
			const VDPixmap& source = mpPresented->GetPixmap();
			CGImageRef image = source.data ? VDCreateDisplayImageMac(source) : nullptr;
			if (source.data && !image)
				return false;
			bool result = false;
			try { result = Compose(image, source.w, source.h, mbSolidColor); }
			catch (...) { if (image) CGImageRelease(image); throw; }
			if (image) CGImageRelease(image);
			if (result && !mbShutdown)
				CompleteCapture();
			return result;
		}

		void NotifyStatus() {
			const int count = mFrames.GetQueuedFrames();
			if (mbNotifying || mbShutdown || count == mLastStatus)
				return;
			const auto fn = mStatusFn;
			if (fn) {
				mLastStatus = count;
				struct NotificationScope {
					bool& mFlag;
					~NotificationScope() { mFlag = false; }
				} scope { mbNotifying };
				mbNotifying = true;
				fn(count);
			}
		}

		bool Render(const VDPixmap& source, bool allowConversion,
			bool useScreenFX, IVDVideoDisplayScreenFXEngine *engine, bool notifyCapture = true,
			bool solidColor = false) {
			if (mbShutdown || (useScreenFX && !engine))
				return false;
			const VDPixmap px = useScreenFX ? engine->ApplyScreenFX(source) : source;
			if (mbShutdown || !mpStaging->Update(px, allowConversion))
				return false;
			// A hidden/zero-size view still accepts an owned video snapshot. The
			// resize callback presents it later, without rereading producer memory.
			if (mView && !VDDisplayViewGetOutputInfoMac(mView).mWidth) {
				mpPresented.swap(mpStaging);
				mbSolidColor = solidColor;
				mbNeedsRefresh = true;
				VDDisplayViewSetMessageMac(mView, nullptr);
				return true;
			}
			CGImageRef image = VDCreateDisplayImageMac(mpStaging->GetPixmap());
			if (!image)
				return false;
			bool composed = false;
			try { composed = Compose(image, px.w, px.h, solidColor); }
			catch (...) { CGImageRelease(image); throw; }
			CGImageRelease(image);
			if (!composed)
				return false;
			mpPresented.swap(mpStaging);
			mbSolidColor = solidColor;
			VDDisplayViewSetMessageMac(mView, nullptr);
			if (notifyCapture)
				CompleteCapture();
			return true;
		}

		void CompleteCapture() {
			// Isolate the callback from reentrant SetSource/Flush/Destroy calls.
			auto fn = std::move(mCaptureFn);
			if (!fn)
				return;
			VDPixmapBuffer snapshot;
			const VDPixmap& px = mComposition.GetPixmap();
			if (px.data) {
				snapshot.assign(px);
				fn(&snapshot);
			} else {
				fn(nullptr);
			}
		}

		void ProcessOne() {
			if (mbShutdown)
				return;
			auto lease = mFrames.Begin();
			if (lease.mpFrame) {
				mSource = {};
				mpSourceFX = nullptr;
				bool success = false;
				const auto profile = mProfileFn;
				try {
					if (profile)
						profile(IVDVideoDisplay::kProfileEvent_BeginPresent, lease.mpFrame->mFrameNumber);
					success = Render(lease.mpFrame->mPixmap, lease.mpFrame->mbAllowConversion,
						lease.mpFrame->mpScreenFX != nullptr, lease.mpFrame->mpScreenFXEngine);
					if (!mbShutdown && profile)
						profile(IVDVideoDisplay::kProfileEvent_EndPresent, lease.mpFrame->mFrameNumber);
				} catch (...) {
					// A bad frame or a failed software FX must not strand the active
					// frame, terminate AppKit, or destroy the last good snapshot.
					success = false;
				}
				mFrames.Complete(lease, success);
			}
			NotifyStatus();
			if (!mbShutdown && mFrames.GetQueuedFrames())
				Schedule();
		}

		void Schedule() {
			if (mbScheduled.exchange(true))
				return;
			const auto state = shared_from_this();
			dispatch_async(dispatch_get_main_queue(), ^{
				state->mbScheduled = false;
				state->ProcessOne();
			});
		}

		void Flush() {
			mFrames.Flush();
			mSource = {};
			mpSourceFX = nullptr;
			mbSourceFX = false;
			NotifyStatus();
		}

		VDGUIHandle mView;
		sint32 mHeadlessWidth = 0;
		sint32 mHeadlessHeight = 0;
		IVDVideoDisplay *mpOwner = nullptr;
		IVDVideoDisplayCallback *mpCallback = nullptr;
		VDDisplayFrameQueue mFrames;
		std::atomic<bool> mbScheduled { false };
		bool mbShutdown = false; // All fields below are confined to the main thread.
		bool mbRefreshScheduled = false;
		bool mbNeedsRefresh = false;
		bool mbSolidColor = false;
		bool mbNotifying = false;
		int mLastStatus = -1;
		VDPixmap mSource {};
		bool mbPersistent = false;
		bool mbAllowConversion = true;
		bool mbSourceFX = false;
		IVDVideoDisplayScreenFXEngine *mpSourceFX = nullptr;
		bool mbSourceRect = false;
		bool mbDestRect = false;
		vdrect32 mSourceRect {};
		vdrect32f mDestRect {};
		uint32 mBackgroundColor = 0;
		IVDVideoDisplay::FilterMode mFilterMode = IVDVideoDisplay::kFilterAnySuitable;
		vdfunction<void(int)> mStatusFn;
		vdfunction<void(IVDVideoDisplay::ProfileEvent, uintptr)> mProfileFn;
		vdfunction<void(const VDPixmap *)> mCaptureFn;
		VDDisplaySoftwareComposition mComposition;
		std::unique_ptr<VDDisplayPresentationBuffer> mpPresented = std::make_unique<VDDisplayPresentationBuffer>();
		std::unique_ptr<VDDisplayPresentationBuffer> mpStaging = std::make_unique<VDDisplayPresentationBuffer>();
	};

	class VDVideoDisplayMac final : public IVDVideoDisplay {
	public:
		VDVideoDisplayMac(VDGUIHandle view, sint32 width, sint32 height)
			: mpState(std::make_shared<DisplayState>(view, width, height)) {
			mpState->mpOwner = this;
			mpState->Initialize();
		}

		void Destroy() override {
			const auto s = mpState;
			bool destroy = false;
			OnDisplayMain([&] {
				if (s->mbShutdown) return;
				destroy = true;
				s->mbShutdown = true;
				s->mpOwner = nullptr;
				s->mpCallback = nullptr;
				s->mStatusFn = nullptr;
				s->mProfileFn = nullptr;
				VDDisplayViewSetRefreshCallbackMac(s->mView, nullptr);
				s->mComposition.Shutdown();
				s->Flush();
				VDDisplayViewClearMac(s->mView);
				auto capture = std::move(s->mCaptureFn);
				if (capture)
					capture(nullptr);
			});
			if (destroy)
				delete this;
		}

		void Reset() override {
			const auto s = mpState;
			OnDisplayMain([&] {
				if (s->mbShutdown) return;
				s->Flush();
				s->mpPresented->Clear();
				s->mpStaging->Clear();
				s->mComposition.Clear();
				VDDisplayViewClearMac(s->mView);
				VDDisplayViewSetMessageMac(s->mView, nullptr);
			});
		}

		void SetSourceMessage(const wchar_t *message) override {
			const auto s = mpState;
			OnDisplayMain([&] {
				if (s->mbShutdown) return;
				s->Flush();
				s->mpPresented->Clear();
				s->mComposition.Clear();
				VDDisplayViewClearMac(s->mView);
				VDDisplayViewSetMessageMac(s->mView, message);
			});
		}

		bool SetSource(bool autoUpdate, const VDPixmap& src, bool allowConversion) override {
			return SetSourceImpl(autoUpdate, src, allowConversion, false, nullptr, nullptr);
		}
		bool SetSourcePersistent(bool autoUpdate, const VDPixmap& src, bool allowConversion,
			const VDVideoDisplayScreenFXInfo *fx, IVDVideoDisplayScreenFXEngine *engine) override {
			return SetSourceImpl(autoUpdate, src, allowConversion, true, fx, engine);
		}
		void SetSourceSubrect(const vdrect32 *rect) override {
			const auto s = mpState;
			OnDisplayMain([&] {
				s->mbSourceRect = rect != nullptr;
				if (rect) s->mSourceRect = *rect;
				s->ApplyLayout();
			});
		}
		void SetSourceSolidColor(uint32 color) override {
			const auto s = mpState;
			OnDisplayMain([&] {
				s->Flush();
				VDPixmap px {};
				px.data = &color;
				px.w = px.h = 1;
				px.pitch = sizeof color;
				px.format = nsVDPixmap::kPixFormat_XRGB8888;
				s->Render(px, false, false, nullptr, true, true);
			});
		}

		void SetDestRect(const vdrect32 *rect, uint32 backgroundColor) override {
			vdrect32f r;
			if (rect) r = vdrect32f((float)rect->left, (float)rect->top, (float)rect->right, (float)rect->bottom);
			SetDestRectF(rect ? &r : nullptr, backgroundColor);
		}
		void SetDestRectF(const vdrect32f *rect, uint32 backgroundColor) override {
			const auto s = mpState;
			OnDisplayMain([&] {
				s->mbDestRect = rect != nullptr;
				if (rect) s->mDestRect = *rect;
				s->mBackgroundColor = backgroundColor;
				s->ApplyLayout();
			});
		}

		void PostBuffer(VDVideoDisplayFrame *frame) override {
			const auto s = mpState;
			if (s->mFrames.Post(frame)) s->Schedule();
		}
		bool RevokeBuffer(bool skip, VDVideoDisplayFrame **frame) override {
			return mpState->mFrames.Revoke(skip, frame);
		}
		void FlushBuffers() override {
			const auto s = mpState;
			OnDisplayMain([&] { s->Flush(); });
		}
		void Invalidate() override {
			const auto s = mpState;
			OnDisplayMain([&] {
				if (s->mbShutdown) return;
				if (s->mFrames.GetLastFrame()) {
					s->RePresent();
				} else if (s->mbPersistent && s->mSource.data) {
					s->Render(s->mSource, s->mbAllowConversion, s->mbSourceFX, s->mpSourceFX);
				} else if (s->mpCallback) {
					s->RePresent();
					if (s->mbShutdown) return;
					s->mpCallback->DisplayRequestUpdate(s->mpOwner);
				} else {
					s->RePresent();
				}
			});
		}
		void Update(int) override {
			const auto s = mpState;
			OnDisplayMain([&] {
				if (s->mFrames.GetQueuedFrames()) s->ProcessOne();
				else if (s->mSource.data) s->Render(s->mSource, s->mbAllowConversion, s->mbSourceFX, s->mpSourceFX);
			});
		}
		void Cache() override {
			// Presentation already owns a pixel snapshot independent of the source.
			const auto s = mpState;
			OnDisplayMain([&] { if (!s->mbPersistent) s->mSource = {}; });
		}
		void SetCallback(IVDVideoDisplayCallback *callback) override {
			const auto s = mpState;
			OnDisplayMain([&] { s->mpCallback = callback; });
		}
		void SetOnFrameStatusUpdated(vdfunction<void(int)> fn) override {
			const auto s = mpState;
			OnDisplayMain([&] { s->mStatusFn = std::move(fn); s->mLastStatus = -1; });
		}
		void SetProfileHook(const vdfunction<void(ProfileEvent, uintptr)>& fn) override {
			const auto s = mpState;
			OnDisplayMain([&] { s->mProfileFn = fn; });
		}
		void RequestCapture(vdfunction<void(const VDPixmap *)> fn) override {
			const auto s = mpState;
			OnDisplayMain([&] {
				auto old = std::move(s->mCaptureFn);
				s->mCaptureFn = std::move(fn);
				if (old) old(nullptr);
				if (s->mbShutdown || s->mComposition.IsRendering()) return;
				if (s->mbNeedsRefresh && !s->RePresent()) return;
				if (!s->mbShutdown && s->mComposition.GetPixmap().data) s->CompleteCapture();
			});
		}

		FilterMode GetFilterMode() override {
			FilterMode result = kFilterAnySuitable;
			const auto s = mpState;
			OnDisplayMain([&] { result = s->mFilterMode; });
			return result;
		}
		void SetFilterMode(FilterMode mode) override {
			const auto s = mpState;
			OnDisplayMain([&] { s->mFilterMode = mode; s->ApplyLayout(); });
		}
		int GetQueuedFrames() const override { return mpState->mFrames.GetQueuedFrames(); }
		bool IsFramePending() const override { return mpState->mFrames.IsFramePending(); }
		float GetSyncDelta() const override { return 0; }
		VDDVSyncStatus GetVSyncStatus() const override { return {}; }
		bool IsScreenFXPreferred() const override { return false; }
		VDDHDRAvailability IsHDRCapable() const override { return VDDHDRAvailability::NoMinidriverSupport; }
		bool MapNormSourcePtToDest(vdfloat2&) const override { return true; }
		bool MapNormDestPtToSource(vdfloat2&) const override { return true; }

		vdrect32 GetMonitorRect() override {
			vdrect32 result {};
			const auto s = mpState;
			OnDisplayMain([&] {
				NSScreen *screen = [[reinterpret_cast<NSView *>(s->mView) window] screen];
				if (screen) {
					const NSRect r = [screen frame];
					result = vdrect32((int)NSMinX(r), (int)NSMinY(r), (int)NSMaxX(r), (int)NSMaxY(r));
				}
			});
			return result;
		}
		void SetFullScreen(bool fullScreen, uint32, uint32, uint32) override {
			const auto s = mpState;
			OnDisplayMain([&] {
				NSWindow *window = [reinterpret_cast<NSView *>(s->mView) window];
				if (window && !!([window styleMask] & NSWindowStyleMaskFullScreen) != fullScreen)
					[window toggleFullScreen:nil];
			});
		}
		void SetTouchEnabled(bool enable) override {
			const auto s = mpState;
			OnDisplayMain([&] {
				[reinterpret_cast<NSView *>(s->mView) setAllowedTouchTypes:enable ? NSTouchTypeMaskDirect | NSTouchTypeMaskIndirect : 0];
			});
		}
		// Optional GPU/timing preferences have no effect on this software backend.
		void SetReturnFocus(bool) override {}
		void SetUse16Bit(bool) override {}
		void SetHDREnabled(bool) override {}
		void SetCustomDesiredRefreshRate(float, float, float) override {}
		void SetPixelSharpness(float, float) override {}
		void SetCompositor(IVDDisplayCompositor *compositor) override {
			const auto s = mpState;
			OnDisplayMain([&] {
				if (s->mbShutdown) return;
				s->mComposition.SetCompositor(compositor);
				if (!s->mbShutdown) s->ApplyLayout();
			});
		}
		void SetSDRBrightness(float) override {}
		void SetAccelerationMode(AccelerationMode) override {}

	private:
		bool SetSourceImpl(bool autoUpdate, const VDPixmap& src, bool allowConversion,
			bool persistent, const VDVideoDisplayScreenFXInfo *fx, IVDVideoDisplayScreenFXEngine *engine) {
			const auto s = mpState;
			bool result = false;
			OnDisplayMain([&] {
				if (s->mbShutdown || (fx && !engine)) return;
				if (src.w <= 0 || src.h <= 0 || src.format <= nsVDPixmap::kPixFormat_Null
					|| src.format >= nsVDPixmap::kPixFormat_Max_Standard) return;
				if (src.data) {
					if (autoUpdate) {
						if (!s->Render(src, allowConversion, fx != nullptr, engine, false)) return;
					} else {
						VDDisplayPresentationBuffer validation;
						if (!validation.Update(src, allowConversion)) return;
					}
				}
				if (s->mbShutdown) return;
				s->mFrames.Flush();
				s->mSource = src;
				s->mbAllowConversion = allowConversion;
				s->mbPersistent = persistent;
				s->mbSourceFX = fx != nullptr;
				s->mpSourceFX = engine;
				result = true;
				s->NotifyStatus();
				if (!s->mbShutdown && !s->mbNeedsRefresh && autoUpdate && src.data) s->CompleteCapture();
			});
			return result;
		}
		std::shared_ptr<DisplayState> mpState;
	};
}

IVDVideoDisplay *VDCreateVideoDisplayMac(VDGUIHandle view, sint32 width, sint32 height) {
	if (![NSThread isMainThread] || width < 0 || height < 0
		|| (!!width != !!height) || (view && (width || height)))
		return nullptr;
	return new VDVideoDisplayMac(view, width, height);
}
