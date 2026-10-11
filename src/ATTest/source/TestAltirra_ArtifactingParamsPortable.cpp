#include <at/attest/portabletest.h>
#include <artifactingparams.h>

bool ATTestAltirraArtifactingParams(ATPortableTestContext& context) {
	const ATArtifactingParams params = ATArtifactingParams::GetDefault();

	AT_PORTABLE_TEST_ASSERT(context, params.mScanlineIntensity == 0.75f);
	AT_PORTABLE_TEST_ASSERT(context, params.mDistortionViewAngleX == 0.0f);
	AT_PORTABLE_TEST_ASSERT(context, params.mDistortionYRatio == 0.0f);
	AT_PORTABLE_TEST_ASSERT(context, !params.mbEnableBloom);
	AT_PORTABLE_TEST_ASSERT(context, params.mbBloomScanlineCompensation);
	AT_PORTABLE_TEST_ASSERT(context, params.mBloomRadius == 0.86f);
	AT_PORTABLE_TEST_ASSERT(context, params.mBloomDirectIntensity == 0.80f);
	AT_PORTABLE_TEST_ASSERT(context, params.mBloomIndirectIntensity == 0.40f);
	AT_PORTABLE_TEST_ASSERT(context, !params.mbEnableHDR);
	AT_PORTABLE_TEST_ASSERT(context, params.mSDRIntensity == 200.0f);
	AT_PORTABLE_TEST_ASSERT(context, params.mHDRIntensity == 350.0f);
	AT_PORTABLE_TEST_ASSERT(context, !params.mbUseSystemSDR);
	AT_PORTABLE_TEST_ASSERT(context, !params.mbUseSystemSDRAsHDR);

	return true;
}
