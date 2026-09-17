// Altirra portable GTIA palette generator tests

#include <array>

#include <at/atcore/configvar.h>
#include <at/attest/portabletest.h>
#include <palettegenerator.h>

extern ATConfigVarRGBColor g_ATCVDisplayMonoColorWhite;

#if !defined(_WIN32)
ATConfigVarRGBColor g_ATCVDisplayMonoColorWhite(
	"test.display.mono_color_white", 0xFFFFFF);
#endif

namespace {
	ATColorParams MakeNTSCTestParams() {
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

	uint32 GetRed(uint32 color) { return (color >> 16) & 0xFF; }
	uint32 GetGreen(uint32 color) { return (color >> 8) & 0xFF; }
	uint32 GetBlue(uint32 color) { return color & 0xFF; }
}

bool ATTestAltirraPaletteGenerator(ATPortableTestContext& context) {
	const ATColorParams params = MakeNTSCTestParams();
	ATColorPaletteGenerator generator;
	generator.Generate(params, ATMonitorMode::Color);

	AT_PORTABLE_TEST_ASSERT(context, generator.mColorMatchingMatrix.has_value());
	AT_PORTABLE_TEST_ASSERT(context, !generator.mTintColor.has_value());
	AT_PORTABLE_TEST_ASSERT(context, generator.mOutputGamma == 0.0f);
	for(size_t i = 0; i < 256; ++i) {
		AT_PORTABLE_TEST_ASSERT(context, !(generator.mPalette[i] & 0xFF000000));
		AT_PORTABLE_TEST_ASSERT(context, !(generator.mSignedPalette[i] & 0xFF000000));
		AT_PORTABLE_TEST_ASSERT(context, !(generator.mUncorrectedSignedPalette[i] & 0xFF000000));
	}

	for(size_t i = 0; i < 16; ++i) {
		const uint32 gray = generator.mUncorrectedPalette[i] & 0xFFFFFF;
		AT_PORTABLE_TEST_ASSERT(context, GetRed(gray) == GetGreen(gray));
		AT_PORTABLE_TEST_ASSERT(context, GetGreen(gray) == GetBlue(gray));
		if (i)
			AT_PORTABLE_TEST_ASSERT(
				context,
				GetRed(gray) >= GetRed(generator.mUncorrectedPalette[i - 1]));
	}
	AT_PORTABLE_TEST_ASSERT(context, generator.mPalette[16 + 8] != generator.mPalette[32 + 8]);

	generator.Generate(params, ATMonitorMode::MonoGreen);
	AT_PORTABLE_TEST_ASSERT(context, !generator.mColorMatchingMatrix.has_value());
	AT_PORTABLE_TEST_ASSERT(context, generator.mTintColor.has_value());
	for(size_t hue = 2; hue < 16; ++hue) {
		for(size_t luma = 0; luma < 16; ++luma)
			AT_PORTABLE_TEST_ASSERT(
				context,
				generator.mPalette[hue * 16 + luma] == generator.mPalette[16 + luma]);
	}

	std::array<uint32, 256> monoRamp {};
	ATColorPaletteGenerator::GenerateMonoRamp(
		params, ATMonitorMode::MonoGreen, monoRamp.data());
	AT_PORTABLE_TEST_ASSERT(context, monoRamp.front() == 0);
	AT_PORTABLE_TEST_ASSERT(context, monoRamp.back() != 0);
	AT_PORTABLE_TEST_ASSERT(context, GetGreen(monoRamp[128]) > GetRed(monoRamp[128]));
	AT_PORTABLE_TEST_ASSERT(context, GetGreen(monoRamp[128]) > GetBlue(monoRamp[128]));

	std::array<uint32, 1024> persistenceRamp {};
	ATColorPaletteGenerator::GenerateMonoPersistenceRamp(
		params, ATMonitorMode::MonoAmber, persistenceRamp.data());
	AT_PORTABLE_TEST_ASSERT(context, persistenceRamp.front() == 0);
	AT_PORTABLE_TEST_ASSERT(context, persistenceRamp.back() != 0);
	AT_PORTABLE_TEST_ASSERT(context, GetRed(persistenceRamp[512]) > GetBlue(persistenceRamp[512]));
	AT_PORTABLE_TEST_ASSERT(context, GetGreen(persistenceRamp[512]) > GetBlue(persistenceRamp[512]));

	generator.Generate(params, ATMonitorMode::Peritel);
	AT_PORTABLE_TEST_ASSERT(context, !generator.mColorMatchingMatrix.has_value());
	AT_PORTABLE_TEST_ASSERT(context, !generator.mTintColor.has_value());
	for(size_t i = 0; i < 8; ++i)
		AT_PORTABLE_TEST_ASSERT(context, generator.mPalette[i * 2] == generator.mPalette[i * 2 + 1]);
	for(size_t hue = 1; hue < 16; ++hue) {
		for(size_t luma = 0; luma < 16; ++luma)
			AT_PORTABLE_TEST_ASSERT(
				context,
				generator.mPalette[hue * 16 + luma] == generator.mPalette[luma]);
	}

	generator.Generate(params, ATMonitorMode::MonoWhite);
	AT_PORTABLE_TEST_ASSERT(context, generator.mTintColor.has_value());
	AT_PORTABLE_TEST_ASSERT(context, generator.mPalette[15] != 0);

	return true;
}
