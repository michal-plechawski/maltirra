// Altirra portable accelerated FLAC primitive tests

#include <array>
#include <bit>
#include <cstdint>
#include <cstdio>

#include <at/attest/portabletest.h>
#include <vd2/system/cpuaccel.h>
#include <vd2/system/vdtypes.h>

#if VD_CPU_ARM64
void ATFLACReconstructLPC_Narrow_NEON(
	sint32 *y, uint32 n, const sint32 *lpcCoeffs, int qlpShift, int order);
void ATFLACReconstructLPC_Medium_NEON(
	sint32 *y, uint32 n, const sint32 *lpcCoeffs, int qlpShift, int order);
void ATFLACReconstructLPC_Wide_NEON(
	sint32 *y, uint32 n, const sint32 *lpcCoeffs, int qlpShift, int order);
uint16 ATFLACUpdateCRC16_Crypto(uint16 crc16, const void *buf, size_t n);
#elif VD_CPU_X64
void ATFLACReconstructLPC_Narrow_SSE2(
	sint32 *y, uint32 n, const sint32 *lpcCoeffs, int qlpShift, int order);
void ATFLACReconstructLPC_Narrow_SSSE3(
	sint32 *y, uint32 n, const sint32 *lpcCoeffs, int qlpShift, int order);
void ATFLACReconstructLPC_Medium_SSSE3(
	sint32 *y, uint32 n, const sint32 *lpcCoeffs, int qlpShift, int order);
uint16 ATFLACUpdateCRC16_PCMUL(uint16 crc16, const void *buf, size_t n);
#endif

#if VD_CPU_ARM64 || VD_CPU_X64
namespace {
	using ATLPCFunction = void (*)(
		sint32 *, uint32, const sint32 *, int, int);

	constexpr size_t kPrefix = 5;
	constexpr size_t kMaximumOrder = 32;
	constexpr size_t kMaximumSamples = 37;
	constexpr size_t kSuffix = 7;
	constexpr size_t kBufferSize =
		kPrefix + kMaximumOrder + kMaximumSamples + kSuffix;
	constexpr sint32 kGuard = static_cast<sint32>(0x5A17C0DEU);

	using ATBuffer = std::array<sint32, kBufferSize>;
	using ATCoefficients = std::array<sint32, kMaximumOrder>;

	sint32 ATFromBits(uint32 value) {
		return std::bit_cast<sint32>(value);
	}

	void ATReconstructLPC32(
		sint32 *y, uint32 n, const sint32 *coefficients,
		int qlpShift, int order) {
		for(uint32 i = 0; i < n; ++i) {
			uint32 prediction = 0;

			for(int j = 0; j < order; ++j) {
				prediction += static_cast<uint32>(
					static_cast<sint64>(y[i + j]) * coefficients[j]);
			}

			const sint32 scaledPrediction =
				ATFromBits(prediction) >> qlpShift;
			y[i + order] = ATFromBits(
				static_cast<uint32>(y[i + order])
					+ static_cast<uint32>(scaledPrediction));
		}
	}

	void ATReconstructLPCWide(
		sint32 *y, uint32 n, const sint32 *coefficients,
		int qlpShift, int order) {
		for(uint32 i = 0; i < n; ++i) {
			sint64 prediction = 0;

			for(int j = 0; j < order; ++j)
				prediction += static_cast<sint64>(y[i + j]) * coefficients[j];

			const sint32 scaledPrediction =
				static_cast<sint32>(prediction >> qlpShift);
			y[i + order] = ATFromBits(
				static_cast<uint32>(y[i + order])
					+ static_cast<uint32>(scaledPrediction));
		}
	}

	ATBuffer ATMakeBuffer(int order, uint32 n, sint32 magnitude, uint32 seed) {
		ATBuffer buffer;
		buffer.fill(kGuard);
		sint32 *const y = buffer.data() + kPrefix;

		for(int i = 0; i < order; ++i) {
			seed = seed * 1664525U + 1013904223U;
			y[i] = static_cast<sint32>(seed % (2U * magnitude + 1U))
				- magnitude;
		}

		for(uint32 i = 0; i < n; ++i) {
			seed = seed * 1664525U + 1013904223U;
			y[order + i] = static_cast<sint32>(seed % 257U) - 128;
		}

		return buffer;
	}

	ATCoefficients ATMakeCoefficients(
		int order, sint32 magnitude, uint32 seed) {
		alignas(16) ATCoefficients coefficients {};

		for(int i = 0; i < order; ++i) {
			seed = seed * 1103515245U + 12345U;
			coefficients[i] =
				static_cast<sint32>(seed % (2U * magnitude + 1U))
					- magnitude;
		}

		return coefficients;
	}

	bool ATCheckLPC32Case(
		ATLPCFunction function, int order, uint32 n, int qlpShift,
		sint32 sampleMagnitude, sint32 coefficientMagnitude, uint32 seed) {
		alignas(16) const ATCoefficients coefficients =
			ATMakeCoefficients(order, coefficientMagnitude, seed ^ 0x9182A53CU);
		const ATBuffer input = ATMakeBuffer(
			order, n, sampleMagnitude, seed ^ 0xC723DA15U);
		ATBuffer expected = input;
		ATBuffer actual = input;

		ATReconstructLPC32(
			expected.data() + kPrefix, n, coefficients.data(), qlpShift, order);
		function(
			actual.data() + kPrefix, n, coefficients.data(), qlpShift, order);

		if (actual == expected)
			return true;

		for(size_t i = 0; i < actual.size(); ++i) {
			if (actual[i] != expected[i]) {
				fprintf(
					stderr,
					"FLAC LPC32 mismatch: order=%d n=%u shift=%d index=%zu "
					"actual=%d expected=%d\n",
					order, n, qlpShift, i - kPrefix,
					actual[i], expected[i]);
				break;
			}
		}

		return false;
	}

	bool ATCheckLPCWideCase(
		ATLPCFunction function, int order, uint32 n, int qlpShift,
		sint32 sampleMagnitude, sint32 coefficientMagnitude, uint32 seed) {
		alignas(16) const ATCoefficients coefficients =
			ATMakeCoefficients(order, coefficientMagnitude, seed ^ 0x52D10B6FU);
		const ATBuffer input = ATMakeBuffer(
			order, n, sampleMagnitude, seed ^ 0xA2169C43U);
		ATBuffer expected = input;
		ATBuffer actual = input;

		ATReconstructLPCWide(
			expected.data() + kPrefix, n, coefficients.data(), qlpShift, order);
		function(
			actual.data() + kPrefix, n, coefficients.data(), qlpShift, order);

		if (actual == expected)
			return true;

		for(size_t i = 0; i < actual.size(); ++i) {
			if (actual[i] != expected[i]) {
				fprintf(
					stderr,
					"FLAC LPC64 mismatch: order=%d n=%u shift=%d index=%zu "
					"actual=%d expected=%d\n",
					order, n, qlpShift, i - kPrefix,
					actual[i], expected[i]);
				break;
			}
		}

		return false;
	}

	bool ATCheckNarrow(ATLPCFunction function) {
		for(int order = 2; order <= 16; ++order) {
			if (!ATCheckLPC32Case(
				function, order, 1, 0, 100, 2,
				0x10001U * static_cast<uint32>(order)))
				return false;

			if (!ATCheckLPC32Case(
				function, order, 37, 7, 12000, 2,
				0x934D25A1U + static_cast<uint32>(order)))
				return false;
		}

		return true;
	}

	bool ATCheckMedium(ATLPCFunction function, int maximumOrder) {
		for(int order = 2; order <= maximumOrder; ++order) {
			if (!ATCheckLPC32Case(
				function, order, 1, 0, 1000, 200,
				0x1327F00DU + static_cast<uint32>(order)))
				return false;

			if (!ATCheckLPC32Case(
				function, order, 29, 11, 2000000, 3,
				0x9E3779B9U * static_cast<uint32>(order)))
				return false;
		}

		return true;
	}

	bool ATCheckWide(ATLPCFunction function) {
		for(int order = 2; order <= 32; ++order) {
			if (!ATCheckLPCWideCase(
				function, order, 1, 15, 1000000, 30000,
				0x7F4A7C15U + static_cast<uint32>(order)))
				return false;

			if (!ATCheckLPCWideCase(
				function, order, 17, 8, 100000000, 3,
				0x6A09E667U * static_cast<uint32>(order)))
				return false;
		}

		return true;
	}

	// The PCMUL/PMULL paths retain the raw polynomial remainder between
	// calls. The scalar lookup path uses a different internal transform,
	// but both representations are zero after a valid FLAC frame.
	uint16 ATUpdateCRC16Reference(uint16 crc, const uint8 *data, size_t n) {
		while(n--) {
			uint32 polynomial =
				(static_cast<uint32>(crc) << 8) | *data++;

			for(int bit = 23; bit >= 16; --bit) {
				if (polynomial & (UINT32_C(1) << bit)) {
					polynomial ^=
						UINT32_C(0x18005) << (bit - 16);
				}
			}

			crc = static_cast<uint16>(polynomial);
		}

		return crc;
	}

	template<typename CRCFunction>
	bool ATCheckCRC16(CRCFunction function) {
		static constexpr uint8 kKnownCodeword[] = {
			'1', '2', '3', '4', '5', '6', '7', '8', '9', 0xFE, 0xE8
		};
		if (ATUpdateCRC16Reference(0, kKnownCodeword, 9) != 0xD52E
			|| ATUpdateCRC16Reference(
				0, kKnownCodeword, std::size(kKnownCodeword)) != 0
			|| function(0, kKnownCodeword, std::size(kKnownCodeword)) != 0)
			return false;

		alignas(16) std::array<uint8, 192> bytes {};
		uint32 state = 0x243F6A88U;

		for(uint8& value : bytes) {
			state = state * 1664525U + 1013904223U;
			value = static_cast<uint8>(state >> 24);
		}

		const auto originalBytes = bytes;
		static constexpr size_t kLengths[] = {
			1, 2, 3, 7, 8, 15, 16, 17, 23, 31, 32, 33, 47, 64, 95
		};
		static constexpr uint16 kInitialValues[] = {
			0, 0xFFFF, 0x1234
		};

		for(size_t offset = 0; offset < 16; ++offset) {
			for(size_t length : kLengths) {
				for(uint16 initialValue : kInitialValues) {
					const uint16 expected = ATUpdateCRC16Reference(
						initialValue, bytes.data() + offset, length);
					const uint16 actual = function(
						initialValue, bytes.data() + offset, length);

					if (actual != expected) {
						fprintf(
							stderr,
							"FLAC CRC16 mismatch: offset=%zu length=%zu "
							"initial=%04X actual=%04X expected=%04X\n",
							offset, length,
							static_cast<unsigned>(initialValue),
							static_cast<unsigned>(actual),
							static_cast<unsigned>(expected));
						return false;
					}
				}
			}
		}

		return bytes == originalBytes;
	}
}
#endif

bool ATTestIOFLACAccel(ATPortableTestContext& context) {
#if VD_CPU_ARM64
	AT_PORTABLE_TEST_ASSERT(context,
		ATCheckNarrow(ATFLACReconstructLPC_Narrow_NEON));
	AT_PORTABLE_TEST_ASSERT(context,
		ATCheckMedium(ATFLACReconstructLPC_Medium_NEON, 32));
	AT_PORTABLE_TEST_ASSERT(context,
		ATCheckWide(ATFLACReconstructLPC_Wide_NEON));

	if (CPUCheckForExtensions() & VDCPUF_SUPPORTS_CRYPTO) {
		AT_PORTABLE_TEST_ASSERT(context,
			ATCheckCRC16(ATFLACUpdateCRC16_Crypto));
	}
#elif VD_CPU_X64
	const long extensions = CPUCheckForExtensions();

	AT_PORTABLE_TEST_ASSERT(context,
		ATCheckNarrow(ATFLACReconstructLPC_Narrow_SSE2));

	if (extensions & CPUF_SUPPORTS_SSSE3) {
		AT_PORTABLE_TEST_ASSERT(context,
			ATCheckNarrow(ATFLACReconstructLPC_Narrow_SSSE3));
		AT_PORTABLE_TEST_ASSERT(context,
			ATCheckMedium(ATFLACReconstructLPC_Medium_SSSE3, 16));
	}

	if (extensions & CPUF_SUPPORTS_CLMUL) {
		AT_PORTABLE_TEST_ASSERT(context,
			ATCheckCRC16(ATFLACUpdateCRC16_PCMUL));
	}
#endif

	return true;
}
