// Portable reference tests for ARM64 NTSC artifacting kernels.

#include <algorithm>
#include <array>
#include <cstddef>
#include <initializer_list>
#include <vector>

#include <at/attest/portabletest.h>
#include <artifacting_neon.h>

#if defined(VD_CPU_ARM64)
namespace {
	constexpr size_t kVectorLanes = 8;

	uint8 NarrowAccumulator(sint32 value) {
		return (uint8)std::clamp<sint32>(value >> 4, 0, 255);
	}

	std::vector<uint8> ReferenceAccum(const std::vector<sint16>& table, const std::vector<uint8>& src, bool twin) {
		const size_t groupCount = src.size() / 4;
		std::vector<sint32> accum((groupCount + 2) * kVectorLanes);

		for(size_t group = 0; group < groupCount; ++group) {
			if (twin) {
				for(size_t phase : { size_t(0), size_t(2) }) {
					const size_t vectorBase = (size_t)src[group * 4 + phase] * 8 + phase * 2;
					for(size_t stage = 0; stage < 3; ++stage) {
						for(size_t lane = 0; lane < kVectorLanes; ++lane)
							accum[(group + stage) * kVectorLanes + lane] += table[(vectorBase + stage) * kVectorLanes + lane];
					}
				}
			} else {
				for(size_t phase = 0; phase < 4; ++phase) {
					const size_t vectorBase = (size_t)src[group * 4 + phase] * 16 + phase * 4;
					for(size_t stage = 0; stage < 3; ++stage) {
						for(size_t lane = 0; lane < kVectorLanes; ++lane)
							accum[(group + stage) * kVectorLanes + lane] += table[(vectorBase + stage) * kVectorLanes + lane];
					}
				}
			}
		}

		std::vector<uint8> result(accum.size());
		for(size_t i = 0; i < result.size(); ++i)
			result[i] = NarrowAccumulator(accum[i]);
		return result;
	}

	std::vector<sint16> MakeAccumTable() {
		constexpr size_t normalVectorCount = 256 * 16;
		constexpr size_t specialVectorBase = 0x1800;
		std::vector<sint16> table((specialVectorBase + 256 * 4 + 4) * kVectorLanes);
		for(size_t vector = 0; vector < normalVectorCount; ++vector) {
			for(size_t lane = 0; lane < kVectorLanes; ++lane)
				table[vector * kVectorLanes + lane] = (sint16)((sint32)((vector * 11 + lane * 7) % 61) - 18);
		}

		for(size_t color = 0; color < 256; ++color) {
			for(size_t stage = 0; stage < 3; ++stage) {
				for(size_t lane = 0; lane < kVectorLanes; ++lane) {
					sint32 sum = 0;
					for(size_t phase = 0; phase < 4; ++phase)
						sum += table[(color * 16 + phase * 4 + stage) * kVectorLanes + lane];
					table[(specialVectorBase + color * 4 + stage) * kVectorLanes + lane] = (sint16)sum;
				}
			}
		}
		return table;
	}

	std::vector<sint16> MakeTwinTable() {
		constexpr size_t normalVectorCount = 256 * 8;
		constexpr size_t specialVectorBase = 0x800;
		std::vector<sint16> table((specialVectorBase + 256 * 4 + 4) * kVectorLanes);
		for(size_t vector = 0; vector < normalVectorCount; ++vector) {
			for(size_t lane = 0; lane < kVectorLanes; ++lane)
				table[vector * kVectorLanes + lane] = (sint16)((sint32)((vector * 13 + lane * 5) % 57) - 16);
		}

		for(size_t color = 0; color < 256; ++color) {
			for(size_t stage = 0; stage < 3; ++stage) {
				for(size_t lane = 0; lane < kVectorLanes; ++lane) {
					const sint32 sum = table[(color * 8 + stage) * kVectorLanes + lane]
						+ table[(color * 8 + 4 + stage) * kVectorLanes + lane];
					table[(specialVectorBase + color * 4 + stage) * kVectorLanes + lane] = (sint16)sum;
				}
			}
		}
		return table;
	}

	bool CheckAccum(ATPortableTestContext& context, const std::vector<sint16>& table, const std::vector<uint8>& src, bool twin) {
		const std::vector<uint8> expected = ReferenceAccum(table, src, twin);
		std::vector<uint8> actual(expected.size(), 0xcd);
		if (twin)
			ATArtifactNTSCAccumTwin_NEON(actual.data(), table.data(), src.data(), (uint32)src.size());
		else
			ATArtifactNTSCAccum_NEON(actual.data(), table.data(), src.data(), (uint32)src.size());
		AT_PORTABLE_TEST_ASSERT(context, actual == expected);
		return true;
	}

	bool TestAccumulation(ATPortableTestContext& context) {
		const auto table = MakeAccumTable();
		const std::vector<uint8> slowSrc { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12 };
		const std::vector<uint8> fastSrc(16, 23);
		return CheckAccum(context, table, slowSrc, false)
			&& CheckAccum(context, table, fastSrc, false);
	}

	bool TestTwinAccumulation(ATPortableTestContext& context) {
		const auto table = MakeTwinTable();
		const std::vector<uint8> slowSrc { 1, 20, 2, 21, 3, 22, 4, 23, 5, 24, 6, 25 };
		const std::vector<uint8> fastSrc(16, 17);
		return CheckAccum(context, table, slowSrc, true)
			&& CheckAccum(context, table, fastSrc, true);
	}

	bool TestFinalPacking(ATPortableTestContext& context) {
		constexpr uint32 count = 8;
		std::array<uint8, count * 2> red {};
		std::array<uint8, count * 2> green {};
		std::array<uint8, count * 2> blue {};
		for(size_t i = 0; i < red.size(); ++i) {
			red[i] = (uint8)(i * 13 + 1);
			green[i] = (uint8)(i * 9 + 2);
			blue[i] = (uint8)(i * 5 + 3);
		}

		std::array<uint8, count * 2 * 4> actual {};
		ATArtifactNTSCFinal_NEON(actual.data(), red.data(), green.data(), blue.data(), count);
		for(size_t i = 0; i < red.size(); ++i) {
			AT_PORTABLE_TEST_ASSERT(context, actual[i * 4 + 0] == blue[i]);
			AT_PORTABLE_TEST_ASSERT(context, actual[i * 4 + 1] == green[i]);
			AT_PORTABLE_TEST_ASSERT(context, actual[i * 4 + 2] == red[i]);
			AT_PORTABLE_TEST_ASSERT(context, actual[i * 4 + 3] == 0xff);
		}
		return true;
	}
}
#endif

bool ATTestAltirraArtifactingNTSCNEON(ATPortableTestContext& context) {
#if defined(VD_CPU_ARM64)
	return TestAccumulation(context)
		&& TestTwinAccumulation(context)
		&& TestFinalPacking(context);
#else
	(void)context;
	return true;
#endif
}
