// Altirra portable accelerated RGB32 to YCbCr 709 tests

#include <array>
#include <cstring>

#include <at/attest/portabletest.h>
#include <vd2/Kasumi/pixmaputils.h>
#include "../../Kasumi/h/uberblit_input.h"
#include "../../Kasumi/h/uberblit_ycbcr_accel.h"

namespace {
	constexpr uint8 kGuard = 0xCD;
	constexpr sint32 kMaximumWidth = 17;
	constexpr sint32 kHeight = 2;
	constexpr size_t kSourcePrefix = 3;
	constexpr size_t kSourcePitch = kMaximumWidth * 4 + 7;
	constexpr size_t kSourceSuffix = 9;
	constexpr size_t kSourceSize =
		kSourcePrefix + kSourcePitch * kHeight + kSourceSuffix;
	constexpr size_t kOutputPrefix = 3;
	constexpr size_t kMaximumOutputPitch = 32;
	constexpr size_t kOutputSuffix = 9;
	constexpr size_t kOutputSize =
		kOutputPrefix + kMaximumOutputPitch * 3 + kOutputSuffix;

	using SourceStorage = std::array<uint8, kSourceSize>;
	using OutputStorage = std::array<uint8, kOutputSize>;

	uint8 ATClampConvertedChannel(sint32 value) {
		value >>= 15;

		if (value < 0)
			return 0;
		if (value > 255)
			return 255;

		return static_cast<uint8>(value);
	}

	void ATConvertPixel(const uint8 *source, uint8& cr, uint8& y, uint8& cb) {
		const sint32 b = source[0];
		const sint32 g = source[1];
		const sint32 r = source[2];

		y = ATClampConvertedChannel(
			2032 * b + 20127 * g + 5983 * r + 0x084000);
		cb = ATClampConvertedChannel(
			14392 * b - 11094 * g - 3298 * r + 0x404000);
		cr = ATClampConvertedChannel(
			-1320 * b - 13073 * g + 14392 * r + 0x404000);
	}

	SourceStorage ATMakeSource(sint32 width) {
		SourceStorage source;
		source.fill(kGuard);
		uint32 randomState = 0x70900000U + static_cast<uint32>(width);

		for(sint32 y = 0; y < kHeight; ++y) {
			uint8 *row = source.data() + kSourcePrefix + y * kSourcePitch;

			for(sint32 x = 0; x < width; ++x) {
				for(sint32 channel = 0; channel < 4; ++channel) {
					randomState = randomState * 1664525U + 1013904223U;
					row[x * 4 + channel] = static_cast<uint8>(randomState >> 24);
				}
			}
		}

		static constexpr uint8 kBoundaryColors[][4] = {
			{ 0, 0, 0, 0 },
			{ 255, 255, 255, 255 },
			{ 0, 0, 255, 17 },
			{ 0, 255, 0, 91 },
			{ 255, 0, 0, 203 },
		};

		for(sint32 x = 0; x < width && x < 5; ++x) {
			memcpy(
				source.data() + kSourcePrefix + x * 4,
				kBoundaryColors[x],
				4);
		}

		return source;
	}

	OutputStorage ATMakeExpected(
		const SourceStorage& source, sint32 width, sint32 y) {
		OutputStorage expected;
		expected.fill(kGuard);
		const size_t outputPitch = static_cast<size_t>((width + 15) & ~15);
		const uint8 *sourceRow =
			source.data() + kSourcePrefix + y * kSourcePitch;
		uint8 *cr = expected.data() + kOutputPrefix;
		uint8 *outputY = cr + outputPitch;
		uint8 *cb = outputY + outputPitch;

		for(sint32 x = 0; x < width; ++x)
			ATConvertPixel(sourceRow + x * 4, cr[x], outputY[x], cb[x]);

		return expected;
	}

	bool ATCheckWidth(ATPortableTestContext& context, sint32 width) {
		SourceStorage source = ATMakeSource(width);
		const SourceStorage originalSource = source;

		VDPixmapGenSrc sourceGenerator;
		const uint32 sourceType =
			kVDPixType_8888 | kVDPixSamp_444 | kVDPixSpace_BGR;
		sourceGenerator.Init(width, kHeight, sourceType, width * 4);
		sourceGenerator.SetSource(
			source.data() + kSourcePrefix, kSourcePitch, nullptr);

		VDPixmapGenRGB32ToYCbCr709_Accel converter;
		converter.Init(&sourceGenerator, 0);
		converter.AddWindowRequest(0, 0);
		converter.Start();

		AT_PORTABLE_TEST_ASSERT(context, converter.GetWidth(0) == width);
		AT_PORTABLE_TEST_ASSERT(context, converter.GetHeight(0) == kHeight);
		AT_PORTABLE_TEST_ASSERT(context, converter.IsStateful());
		const uint32 expectedType =
			(sourceType & ~(kVDPixType_Mask | kVDPixSpace_Mask))
				| kVDPixType_8 | kVDPixSpace_YCC_709;
		for(uint32 output = 0; output < 3; ++output)
			AT_PORTABLE_TEST_ASSERT(context, converter.GetType(output) == expectedType);

		const size_t outputPitch = static_cast<size_t>((width + 15) & ~15);
		for(sint32 y = 0; y < kHeight; ++y) {
			OutputStorage actual;
			actual.fill(kGuard);
			const OutputStorage expected = ATMakeExpected(source, width, y);
			converter.ProcessRow(actual.data() + kOutputPrefix, y);
			AT_PORTABLE_TEST_ASSERT(context, actual == expected);

			for(uint32 output = 0; output < 3; ++output) {
				const uint8 *row = static_cast<const uint8 *>(
					converter.GetRow(y, output));
				AT_PORTABLE_TEST_ASSERT(context, !memcmp(
					row,
					expected.data() + kOutputPrefix + outputPitch * output,
					width));
			}
		}

		AT_PORTABLE_TEST_ASSERT(context, source == originalSource);
		return true;
	}
}

bool ATTestKasumiYCbCr709(ATPortableTestContext& context) {
	for(sint32 width = 1; width <= kMaximumWidth; ++width) {
		if (!ATCheckWidth(context, width))
			return false;
	}

	return true;
}
