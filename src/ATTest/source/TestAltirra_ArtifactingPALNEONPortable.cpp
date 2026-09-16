// Portable reference tests for ARM64 PAL artifacting kernels.

#include <algorithm>
#include <array>
#include <cstddef>
#include <initializer_list>
#include <vector>

#include <at/attest/portabletest.h>
#include <artifacting_neon.h>
#include <artifacting_pal.h>

#if defined(VD_CPU_ARM64)
namespace {
	uint32 PackSigned(sint32 lo, sint32 hi) {
		return (uint16)(sint16)lo | ((uint32)(uint16)(sint16)hi << 16);
	}

	sint16 PackedLane(const uint32 *words, size_t lane) {
		const uint32 word = words[lane >> 1];
		return (sint16)(lane & 1 ? word >> 16 : word);
	}

	std::vector<uint32> MakeKernels(size_t wordStride, uint32 seed) {
		std::vector<uint32> kernels(256 * wordStride);
		for(size_t word = 0; word < kernels.size(); ++word) {
			const sint32 lo = (sint32)((word * 11 + seed) % 31) - 15;
			const sint32 hi = (sint32)((word * 7 + seed * 3) % 29) - 14;
			kernels[word] = PackSigned(lo, hi);
		}
		return kernels;
	}

	std::vector<uint32> ReferenceFilter(const std::vector<uint8>& src, const std::vector<uint32>& kernels,
		size_t wordStride, size_t phaseStep, size_t kernelWords, bool twin) {
		const size_t outputWordCount = src.size() + 4;
		std::vector<sint32> lanes(outputWordCount * 2);

		for(size_t i = 0; i < src.size(); ++i) {
			if (twin && (i & 1))
				continue;

			const size_t outputLaneBase = (i / 4) * 8;
			const size_t kernelWordBase = (size_t)src[i] * wordStride + (i & 7) * phaseStep;
			for(size_t word = 0; word < kernelWords; ++word) {
				for(size_t half = 0; half < 2; ++half) {
					const size_t outputLane = outputLaneBase + word * 2 + half;
					if (outputLane < lanes.size())
						lanes[outputLane] += PackedLane(kernels.data() + kernelWordBase, word * 2 + half);
				}
			}
		}

		std::vector<uint32> result(outputWordCount);
		for(size_t word = 0; word < result.size(); ++word)
			result[word] = PackSigned(lanes[word * 2], lanes[word * 2 + 1]);
		return result;
	}

	bool TestFilters(ATPortableTestContext& context) {
		const std::vector<uint8> src { 0, 3, 1, 7, 2, 5, 4, 6, 9, 8, 10, 12, 11, 15, 13, 14 };

		const auto lumaKernels = MakeKernels(64, 1);
		const auto expectedLuma = ReferenceFilter(src, lumaKernels, 64, 8, 8, false);
		std::vector<uint32> actualLuma(expectedLuma.size(), 0xcdcdcdcd);
		ATArtifactPALLuma_NEON(actualLuma.data(), src.data(), (uint32)src.size(), lumaKernels.data());
		AT_PORTABLE_TEST_ASSERT(context, actualLuma == expectedLuma);

		const auto lumaTwinKernels = MakeKernels(32, 2);
		const auto expectedLumaTwin = ReferenceFilter(src, lumaTwinKernels, 32, 4, 8, true);
		std::vector<uint32> actualLumaTwin(expectedLumaTwin.size(), 0xcdcdcdcd);
		ATArtifactPALLumaTwin_NEON(actualLumaTwin.data(), src.data(), (uint32)src.size(), lumaTwinKernels.data());
		AT_PORTABLE_TEST_ASSERT(context, actualLumaTwin == expectedLumaTwin);

		const auto chromaKernels = MakeKernels(128, 3);
		const auto expectedChroma = ReferenceFilter(src, chromaKernels, 128, 16, 16, false);
		std::vector<uint32> actualChroma(expectedChroma.size(), 0xcdcdcdcd);
		ATArtifactPALChroma_NEON(actualChroma.data(), src.data(), (uint32)src.size(), chromaKernels.data());
		AT_PORTABLE_TEST_ASSERT(context, actualChroma == expectedChroma);

		const auto chromaTwinKernels = MakeKernels(64, 4);
		const auto expectedChromaTwin = ReferenceFilter(src, chromaTwinKernels, 64, 8, 16, true);
		std::vector<uint32> actualChromaTwin(expectedChromaTwin.size(), 0xcdcdcdcd);
		ATArtifactPALChromaTwin_NEON(actualChromaTwin.data(), src.data(), (uint32)src.size(), chromaTwinKernels.data());
		AT_PORTABLE_TEST_ASSERT(context, actualChromaTwin == expectedChromaTwin);
		return true;
	}

	sint16 Add16(sint16 x, sint16 y) {
		return (sint16)((uint16)x + (uint16)y);
	}

	sint16 DoublingMultiplyHigh(sint16 value, sint16 coefficient) {
		const sint64 product = (sint64)value * coefficient * 2;
		const sint64 shifted = product >> 16;
		return (sint16)std::clamp<sint64>(shifted, -32768, 32767);
	}

	uint8 ShiftNarrow(sint16 value) {
		return (uint8)std::clamp<sint32>((sint32)value >> 6, 0, 255);
	}

	bool TestFinalConversion(ATPortableTestContext& context) {
		constexpr uint32 count = 8;
		const std::array<uint32, 8> y {
			PackSigned(-1000, 0), PackSigned(64, 2048),
			PackSigned(5000, 9000), PackSigned(14000, 20000),
			PackSigned(300, 700), PackSigned(1600, 3200),
			PackSigned(6400, 10000), PackSigned(15000, 22000)
		};
		std::array<uint32, 12> u {};
		std::array<uint32, 12> v {};
		const std::array<uint32, 8> oldU {
			PackSigned(-400, 200), PackSigned(600, -800),
			PackSigned(1000, -1200), PackSigned(1400, -1600),
			PackSigned(-1800, 2000), PackSigned(2200, -2400),
			PackSigned(2600, -2800), PackSigned(3000, -3200)
		};
		const std::array<uint32, 8> oldV {
			PackSigned(300, -500), PackSigned(700, -900),
			PackSigned(1100, -1300), PackSigned(1500, -1700),
			PackSigned(1900, -2100), PackSigned(2300, -2500),
			PackSigned(2700, -2900), PackSigned(3100, -3300)
		};
		for(size_t word = 0; word < 8; ++word) {
			u[word + 4] = PackSigned((sint32)word * 220 - 330, 440 - (sint32)word * 180);
			v[word + 4] = PackSigned(510 - (sint32)word * 190, (sint32)word * 240 - 360);
		}

		auto actualU = oldU;
		auto actualV = oldV;
		std::array<uint32, count * 2> actual {};
		std::array<uint32, count * 2> expected {};
		for(size_t lane = 0; lane < count * 2; ++lane) {
			const sint16 yValue = PackedLane(y.data(), lane);
			const sint16 uValue = Add16(PackedLane(u.data() + 4, lane), PackedLane(oldU.data(), lane));
			const sint16 vValue = Add16(PackedLane(v.data() + 4, lane), PackedLane(oldV.data(), lane));
			const sint16 red = Add16(yValue, vValue);
			const sint16 blue = Add16(yValue, uValue);
			const sint16 green = Add16(Add16(yValue, DoublingMultiplyHigh(uValue, -6364)), DoublingMultiplyHigh(vValue, -16692));
			expected[lane] = (uint32)ShiftNarrow(blue)
				| ((uint32)ShiftNarrow(green) << 8)
				| ((uint32)ShiftNarrow(red) << 16);
		}

		ATArtifactPALFinal_NEON(actual.data(), y.data(), u.data(), v.data(), actualU.data(), actualV.data(), count);
		AT_PORTABLE_TEST_ASSERT(context, actual == expected);
		AT_PORTABLE_TEST_ASSERT(context, std::equal(actualU.begin(), actualU.end(), u.begin() + 4));
		AT_PORTABLE_TEST_ASSERT(context, std::equal(actualV.begin(), actualV.end(), v.begin() + 4));
		return true;
	}

	bool TestFinalMono(ATPortableTestContext& context) {
		constexpr uint32 count = 4;
		const std::array<uint32, 4> y {
			PackSigned(-100, 0), PackSigned(31, 32),
			PackSigned(63, 64), PackSigned(16320, 20000)
		};
		std::array<uint32, 256> palette {};
		for(uint32 i = 0; i < palette.size(); ++i)
			palette[i] = UINT32_C(0x010101) * i;
		std::array<uint32, count * 2> actual {};
		ATArtifactPALFinalMono_NEON(actual.data(), y.data(), count, palette.data());
		for(size_t lane = 0; lane < actual.size(); ++lane) {
			const uint32 index = (uint32)std::clamp<sint32>(((sint32)PackedLane(y.data(), lane) + 32) >> 6, 0, 255);
			AT_PORTABLE_TEST_ASSERT(context, actual[lane] == palette[index]);
		}
		return true;
	}

	bool TestPAL32(ATPortableTestContext& context) {
		const std::array<uint32, 7> pixels {
			0x503c2814, 0x5a78b4f0, 0xc8fa0a00, 0x1e14c8ff,
			0x40302010, 0xe0c09060, 0x80604020
		};
		const std::array<uint32, 7> delay {
			0x46503c28, 0x6e64a0c8, 0x280014fa, 0xdcf0500a,
			0x40302010, 0x20104080, 0xa0806040
		};

		for(bool compress : { false, true }) {
			auto scalarPixels = pixels;
			auto scalarDelay = delay;
			auto neonPixels = pixels;
			auto neonDelay = delay;
			ATArtifactPAL32(scalarPixels.data(), scalarDelay.data(), (uint32)scalarPixels.size(), compress);
			ATArtifactPAL32_NEON(neonPixels.data(), neonDelay.data(), (uint32)neonPixels.size(), compress);
			for(size_t i = 0; i < pixels.size(); ++i)
				AT_PORTABLE_TEST_ASSERT(context, (neonPixels[i] & 0xffffff) == (scalarPixels[i] & 0xffffff));
			AT_PORTABLE_TEST_ASSERT(context, neonDelay == scalarDelay);
		}
		return true;
	}
}
#endif

bool ATTestAltirraArtifactingPALNEON(ATPortableTestContext& context) {
#if defined(VD_CPU_ARM64)
	return TestFilters(context)
		&& TestFinalConversion(context)
		&& TestFinalMono(context)
		&& TestPAL32(context);
#else
	(void)context;
	return true;
#endif
}
