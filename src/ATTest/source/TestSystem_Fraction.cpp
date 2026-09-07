// Altirra portable fraction tests

#include <limits>
#include <vd2/system/Fraction.h>
#include <at/attest/portabletest.h>

namespace {
	uint32 GreatestCommonDivisor(uint32 x, uint32 y) {
		while(y) {
			const uint32 remainder = x % y;
			x = y;
			y = remainder;
		}

		return x;
	}

	uint32 NextFractionRandom(uint32& state) {
		state = state * UINT32_C(1664525) + UINT32_C(1013904223);
		return state;
	}
}

bool ATTestSystemFraction(ATPortableTestContext& context) {
	AT_PORTABLE_TEST_ASSERT(context, VDFraction(2, 4) == VDFraction(1, 2));
	AT_PORTABLE_TEST_ASSERT(context, VDFraction(1, 3) < VDFraction(1, 2));
	AT_PORTABLE_TEST_ASSERT(context, VDFraction(2, 3) > VDFraction(1, 2));
	AT_PORTABLE_TEST_ASSERT(context, VDFraction(1, 2) <= VDFraction(2, 4));
	AT_PORTABLE_TEST_ASSERT(context, VDFraction(1, 2) >= VDFraction(2, 4));
	AT_PORTABLE_TEST_ASSERT(context, VDFraction(1, 2) != VDFraction(2, 3));

	const VDFraction reduced = VDFraction::reduce64(100, 250);
	AT_PORTABLE_TEST_ASSERT(context, reduced.getHi() == 2 && reduced.getLo() == 5);
	const VDFraction zero = VDFraction::reduce64(0, 250);
	AT_PORTABLE_TEST_ASSERT(context, zero.getHi() == 0 && zero.getLo() == 1);
	const VDFraction undefined = VDFraction::reduce64(1, 0);
	AT_PORTABLE_TEST_ASSERT(context, undefined.getHi() == 0 && undefined.getLo() == 0);

	uint32 randomState = UINT32_C(0xC001D00D);
	for(int i = 0; i < 4096; ++i) {
		const uint32 numerator = NextFractionRandom(randomState);
		const uint32 denominator = NextFractionRandom(randomState) | 1;
		const uint32 divisor = GreatestCommonDivisor(numerator, denominator);
		const VDFraction randomReduced = VDFraction::reduce64(numerator, denominator);

		AT_PORTABLE_TEST_ASSERT(context, randomReduced.getHi() == numerator / divisor);
		AT_PORTABLE_TEST_ASSERT(context, randomReduced.getLo() == denominator / divisor);
	}

	AT_PORTABLE_TEST_ASSERT(context,
		VDFraction(2, 3) * VDFraction(9, 4) == VDFraction(3, 2));
	AT_PORTABLE_TEST_ASSERT(context,
		VDFraction(2, 3) / VDFraction(9, 4) == VDFraction(8, 27));
	AT_PORTABLE_TEST_ASSERT(context, VDFraction(3, 5) * 10 == VDFraction(6, 1));
	AT_PORTABLE_TEST_ASSERT(context, VDFraction(3, 5) / 3 == VDFraction(1, 5));

	VDFraction assigned(2, 3);
	assigned *= VDFraction(9, 4);
	AT_PORTABLE_TEST_ASSERT(context, assigned == VDFraction(3, 2));
	assigned /= VDFraction(3, 5);
	AT_PORTABLE_TEST_ASSERT(context, assigned == VDFraction(5, 2));
	assigned *= 4;
	assigned /= 5;
	AT_PORTABLE_TEST_ASSERT(context, assigned == VDFraction(2, 1));

	const VDFraction threeHalves(3, 2);
	AT_PORTABLE_TEST_ASSERT(context, threeHalves.scale64t(5) == 7);
	AT_PORTABLE_TEST_ASSERT(context, threeHalves.scale64u(5) == 8);
	AT_PORTABLE_TEST_ASSERT(context, threeHalves.scale64r(5) == 8);
	AT_PORTABLE_TEST_ASSERT(context, threeHalves.scale64t(-5) == -7);
	AT_PORTABLE_TEST_ASSERT(context, threeHalves.scale64u(-5) == -7);
	AT_PORTABLE_TEST_ASSERT(context, threeHalves.scale64r(-5) == -8);
	AT_PORTABLE_TEST_ASSERT(context, threeHalves.scale64it(5) == 3);
	AT_PORTABLE_TEST_ASSERT(context, threeHalves.scale64iu(5) == 4);
	AT_PORTABLE_TEST_ASSERT(context, threeHalves.scale64ir(5) == 3);
	AT_PORTABLE_TEST_ASSERT(context, threeHalves.scale64ir(1) == 1);

	const VDFraction identity(1, 1);
	AT_PORTABLE_TEST_ASSERT(context,
		identity.scale64t(std::numeric_limits<sint64>::min())
		== std::numeric_limits<sint64>::min());
	AT_PORTABLE_TEST_ASSERT(context, VDFraction(UINT32_MAX, UINT32_MAX).roundup32ul() == 1);
	AT_PORTABLE_TEST_ASSERT(context, threeHalves.roundup32ul() == 2);

	AT_PORTABLE_TEST_ASSERT(context, VDFraction(0.0) == VDFraction(0, 1));
	AT_PORTABLE_TEST_ASSERT(context, VDFraction(0.5) == VDFraction(1, 2));
	AT_PORTABLE_TEST_ASSERT(context, VDFraction(1.5) == VDFraction(3, 2));
	AT_PORTABLE_TEST_ASSERT(context, VDFraction(1e-20) == VDFraction(0, 1));
	AT_PORTABLE_TEST_ASSERT(context, VDFraction(1e20) == VDFraction(UINT32_MAX, 1));
	AT_PORTABLE_TEST_ASSERT(context, VDFraction(3, 2).asDouble() == 1.5);
	AT_PORTABLE_TEST_ASSERT(context, VDFraction(3, 2).AsInverseDouble() == 2.0 / 3.0);

	VDFraction parsed;
	AT_PORTABLE_TEST_ASSERT(context, parsed.Parse("1.5"));
	AT_PORTABLE_TEST_ASSERT(context, parsed == VDFraction(3, 2));
	AT_PORTABLE_TEST_ASSERT(context, parsed.Parse("\t .125  "));
	AT_PORTABLE_TEST_ASSERT(context, parsed == VDFraction(1, 8));
	AT_PORTABLE_TEST_ASSERT(context, parsed.Parse("42."));
	AT_PORTABLE_TEST_ASSERT(context, parsed == VDFraction(42, 1));
	AT_PORTABLE_TEST_ASSERT(context, !parsed.Parse("-1"));
	AT_PORTABLE_TEST_ASSERT(context, !parsed.Parse("1.2x"));
	AT_PORTABLE_TEST_ASSERT(context, !parsed.Parse("4294967296"));

	return true;
}
