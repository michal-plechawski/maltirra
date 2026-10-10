// macOS scaled-output capture and software overlay integration (headless).

#include <memory>
#include <CoreFoundation/CoreFoundation.h>
#include <at/attest/portabletest.h>
#include <vd2/Kasumi/pixmaputils.h>
#include <vd2/VDDisplay/compositor.h>
#include <vd2/VDDisplay/renderer.h>
#include <vd2/VDDisplay/display_macos.h>

namespace {
	struct DestroyDisplay {
		void operator()(IVDVideoDisplay *display) const { if (display) display->Destroy(); }
	};
	class TestCompositor final : public vdrefcounted<IVDDisplayCompositor> {
	public:
		void AttachCompositor(IVDDisplayCompositionEngine& engine) override {
			mpEngine = &engine;
			++mAttach;
		}
		void DetachCompositor() override { mpEngine = nullptr; ++mDetach; }
		void PreComposite(const VDDisplayCompositeInfo& info) override {
			mInfo = info;
			mpEngine->LoadCustomEffect(L"");
			if (mOnPre) mOnPre();
		}
		void Composite(IVDDisplayRenderer& renderer, const VDDisplayCompositeInfo&) override {
			renderer.SetColorRGB(0x00AA5500);
			renderer.FillRect(0, 0, 2, 1);
		}
		int mAttach = 0, mDetach = 0;
		IVDDisplayCompositionEngine *mpEngine = nullptr;
		VDDisplayCompositeInfo mInfo {};
		vdfunction<void()> mOnPre;
	};
}

bool ATTestVDDisplayCompositionMac(ATPortableTestContext& context) {
	VDPixmapBuffer capture;
	vdrefptr<TestCompositor> compositor(new TestCompositor);
	bool captured = false;
	bool cancelledComposition = false;
	bool captureFromPre = false;
	std::unique_ptr<IVDVideoDisplay, DestroyDisplay> display(VDCreateVideoDisplayMac(nullptr, 12, 8));
	AT_PORTABLE_TEST_ASSERT(context, display != nullptr);
	AT_PORTABLE_TEST_ASSERT(context, VDCreateVideoDisplayMac(nullptr, -1, 8) == nullptr);
	AT_PORTABLE_TEST_ASSERT(context, VDCreateVideoDisplayMac(nullptr, 12, 0) == nullptr);
	display->SetFilterMode(IVDVideoDisplay::kFilterPoint);
	display->SetCompositor(compositor);
	AT_PORTABLE_TEST_ASSERT(context, compositor->mAttach == 1);
	const vdrect32 dest(2, 2, 10, 6);
	display->SetDestRect(&dest, 0x00101112);
	uint32 pixels[4] { 0x00FF0000, 0x0000FF00, 0x000000FF, 0x00FFFFFF };
	VDPixmap source {};
	source.data = pixels;
	source.w = source.h = 2;
	source.pitch = 2 * sizeof(uint32);
	source.format = nsVDPixmap::kPixFormat_XRGB8888;
	AT_PORTABLE_TEST_ASSERT(context, display->SetSource(true, source, false));
	const auto captureFrame = [&](const VDPixmap *px) {
		captured = px != nullptr;
		if (px) capture.assign(*px);
	};
	display->RequestCapture(captureFrame);
	AT_PORTABLE_TEST_ASSERT(context, captured);
	AT_PORTABLE_TEST_ASSERT(context, capture.w == 12 && capture.h == 8);
	AT_PORTABLE_TEST_ASSERT(context, compositor->mInfo.mWidth == 12 && compositor->mInfo.mHeight == 8);
	const auto pixelAt = [&](int x, int y) { return capture.GetPixelRow<uint32>(y)[x] & 0x00FFFFFF; };
	AT_PORTABLE_TEST_ASSERT(context, pixelAt(0, 0) == 0x00AA5500);
	AT_PORTABLE_TEST_ASSERT(context, pixelAt(0, 1) == 0x00101112);
	AT_PORTABLE_TEST_ASSERT(context, pixelAt(2, 2) == 0x00FF0000);
	AT_PORTABLE_TEST_ASSERT(context, pixelAt(9, 2) == 0x0000FF00);
	AT_PORTABLE_TEST_ASSERT(context, pixelAt(2, 5) == 0x000000FF);
	AT_PORTABLE_TEST_ASSERT(context, pixelAt(9, 5) == 0x00FFFFFF);

	// Layout changes rerender owned pixels without touching expired source data.
	display->Cache();
	pixels[0] = pixels[1] = pixels[2] = pixels[3] = 0;
	const vdrect32 crop(1, 0, 2, 2);
	display->SetSourceSubrect(&crop);
	display->RequestCapture(captureFrame);
	AT_PORTABLE_TEST_ASSERT(context, pixelAt(2, 2) == 0x0000FF00);
	AT_PORTABLE_TEST_ASSERT(context, pixelAt(9, 5) == 0x00FFFFFF);
	display->SetSourceSolidColor(0x00334455);
	display->RequestCapture(captureFrame);
	AT_PORTABLE_TEST_ASSERT(context, pixelAt(2, 2) == 0x00334455);
	AT_PORTABLE_TEST_ASSERT(context, pixelAt(9, 5) == 0x00334455);
	AT_PORTABLE_TEST_ASSERT(context, pixelAt(0, 0) == 0x00AA5500);

	// A capture requested inside PreComposite must wait for the finished
	// overlay transaction, rather than return an older bitmap mid-render.
	compositor->mOnPre = [&] {
		display->RequestCapture([&](const VDPixmap *px) {
			if (px) {
				captureFromPre = true;
				capture.assign(*px);
			}
		});
	};
	display->Invalidate();
	AT_PORTABLE_TEST_ASSERT(context, captureFromPre);
	AT_PORTABLE_TEST_ASSERT(context, pixelAt(0, 0) == 0x00AA5500);

	// A compositor can detach itself from PreComposite without committing its
	// stale overlay. The deferred refresh applies the newest binding safely.
	compositor->mOnPre = [&] {
		cancelledComposition = true;
		display->SetCompositor(nullptr);
	};
	display->Invalidate();
	CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.01, false);
	AT_PORTABLE_TEST_ASSERT(context, cancelledComposition);
	AT_PORTABLE_TEST_ASSERT(context, compositor->mDetach == 1);
	display->RequestCapture(captureFrame);
	AT_PORTABLE_TEST_ASSERT(context, pixelAt(0, 0) == 0x00101112);
	AT_PORTABLE_TEST_ASSERT(context, pixelAt(2, 2) == 0x00334455);
	compositor->mOnPre = nullptr;
	display->SetCompositor(compositor);
	AT_PORTABLE_TEST_ASSERT(context, compositor->mAttach == 2);
	display.reset();
	AT_PORTABLE_TEST_ASSERT(context, compositor->mDetach == 2 && compositor->mpEngine == nullptr);

	// Destruction from PreComposite must detach before the borrowed composition
	// engine disappears, cancel the output, and keep the current callback alive.
	display.reset(VDCreateVideoDisplayMac(nullptr, 4, 4));
	IVDVideoDisplay *destroying = display.release();
	compositor->mOnPre = [&] { destroying->Destroy(); };
	destroying->SetCompositor(compositor);
	AT_PORTABLE_TEST_ASSERT(context, compositor->mAttach == 3 && compositor->mDetach == 3);
	AT_PORTABLE_TEST_ASSERT(context, compositor->mpEngine == nullptr);
	compositor->mOnPre = nullptr;
	CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.01, false);
	return true;
}
