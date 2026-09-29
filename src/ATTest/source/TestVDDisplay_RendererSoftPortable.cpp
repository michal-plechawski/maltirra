// Portable VDDisplay software renderer tests.

#include <algorithm>

#include <at/attest/portabletest.h>
#include <vd2/VDDisplay/renderer.h>
#include <vd2/VDDisplay/renderersoft.h>

namespace {
	constexpr uint32 kSentinel = 0xA5A5A5A5;
	constexpr sint32 kTargetW = 8;
	constexpr sint32 kTargetH = 6;
	constexpr sint32 kTargetPitchPixels = 10;

	VDPixmap MakePixmap(void *data, sint32 width, sint32 height, ptrdiff_t pitch, sint32 format) {
		VDPixmap px {};
		px.data = data;
		px.w = width;
		px.h = height;
		px.pitch = pitch;
		px.format = format;
		return px;
	}

	bool IsTargetPaddingIntact(const uint32 *target) {
		for(sint32 y=0; y<kTargetH; ++y) {
			for(sint32 x=kTargetW; x<kTargetPitchPixels; ++x) {
				if (target[y * kTargetPitchPixels + x] != kSentinel)
					return false;
			}
		}

		return true;
	}
}

bool ATTestVDDisplayRendererSoft(ATPortableTestContext& context) {
	uint32 target[kTargetH * kTargetPitchPixels];
	std::fill(std::begin(target), std::end(target), kSentinel);
	const VDPixmap targetPixmap = MakePixmap(
		target,
		kTargetW,
		kTargetH,
		kTargetPitchPixels * sizeof(uint32),
		nsVDPixmap::kPixFormat_XRGB8888
	);

	VDDisplayRendererSoft renderer;
	renderer.Init();
	AT_PORTABLE_TEST_ASSERT(context, renderer.Begin(targetPixmap));
	AT_PORTABLE_TEST_ASSERT(context, !renderer.GetCaps().mbSupportsAlphaBlending);
	AT_PORTABLE_TEST_ASSERT(context, renderer.GetCaps().mbSupportsColorBlt);

	renderer.SetColorRGB(0x00112233);
	renderer.FillRect(-1, 1, 4, 3);
	for(sint32 y=0; y<kTargetH; ++y) {
		for(sint32 x=0; x<kTargetW; ++x) {
			const uint32 expected = y >= 1 && y < 4 && x < 3 ? 0x00112233 : kSentinel;
			AT_PORTABLE_TEST_ASSERT(context, target[y * kTargetPitchPixels + x] == expected);
		}
	}
	AT_PORTABLE_TEST_ASSERT(context, IsTargetPaddingIntact(target));

	std::fill(std::begin(target), std::end(target), kSentinel);
	const vdrect32 fillRects[] {
		vdrect32(1, 0, 3, 1),
		vdrect32(6, 4, 10, 7),
		vdrect32(2, 2, 2, 4)
	};
	renderer.MultiFillRect(fillRects, 3);
	AT_PORTABLE_TEST_ASSERT(context, target[1] == 0x00112233);
	AT_PORTABLE_TEST_ASSERT(context, target[2] == 0x00112233);
	AT_PORTABLE_TEST_ASSERT(context, target[4 * kTargetPitchPixels + 6] == 0x00112233);
	AT_PORTABLE_TEST_ASSERT(context, target[5 * kTargetPitchPixels + 7] == 0x00112233);
	AT_PORTABLE_TEST_ASSERT(context, IsTargetPaddingIntact(target));

	uint32 source[3 * 4] {
		0x00000101, 0x00000102, 0x00000103, 0x00000104,
		0x00000201, 0x00000202, 0x00000203, 0x00000204,
		0x00000301, 0x00000302, 0x00000303, 0x00000304
	};
	VDDisplayImageView imageView;
	imageView.SetImage(MakePixmap(source, 4, 3, 4 * sizeof(uint32), nsVDPixmap::kPixFormat_XRGB8888), true);

	std::fill(std::begin(target), std::end(target), kSentinel);
	renderer.Blt(5, 1, imageView, 2, 1, 4, 1);
	AT_PORTABLE_TEST_ASSERT(context, target[1 * kTargetPitchPixels + 5] == source[1 * 4 + 2]);
	AT_PORTABLE_TEST_ASSERT(context, target[1 * kTargetPitchPixels + 6] == source[1 * 4 + 3]);
	AT_PORTABLE_TEST_ASSERT(context, target[1 * kTargetPitchPixels + 7] == kSentinel);

	source[0] = 0x0000CAFE;
	const vdrect32 dirtyRect(0, 0, 1, 1);
	imageView.Invalidate(&dirtyRect, 1);
	std::fill(std::begin(target), std::end(target), kSentinel);
	renderer.Blt(0, 0, imageView);
	AT_PORTABLE_TEST_ASSERT(context, target[0] == 0x0000CAFE);
	AT_PORTABLE_TEST_ASSERT(context, target[1] == source[1]);
	AT_PORTABLE_TEST_ASSERT(context, target[1 * kTargetPitchPixels] == source[4]);
	AT_PORTABLE_TEST_ASSERT(context, IsTargetPaddingIntact(target));

	std::fill(std::begin(target), std::end(target), kSentinel);
	VDDisplayBltOptions bltOptions;
	renderer.StretchBlt(0, 0, kTargetW, kTargetH, imageView, 0, 0, 4, 3, bltOptions);
	AT_PORTABLE_TEST_ASSERT(context, target[0] == source[0]);
	AT_PORTABLE_TEST_ASSERT(context, target[(kTargetH - 1) * kTargetPitchPixels + kTargetW - 1] == source[11]);
	AT_PORTABLE_TEST_ASSERT(context, IsTargetPaddingIntact(target));

	std::fill(std::begin(target), std::end(target), kSentinel);
	renderer.SetColorRGB(0x0000BEEF);
	AT_PORTABLE_TEST_ASSERT(context, renderer.PushViewport(vdrect32(2, 2, 6, 5), 2, 2));
	renderer.FillRect(0, 0, 1, 1);
	renderer.PopViewport();
	AT_PORTABLE_TEST_ASSERT(context, target[2 * kTargetPitchPixels + 2] == 0x0000BEEF);
	AT_PORTABLE_TEST_ASSERT(context, target[0] == kSentinel);

	const vdpoint32 line[] { vdpoint32(1, 1), vdpoint32(4, 1) };
	renderer.SetColorRGB(0x00010203);
	renderer.PolyLine(line, 1);
	AT_PORTABLE_TEST_ASSERT(context, target[1 * kTargetPitchPixels + 1] == 0x00010203);
	AT_PORTABLE_TEST_ASSERT(context, target[1 * kTargetPitchPixels + 2] == 0x00010203);
	AT_PORTABLE_TEST_ASSERT(context, target[1 * kTargetPitchPixels + 3] == 0x00010203);
	AT_PORTABLE_TEST_ASSERT(context, target[1 * kTargetPitchPixels + 4] == kSentinel);
	AT_PORTABLE_TEST_ASSERT(context, IsTargetPaddingIntact(target));

	VDPixmap unsupportedPixmap = targetPixmap;
	unsupportedPixmap.format = nsVDPixmap::kPixFormat_Y8;
	AT_PORTABLE_TEST_ASSERT(context, !renderer.Begin(unsupportedPixmap));
	return true;
}
