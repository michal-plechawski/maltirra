// Altirra portable half-precision conversion tests

#include <bit>
#include <cmath>
#include <limits>
#include <vd2/system/halffloat.h>
#include <at/attest/portabletest.h>

namespace {
	uint16 ToHalf(float value) {
		return VDConvertFloatToHalf(&value);
	}

	float FromHalf(uint16 value) {
		float result = 0;
		VDConvertHalfToFloat(value, &result);
		return result;
	}

	uint32 ReferenceHalfToFloatBits(uint16 value) {
		const uint32 sign = (uint32)(value & 0x8000) << 16;
		uint32 exponent = (value >> 10) & 0x1f;
		uint32 mantissa = value & 0x03ff;

		if (exponent == 0x1f)
			return sign | UINT32_C(0x7F800000) | (mantissa << 13);

		if (exponent) {
			exponent += 127 - 15;
			return sign | (exponent << 23) | (mantissa << 13);
		}

		if (!mantissa)
			return sign;

		int unbiasedExponent = -14;
		while(!(mantissa & 0x0400)) {
			mantissa <<= 1;
			--unbiasedExponent;
		}

		mantissa &= 0x03ff;
		return sign | ((uint32)(unbiasedExponent + 127) << 23) | (mantissa << 13);
	}
}

bool ATTestSystemHalfFloat(ATPortableTestContext& context) {
	for(uint32 value = 0; value <= UINT16_MAX; ++value) {
		const uint16 half = (uint16)value;
		const uint32 actualBits = std::bit_cast<uint32>(FromHalf(half));
		AT_PORTABLE_TEST_ASSERT(context, actualBits == ReferenceHalfToFloatBits(half));

		if ((half & 0x7c00) != 0x7c00 || !(half & 0x03ff))
			AT_PORTABLE_TEST_ASSERT(context, ToHalf(FromHalf(half)) == half);
	}

	AT_PORTABLE_TEST_ASSERT(context, ToHalf(0.0f) == UINT16_C(0x0000));
	AT_PORTABLE_TEST_ASSERT(context, ToHalf(-0.0f) == UINT16_C(0x8000));
	AT_PORTABLE_TEST_ASSERT(context, ToHalf(1.0f) == UINT16_C(0x3c00));
	AT_PORTABLE_TEST_ASSERT(context, ToHalf(-2.0f) == UINT16_C(0xc000));
	AT_PORTABLE_TEST_ASSERT(context, ToHalf(65504.0f) == UINT16_C(0x7bff));
	AT_PORTABLE_TEST_ASSERT(context, ToHalf(std::numeric_limits<float>::infinity()) == UINT16_C(0x7c00));
	AT_PORTABLE_TEST_ASSERT(context, ToHalf(-std::numeric_limits<float>::infinity()) == UINT16_C(0xfc00));

	AT_PORTABLE_TEST_ASSERT(context, ToHalf(std::ldexp(1.0f, -24)) == UINT16_C(0x0001));
	AT_PORTABLE_TEST_ASSERT(context, ToHalf(std::ldexp(1.0f, -14)) == UINT16_C(0x0400));
	AT_PORTABLE_TEST_ASSERT(context, ToHalf(1.0f + std::ldexp(1.0f, -11)) == UINT16_C(0x3c00));
	AT_PORTABLE_TEST_ASSERT(context, ToHalf(1.0f + 3.0f * std::ldexp(1.0f, -11)) == UINT16_C(0x3c02));

	const uint16 nanValue = ToHalf(std::numeric_limits<float>::quiet_NaN());
	AT_PORTABLE_TEST_ASSERT(context, (nanValue & 0x7c00) == 0x7c00);
	AT_PORTABLE_TEST_ASSERT(context, (nanValue & 0x03ff) != 0);

	return true;
}
