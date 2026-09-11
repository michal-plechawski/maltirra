// Altirra portable reference YCbCr conversion tests

#include <algorithm>
#include <array>
#include <cstring>
#include <iterator>

#include <at/attest/portabletest.h>
#include <vd2/Kasumi/pixmap.h>

#define AT_DECLARE_YUV_REFERENCE(source, destination) \
	extern void VDCDECL VDPixmapBlt_##source##_to_##destination##_reference( \
		void *dst, ptrdiff_t dstPitch, const void *src, ptrdiff_t srcPitch, \
		vdpixsize width, vdpixsize height)

AT_DECLARE_YUV_REFERENCE(UYVY, XRGB1555);
AT_DECLARE_YUV_REFERENCE(UYVY, RGB565);
AT_DECLARE_YUV_REFERENCE(UYVY, RGB888);
AT_DECLARE_YUV_REFERENCE(UYVY, XRGB8888);
AT_DECLARE_YUV_REFERENCE(YUYV, XRGB8888);
AT_DECLARE_YUV_REFERENCE(Y8, XRGB1555);
AT_DECLARE_YUV_REFERENCE(Y8, RGB565);
AT_DECLARE_YUV_REFERENCE(Y8, RGB888);
AT_DECLARE_YUV_REFERENCE(Y8, XRGB8888);

extern void VDCDECL VDPixmapBlt_YUVPlanar_decode_reference(
	const VDPixmap& dst, const VDPixmap& src, vdpixsize width, vdpixsize height);
extern void VDCDECL VDPixmapBlt_YUVPlanar_convert_reference(
	const VDPixmap& dst, const VDPixmap& src, vdpixsize width, vdpixsize height);

namespace {
	constexpr uint8 kGuard = 0xCD;

	bool ATAllBytesAre(const uint8 *data, size_t size, uint8 value) {
		for(size_t i = 0; i < size; ++i) {
			if (data[i] != value)
				return false;
		}

		return true;
	}

	VDPixmap ATMakePixmap(
		void *data,
		ptrdiff_t pitch,
		int width,
		int height,
		int format,
		void *data2 = nullptr,
		ptrdiff_t pitch2 = 0,
		void *data3 = nullptr,
		ptrdiff_t pitch3 = 0) {
		VDPixmap pixmap {};
		pixmap.data = data;
		pixmap.w = width;
		pixmap.h = height;
		pixmap.pitch = pitch;
		pixmap.format = format;
		pixmap.data2 = data2;
		pixmap.pitch2 = pitch2;
		pixmap.data3 = data3;
		pixmap.pitch3 = pitch3;
		return pixmap;
	}
}

bool ATTestKasumiYUVReference(ATPortableTestContext& context) {
	using namespace nsVDPixmap;

	static constexpr uint8 kLuma[5] = { 16, 81, 126, 145, 235 };
	static constexpr uint8 kGray[5] = { 0, 76, 128, 150, 255 };
	uint8 lumaRows[16] {};
	std::copy(std::begin(kLuma), std::end(kLuma), lumaRows);
	std::reverse_copy(std::begin(kLuma), std::end(kLuma), lumaRows + 8);

	alignas(16) std::array<uint8, 68> rgb32Storage;
	rgb32Storage.fill(kGuard);
	VDPixmapBlt_Y8_to_XRGB8888_reference(
		rgb32Storage.data() + 4, 28, lumaRows, 8, 5, 2);
	AT_PORTABLE_TEST_ASSERT(context,
		ATAllBytesAre(rgb32Storage.data(), 4, kGuard));
	for(int y = 0; y < 2; ++y) {
		const auto *row = reinterpret_cast<const uint32 *>(
			rgb32Storage.data() + 4 + 28 * y);
		for(int x = 0; x < 5; ++x) {
			const uint8 gray = kGray[y ? 4 - x : x];
			AT_PORTABLE_TEST_ASSERT(context, row[x] == 0x010101U * gray);
		}
		AT_PORTABLE_TEST_ASSERT(context,
			ATAllBytesAre(reinterpret_cast<const uint8 *>(row + 5), 8, kGuard));
	}
	AT_PORTABLE_TEST_ASSERT(context,
		ATAllBytesAre(rgb32Storage.data() + 60, 8, kGuard));

	std::array<uint8, 44> rgb24Storage;
	rgb24Storage.fill(kGuard);
	VDPixmapBlt_Y8_to_RGB888_reference(
		rgb24Storage.data() + 3, 19, lumaRows, 8, 5, 2);
	for(int y = 0; y < 2; ++y) {
		const uint8 *row = rgb24Storage.data() + 3 + 19 * y;
		for(int x = 0; x < 5; ++x) {
			const uint8 gray = kGray[y ? 4 - x : x];
			AT_PORTABLE_TEST_ASSERT(context,
				row[3 * x] == gray && row[3 * x + 1] == gray
					&& row[3 * x + 2] == gray);
		}
		AT_PORTABLE_TEST_ASSERT(context, ATAllBytesAre(row + 15, 4, kGuard));
	}
	AT_PORTABLE_TEST_ASSERT(context,
		ATAllBytesAre(rgb24Storage.data() + 41, 3, kGuard));

	alignas(16) uint16 rgb1555[5] {};
	alignas(16) uint16 rgb565[5] {};
	VDPixmapBlt_Y8_to_XRGB1555_reference(rgb1555, 10, kLuma, 5, 5, 1);
	VDPixmapBlt_Y8_to_RGB565_reference(rgb565, 10, kLuma, 5, 5, 1);
	AT_PORTABLE_TEST_ASSERT(context, rgb1555[0] == 0 && rgb1555[4] == 0x7FFF);
	AT_PORTABLE_TEST_ASSERT(context, rgb565[0] == 0 && rgb565[4] == 0xFFFF);
	AT_PORTABLE_TEST_ASSERT(context, rgb1555[2] == 0x4210);
	AT_PORTABLE_TEST_ASSERT(context, rgb565[2] == 0x8410);

	static constexpr uint8 kUYVY[12] = {
		90, 81, 240, 81,
		54, 145, 34, 145,
		240, 41, 110, 41,
	};
	static constexpr uint8 kYUYV[12] = {
		81, 90, 81, 240,
		145, 54, 145, 34,
		41, 240, 41, 110,
	};
	alignas(16) uint32 packedUYVY[6] {};
	alignas(16) uint32 packedYUYV[6] {};
	VDPixmapBlt_UYVY_to_XRGB8888_reference(
		packedUYVY, sizeof packedUYVY, kUYVY, sizeof kUYVY, 6, 1);
	VDPixmapBlt_YUYV_to_XRGB8888_reference(
		packedYUYV, sizeof packedYUYV, kYUYV, sizeof kYUYV, 6, 1);
	AT_PORTABLE_TEST_ASSERT(context,
		!memcmp(packedUYVY, packedYUYV, sizeof packedUYVY));
	AT_PORTABLE_TEST_ASSERT(context, packedUYVY[0] == 0x00FF0000);
	AT_PORTABLE_TEST_ASSERT(context, packedUYVY[2] == 0x0000FF01);
	AT_PORTABLE_TEST_ASSERT(context, packedUYVY[4] == 0x000000FF);
	AT_PORTABLE_TEST_ASSERT(context, packedUYVY[5] == 0x000000FF);

	alignas(16) uint8 packedRGB24[18] {};
	alignas(16) uint16 packed1555[6] {};
	alignas(16) uint16 packed565[6] {};
	VDPixmapBlt_UYVY_to_RGB888_reference(
		packedRGB24, sizeof packedRGB24, kUYVY, sizeof kUYVY, 6, 1);
	VDPixmapBlt_UYVY_to_XRGB1555_reference(
		packed1555, sizeof packed1555, kUYVY, sizeof kUYVY, 6, 1);
	VDPixmapBlt_UYVY_to_RGB565_reference(
		packed565, sizeof packed565, kUYVY, sizeof kUYVY, 6, 1);
	AT_PORTABLE_TEST_ASSERT(context,
		packedRGB24[0] == 0 && packedRGB24[1] == 0 && packedRGB24[2] == 255);
	AT_PORTABLE_TEST_ASSERT(context, packed1555[0] == 0x7C00);
	AT_PORTABLE_TEST_ASSERT(context, packed565[0] == 0xF800);

	uint8 y444[16] {};
	uint8 cb444[16] {};
	uint8 cr444[16] {};
	for(int y = 0; y < 2; ++y) {
		const uint8 yValues[6] = { 81, 145, 41, 16, 126, 235 };
		const uint8 cbValues[6] = { 90, 54, 240, 128, 128, 128 };
		const uint8 crValues[6] = { 240, 34, 110, 128, 128, 128 };
		memcpy(y444 + y * 8, yValues, 6);
		memcpy(cb444 + y * 8, cbValues, 6);
		memcpy(cr444 + y * 8, crValues, 6);
	}
	alignas(16) std::array<uint8, 64> planarRGB;
	planarRGB.fill(kGuard);
	const VDPixmap src444 = ATMakePixmap(
		y444, 8, 6, 2, kPixFormat_YUV444_Planar,
		cb444, 8, cr444, 8);
	const VDPixmap dst444 = ATMakePixmap(
		planarRGB.data() + 4, 28, 6, 2, kPixFormat_XRGB8888);
	VDPixmapBlt_YUVPlanar_decode_reference(dst444, src444, 6, 2);
	for(int y = 0; y < 2; ++y) {
		const auto *row = reinterpret_cast<const uint32 *>(
			planarRGB.data() + 4 + y * 28);
		AT_PORTABLE_TEST_ASSERT(context, row[0] == 0x00FF0000);
		AT_PORTABLE_TEST_ASSERT(context, row[1] == 0x0000FF01);
		AT_PORTABLE_TEST_ASSERT(context, row[2] == 0x000000FF);
		AT_PORTABLE_TEST_ASSERT(context, row[3] == 0x00000000);
		AT_PORTABLE_TEST_ASSERT(context, row[4] == 0x00808080);
		AT_PORTABLE_TEST_ASSERT(context, row[5] == 0x00FFFFFF);
		AT_PORTABLE_TEST_ASSERT(context, ATAllBytesAre(
			reinterpret_cast<const uint8 *>(row + 6), 4, kGuard));
	}

	uint8 y420[21];
	uint8 cb420[10];
	uint8 cr420[10];
	memset(y420, 81, sizeof y420);
	memset(cb420, 90, sizeof cb420);
	memset(cr420, 240, sizeof cr420);
	alignas(16) std::array<uint8, 88> planar420RGB;
	planar420RGB.fill(kGuard);
	const VDPixmap src420 = ATMakePixmap(
		y420, 7, 5, 3, kPixFormat_YUV420_Planar,
		cb420, 5, cr420, 5);
	const VDPixmap dst420RGB = ATMakePixmap(
		planar420RGB.data() + 4, 28, 5, 3, kPixFormat_XRGB8888);
	VDPixmapBlt_YUVPlanar_decode_reference(dst420RGB, src420, 5, 3);
	for(int y = 0; y < 3; ++y) {
		const auto *row = reinterpret_cast<const uint32 *>(
			planar420RGB.data() + 4 + y * 28);
		for(int x = 0; x < 5; ++x)
			AT_PORTABLE_TEST_ASSERT(context, row[x] == 0x00FF0000);
		AT_PORTABLE_TEST_ASSERT(context,
			ATAllBytesAre(reinterpret_cast<const uint8 *>(row + 5), 8, kGuard));
	}

	uint8 convertedY[24];
	uint8 convertedCb[10];
	uint8 convertedCr[10];
	memset(convertedY, kGuard, sizeof convertedY);
	memset(convertedCb, kGuard, sizeof convertedCb);
	memset(convertedCr, kGuard, sizeof convertedCr);
	const VDPixmap sourceY8 = ATMakePixmap(
		y420, 7, 5, 3, kPixFormat_Y8);
	const VDPixmap converted420 = ATMakePixmap(
		convertedY, 8, 5, 3, kPixFormat_YUV420_Planar,
		convertedCb, 5, convertedCr, 5);
	VDPixmapBlt_YUVPlanar_convert_reference(converted420, sourceY8, 5, 3);
	for(int y = 0; y < 3; ++y) {
		AT_PORTABLE_TEST_ASSERT(context,
			ATAllBytesAre(convertedY + y * 8, 5, 81));
		AT_PORTABLE_TEST_ASSERT(context,
			ATAllBytesAre(convertedY + y * 8 + 5, 3, kGuard));
	}
	for(int y = 0; y < 2; ++y) {
		AT_PORTABLE_TEST_ASSERT(context,
			ATAllBytesAre(convertedCb + y * 5, 3, 128));
		AT_PORTABLE_TEST_ASSERT(context,
			ATAllBytesAre(convertedCr + y * 5, 3, 128));
		AT_PORTABLE_TEST_ASSERT(context,
			ATAllBytesAre(convertedCb + y * 5 + 3, 2, kGuard));
		AT_PORTABLE_TEST_ASSERT(context,
			ATAllBytesAre(convertedCr + y * 5 + 3, 2, kGuard));
	}

	uint8 source444Y[21];
	uint8 source444Cb[21];
	uint8 source444Cr[21];
	uint8 compressedY[24];
	uint8 compressedCb[10];
	uint8 compressedCr[10];
	memset(source444Y, 81, sizeof source444Y);
	memset(source444Cb, 90, sizeof source444Cb);
	memset(source444Cr, 240, sizeof source444Cr);
	memset(compressedY, kGuard, sizeof compressedY);
	memset(compressedCb, kGuard, sizeof compressedCb);
	memset(compressedCr, kGuard, sizeof compressedCr);
	const VDPixmap constant444 = ATMakePixmap(
		source444Y, 7, 5, 3, kPixFormat_YUV444_Planar,
		source444Cb, 7, source444Cr, 7);
	const VDPixmap compressed420 = ATMakePixmap(
		compressedY, 8, 5, 3, kPixFormat_YUV420_Planar,
		compressedCb, 5, compressedCr, 5);
	VDPixmapBlt_YUVPlanar_convert_reference(compressed420, constant444, 5, 3);
	for(int y = 0; y < 3; ++y) {
		AT_PORTABLE_TEST_ASSERT(context,
			ATAllBytesAre(compressedY + y * 8, 5, 81));
		AT_PORTABLE_TEST_ASSERT(context,
			ATAllBytesAre(compressedY + y * 8 + 5, 3, kGuard));
	}
	for(int y = 0; y < 2; ++y) {
		AT_PORTABLE_TEST_ASSERT(context,
			ATAllBytesAre(compressedCb + y * 5, 3, 90));
		AT_PORTABLE_TEST_ASSERT(context,
			ATAllBytesAre(compressedCr + y * 5, 3, 240));
		AT_PORTABLE_TEST_ASSERT(context,
			ATAllBytesAre(compressedCb + y * 5 + 3, 2, kGuard));
		AT_PORTABLE_TEST_ASSERT(context,
			ATAllBytesAre(compressedCr + y * 5 + 3, 2, kGuard));
	}

	return true;
}
