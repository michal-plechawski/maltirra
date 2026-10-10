// macOS software adapter integration without NSApplication or a window.

#include <atomic>
#include <memory>
#include <thread>
#include <stdexcept>
#include <CoreFoundation/CoreFoundation.h>
#include <at/attest/portabletest.h>
#include <vd2/VDDisplay/display_macos.h>

namespace {
	struct DestroyDisplay {
		void operator()(IVDVideoDisplay *display) const { if (display) display->Destroy(); }
	};
	struct TestCallback final : IVDVideoDisplayCallback {
		void DisplayRequestUpdate(IVDVideoDisplay *) override { ++mRequests; }
		int mRequests = 0;
	};
	struct TestScreenFX final : IVDVideoDisplayScreenFXEngine {
		VDPixmap ApplyScreenFX(const VDPixmap& source) override {
			if (mbThrow) throw std::runtime_error("test screen FX failure");
			++mCalls;
			mPixel = *static_cast<const uint32 *>(source.data) ^ 0x00FFFFFF;
			VDPixmap result = source;
			result.data = &mPixel;
			return result;
		}
		int mCalls = 0;
		uint32 mPixel = 0;
		bool mbThrow = false;
	};
	struct TestFrame final : VDVideoDisplayFrame {
		explicit TestFrame(std::atomic<int>& destroyed, uint32 pixel)
			: mDestroyed(destroyed), mPixel(pixel) {
			mPixmap.data = &mPixel;
			mPixmap.w = mPixmap.h = 1;
			mPixmap.pitch = sizeof mPixel;
			mPixmap.format = nsVDPixmap::kPixFormat_XRGB8888;
		}
		~TestFrame() { ++mDestroyed; }
		std::atomic<int>& mDestroyed;
		uint32 mPixel;
	};
}

bool ATTestVDDisplayAdapterMac(ATPortableTestContext& context) {
	std::atomic<int> destroyed { 0 };
	TestCallback callback;
	TestScreenFX screenFX;
	int status = -1;
	int captures = 0;
	int cancellations = 0;
	int beginEvents = 0;
	int endEvents = 0;
	uint32 capturedColor = 0;
	uint32 pixel = 0x00112233;
	bool reentrantCaptureValid = false;
	bool reentrantSourceValid = false;
	int flushCallbacks = 0;
	int nestedDestroyCallbacks = 0;
	uint32 newerPixel = 0x00FEDCBA;
	const auto capture = [&](const VDPixmap *px) {
		if (px) {
			++captures;
			capturedColor = px->GetPixelRow<uint32>(0)[0] & 0x00FFFFFF;
		} else {
			++cancellations;
		}
	};
	std::unique_ptr<IVDVideoDisplay, DestroyDisplay> display(VDCreateVideoDisplayMac(nullptr));
	AT_PORTABLE_TEST_ASSERT(context, display != nullptr);
	AT_PORTABLE_TEST_ASSERT(context, display->IsHDRCapable() == VDDHDRAvailability::NoMinidriverSupport);
	AT_PORTABLE_TEST_ASSERT(context, !display->IsScreenFXPreferred());
	AT_PORTABLE_TEST_ASSERT(context, display->GetVSyncStatus().mOffset < 0);
	display->SetCallback(&callback);
	display->SetOnFrameStatusUpdated([&](int frames) { status = frames; });
	display->SetProfileHook([&](IVDVideoDisplay::ProfileEvent event, uintptr) {
		if (event == IVDVideoDisplay::kProfileEvent_BeginPresent) ++beginEvents;
		if (event == IVDVideoDisplay::kProfileEvent_EndPresent) ++endEvents;
	});
	display->SetFilterMode(IVDVideoDisplay::kFilterPoint);
	AT_PORTABLE_TEST_ASSERT(context, display->GetFilterMode() == IVDVideoDisplay::kFilterPoint);

	VDPixmap source {};
	source.data = &pixel;
	source.w = source.h = 1;
	source.pitch = sizeof pixel;
	source.format = nsVDPixmap::kPixFormat_XRGB8888;
	display->RequestCapture(capture);
	AT_PORTABLE_TEST_ASSERT(context, captures == 0);
	AT_PORTABLE_TEST_ASSERT(context, display->SetSource(true, source, false));
	AT_PORTABLE_TEST_ASSERT(context, captures == 1 && capturedColor == 0x00112233);

	// Non-auto setup must not replace the previously presented pixels.
	pixel = 0x00445566;
	AT_PORTABLE_TEST_ASSERT(context, display->SetSource(false, source, false));
	display->RequestCapture(capture);
	AT_PORTABLE_TEST_ASSERT(context, capturedColor == 0x00112233);
	display->Update();
	display->RequestCapture(capture);
	AT_PORTABLE_TEST_ASSERT(context, capturedColor == 0x00445566);
	display->Cache();
	pixel = 0;
	display->Invalidate();
	AT_PORTABLE_TEST_ASSERT(context, callback.mRequests == 1);
	display->RequestCapture(capture);
	AT_PORTABLE_TEST_ASSERT(context, capturedColor == 0x00445566);

	// Persistent sources are reread on explicit invalidation.
	AT_PORTABLE_TEST_ASSERT(context, display->SetSourcePersistent(true, source, false));
	pixel = 0x00778899;
	display->Invalidate();
	display->RequestCapture(capture);
	AT_PORTABLE_TEST_ASSERT(context, capturedColor == 0x00778899);
	VDVideoDisplayScreenFXInfo fx;
	AT_PORTABLE_TEST_ASSERT(context, !display->SetSourcePersistent(true, source, false, &fx, nullptr));
	AT_PORTABLE_TEST_ASSERT(context, display->SetSourcePersistent(true, source, false, &fx, &screenFX));
	display->RequestCapture(capture);
	AT_PORTABLE_TEST_ASSERT(context, screenFX.mCalls == 1 && capturedColor == 0x00887766);

	// Posted buffers own their references; after presentation only an older
	// frame may be revoked, never the latest cached or currently active frame.
	vdrefptr<VDVideoDisplayFrame> first(new TestFrame(destroyed, 0x00010203));
	vdrefptr<VDVideoDisplayFrame> second(new TestFrame(destroyed, 0x00040506));
	display->PostBuffer(first);
	display->PostBuffer(second);
	first = nullptr;
	second = nullptr;
	AT_PORTABLE_TEST_ASSERT(context, destroyed == 0);
	AT_PORTABLE_TEST_ASSERT(context, display->GetQueuedFrames() == 2);
	display->Update();
	AT_PORTABLE_TEST_ASSERT(context, display->GetQueuedFrames() == 1 && status == 1);
	display->Update();
	AT_PORTABLE_TEST_ASSERT(context, display->GetQueuedFrames() == 0 && status == 0);
	AT_PORTABLE_TEST_ASSERT(context, beginEvents == 2 && endEvents == 2);
	display->RequestCapture(capture);
	AT_PORTABLE_TEST_ASSERT(context, capturedColor == 0x00040506);
	vdrefptr<VDVideoDisplayFrame> recycled;
	AT_PORTABLE_TEST_ASSERT(context, display->RevokeBuffer(false, ~recycled));
	AT_PORTABLE_TEST_ASSERT(context, *static_cast<const uint32 *>(recycled->mPixmap.data) == 0x00010203);
	recycled = nullptr;
	AT_PORTABLE_TEST_ASSERT(context, destroyed == 1);
	AT_PORTABLE_TEST_ASSERT(context, !display->RevokeBuffer(true, ~recycled));

	// A screen FX exception on a posted frame must preserve the displayed
	// snapshot and return the failed frame to the producer instead of stalling.
	vdrefptr<VDVideoDisplayFrame> failed(new TestFrame(destroyed, 0x00FFFFFF));
	failed->mpScreenFX = &fx;
	failed->mpScreenFXEngine = &screenFX;
	screenFX.mbThrow = true;
	display->PostBuffer(failed);
	display->Update();
	AT_PORTABLE_TEST_ASSERT(context, display->GetQueuedFrames() == 0);
	AT_PORTABLE_TEST_ASSERT(context, display->RevokeBuffer(false, ~recycled));
	AT_PORTABLE_TEST_ASSERT(context, recycled == failed);
	display->RequestCapture(capture);
	AT_PORTABLE_TEST_ASSERT(context, capturedColor == 0x00040506);
	recycled = nullptr;
	failed = nullptr;
	display->FlushBuffers();
	AT_PORTABLE_TEST_ASSERT(context, destroyed == 3);

	// The source-backed capture snapshot remains valid across a reentrant reset.
	display->RequestCapture([&](const VDPixmap *px) {
		if (!px) return;
		const uint32 original = px->GetPixelRow<uint32>(0)[0];
		display->Reset();
		reentrantCaptureValid = px->GetPixelRow<uint32>(0)[0] == original;
	});
	AT_PORTABLE_TEST_ASSERT(context, reentrantCaptureValid);

	// Replaced requests and destruction report cancellation once, with null.
	const int cancelledBefore = cancellations;
	display->RequestCapture(capture);
	display->RequestCapture(capture);
	AT_PORTABLE_TEST_ASSERT(context, cancellations == cancelledBefore + 1);
	display.reset();
	AT_PORTABLE_TEST_ASSERT(context, cancellations == cancelledBefore + 2);

	// A worker producer is dispatched through the main queue automatically.
	display.reset(VDCreateVideoDisplayMac(nullptr));
	vdrefptr<VDVideoDisplayFrame> asynchronous(new TestFrame(destroyed, 0x00ABCDEF));
	std::thread producer([&] { display->PostBuffer(asynchronous); });
	producer.join();
	for(int i = 0; i < 100 && display->GetQueuedFrames(); ++i)
		CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.01, false);
	AT_PORTABLE_TEST_ASSERT(context, display->GetQueuedFrames() == 0);
	display->RequestCapture(capture);
	AT_PORTABLE_TEST_ASSERT(context, capturedColor == 0x00ABCDEF);
	display.reset();
	asynchronous = nullptr;
	AT_PORTABLE_TEST_ASSERT(context, destroyed == 4);

	// Callback reentry must not recursively notify or overwrite a newer source.
	display.reset(VDCreateVideoDisplayMac(nullptr));
	display->SetOnFrameStatusUpdated([&](int) {
		++flushCallbacks;
		display->FlushBuffers();
	});
	display->FlushBuffers();
	AT_PORTABLE_TEST_ASSERT(context, flushCallbacks == 1);
	display->SetOnFrameStatusUpdated(nullptr);
	display->RequestCapture([&](const VDPixmap *px) {
		if (!px) return;
		VDPixmap newer = source;
		newer.data = &newerPixel;
		reentrantSourceValid = display->SetSourcePersistent(true, newer, false);
	});
	AT_PORTABLE_TEST_ASSERT(context, display->SetSourcePersistent(true, source, false));
	AT_PORTABLE_TEST_ASSERT(context, reentrantSourceValid);
	newerPixel = 0x00123456;
	display->Invalidate();
	display->RequestCapture(capture);
	AT_PORTABLE_TEST_ASSERT(context, capturedColor == 0x00123456);
	display->Reset();
	IVDVideoDisplay *destroying = display.release();
	destroying->RequestCapture([&](const VDPixmap *px) {
		if (!px) {
			++nestedDestroyCallbacks;
			destroying->Destroy();
		}
	});
	destroying->Destroy();
	AT_PORTABLE_TEST_ASSERT(context, nestedDestroyCallbacks == 1);
	// Drain cancelled tasks too: they must not invoke old callbacks or frames.
	CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.01, false);
	return true;
}
