// Native NSView integration without NSApplication, NSWindow, or a GUI session.

#include <memory>
#import <AppKit/AppKit.h>
#include <at/attest/portabletest.h>
#include <vd2/Kasumi/pixmaputils.h>
#include <vd2/VDDisplay/display_macos.h>
#include <vd2/VDDisplay/displayview_macos.h>

namespace {
	struct DestroyView {
		void operator()(__VDGUIHandle *view) const { VDDestroyDisplayViewMac(view); }
	};
	struct DestroyDisplay {
		void operator()(IVDVideoDisplay *display) const { if (display) display->Destroy(); }
	};
}

bool ATTestVDDisplayNativeViewMac(ATPortableTestContext& context) {
	@autoreleasepool {
		VDPixmapBuffer capture;
		bool captured = false;
		std::unique_ptr<__VDGUIHandle, DestroyView> handle(VDCreateDisplayViewMac());
		AT_PORTABLE_TEST_ASSERT(context, handle != nullptr);
		NSView *view = reinterpret_cast<NSView *>(handle.get());
		[view setFrameSize:NSMakeSize(8, 4)];
		const auto output = VDDisplayViewGetOutputInfoMac(handle.get());
		AT_PORTABLE_TEST_ASSERT(context, output.mWidth > 0 && output.mHeight > 0);
		std::unique_ptr<IVDVideoDisplay, DestroyDisplay> display(VDCreateVideoDisplayMac(handle.get()));
		AT_PORTABLE_TEST_ASSERT(context, display != nullptr);
		display->SetFilterMode(IVDVideoDisplay::kFilterPoint);
		uint32 pixels[] { 0x00FF0000, 0x0000FF00, 0x000000FF, 0x00FFFFFF };
		VDPixmap source {};
		source.data = pixels;
		source.w = source.h = 2;
		source.pitch = 2 * sizeof(uint32);
		source.format = nsVDPixmap::kPixFormat_XRGB8888;
		AT_PORTABLE_TEST_ASSERT(context, display->SetSource(true, source, false));
		const auto takeCapture = [&](const VDPixmap *px) {
			captured = px != nullptr;
			if (px) capture.assign(*px);
		};
		display->RequestCapture(takeCapture);
		AT_PORTABLE_TEST_ASSERT(context, captured);
		AT_PORTABLE_TEST_ASSERT(context, capture.w == output.mWidth && capture.h == output.mHeight);

		// Draw the actual production NSView into a bitmap graphics context.
		// AppKit's top-left view transform is reproduced at backing resolution.
		VDPixmapBuffer drawn(output.mWidth, output.mHeight, nsVDPixmap::kPixFormat_XRGB8888);
		CGColorSpaceRef colorSpace = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
		const CGBitmapInfo bitmapInfo = (CGBitmapInfo)(
			(uint32)kCGBitmapByteOrder32Little | (uint32)kCGImageAlphaNoneSkipFirst);
		CGContextRef cg = CGBitmapContextCreate(drawn.data, drawn.w, drawn.h, 8,
			(size_t)drawn.pitch, colorSpace, bitmapInfo);
		CGColorSpaceRelease(colorSpace);
		AT_PORTABLE_TEST_ASSERT(context, cg != nullptr);
		CGContextTranslateCTM(cg, 0, drawn.h);
		CGContextScaleCTM(cg, output.mScaleX, -output.mScaleY);
		[NSGraphicsContext saveGraphicsState];
		[NSGraphicsContext setCurrentContext:[NSGraphicsContext graphicsContextWithCGContext:cg flipped:YES]];
		[view drawRect:[view bounds]];
		[NSGraphicsContext restoreGraphicsState];
		CGContextRelease(cg);
		for(int y = 0; y < drawn.h; ++y)
			for(int x = 0; x < drawn.w; ++x)
				AT_PORTABLE_TEST_ASSERT(context, (drawn.GetPixelRow<uint32>(y)[x] & 0x00FFFFFF)
					== (capture.GetPixelRow<uint32>(y)[x] & 0x00FFFFFF));

		// Changing the view must regenerate output at its new backing size even
		// when the emulator source is cached and no new video frame is posted.
		display->Cache();
		[view setFrameSize:NSMakeSize(16, 8)];
		[view viewDidChangeBackingProperties];
		const auto resized = VDDisplayViewGetOutputInfoMac(handle.get());
		// Capture immediately, before the deferred resize work runs.
		display->RequestCapture(takeCapture);
		AT_PORTABLE_TEST_ASSERT(context, capture.w == resized.mWidth && capture.h == resized.mHeight);
		AT_PORTABLE_TEST_ASSERT(context, capture.w > output.mWidth && capture.h > output.mHeight);
		AT_PORTABLE_TEST_ASSERT(context, (capture.GetPixelRow<uint32>(0)[0] & 0x00FFFFFF) == 0x00FF0000);
		AT_PORTABLE_TEST_ASSERT(context,
			(capture.GetPixelRow<uint32>(capture.h - 1)[capture.w - 1] & 0x00FFFFFF) == 0x00FFFFFF);

		// A zero-size view must accept new video and defer capture until its
		// surface is available again, rather than lose the emulator's last frame.
		[view setFrameSize:NSMakeSize(0, 0)];
		pixels[0] = 0x00112233;
		AT_PORTABLE_TEST_ASSERT(context, display->SetSource(true, source, false));
		captured = false;
		display->RequestCapture(takeCapture);
		AT_PORTABLE_TEST_ASSERT(context, !captured);
		[view setFrameSize:NSMakeSize(8, 4)];
		CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.01, false);
		AT_PORTABLE_TEST_ASSERT(context, captured);
		AT_PORTABLE_TEST_ASSERT(context, (capture.GetPixelRow<uint32>(0)[0] & 0x00FFFFFF) == 0x00112233);

		// Releasing the caller's view reference must not invalidate the backend.
		handle.reset();
		display->Invalidate();
		display->RequestCapture(takeCapture);
		AT_PORTABLE_TEST_ASSERT(context, captured);
		display.reset();
		CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.01, false);
	}
	return true;
}
