// Altirra portable GTIA palette solver tests

#include <memory>

#include <at/attest/portabletest.h>
#include <palettegenerator.h>
#include <palettesolver.h>

namespace {
	ATColorParams MakeSolverTestParams() {
		ATColorParams params {};
		params.mHueStart = -57.0f;
		params.mHueRange = 27.1f * 15.0f;
		params.mBrightness = -0.04f;
		params.mContrast = 1.04f;
		params.mSaturation = 0.20f;
		params.mGammaCorrect = 1.0f;
		params.mIntensityScale = 1.0f;
		params.mArtifactHue = 252.0f;
		params.mArtifactSat = 1.15f;
		params.mArtifactSharpness = 0.50f;
		params.mRedScale = 1.0f;
		params.mGrnScale = 1.0f;
		params.mBluScale = 1.0f;
		params.mLumaRampMode = kATLumaRampMode_XL;
		params.mColorMatchingMode = ATColorMatchingMode::SRGB;
		return params;
	}
}

bool ATTestAltirraPaletteSolver(ATPortableTestContext& context) {
	ATColorParams targetParams = MakeSolverTestParams();
	ATColorPaletteGenerator generator;
	generator.Generate(targetParams, ATMonitorMode::Color);

	std::unique_ptr<IATColorPaletteSolver> solver(ATCreateColorPaletteSolver());
	AT_PORTABLE_TEST_ASSERT(context, !solver->GetCurrentError().has_value());

	solver->Init(targetParams, generator.mPalette, false, false);
	AT_PORTABLE_TEST_ASSERT(context, solver->GetCurrentError() == 0);
	for(size_t i = 0; i < 128; ++i) {
		const auto status = solver->Iterate();
		AT_PORTABLE_TEST_ASSERT(
			context,
			status == IATColorPaletteSolver::Status::RunningNoImprovement);
		AT_PORTABLE_TEST_ASSERT(context, solver->GetCurrentError() == 0);
	}

	ATColorParams initialParams = targetParams;
	initialParams.mHueStart = -20.0f;
	initialParams.mBrightness = 0.12f;
	initialParams.mContrast = 0.75f;
	initialParams.mSaturation = 0.35f;
	initialParams.mGammaCorrect = 1.55f;
	solver->Reinit(initialParams);
	const std::optional<uint32> reinitializedError = solver->GetCurrentError();
	AT_PORTABLE_TEST_ASSERT(context, reinitializedError.has_value());
	AT_PORTABLE_TEST_ASSERT(context, *reinitializedError > 0);

	ATColorParams solution {};
	solver->GetCurrentSolution(solution);
	AT_PORTABLE_TEST_ASSERT(context, solution.mHueStart == initialParams.mHueStart);
	AT_PORTABLE_TEST_ASSERT(context, solution.mBrightness == initialParams.mBrightness);
	AT_PORTABLE_TEST_ASSERT(context, solution.mGammaCorrect == initialParams.mGammaCorrect);

	uint32 previousError = *reinitializedError;
	bool improved = false;
	for(size_t i = 0; i < 10000; ++i) {
		solver->Iterate();
		const uint32 error = *solver->GetCurrentError();
		AT_PORTABLE_TEST_ASSERT(context, error <= previousError);
		improved |= error < previousError;
		previousError = error;
	}
	AT_PORTABLE_TEST_ASSERT(context, improved);
	AT_PORTABLE_TEST_ASSERT(context, previousError < *reinitializedError);

	solver->Init(initialParams, generator.mPalette, true, true);
	const uint32 lockedInitialError = *solver->GetCurrentError();
	for(size_t i = 0; i < 10000; ++i)
		solver->Iterate();
	solver->GetCurrentSolution(solution);
	AT_PORTABLE_TEST_ASSERT(context, solution.mHueStart == initialParams.mHueStart);
	AT_PORTABLE_TEST_ASSERT(context, solution.mGammaCorrect == initialParams.mGammaCorrect);
	AT_PORTABLE_TEST_ASSERT(context, *solver->GetCurrentError() <= lockedInitialError);

	return true;
}
