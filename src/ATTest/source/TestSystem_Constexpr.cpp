// Altirra portable constexpr primitive tests

#include <vd2/system/vdtypes.h>
#include <vd2/system/constexpr.h>
#include <at/attest/portabletest.h>

namespace {
	constexpr float kCompileTimeCosZero = VDCxCos(0.0f);
	static_assert(kCompileTimeCosZero == 1.0f);

	bool NearlyEqual(float x, float y, float tolerance) {
		const float delta = x > y ? x - y : y - x;
		return delta <= tolerance;
	}
}

bool ATTestSystemConstexpr(ATPortableTestContext& context) {
	volatile float zero = 0.0f;
	volatile float quarter = 0.25f;
	volatile float nine = 9.0f;
	volatile float minusThreeAndHalf = -3.5f;

	AT_PORTABLE_TEST_ASSERT(context, VDCxSinPi(zero) == 0.0f);
	AT_PORTABLE_TEST_ASSERT(context, VDCxCosPi(zero) == 1.0f);
	AT_PORTABLE_TEST_ASSERT(context, VDCxCos(zero) == kCompileTimeCosZero);
	AT_PORTABLE_TEST_ASSERT(context, NearlyEqual(VDCxSinPi(quarter), 0.70710677f, 1e-7f));
	AT_PORTABLE_TEST_ASSERT(context, NearlyEqual(VDCxCosPi(quarter), 0.70710677f, 1e-7f));
	AT_PORTABLE_TEST_ASSERT(context, VDCxSqrt(nine) == 3.0f);
	AT_PORTABLE_TEST_ASSERT(context, VDCxFloor(minusThreeAndHalf) == -4.0f);
	AT_PORTABLE_TEST_ASSERT(context, VDCxExp(zero) == 1.0f);
	AT_PORTABLE_TEST_ASSERT(context, VDCxSincPi(zero) == 1.0f);

	return true;
}
