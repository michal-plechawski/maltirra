// Portable VDDisplay presentation buffer tests.

#include <algorithm>
#include <iterator>

#include <at/attest/portabletest.h>
#include <vd2/VDDisplay/internal/presentationbuffer.h>

namespace {
	VDPixmap MakePixmap(void *data, sint32 width, sint32 height, ptrdiff_t pitch, sint32 format) {
		VDPixmap pixmap {};
		pixmap.data = data;
		pixmap.w = width;
		pixmap.h = height;
		pixmap.pitch = pitch;
		pixmap.format = format;
		return pixmap;
	}
}

bool ATTestVDDisplayPresentationBuffer(ATPortableTestContext& context) {
	using namespace nsVDPixmap;

	VDDisplayPresentationBuffer buffer;
	AT_PORTABLE_TEST_ASSERT(context, buffer.GetPixmap().data == nullptr);
	AT_PORTABLE_TEST_ASSERT(context, buffer.GetPixmap().format == kPixFormat_Null);
	AT_PORTABLE_TEST_ASSERT(context, !buffer.Update(VDPixmap {}, true));

	uint8 rgb888[24];
	std::fill(std::begin(rgb888), std::end(rgb888), UINT8_C(0xEE));
	const uint8 colors[][3] {
		{ 0x33, 0x22, 0x11 }, { 0x66, 0x55, 0x44 }, { 0x99, 0x88, 0x77 },
		{ 0xCC, 0xBB, 0xAA }, { 0x03, 0x02, 0x01 }, { 0x30, 0x20, 0x10 }
	};
	for(int y = 0; y < 2; ++y)
		for(int x = 0; x < 3; ++x)
			std::copy(std::begin(colors[y * 3 + x]), std::end(colors[y * 3 + x]), rgb888 + y * 12 + x * 3);

	const VDPixmap rgbSource = MakePixmap(rgb888, 3, 2, 12, kPixFormat_RGB888);
	AT_PORTABLE_TEST_ASSERT(context, !buffer.Update(rgbSource, false));
	AT_PORTABLE_TEST_ASSERT(context, buffer.GetPixmap().data == nullptr);
	AT_PORTABLE_TEST_ASSERT(context, buffer.Update(rgbSource, true));

	const VDPixmap& converted = buffer.GetPixmap();
	AT_PORTABLE_TEST_ASSERT(context, converted.w == 3);
	AT_PORTABLE_TEST_ASSERT(context, converted.h == 2);
	AT_PORTABLE_TEST_ASSERT(context, converted.pitch >= 3 * (ptrdiff_t)sizeof(uint32));
	AT_PORTABLE_TEST_ASSERT(context, converted.format == kPixFormat_XRGB8888);
	static constexpr uint32 expected[][3] {
		{ 0x00112233, 0x00445566, 0x00778899 },
		{ 0x00AABBCC, 0x00010203, 0x00102030 }
	};
	for(int y = 0; y < 2; ++y) {
		const uint32 *row = converted.GetPixelRow<uint32>(y);
		for(int x = 0; x < 3; ++x)
			AT_PORTABLE_TEST_ASSERT(context, (row[x] & 0x00FFFFFF) == expected[y][x]);
	}

	// The presentation buffer owns its snapshot instead of borrowing the source.
	rgb888[0] = 0;
	AT_PORTABLE_TEST_ASSERT(context,
		(buffer.GetPixmap().GetPixelRow<uint32>(0)[0] & 0x00FFFFFF) == expected[0][0]);

	uint32 bottomUpPixels[] {
		0x00010203, 0x00102030,
		0x00A0B0C0, 0x000A0B0C
	};
	const VDPixmap bottomUpSource = MakePixmap(
		bottomUpPixels + 2, 2, 2, -(ptrdiff_t)(2 * sizeof(uint32)), kPixFormat_XRGB8888);
	AT_PORTABLE_TEST_ASSERT(context, buffer.Update(bottomUpSource, false));
	AT_PORTABLE_TEST_ASSERT(context, buffer.GetPixmap().GetPixelRow<uint32>(0)[0] == 0x00A0B0C0);
	AT_PORTABLE_TEST_ASSERT(context, buffer.GetPixmap().GetPixelRow<uint32>(0)[1] == 0x000A0B0C);
	AT_PORTABLE_TEST_ASSERT(context, buffer.GetPixmap().GetPixelRow<uint32>(1)[0] == 0x00010203);
	AT_PORTABLE_TEST_ASSERT(context, buffer.GetPixmap().GetPixelRow<uint32>(1)[1] == 0x00102030);

	VDPixmap invalidPitchSource = rgbSource;
	invalidPitchSource.pitch = 8;
	AT_PORTABLE_TEST_ASSERT(context, !buffer.Update(invalidPitchSource, true));
	AT_PORTABLE_TEST_ASSERT(context, buffer.GetPixmap().w == 2);
	AT_PORTABLE_TEST_ASSERT(context, buffer.GetPixmap().h == 2);
	AT_PORTABLE_TEST_ASSERT(context, buffer.GetPixmap().GetPixelRow<uint32>(0)[0] == 0x00A0B0C0);

	uint32 argbPixel = 0x80402010;
	const VDPixmap unsupportedSource = MakePixmap(&argbPixel, 1, 1, sizeof argbPixel, kPixFormat_ARGB8888);
	AT_PORTABLE_TEST_ASSERT(context, !buffer.Update(unsupportedSource, true));
	AT_PORTABLE_TEST_ASSERT(context, buffer.GetPixmap().w == 2);
	AT_PORTABLE_TEST_ASSERT(context, buffer.GetPixmap().h == 2);

	buffer.Clear();
	AT_PORTABLE_TEST_ASSERT(context, buffer.GetPixmap().data == nullptr);
	AT_PORTABLE_TEST_ASSERT(context, buffer.GetPixmap().w == 0);
	AT_PORTABLE_TEST_ASSERT(context, buffer.GetPixmap().h == 0);
	AT_PORTABLE_TEST_ASSERT(context, buffer.GetPixmap().format == kPixFormat_Null);
	return true;
}
