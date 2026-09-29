// Portable VDDisplay bloom parameter tests.

#include <cmath>

#include <at/attest/portabletest.h>
#include <vd2/VDDisplay/display.h>
#include <vd2/VDDisplay/internal/bloom.h>

namespace {
	bool NearlyEqual(float x, float y, float tolerance = 1e-4f) {
		return std::fabs(x - y) <= tolerance;
	}

	float EvaluateCubic(const vdfloat4& c, float x) {
		return ((c.x * x + c.y) * x + c.z) * x + c.w;
	}

	float EvaluateCubicDerivative(const vdfloat4& c, float x) {
		return (3.0f * c.x * x + 2.0f * c.y) * x + c.z;
	}

	bool IsFinite(const VDDBloomV2RenderParams& params) {
		for(const vdfloat2& blend : params.mPassBlendFactors) {
			if (!std::isfinite(blend.x) || !std::isfinite(blend.y))
				return false;
		}

		return std::isfinite(params.mShoulder.x)
			&& std::isfinite(params.mShoulder.y)
			&& std::isfinite(params.mShoulder.z)
			&& std::isfinite(params.mShoulder.w)
			&& std::isfinite(params.mThresholds.x)
			&& std::isfinite(params.mThresholds.y)
			&& std::isfinite(params.mThresholds.z)
			&& std::isfinite(params.mThresholds.w)
			&& std::isfinite(params.mBaseUVStepScale)
			&& std::isfinite(params.mBaseWeights.x)
			&& std::isfinite(params.mBaseWeights.y)
			&& std::isfinite(params.mBaseWeights.z)
			&& std::isfinite(params.mBaseWeights.w);
	}
}

bool ATTestVDDisplayBloom(ATPortableTestContext& context) {
	const VDDBloomV2Settings oldSettings = g_VDDispBloomV2Settings;
	const uint32 oldChangeCounter = g_VDDispBloomCoeffsChanged;
	const VDDBloomV2Settings settings;
	VDDSetBloomV2Settings(settings);

	AT_PORTABLE_TEST_ASSERT(context, g_VDDispBloomCoeffsChanged == oldChangeCounter + 1);
	AT_PORTABLE_TEST_ASSERT(context, g_VDDispBloomV2Settings.mCoeffWidthBase == settings.mCoeffWidthBase);
	AT_PORTABLE_TEST_ASSERT(context, g_VDDispBloomV2Settings.mCoeffWidthBaseSlope == settings.mCoeffWidthBaseSlope);
	AT_PORTABLE_TEST_ASSERT(context, g_VDDispBloomV2Settings.mCoeffWidthAdjustSlope == settings.mCoeffWidthAdjustSlope);
	AT_PORTABLE_TEST_ASSERT(context, g_VDDispBloomV2Settings.mShoulderX == settings.mShoulderX);
	AT_PORTABLE_TEST_ASSERT(context, g_VDDispBloomV2Settings.mShoulderY == settings.mShoulderY);
	AT_PORTABLE_TEST_ASSERT(context, g_VDDispBloomV2Settings.mLimitX == settings.mLimitX);
	AT_PORTABLE_TEST_ASSERT(context, g_VDDispBloomV2Settings.mLimitSlope == settings.mLimitSlope);

	VDDBloomV2ControlParams control {};
	control.mBaseRadius = 4.4f;
	control.mAdjustRadius = 1.0f;
	control.mDirectIntensity = 0.25f;
	control.mIndirectIntensity = 1.0f;

	const VDDBloomV2RenderParams params = VDDComputeBloomV2Parameters(control);
	AT_PORTABLE_TEST_ASSERT(context, IsFinite(params));
	AT_PORTABLE_TEST_ASSERT(context, NearlyEqual(params.mThresholds.x, 1.0f));
	AT_PORTABLE_TEST_ASSERT(context, NearlyEqual(params.mThresholds.y, 0.305f));
	AT_PORTABLE_TEST_ASSERT(context, NearlyEqual(params.mThresholds.z, 1.781f));
	AT_PORTABLE_TEST_ASSERT(context, NearlyEqual(params.mThresholds.w, 0.0f));
	AT_PORTABLE_TEST_ASSERT(context, NearlyEqual(EvaluateCubic(params.mShoulder, 0.305f), 0.305f));
	AT_PORTABLE_TEST_ASSERT(context, NearlyEqual(EvaluateCubicDerivative(params.mShoulder, 0.305f), 1.0f));
	AT_PORTABLE_TEST_ASSERT(context, NearlyEqual(EvaluateCubic(params.mShoulder, 1.781f), 1.0f));
	AT_PORTABLE_TEST_ASSERT(context, NearlyEqual(EvaluateCubicDerivative(params.mShoulder, 1.781f), 0.086f));

	const vdfloat2 expectedBlendFactors[] = {
		{ 0.261204f, 0.738796f },
		{ 0.323663f, 0.676337f },
		{ 0.343292f, 0.656708f },
		{ 0.349962f, 0.650038f },
		{ 1.0f, 0.0f },
		{ 1.0f, 0.0f }
	};

	for(uint32 i=0; i<6; ++i) {
		AT_PORTABLE_TEST_ASSERT(context, NearlyEqual(params.mPassBlendFactors[i].x, expectedBlendFactors[i].x));
		AT_PORTABLE_TEST_ASSERT(context, NearlyEqual(params.mPassBlendFactors[i].y, expectedBlendFactors[i].y));
	}

	AT_PORTABLE_TEST_ASSERT(context, NearlyEqual(params.mBaseUVStepScale, 1.0f));
	AT_PORTABLE_TEST_ASSERT(context, NearlyEqual(params.mBaseWeights.x, 0.0f));
	AT_PORTABLE_TEST_ASSERT(context, NearlyEqual(params.mBaseWeights.y, 0.0f));
	AT_PORTABLE_TEST_ASSERT(context, NearlyEqual(params.mBaseWeights.z, 0.25f));
	AT_PORTABLE_TEST_ASSERT(context, NearlyEqual(params.mBaseWeights.w, 0.0f));

	control.mbRenderLinear = true;
	const VDDBloomV2RenderParams linearParams = VDDComputeBloomV2Parameters(control);
	AT_PORTABLE_TEST_ASSERT(context, linearParams.mShoulder.x == 0.0f);
	AT_PORTABLE_TEST_ASSERT(context, linearParams.mShoulder.y == 0.0f);
	AT_PORTABLE_TEST_ASSERT(context, linearParams.mShoulder.z == 0.0f);
	AT_PORTABLE_TEST_ASSERT(context, linearParams.mShoulder.w == 0.0f);
	AT_PORTABLE_TEST_ASSERT(context, linearParams.mThresholds.x == 1.0f);
	AT_PORTABLE_TEST_ASSERT(context, linearParams.mThresholds.y == 100.0f);
	AT_PORTABLE_TEST_ASSERT(context, linearParams.mThresholds.z == 100.0f);
	AT_PORTABLE_TEST_ASSERT(context, linearParams.mThresholds.w == 0.0f);
	for(uint32 i=0; i<6; ++i) {
		AT_PORTABLE_TEST_ASSERT(context, linearParams.mPassBlendFactors[i].x == params.mPassBlendFactors[i].x);
		AT_PORTABLE_TEST_ASSERT(context, linearParams.mPassBlendFactors[i].y == params.mPassBlendFactors[i].y);
	}
	AT_PORTABLE_TEST_ASSERT(context, linearParams.mBaseWeights.x == params.mBaseWeights.x);
	AT_PORTABLE_TEST_ASSERT(context, linearParams.mBaseWeights.y == params.mBaseWeights.y);
	AT_PORTABLE_TEST_ASSERT(context, linearParams.mBaseWeights.z == params.mBaseWeights.z);
	AT_PORTABLE_TEST_ASSERT(context, linearParams.mBaseWeights.w == params.mBaseWeights.w);

	control.mbRenderLinear = false;
	control.mBaseRadius = 2.2f;
	control.mAdjustRadius = 256.0f;
	control.mDirectIntensity = 0.0f;
	const VDDBloomV2RenderParams extremeRadiusParams = VDDComputeBloomV2Parameters(control);
	AT_PORTABLE_TEST_ASSERT(context, IsFinite(extremeRadiusParams));

	VDDSetBloomV2Settings(oldSettings);
	return true;
}
