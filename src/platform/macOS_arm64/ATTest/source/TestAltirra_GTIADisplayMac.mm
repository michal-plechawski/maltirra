// Real GTIA scanline -> palette conversion -> native AppKit display/capture.
// This is a video-pipeline contract, not a running ANTIC/CPU machine.
#import <AppKit/AppKit.h>
#include <array>
#include <memory>
#include <at/attest/portabletest.h>
#include <at/atnativeui/uinativewindow.h>
#include <vd2/system/refcount.h>
#include <vd2/Kasumi/pixmaputils.h>
#include <vd2/VDDisplay/display_macos.h>
#include <vd2/VDDisplay/displayview_macos.h>
#include <gtiarenderer.h>
#include <palettegenerator.h>

namespace {
	struct WindowGuard {
		vdrefptr<ATUINativeWindow>& window;
		~WindowGuard() { if (window) window->Destroy(); }
	};
	struct ViewDeleter {
		void operator()(__VDGUIHandle *view) const { VDDestroyDisplayViewMac(view); }
	};
	struct DisplayDeleter {
		void operator()(IVDVideoDisplay *display) const { if (display) display->Destroy(); }
	};
}

bool ATTestAltirraGTIADisplayMac(ATPortableTestContext& context) {
	@autoreleasepool {
		vdrefptr<ATUINativeWindow> window(new ATUINativeWindow);
		WindowGuard guard { window };
		AT_PORTABLE_TEST_ASSERT(context, window->CreateWindow({ 50, 50, 450, 250 }, L"GTIA pipeline"));
		std::unique_ptr<__VDGUIHandle, ViewDeleter> handle(VDCreateDisplayViewMac());
		AT_PORTABLE_TEST_ASSERT(context, handle != nullptr);
		NSView *view = reinterpret_cast<NSView *>(handle.get());
		[view setFrame:NSMakeRect(0, 0, 376, 8)];
		[reinterpret_cast<NSView *>(window->GetClientViewHandle()) addSubview:view];
		std::unique_ptr<IVDVideoDisplay, DisplayDeleter> display(VDCreateVideoDisplayMac(handle.get()));
		AT_PORTABLE_TEST_ASSERT(context, display != nullptr);
		display->SetFilterMode(IVDVideoDisplay::kFilterPoint);

		ATColorParams params {};
		params.mHueStart = -57;
		params.mHueRange = 27.1f * 15;
		params.mContrast = 1.04f;
		params.mBrightness = -0.04f;
		params.mSaturation = 0.2f;
		params.mGammaCorrect = params.mIntensityScale = 1;
		params.mRedScale = params.mGrnScale = params.mBluScale = 1;
		params.mLumaRampMode = kATLumaRampMode_XL;
		params.mColorMatchingMode = ATColorMatchingMode::SRGB;
		ATColorPaletteGenerator palette;
		palette.Generate(params, ATMonitorMode::Color);
		alignas(16) std::array<uint8, 480> indices {};
		alignas(16) std::array<uint8, 240> merge {};
		alignas(16) std::array<uint8, 240> antic {};
		for(int x = 0; x < 240; ++x)
			antic[x] = x & 1 ? (x / 2) & 3 : (x / 8) & 3;
		ATGTIARenderer renderer;
		renderer.SetRegisterImmediate(0x1a, 0x40);
		renderer.SetRegisterImmediate(0x1b, 0x40);
		renderer.BeginScanline(indices.data(), merge.data(), antic.data(), false);
		renderer.RenderScanline(222, true, false, false);
		renderer.EndScanline();
		// Feed the actual indexed output, not a separately invented RGB pattern.
		VDPixmap source {};
		source.data = indices.data() + 68;
		source.w = 376;
		source.h = 1;
		source.pitch = 456;
		source.format = nsVDPixmap::kPixFormat_Pal8;
		source.palette = palette.mPalette;
		AT_PORTABLE_TEST_ASSERT(context, display->SetSource(true, source, true));
		VDPixmapBuffer capture;
		display->RequestCapture([&](const VDPixmap *px) { if (px) capture.assign(*px); });
		const auto output = VDDisplayViewGetOutputInfoMac(handle.get());
		AT_PORTABLE_TEST_ASSERT(context, capture.w == output.mWidth && capture.h == output.mHeight);
		AT_PORTABLE_TEST_ASSERT(context, capture.w >= 376 && capture.w % 376 == 0 && capture.h > 0);
		AT_PORTABLE_TEST_ASSERT(context, palette.mPalette[0x40] != palette.mPalette[0x4e]);
		for(int y = 0; y < capture.h; ++y) {
			for(int x = 0; x < capture.w; ++x) {
				const int sourceX = x / (capture.w / 376);
				AT_PORTABLE_TEST_ASSERT(context, (capture.GetPixelRow<uint32>(y)[x] & 0xffffff)
					== palette.mPalette[indices[68 + sourceX]]);
			}
		}
		VDPixmapBuffer drawn(capture.w, capture.h, nsVDPixmap::kPixFormat_XRGB8888);
		CGColorSpaceRef space = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
		CGContextRef cg = CGBitmapContextCreate(drawn.data, drawn.w, drawn.h, 8, drawn.pitch, space,
			(CGBitmapInfo)((uint32)kCGBitmapByteOrder32Little | (uint32)kCGImageAlphaNoneSkipFirst));
		CGColorSpaceRelease(space);
		AT_PORTABLE_TEST_ASSERT(context, cg != nullptr);
		CGContextTranslateCTM(cg, 0, drawn.h);
		CGContextScaleCTM(cg, output.mScaleX, -output.mScaleY);
		[NSGraphicsContext saveGraphicsState];
		[NSGraphicsContext setCurrentContext:[NSGraphicsContext graphicsContextWithCGContext:cg flipped:YES]];
		[view drawRect:[view bounds]];
		[NSGraphicsContext restoreGraphicsState];
		CGContextRelease(cg);
		for(int y = 0; y < capture.h; ++y)
			for(int x = 0; x < capture.w; ++x)
				AT_PORTABLE_TEST_ASSERT(context, (drawn.GetPixelRow<uint32>(y)[x] & 0xffffff)
					== (capture.GetPixelRow<uint32>(y)[x] & 0xffffff));
		// Destroy display before the stack-backed scanline and palette expire.
		display.reset();
	}
	return true;
}
