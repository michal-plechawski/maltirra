// Portable VDDisplay screen-mask parameter tests.

#include <cmath>

#include <at/attest/portabletest.h>
#include <vd2/VDDisplay/displaytypes.h>

namespace {
	bool NearlyEqual(float x, float y) {
		return std::fabs(x - y) <= 1e-6f;
	}
}

bool ATTestVDDisplayDisplayTypes(ATPortableTestContext& context) {
	VDDScreenMaskParams params;
	params.mOpenness = 1.0f;
	AT_PORTABLE_TEST_ASSERT(context, params.GetMaskIntensityScale() == 1.0f);

	params.mType = VDDScreenMaskType::ApertureGrille;
	AT_PORTABLE_TEST_ASSERT(context, NearlyEqual(params.GetMaskIntensityScale(), 1.0f / 3.0f));

	params.mType = VDDScreenMaskType::DotTriad;
	AT_PORTABLE_TEST_ASSERT(context, NearlyEqual(params.GetMaskIntensityScale(), 0.3022999f));

	params.mType = VDDScreenMaskType::SlotMask;
	AT_PORTABLE_TEST_ASSERT(context, NearlyEqual(params.GetMaskIntensityScale(), 1.0f / 3.0f));

	params.mOpenness = 0.0f;
	AT_PORTABLE_TEST_ASSERT(context, params.GetMaskIntensityScale() == 0.0f);
	params.mType = VDDScreenMaskType::DotTriad;
	AT_PORTABLE_TEST_ASSERT(context, params.GetMaskIntensityScale() == 0.0f);
	params.mType = VDDScreenMaskType::ApertureGrille;
	AT_PORTABLE_TEST_ASSERT(context, params.GetMaskIntensityScale() == 0.0f);
	return true;
}
