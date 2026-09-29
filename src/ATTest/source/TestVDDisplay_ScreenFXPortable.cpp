// Portable VDDisplay screen effect tests.

#include <algorithm>
#include <cmath>

#include <at/attest/portabletest.h>
#include <vd2/VDDisplay/displaytypes.h>
#include <vd2/VDDisplay/internal/screenfx.h>

namespace {
	bool NearlyEqual(float x, float y, float tolerance = 1e-5f) {
		return std::fabs(x - y) <= tolerance;
	}

	uint32 ChannelSum(const uint32 *pixels, uint32 n, uint32 shift) {
		uint32 sum = 0;
		for(uint32 i=0; i<n; ++i)
			sum += (pixels[i] >> shift) & 0xFF;
		return sum;
	}
}

bool ATTestVDDisplayScreenFX(ATPortableTestContext& context) {
	uint32 gammaRamp[4] {};
	VDDisplayCreateGammaRamp(gammaRamp, 4, false, 0.0f, 1.0f);
	AT_PORTABLE_TEST_ASSERT(context, gammaRamp[0] == 0x00000000);
	AT_PORTABLE_TEST_ASSERT(context, gammaRamp[1] == 0x40404040);
	AT_PORTABLE_TEST_ASSERT(context, gammaRamp[2] == 0x80808080);
	AT_PORTABLE_TEST_ASSERT(context, gammaRamp[3] == 0xBFBFBFBF);

	VDDisplayCreateGammaRamp(gammaRamp, 4, true, 2.0f, 1.0f);
	AT_PORTABLE_TEST_ASSERT(context, gammaRamp[0] == 0x00000000);
	AT_PORTABLE_TEST_ASSERT(context, gammaRamp[1] == 0x80808080);
	AT_PORTABLE_TEST_ASSERT(context, gammaRamp[2] == 0xB4B4B4B4);
	AT_PORTABLE_TEST_ASSERT(context, gammaRamp[3] == 0xDDDDDDDD);

	uint32 srgbRamp[256] {};
	VDDisplayCreateGammaRamp(srgbRamp, 256, true, 0.0f, 1.0f);
	for(uint32 i=1; i<256; ++i)
		AT_PORTABLE_TEST_ASSERT(context, srgbRamp[i] >= srgbRamp[i - 1]);

	uint32 scanlineTexture[6] {};
	VDDisplayCreateScanlineMaskTexture(scanlineTexture, 0, 1, 2, 6, 0.0f, true);
	AT_PORTABLE_TEST_ASSERT(context, scanlineTexture[0] == 0xFFFFFFFF);
	for(uint32 i=1; i<6; ++i)
		AT_PORTABLE_TEST_ASSERT(context, scanlineTexture[i] == 0x00000000);

	VDDisplayCreateScanlineMaskTexture(scanlineTexture, 0, 2, 2, 6, 0.0f, true);
	for(uint32 px : scanlineTexture)
		AT_PORTABLE_TEST_ASSERT(context, px == 0x80808080);

	VDDisplayCreateScanlineMaskTexture(scanlineTexture, 0, 1, 2, 6, 1.0f, false);
	for(uint32 px : scanlineTexture)
		AT_PORTABLE_TEST_ASSERT(context, px == 0xFFFFFFFF);

	std::fill(std::begin(scanlineTexture), std::end(scanlineTexture), 0xA5A5A5A5);
	VDDisplayCreateScanlineMaskTexture(scanlineTexture, 0, 1, 0, 6, 0.0f, true);
	for(uint32 px : scanlineTexture)
		AT_PORTABLE_TEST_ASSERT(context, px == 0xA5A5A5A5);

	uint32 pitchedScanlineTexture[6 * 2];
	std::fill(std::begin(pitchedScanlineTexture), std::end(pitchedScanlineTexture), 0xA5A5A5A5);
	VDDisplayCreateScanlineMaskTexture(pitchedScanlineTexture, 2 * sizeof(uint32), 1, 2, 6, 0.0f, true);
	AT_PORTABLE_TEST_ASSERT(context, pitchedScanlineTexture[0] == 0xFFFFFFFF);
	for(uint32 y=1; y<6; ++y)
		AT_PORTABLE_TEST_ASSERT(context, pitchedScanlineTexture[y * 2] == 0x00000000);
	for(uint32 y=0; y<6; ++y)
		AT_PORTABLE_TEST_ASSERT(context, pitchedScanlineTexture[y * 2 + 1] == 0xA5A5A5A5);

	VDDScreenMaskParams maskParams {};
	maskParams.mSourcePixelsPerDot = 3.0f;
	maskParams.mOpenness = 0.6f;

	const VDDisplayApertureGrilleParams apertureParams(maskParams, 640.0f, 320.0f);
	AT_PORTABLE_TEST_ASSERT(context, NearlyEqual(apertureParams.mPixelsPerTriad, 6.0f));
	AT_PORTABLE_TEST_ASSERT(context, NearlyEqual(apertureParams.mRedCenter, 1.0f / 6.0f));
	AT_PORTABLE_TEST_ASSERT(context, NearlyEqual(apertureParams.mGrnCenter, 0.5f));
	AT_PORTABLE_TEST_ASSERT(context, NearlyEqual(apertureParams.mBluCenter, 5.0f / 6.0f));
	AT_PORTABLE_TEST_ASSERT(context, NearlyEqual(apertureParams.mRedWidth, 0.1f));

	uint32 apertureTexture[18] {};
	VDDisplayCreateApertureGrilleTexture(apertureTexture, 18, 0.0f, apertureParams);
	for(uint32 i=6; i<12; ++i)
		AT_PORTABLE_TEST_ASSERT(context, apertureTexture[i] == apertureTexture[i + 6]);
	const uint32 apertureRed = ChannelSum(apertureTexture + 6, 12, 16);
	const uint32 apertureGreen = ChannelSum(apertureTexture + 6, 12, 8);
	const uint32 apertureBlue = ChannelSum(apertureTexture + 6, 12, 0);
	AT_PORTABLE_TEST_ASSERT(context, apertureRed > 0);
	AT_PORTABLE_TEST_ASSERT(context, apertureGreen > 0);
	AT_PORTABLE_TEST_ASSERT(context, apertureBlue > 0);
	AT_PORTABLE_TEST_ASSERT(context, apertureRed == apertureGreen);
	AT_PORTABLE_TEST_ASSERT(context, apertureGreen == apertureBlue);

	const VDDisplaySlotMaskParams slotParams(maskParams, 640.0f, 320.0f);
	AT_PORTABLE_TEST_ASSERT(context, NearlyEqual(slotParams.mPixelsPerBlockH, 6.0f));
	AT_PORTABLE_TEST_ASSERT(context, NearlyEqual(slotParams.mPixelsPerBlockV, 6.0f));
	AT_PORTABLE_TEST_ASSERT(context, NearlyEqual(slotParams.mRedCenter, 1.0f));
	AT_PORTABLE_TEST_ASSERT(context, NearlyEqual(slotParams.mGrnCenter, 3.0f));
	AT_PORTABLE_TEST_ASSERT(context, NearlyEqual(slotParams.mBluCenter, 5.0f));
	AT_PORTABLE_TEST_ASSERT(context, NearlyEqual(slotParams.mRedWidth, 0.6f));
	AT_PORTABLE_TEST_ASSERT(context, NearlyEqual(slotParams.mRedHeight, 2.6f));

	uint32 slotTexture[6 * 16];
	std::fill(std::begin(slotTexture), std::end(slotTexture), 0xDEADBEEF);
	VDDisplayCreateSlotMaskTexture(slotTexture, 16 * sizeof(uint32), 12, 6, 0.0f, 0.0f, 12.0f, 6.0f, slotParams);
	for(uint32 y=0; y<6; ++y) {
		for(uint32 x=0; x<12; ++x)
			AT_PORTABLE_TEST_ASSERT(context, (slotTexture[y * 16 + x] & 0xFF000000) == 0);
		for(uint32 x=12; x<16; ++x)
			AT_PORTABLE_TEST_ASSERT(context, slotTexture[y * 16 + x] == 0xDEADBEEF);
	}

	const VDDisplayTriadDotMaskParams dotParams(maskParams, 640.0f, 320.0f);
	AT_PORTABLE_TEST_ASSERT(context, NearlyEqual(dotParams.mPixelsPerTriadH, 12.0f));
	AT_PORTABLE_TEST_ASSERT(context, NearlyEqual(dotParams.mPixelsPerTriadV, 4.0f * std::sqrt(3.0f)));
	AT_PORTABLE_TEST_ASSERT(context, NearlyEqual(dotParams.mRedWidth, 1.2f));

	uint32 dotTexture[8 * 16];
	std::fill(std::begin(dotTexture), std::end(dotTexture), 0xDEADBEEF);
	VDDisplayCreateTriadDotMaskTexture(dotTexture, 16 * sizeof(uint32), 12, 8, 0.0f, 0.0f, 12.0f, 8.0f, dotParams);
	for(uint32 y=0; y<8; ++y) {
		for(uint32 x=0; x<12; ++x)
			AT_PORTABLE_TEST_ASSERT(context, (dotTexture[y * 16 + x] & 0xFF000000) == 0);
		for(uint32 x=12; x<16; ++x)
			AT_PORTABLE_TEST_ASSERT(context, dotTexture[y * 16 + x] == 0xDEADBEEF);
	}

	VDDisplayDistortionMapping mapping;
	mapping.Init(30.0f, 1.0f, 4.0f / 3.0f);
	vdfloat2 point { 0.2f, 0.3f };
	const vdfloat2 originalPoint = point;
	AT_PORTABLE_TEST_ASSERT(context, mapping.MapImageToScreen(point));
	AT_PORTABLE_TEST_ASSERT(context, mapping.MapScreenToImage(point));
	AT_PORTABLE_TEST_ASSERT(context, NearlyEqual(point.x, originalPoint.x));
	AT_PORTABLE_TEST_ASSERT(context, NearlyEqual(point.y, originalPoint.y));

	vdfloat2 outsidePoint { -0.25f, 1.25f };
	AT_PORTABLE_TEST_ASSERT(context, !mapping.MapImageToScreen(outsidePoint));
	AT_PORTABLE_TEST_ASSERT(context, outsidePoint.x >= 0.0f && outsidePoint.x <= 1.0f);
	AT_PORTABLE_TEST_ASSERT(context, outsidePoint.y >= 0.0f && outsidePoint.y <= 1.0f);

	return true;
}
