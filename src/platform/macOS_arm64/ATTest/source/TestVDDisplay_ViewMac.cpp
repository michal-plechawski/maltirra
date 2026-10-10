// Portable macOS VDDisplay view support tests.

#include <cstring>
#include <limits>

#include <CoreFoundation/CoreFoundation.h>
#include <CoreGraphics/CoreGraphics.h>

#include <at/attest/portabletest.h>
#include <vd2/VDDisplay/displayview_macos.h>

bool ATTestVDDisplayViewMac(ATPortableTestContext& context) {
	using namespace nsVDPixmap;

	uint32 pixels[6] {
		0x00112233, 0x00445566, 0xDEADBEEF,
		0x00778899, 0x00AABBCC, 0xCAFEBABE
	};

	VDPixmap pixmap {};
	pixmap.data = pixels;
	pixmap.w = 2;
	pixmap.h = 2;
	pixmap.pitch = 3 * sizeof(uint32);
	pixmap.format = kPixFormat_XRGB8888;

	CGImageRef image = VDCreateDisplayImageMac(pixmap);
	AT_PORTABLE_TEST_ASSERT(context, image != nullptr);
	AT_PORTABLE_TEST_ASSERT(context, CGImageGetWidth(image) == 2);
	AT_PORTABLE_TEST_ASSERT(context, CGImageGetHeight(image) == 2);
	AT_PORTABLE_TEST_ASSERT(context, CGImageGetBitsPerComponent(image) == 8);
	AT_PORTABLE_TEST_ASSERT(context, CGImageGetBitsPerPixel(image) == 32);
	AT_PORTABLE_TEST_ASSERT(context, CGImageGetBytesPerRow(image) == 2 * sizeof(uint32));
	AT_PORTABLE_TEST_ASSERT(context, CGImageGetAlphaInfo(image) == kCGImageAlphaNoneSkipFirst);
	AT_PORTABLE_TEST_ASSERT(context,
		(CGImageGetBitmapInfo(image) & kCGBitmapByteOrderMask) == kCGBitmapByteOrder32Little);

	// Image creation must snapshot both rows and strip source pitch padding.
	pixels[0] = 0;
	pixels[3] = 0;
	CFDataRef data = CGDataProviderCopyData(CGImageGetDataProvider(image));
	AT_PORTABLE_TEST_ASSERT(context, data != nullptr);
	AT_PORTABLE_TEST_ASSERT(context, CFDataGetLength(data) == 4 * (CFIndex)sizeof(uint32));
	uint32 copiedPixels[4] {};
	std::memcpy(copiedPixels, CFDataGetBytePtr(data), sizeof copiedPixels);
	AT_PORTABLE_TEST_ASSERT(context, copiedPixels[0] == 0x00112233);
	AT_PORTABLE_TEST_ASSERT(context, copiedPixels[1] == 0x00445566);
	AT_PORTABLE_TEST_ASSERT(context, copiedPixels[2] == 0x00778899);
	AT_PORTABLE_TEST_ASSERT(context, copiedPixels[3] == 0x00AABBCC);
	CFRelease(data);
	CGImageRelease(image);

	// Render through Core Graphics to verify the XRGB byte order, not just the
	// provider metadata.
	uint32 sourceColor = 0x00112233;
	pixmap.data = &sourceColor;
	pixmap.w = 1;
	pixmap.h = 1;
	pixmap.pitch = sizeof sourceColor;
	CGImageRef colorImage = VDCreateDisplayImageMac(pixmap);
	AT_PORTABLE_TEST_ASSERT(context, colorImage != nullptr);

	uint32 renderedColor = 0;
	CGColorSpaceRef colorSpace = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
	AT_PORTABLE_TEST_ASSERT(context, colorSpace != nullptr);
	const CGBitmapInfo bitmapInfo = (CGBitmapInfo)(
		(uint32)kCGBitmapByteOrder32Little | (uint32)kCGImageAlphaPremultipliedFirst);
	CGContextRef bitmapContext = CGBitmapContextCreate(
		&renderedColor, 1, 1, 8, sizeof renderedColor, colorSpace, bitmapInfo);
	AT_PORTABLE_TEST_ASSERT(context, bitmapContext != nullptr);
	CGContextSetBlendMode(bitmapContext, kCGBlendModeCopy);
	CGContextDrawImage(bitmapContext, CGRectMake(0, 0, 1, 1), colorImage);
	AT_PORTABLE_TEST_ASSERT(context, renderedColor == 0xFF112233);
	CGContextRelease(bitmapContext);
	CGColorSpaceRelease(colorSpace);
	CGImageRelease(colorImage);

	pixmap.data = pixels;
	pixmap.w = 2;
	pixmap.h = 2;
	pixmap.format = kPixFormat_RGB888;
	AT_PORTABLE_TEST_ASSERT(context, VDCreateDisplayImageMac(pixmap) == nullptr);
	pixmap.format = kPixFormat_XRGB8888;
	pixmap.pitch = sizeof(uint32);
	AT_PORTABLE_TEST_ASSERT(context, VDCreateDisplayImageMac(pixmap) == nullptr);
	pixmap.pitch = -(ptrdiff_t)(2 * sizeof(uint32));
	AT_PORTABLE_TEST_ASSERT(context, VDCreateDisplayImageMac(pixmap) == nullptr);
	pixmap.pitch = std::numeric_limits<ptrdiff_t>::max();
	pixmap.h = 3;
	AT_PORTABLE_TEST_ASSERT(context, VDCreateDisplayImageMac(pixmap) == nullptr);

	// Exercise the exact draw helper used by NSView in a top-left context.
	// Asymmetric rows catch double flips; a destination inset checks background
	// fill and clipping, and a bottom-row crop checks source coordinates.
	uint32 pattern[4] { 0x00FF0000, 0x0000FF00, 0x000000FF, 0x00FFFFFF };
	pixmap.data = pattern;
	pixmap.w = pixmap.h = 2;
	pixmap.pitch = 2 * sizeof(uint32);
	CGImageRef patternImage = VDCreateDisplayImageMac(pixmap);
	AT_PORTABLE_TEST_ASSERT(context, patternImage != nullptr);
	uint32 rendered[16] {};
	colorSpace = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
	bitmapContext = CGBitmapContextCreate(rendered, 4, 4, 8, 4 * sizeof(uint32), colorSpace, bitmapInfo);
	AT_PORTABLE_TEST_ASSERT(context, bitmapContext != nullptr);
	CGContextTranslateCTM(bitmapContext, 0, 4);
	CGContextScaleCTM(bitmapContext, 1, -1);
	const CGRect bounds = CGRectMake(0, 0, 4, 4);
	const vdrect32f dest(1, 1, 3, 3);
	VDDrawDisplayImageMac(bitmapContext, patternImage, bounds, nullptr, &dest, 0x00123456, false);
	// Bitmap storage is top-to-bottom even though its default user-space CTM
	// points upward. The simulated flipped view plus helper must preserve rows.
	const auto pixelAt = [&](int x, int y) { return rendered[y * 4 + x] & 0x00FFFFFF; };
	AT_PORTABLE_TEST_ASSERT(context, pixelAt(0, 0) == 0x00123456);
	AT_PORTABLE_TEST_ASSERT(context, pixelAt(3, 3) == 0x00123456);
	AT_PORTABLE_TEST_ASSERT(context, pixelAt(1, 1) == 0x00FF0000);
	AT_PORTABLE_TEST_ASSERT(context, pixelAt(2, 1) == 0x0000FF00);
	AT_PORTABLE_TEST_ASSERT(context, pixelAt(1, 2) == 0x000000FF);
	AT_PORTABLE_TEST_ASSERT(context, pixelAt(2, 2) == 0x00FFFFFF);
	const vdrect32 sourceCrop(0, 1, 2, 2);
	VDDrawDisplayImageMac(bitmapContext, patternImage, bounds, &sourceCrop, &dest, 0, false);
	AT_PORTABLE_TEST_ASSERT(context, pixelAt(1, 1) == 0x000000FF);
	AT_PORTABLE_TEST_ASSERT(context, pixelAt(2, 2) == 0x00FFFFFF);
	CGContextRelease(bitmapContext);
	CGColorSpaceRelease(colorSpace);
	CGImageRelease(patternImage);

	return true;
}
