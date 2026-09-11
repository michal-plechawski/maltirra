// Altirra portable accelerated resampling stage tests

#include <algorithm>
#include <array>
#include <cstring>
#include <vector>

#include <at/attest/portabletest.h>
#include <vd2/Kasumi/resample_kernels.h>
#include "../../Kasumi/h/resample_stages_reference.h"

#if VD_CPU_ARM64
#include "../../Kasumi/h/resample_stages_arm64.h"
using ATResamplerRowStage32 = VDResamplerSeparableTableRowStageNEON;
using ATResamplerColStage32 = VDResamplerSeparableTableColStageNEON;
using ATResamplerRowStage8 = VDResamplerSeparableTableRowStage8NEON;
using ATResamplerColStage8 = VDResamplerSeparableTableColStage8NEON;
#elif VD_CPU_X64
#include <vd2/Kasumi/internal/resample_stages_x64.h>
using ATResamplerRowStage32 = VDResamplerSeparableTableRowStageSSE2;
using ATResamplerColStage32 = VDResamplerSeparableTableColStageSSE2;
using ATResamplerRowStage8 = VDResamplerSeparableTableRowStage8SSE2;
using ATResamplerColStage8 = VDResamplerSeparableTableColStage8SSE2;
#endif

#if VD_CPU_ARM64 || VD_CPU_X64
namespace {
	constexpr uint8 kGuard = 0xCD;
	constexpr uint32 kPixelGuard = 0xCDCDCDCDU;
	constexpr size_t kMaximumFilterWidth = 12;
	constexpr size_t kMaximumOutputWidth = 17;
	constexpr size_t kSourceWidth = 64;
	constexpr size_t kBytePrefix = 3;
	constexpr size_t kByteSuffix = 13;

	constexpr uint32 kOutputWidths[] = { 1, 2, 3, 4, 7, 8, 9, 17 };

	uint32 ATMakePixel(uint32 index, uint32 row = 0) {
		const uint32 b = (index * 37 + row * 29 + 11) & 0xFF;
		const uint32 g = (index * 73 + row * 17 + 91) & 0xFF;
		const uint32 r = (index * 19 + row * 101 + 227) & 0xFF;
		return b | (g << 8) | (r << 16);
	}

	uint8 ATMakeSample(uint32 index, uint32 row = 0) {
		return static_cast<uint8>(index * 53 + row * 97 + 31);
	}

	bool ATCheckRow32(
		ATPortableTestContext& context, const IVDResamplerFilter& filter) {
		std::array<uint32, kSourceWidth> source;
		for(uint32 i = 0; i < source.size(); ++i)
			source[i] = ATMakePixel(i);
		const auto originalSource = source;

		VDResamplerRowStageSeparableTable32 reference(filter);
		ATResamplerRowStage32 accelerated(filter);
		AT_PORTABLE_TEST_ASSERT(
			context, reference.GetWindowSize() == accelerated.GetWindowSize());

		for(uint32 width : kOutputWidths) {
			std::array<uint32, kMaximumOutputWidth + 2> expected;
			std::array<uint32, kMaximumOutputWidth + 2> actual;
			expected.fill(kPixelGuard);
			actual.fill(kPixelGuard);

			constexpr uint32 u = 0x00034000;
			constexpr uint32 dudx = 0x0000C000;
			reference.Process(expected.data() + 1, source.data(), width, u, dudx);
			accelerated.Process(actual.data() + 1, source.data(), width, u, dudx);
			AT_PORTABLE_TEST_ASSERT(context, actual == expected);
		}

		AT_PORTABLE_TEST_ASSERT(context, source == originalSource);
		return true;
	}

	template<typename Pixel, size_t RowWidth>
	using ATRows = std::array<std::array<Pixel, RowWidth>, kMaximumFilterWidth>;

	bool ATCheckCol32(
		ATPortableTestContext& context, const IVDResamplerFilter& filter) {
		ATRows<uint32, kMaximumOutputWidth + 4> rows;
		std::array<const void *, kMaximumFilterWidth> rowPointers;
		for(uint32 y = 0; y < rows.size(); ++y) {
			for(uint32 x = 0; x < rows[y].size(); ++x)
				rows[y][x] = ATMakePixel(x, y);
			rowPointers[y] = rows[y].data();
		}
		const auto originalRows = rows;

		VDResamplerColStageSeparableTable32 reference(filter);
		ATResamplerColStage32 accelerated(filter);
		AT_PORTABLE_TEST_ASSERT(
			context, reference.GetWindowSize() == accelerated.GetWindowSize());

		for(uint32 width : kOutputWidths) {
			for(sint32 phase : { 0x0000, 0x4000, 0xFF00 }) {
				std::array<uint32, kMaximumOutputWidth + 2> expected;
				std::array<uint32, kMaximumOutputWidth + 2> actual;
				expected.fill(kPixelGuard);
				actual.fill(kPixelGuard);
				reference.Process(
					expected.data() + 1, rowPointers.data(), width, phase);
				accelerated.Process(
					actual.data() + 1, rowPointers.data(), width, phase);
				AT_PORTABLE_TEST_ASSERT(context, actual == expected);
			}
		}

		AT_PORTABLE_TEST_ASSERT(context, rows == originalRows);
		return true;
	}

	using ATByteSource =
		std::array<uint8, kBytePrefix + kSourceWidth + kByteSuffix>;
	using ATByteOutput =
		std::array<uint8, kBytePrefix + kMaximumOutputWidth + kByteSuffix>;

	ATByteSource ATMakeByteSource() {
		ATByteSource source;
		source.fill(kGuard);
		for(uint32 i = 0; i < kSourceWidth; ++i)
			source[kBytePrefix + i] = ATMakeSample(i);
		return source;
	}

	bool ATByteOutputMatches(
		const ATByteOutput& actual,
		const ATByteOutput& expected,
		uint32 width,
		uint8 tolerance) {
		for(size_t i = 0; i < actual.size(); ++i) {
			if (i >= kBytePrefix && i < kBytePrefix + width) {
				const int difference =
					static_cast<int>(actual[i]) - static_cast<int>(expected[i]);
				if (difference < -tolerance || difference > tolerance)
					return false;
			} else if (actual[i] != expected[i]) {
				return false;
			}
		}

		return true;
	}

	bool ATCheckRow8Direct(
		ATPortableTestContext& context, const IVDResamplerFilter& filter) {
		const ATByteSource source = ATMakeByteSource();
		const ATByteSource originalSource = source;

		VDResamplerRowStageSeparableTable8 reference(filter);
		ATResamplerRowStage8 accelerated(filter);
		AT_PORTABLE_TEST_ASSERT(
			context, reference.GetWindowSize() == accelerated.GetWindowSize());

		for(uint32 width : kOutputWidths) {
			ATByteOutput expected;
			ATByteOutput actual;
			expected.fill(kGuard);
			actual.fill(kGuard);

			constexpr uint32 u = 0x00034000;
			constexpr uint32 dudx = 0x0000C000;
			reference.Process(
				expected.data() + kBytePrefix,
				source.data() + kBytePrefix, width, u, dudx);
			accelerated.Process(
				actual.data() + kBytePrefix,
				source.data() + kBytePrefix, width, u, dudx);
			AT_PORTABLE_TEST_ASSERT(context, actual == expected);
		}

		AT_PORTABLE_TEST_ASSERT(context, source == originalSource);
		return true;
	}

	bool ATCheckRow8Prepared(
		ATPortableTestContext& context,
		const IVDResamplerFilter& filter,
		uint32 dudx) {
		const ATByteSource source = ATMakeByteSource();
		const ATByteSource originalSource = source;

		for(uint32 width : kOutputWidths) {
			VDResamplerAxis axis {};
			axis.u = 0x00034000;
			axis.dudx = static_cast<sint32>(dudx);
			axis.dx_active = width;

			VDResamplerRowStageSeparableTable8 reference(filter);
			ATResamplerRowStage8 accelerated(filter);
			IVDResamplerSeparableRowStage2 *prepared = accelerated.AsRowStage2();
			AT_PORTABLE_TEST_ASSERT(context, prepared != nullptr);
			prepared->Init(axis, kSourceWidth);

			ATByteOutput expected;
			ATByteOutput actual;
			expected.fill(kGuard);
			actual.fill(kGuard);
			reference.Process(
				expected.data() + kBytePrefix,
				source.data() + kBytePrefix,
				width,
				static_cast<uint32>(axis.u),
				static_cast<uint32>(axis.dudx));
			prepared->Process(
				actual.data() + kBytePrefix,
				source.data() + kBytePrefix,
				width);
			const uint8 tolerance =
				filter.GetFilterWidth() == 2 && dudx <= 0x00020000 ? 1 : 0;
			AT_PORTABLE_TEST_ASSERT(
				context, ATByteOutputMatches(actual, expected, width, tolerance));
		}

		AT_PORTABLE_TEST_ASSERT(context, source == originalSource);
		return true;
	}

	bool ATCheckRow8Clipped(
		ATPortableTestContext& context,
		const IVDResamplerFilter& filter,
		uint32 sourceWidth,
		sint32 destinationWidth,
		sint32 u,
		sint32 dudx) {
		AT_PORTABLE_TEST_ASSERT(context, sourceWidth > 0);
		AT_PORTABLE_TEST_ASSERT(context, sourceWidth <= kSourceWidth);
		AT_PORTABLE_TEST_ASSERT(
			context, destinationWidth <= static_cast<sint32>(kMaximumOutputWidth));

		const ATByteSource source = ATMakeByteSource();
		const ATByteSource originalSource = source;
		const uint8 *sourcePixels = source.data() + kBytePrefix;

		VDResamplerAxis axis {};
		axis.Init(dudx);
		axis.Compute(
			destinationWidth,
			u,
			static_cast<sint32>(sourceWidth),
			filter.GetFilterWidth());
		const uint32 middleWidth =
			axis.dx_preclip + axis.dx_active + axis.dx_postclip + axis.dx_dualclip;
		AT_PORTABLE_TEST_ASSERT(context, middleWidth > 0);

		ATResamplerRowStage8 accelerated(filter);
		IVDResamplerSeparableRowStage2 *prepared = accelerated.AsRowStage2();
		AT_PORTABLE_TEST_ASSERT(context, prepared != nullptr);
		prepared->Init(axis, sourceWidth);

		ATByteOutput expected;
		ATByteOutput actual;
		expected.fill(kGuard);
		actual.fill(kGuard);
		prepared->Process(
			actual.data() + kBytePrefix, sourcePixels, middleWidth);

		if (sourceWidth == 1) {
			std::fill_n(
				expected.data() + kBytePrefix, middleWidth, sourcePixels[0]);
		} else {
			const uint32 filterWidth = filter.GetFilterWidth();
			std::vector<sint32> filterBank(filterWidth * 256);
			VDResamplerGenerateTable(filterBank.data(), filter);

			for(uint32 x = 0; x < middleWidth; ++x) {
				const sint32 position = axis.u + axis.dudx * static_cast<sint32>(x);
				const sint32 sourceOffset = position >> 16;
				const sint32 *coefficients = filterBank.data()
					+ filterWidth * ((position >> 8) & 0xFF);
				sint32 accumulator = 0x2000;

				for(uint32 tap = 0; tap < filterWidth; ++tap) {
					const sint32 sampleOffset = std::clamp<sint32>(
						sourceOffset + static_cast<sint32>(tap),
						0,
						static_cast<sint32>(sourceWidth) - 1);
					accumulator += sourcePixels[sampleOffset] * coefficients[tap];
				}

				accumulator >>= 14;
				expected[kBytePrefix + x] = static_cast<uint8>(
					std::clamp<sint32>(accumulator, 0, 255));
			}
		}

		const uint8 tolerance = filter.GetFilterWidth() == 2 ? 1 : 0;
		AT_PORTABLE_TEST_ASSERT(
			context,
			ATByteOutputMatches(actual, expected, middleWidth, tolerance));
		AT_PORTABLE_TEST_ASSERT(context, source == originalSource);
		return true;
	}

	bool ATCheckRow8SingleSample(
		ATPortableTestContext& context, const IVDResamplerFilter& filter) {
		const ATByteSource source = ATMakeByteSource();
		const ATByteSource originalSource = source;
		VDResamplerAxis axis {};
		axis.dx_active = kMaximumOutputWidth;

		ATResamplerRowStage8 accelerated(filter);
		IVDResamplerSeparableRowStage2 *prepared = accelerated.AsRowStage2();
		AT_PORTABLE_TEST_ASSERT(context, prepared != nullptr);
		prepared->Init(axis, 1);

		ATByteOutput expected;
		ATByteOutput actual;
		expected.fill(kGuard);
		actual.fill(kGuard);
		std::fill_n(
			expected.data() + kBytePrefix,
			kMaximumOutputWidth,
			source[kBytePrefix]);
		prepared->Process(
			actual.data() + kBytePrefix,
			source.data() + kBytePrefix,
			kMaximumOutputWidth);

		AT_PORTABLE_TEST_ASSERT(context, actual == expected);
		AT_PORTABLE_TEST_ASSERT(context, source == originalSource);
		return true;
	}

	bool ATCheckCol8(
		ATPortableTestContext& context, const IVDResamplerFilter& filter) {
		constexpr size_t kRowWidth =
			kBytePrefix + kMaximumOutputWidth + kByteSuffix;
		ATRows<uint8, kRowWidth> rows;
		std::array<const void *, kMaximumFilterWidth> rowPointers;
		for(uint32 y = 0; y < rows.size(); ++y) {
			rows[y].fill(kGuard);
			for(uint32 x = 0; x < kMaximumOutputWidth; ++x)
				rows[y][kBytePrefix + x] = ATMakeSample(x, y);
			rowPointers[y] = rows[y].data() + kBytePrefix;
		}
		const auto originalRows = rows;

		VDResamplerColStageSeparableTable8 reference(filter);
		ATResamplerColStage8 accelerated(filter);
		AT_PORTABLE_TEST_ASSERT(
			context, reference.GetWindowSize() == accelerated.GetWindowSize());

		for(uint32 width : kOutputWidths) {
			for(sint32 phase : { 0x0000, 0x4000, 0xFE00 }) {
				ATByteOutput expected;
				ATByteOutput actual;
				expected.fill(kGuard);
				actual.fill(kGuard);
				reference.Process(
					expected.data() + kBytePrefix,
					rowPointers.data(), width, phase);
				accelerated.Process(
					actual.data() + kBytePrefix,
					rowPointers.data(), width, phase);
				const uint8 tolerance = filter.GetFilterWidth() == 2 ? 1 : 0;
				AT_PORTABLE_TEST_ASSERT(
					context, ATByteOutputMatches(actual, expected, width, tolerance));
			}
		}

		AT_PORTABLE_TEST_ASSERT(context, rows == originalRows);
		return true;
	}

	bool ATCheckFilter(
		ATPortableTestContext& context, const IVDResamplerFilter& filter) {
		AT_PORTABLE_TEST_ASSERT(
			context, filter.GetFilterWidth() <= kMaximumFilterWidth);
		if (!ATCheckRow32(context, filter))
			return false;
		if (!ATCheckCol32(context, filter))
			return false;
		if (!ATCheckRow8Direct(context, filter))
			return false;
		if (!ATCheckRow8Prepared(context, filter, 0x0000C000))
			return false;
		if (!ATCheckCol8(context, filter))
			return false;
		return true;
	}
}
#endif

bool ATTestKasumiResampleStages(ATPortableTestContext& context) {
#if VD_CPU_ARM64 || VD_CPU_X64
	VDResamplerLinearFilter linear2(1.0);
	VDResamplerLinearFilter linear4(0.5);
	VDResamplerCubicFilter cubic6(2.0 / 3.0, -0.75);
	VDResamplerCubicFilter cubic8(0.5, -0.75);
	VDResamplerCubicFilter cubic10(0.4, -0.75);
	VDResamplerLanczos3Filter lanczos12(0.5);

	AT_PORTABLE_TEST_ASSERT(context, linear2.GetFilterWidth() == 2);
	AT_PORTABLE_TEST_ASSERT(context, linear4.GetFilterWidth() == 4);
	AT_PORTABLE_TEST_ASSERT(context, cubic6.GetFilterWidth() == 6);
	AT_PORTABLE_TEST_ASSERT(context, cubic8.GetFilterWidth() == 8);
	AT_PORTABLE_TEST_ASSERT(context, cubic10.GetFilterWidth() == 10);
	AT_PORTABLE_TEST_ASSERT(context, lanczos12.GetFilterWidth() == 12);

	if (!ATCheckFilter(context, linear2))
		return false;
	if (!ATCheckRow8Prepared(context, linear2, 0x00024000))
		return false;
	if (!ATCheckFilter(context, linear4))
		return false;
	if (!ATCheckFilter(context, cubic6))
		return false;
	if (!ATCheckFilter(context, cubic8))
		return false;
	if (!ATCheckFilter(context, cubic10))
		return false;
	if (!ATCheckFilter(context, lanczos12))
		return false;

	if (!ATCheckRow8SingleSample(context, linear2))
		return false;
	if (!ATCheckRow8Clipped(context, linear2, 5, 17, -0x00008000, 0x00006000))
		return false;
	if (!ATCheckRow8Clipped(context, cubic6, 3, 17, -0x00020000, 0x0000C000))
		return false;
	if (!ATCheckRow8Clipped(context, lanczos12, 7, 17, -0x00050000, 0x0000C000))
		return false;
#else
	(void)context;
#endif

	return true;
}
