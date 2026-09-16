// Portable tests for the scalar PAL artifacting kernels.

#include <algorithm>
#include <array>
#include <cstddef>
#include <initializer_list>
#include <vector>

#include <at/attest/portabletest.h>
#include <artifacting_pal.h>

namespace {
	constexpr uint32 Pack16(uint32 lo, uint32 hi) {
		return lo | (hi << 16);
	}

	constexpr uint32 PackBGRA(uint8 b, uint8 g, uint8 r, uint8 a) {
		return (uint32)b | ((uint32)g << 8) | ((uint32)r << 16) | ((uint32)a << 24);
	}

	void ReferenceLuma(uint32 *dst, const uint8 *src, uint32 n, const uint32 *kernels) {
		std::fill(dst, dst + n + 3, 0);
		dst[0] = 0x40004000;
		dst[1] = 0x40004000;
		dst[2] = 0x40004000;

		for(uint32 i = 0; i < n; ++i) {
			const uint32 *kernel = kernels + 32 * src[i] + 4 * (i & 7);
			for(uint32 j = 0; j < 4; ++j)
				dst[i + j] += kernel[j];
		}
	}

	void ReferenceChroma(uint32 *dst, const uint8 *src, uint32 n, const uint32 *kernels) {
		for(uint32 i = 0; i < n; ++i) {
			const uint32 *kernel = kernels + 96 * src[i] + 12 * (i & 7);
			for(uint32 j = 0; j < 12; ++j)
				dst[i + j] += kernel[j];
		}
	}

	void ReferenceFinal(uint32 *dst, const uint32 *ybuf, const uint32 *ubuf, const uint32 *vbuf, uint32 *ulbuf, uint32 *vlbuf, uint32 n) {
		constexpr sint32 coug_coub = -3182;
		constexpr sint32 covg_covr = -8346;

		for(uint32 i = 0; i < n; ++i) {
			const uint32 y = ybuf[i];
			uint32 u = ubuf[i + 4];
			uint32 v = vbuf[i + 4];
			const uint32 up = ulbuf[i + 4];
			const uint32 vp = vlbuf[i + 4];
			ulbuf[i + 4] = u;
			vlbuf[i + 4] = v;
			u += up;
			v += vp;

			const sint32 yv[2] { (sint32)(y & 0xffff), (sint32)(y >> 16) };
			const sint32 uv[2] { (sint32)(u & 0xffff), (sint32)(u >> 16) };
			const sint32 vv[2] { (sint32)(v & 0xffff), (sint32)(v >> 16) };

			for(uint32 j = 0; j < 2; ++j) {
				sint32 r = (yv[j] + vv[j] - 0x8020) >> 6;
				sint32 g = ((yv[j] << 14) + uv[j] * coug_coub + vv[j] * covg_covr
					+ 0x80000 - 0x10000000 - 0x4000 * (coug_coub + covg_covr)) >> 20;
				sint32 b = (yv[j] + uv[j] - 0x8020) >> 6;
				r = std::clamp<sint32>(r, 0, 255);
				g = std::clamp<sint32>(g, 0, 255);
				b = std::clamp<sint32>(b, 0, 255);
				dst[2 * i + j] = ((uint32)r << 16) | ((uint32)g << 8) | (uint32)b;
			}
		}
	}

	void ReferencePAL32(void *dst0, void *delay0, uint32 n, bool compressExtendedRange) {
		uint8 *dst = static_cast<uint8 *>(dst0);
		uint8 *delay = static_cast<uint8 *>(delay0);

		for(uint32 i = 0; i < n; ++i) {
			const int b1 = delay[0];
			const int g1 = delay[1];
			const int r1 = delay[2];
			const int y1 = delay[3];
			const int b2 = dst[0];
			const int g2 = dst[1];
			const int r2 = dst[2];
			const int y2 = dst[3];

			std::copy(dst, dst + 4, delay);
			const int adjustment = y2 - y1;
			int r = (r1 + r2 + adjustment + 1) >> 1;
			int g = (g1 + g2 + adjustment + 1) >> 1;
			int b = (b1 + b2 + adjustment + 1) >> 1;

			if (compressExtendedRange) {
				r = r + r - 128;
				g = g + g - 128;
				b = b + b - 128;
			}

			dst[0] = (uint8)std::clamp(b, 0, 255);
			dst[1] = (uint8)std::clamp(g, 0, 255);
			dst[2] = (uint8)std::clamp(r, 0, 255);
			dst += 4;
			delay += 4;
		}
	}

	template<class T, size_t N>
	bool EqualArrays(const std::array<T, N>& x, const std::array<T, N>& y) {
		return std::equal(x.begin(), x.end(), y.begin());
	}

	bool TestLumaAndChroma(ATPortableTestContext& context) {
		constexpr uint32 n = 11;
		const std::array<uint8, n> src { 0, 3, 1, 7, 2, 5, 4, 6, 9, 8, 10 };
		std::vector<uint32> kernels(256 * 96);
		for(size_t i = 0; i < kernels.size(); ++i) {
			const uint32 lo = (uint32)(i % 29) + 1;
			const uint32 hi = (uint32)((i * 7) % 31) + 1;
			kernels[i] = Pack16(lo, hi);
		}

		std::array<uint32, n + 3> actualLuma {};
		std::array<uint32, n + 3> expectedLuma {};
		ReferenceLuma(expectedLuma.data(), src.data(), n, kernels.data());
		ATArtifactPALLuma(actualLuma.data(), src.data(), n, kernels.data());
		AT_PORTABLE_TEST_ASSERT(context, EqualArrays(actualLuma, expectedLuma));

		std::array<uint32, n + 11> actualChroma {};
		std::array<uint32, n + 11> expectedChroma {};
		for(size_t i = 0; i < actualChroma.size(); ++i)
			actualChroma[i] = expectedChroma[i] = Pack16((uint32)i + 11, (uint32)i + 23);

		ReferenceChroma(expectedChroma.data(), src.data(), n, kernels.data());
		ATArtifactPALChroma(actualChroma.data(), src.data(), n, kernels.data());
		AT_PORTABLE_TEST_ASSERT(context, EqualArrays(actualChroma, expectedChroma));
		return true;
	}

	bool TestFinalConversion(ATPortableTestContext& context) {
		constexpr uint32 n = 4;
		const std::array<uint32, n> y {
			Pack16(0x3800, 0x4800), Pack16(0x4000, 0x5200),
			Pack16(0x7000, 0x2000), Pack16(0x7f00, 0x4100)
		};
		std::array<uint32, n + 4> u {};
		std::array<uint32, n + 4> v {};
		std::array<uint32, n + 4> actualULine {};
		std::array<uint32, n + 4> actualVLine {};
		for(uint32 i = 0; i < n; ++i) {
			u[i + 4] = Pack16(0x3c00 + i * 0x180, 0x4400 - i * 0x100);
			v[i + 4] = Pack16(0x4600 - i * 0x140, 0x3a00 + i * 0x120);
			actualULine[i + 4] = Pack16(0x4100 - i * 0x80, 0x3f00 + i * 0xc0);
			actualVLine[i + 4] = Pack16(0x3d00 + i * 0xa0, 0x4300 - i * 0x60);
		}

		auto expectedULine = actualULine;
		auto expectedVLine = actualVLine;
		std::array<uint32, n * 2> actual {};
		std::array<uint32, n * 2> expected {};
		ReferenceFinal(expected.data(), y.data(), u.data(), v.data(), expectedULine.data(), expectedVLine.data(), n);
		ATArtifactPALFinal(actual.data(), y.data(), u.data(), v.data(), actualULine.data(), actualVLine.data(), n);

		AT_PORTABLE_TEST_ASSERT(context, EqualArrays(actual, expected));
		AT_PORTABLE_TEST_ASSERT(context, EqualArrays(actualULine, expectedULine));
		AT_PORTABLE_TEST_ASSERT(context, EqualArrays(actualVLine, expectedVLine));

		std::array<uint32, 256> monoTable {};
		for(uint32 i = 0; i < monoTable.size(); ++i)
			monoTable[i] = i * UINT32_C(0x010101);
		const std::array<uint32, 3> monoY {
			Pack16(0x3000, 0x4000), Pack16(0x4040, 0x8000), Pack16(0x7fc0, 0xffff)
		};
		std::array<uint32, 6> mono {};
		ATArtifactPALFinalMono(mono.data(), monoY.data(), (uint32)monoY.size(), monoTable.data());
		const std::array<uint32, 6> expectedMono {
			monoTable[0], monoTable[0], monoTable[1], monoTable[255], monoTable[255], monoTable[255]
		};
		AT_PORTABLE_TEST_ASSERT(context, EqualArrays(mono, expectedMono));
		return true;
	}

	bool TestPAL32(ATPortableTestContext& context) {
		const std::array<uint32, 4> initialPixels {
			PackBGRA(20, 40, 60, 80), PackBGRA(240, 180, 120, 90),
			PackBGRA(0, 10, 250, 200), PackBGRA(255, 200, 20, 30)
		};
		const std::array<uint32, 4> initialDelay {
			PackBGRA(40, 60, 80, 70), PackBGRA(200, 160, 100, 110),
			PackBGRA(250, 20, 0, 40), PackBGRA(10, 80, 240, 220)
		};

		for(bool compress : { false, true }) {
			auto actualPixels = initialPixels;
			auto actualDelay = initialDelay;
			auto expectedPixels = initialPixels;
			auto expectedDelay = initialDelay;

			ReferencePAL32(expectedPixels.data(), expectedDelay.data(), (uint32)expectedPixels.size(), compress);
			ATArtifactPAL32(actualPixels.data(), actualDelay.data(), (uint32)actualPixels.size(), compress);
			AT_PORTABLE_TEST_ASSERT(context, EqualArrays(actualPixels, expectedPixels));
			AT_PORTABLE_TEST_ASSERT(context, EqualArrays(actualDelay, expectedDelay));
		}

		return true;
	}
}

bool ATTestAltirraArtifactingPALScalar(ATPortableTestContext& context) {
	return TestLumaAndChroma(context)
		&& TestFinalConversion(context)
		&& TestPAL32(context);
}
