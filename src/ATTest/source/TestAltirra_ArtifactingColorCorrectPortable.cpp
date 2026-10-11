// Independent lane-by-lane fixed-point oracles, including SIMD quantization.
#include <algorithm>
#include <array>
#include <at/attest/portabletest.h>
#include <artifacting_colorcorrect.h>
#include <vd2/system/cpuaccel.h>

namespace {
	using Kernel = void (*)(uint8 *, uint32, const sint16 *, const uint8 *, const sint16 (*)[3]);
	bool Check(ATPortableTestContext& context, Kernel kernel, int rounding) {
		std::array<sint16, 256> linear;
		std::array<uint8, 1024> gamma;
		for(int i = 0; i < 256; ++i) linear[i] = (i * 8191) / 255;
		for(int i = 0; i < 1024; ++i) gamma[i] = (i * 37) % 251;
		const sint16 matrices[][3][3] {
			{ { 16384, 0, 0 }, { 0, 16384, 0 }, { 0, 0, 16384 } },
			{ { 19000, -1200, 2300 }, { -2300, 18000, -1900 }, { 800, 2200, 15000 } },
			{ { 32767, -32768, 12345 }, { 32767, -32768, -7654 }, { 32767, -32768, 30567 } },
			{ { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 } }
		};
		for(const auto& matrix : matrices) {
			for(int count = 0; count <= 17; ++count) {
				std::array<uint8, 17 * 4 + 2> actual, expected;
				actual.fill(0xed);
				for(size_t i = 1; i + 1 < actual.size(); ++i) actual[i] = (i * 17 + count * 5) & 255;
				expected = actual;
				for(int i = 0; i < count; ++i) {
					const uint8 *pixel = actual.data() + 1 + i * 4;
					for(int output = 0; output < 3; ++output) {
						sint64 sum = 0;
						for(int input = 0; input < 3; ++input) {
							const sint64 product = (sint64)linear[pixel[2-input]] * matrix[input][output];
							sum += rounding == 1 ? product >> 16 : product;
						}
						const sint64 index = rounding == 1 ? sum >> 1
							: rounding == 2 ? (std::max<sint64>(0, sum >> 16) + 1) >> 1 : sum >> 17;
						expected[1 + i * 4 + 2 - output] = gamma[std::clamp<sint64>(index, 0, 1023)];
					}
				}
				kernel(actual.data() + 1, count, linear.data(), gamma.data(), matrix);
				AT_PORTABLE_TEST_ASSERT(context, actual == expected);
			}
		}
		return true;
	}
}

bool ATTestAltirraArtifactingColorCorrect(ATPortableTestContext& context) {
	if (!Check(context, ATArtifactColorCorrect_Scalar, 0)) return false;
#if defined(VD_CPU_ARM64)
	return Check(context, ATArtifactColorCorrect_NEON, 2);
#elif defined(VD_CPU_X86) || defined(VD_CPU_X64)
	return !(CPUGetEnabledExtensions() & CPUF_SUPPORTS_SSE2) || Check(context, ATArtifactColorCorrect_SSE2, 1);
#else
	return true;
#endif
}
