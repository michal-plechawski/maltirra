// Portable behavioral tests for the production GTIA artifacting engine.

#include <algorithm>
#include <array>
#include <cmath>
#include <iterator>
#include <memory>

#include <artifacting.h>
#include <at/attest/portabletest.h>
#include <palettegenerator.h>
#include <vd2/system/vecmath.h>
#include <vd2/system/vectors.h>

namespace {
	constexpr uint32 kCanary = 0xdeadbeef;
	constexpr size_t kGuardWords = 4;
	using SourceLine = std::array<uint8, ATArtifactingEngine::N>;
	using OutputLine = std::array<uint32, ATArtifactingEngine::N>;

	ATColorParams MakeColorParams() {
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
		params.mRedScale = params.mGrnScale = params.mBluScale = 1.0f;
		params.mLumaRampMode = kATLumaRampMode_XL;
		params.mColorMatchingMode = ATColorMatchingMode::None;
		return params;
	}

	void Configure(ATArtifactingEngine& engine, const ATColorParams& params) {
		engine.SetColorParams(params, nullptr, nullptr, ATMonitorMode::Color, 0);
	}

	void BeginPlainFrame(ATArtifactingEngine& engine) {
		engine.BeginFrame(false, false, false, false, false, false, false,
			false, false, false, false);
	}

	uint32 BlendByteAverage(uint32 a, uint32 b) {
		return (a + b + 1) >> 1;
	}

	uint32 BlendRGB(uint32 a, uint32 b) {
		uint32 result = 0;
		for(int shift : { 0, 8, 16 })
			result |= BlendByteAverage((a >> shift) & 255, (b >> shift) & 255) << shift;
		return result;
	}

	bool NearRGB(uint32 a, uint32 b, int tolerance) {
		for(int shift : { 0, 8, 16 }) {
			const int ca = (a >> shift) & 255;
			const int cb = (b >> shift) & 255;
			if (std::abs(ca - cb) > tolerance)
				return false;
		}
		return true;
	}

	bool NearPaletteRGB(uint32 actual, uint32 expected) {
		// This reference uses color matching mode None so both paths share the
		// same direct RGB transform. One code accommodates their final float
		// packing/rounding and the slightly different coefficient literals.
		return NearRGB(actual, expected, 1);
	}

	bool TestIndexedPaletteAndExpansion(ATPortableTestContext& context, ATArtifactingEngine& engine,
		const ATColorPaletteGenerator& palette) {
		alignas(16) SourceLine src {};
		for(size_t i = 0; i < src.size(); ++i)
			src[i] = (uint8)i;

		alignas(16) std::array<uint32, ATArtifactingEngine::N + 2 * kGuardWords> guarded;
		guarded.fill(kCanary);
		BeginPlainFrame(engine);
		engine.Artifact8(0, guarded.data() + kGuardWords, src.data(), true, false, true);
		for(size_t i = 0; i < kGuardWords; ++i)
			AT_PORTABLE_TEST_ASSERT(context, guarded[i] == kCanary && guarded[guarded.size() - 1 - i] == kCanary);
		for(size_t i = 0; i < src.size(); ++i)
			AT_PORTABLE_TEST_ASSERT(context, NearPaletteRGB(guarded[i + kGuardWords], palette.mPalette[src[i]]));

		guarded.fill(kCanary);
		engine.Artifact8(1, guarded.data() + kGuardWords, src.data(), false, false, true);
		for(size_t i = 0; i < kGuardWords; ++i)
			AT_PORTABLE_TEST_ASSERT(context, guarded[i] == kCanary && guarded[guarded.size() - 1 - i] == kCanary);
		for(size_t i = 0; i < src.size(); i += 2) {
			const uint32 expected = palette.mPalette[src[i]];
			AT_PORTABLE_TEST_ASSERT(context, NearPaletteRGB(guarded[i + kGuardWords], expected));
			AT_PORTABLE_TEST_ASSERT(context, NearPaletteRGB(guarded[i + 1 + kGuardWords], expected));
		}
		return true;
	}

	bool TestNTSCAndPAL(ATPortableTestContext& context, ATArtifactingEngine& engine,
		const ATColorPaletteGenerator& palette) {
		alignas(16) SourceLine src {};
		for(size_t x = 0; x < src.size(); ++x) {
			static constexpr uint8 lumas[] { 0, 4, 12, 4, 0, 8, 15, 8 };
			src[x] = (uint8)(0x70 | lumas[x % std::size(lumas)]);
		}

		alignas(16) OutputLine plain {}, ntsc {}, pal0 {}, pal1 {};
		BeginPlainFrame(engine);
		engine.Artifact8(0, plain.data(), src.data(), true, false, true);

		engine.BeginFrame(false, true, false, false, false, false, false,
			false, false, false, false);
		engine.Artifact8(0, ntsc.data(), src.data(), true, false, true);
		bool ntscChanged = false;
		for(size_t i = 0; i < ntsc.size(); ++i)
			ntscChanged |= (ntsc[i] & 0xffffff) != (plain[i] & 0xffffff);
		AT_PORTABLE_TEST_ASSERT(context, ntscChanged);

		// NTSC color artifacts are disabled when ANTIC sends low-resolution pairs.
		alignas(16) OutputLine ntscLoRes {};
		engine.Artifact8(1, ntscLoRes.data(), src.data(), false, false, true);
		for(size_t i = 0; i < ntscLoRes.size(); i += 2) {
			AT_PORTABLE_TEST_ASSERT(context, ntscLoRes[i] == ntscLoRes[i + 1]);
			AT_PORTABLE_TEST_ASSERT(context, NearPaletteRGB(ntscLoRes[i], palette.mPalette[src[i]]));
		}

		engine.BeginFrame(true, true, false, false, false, false, false,
			false, false, false, false);
		engine.Artifact8(0, pal0.data(), src.data(), true, false, true);
		engine.Artifact8(1, pal1.data(), src.data(), true, false, true);
		const uint32 prev = palette.mPalette[src[0] & 15];
		const uint32 next = palette.mPalette[src[0]];
		AT_PORTABLE_TEST_ASSERT(context, NearPaletteRGB(pal0[0], BlendRGB(prev, next)));
		AT_PORTABLE_TEST_ASSERT(context, NearPaletteRGB(pal1[0], next));
		return true;
	}

	bool IsNearBlack(uint32 color) {
		return ((color >> 16) & 255) <= 4
			&& ((color >> 8) & 255) <= 4
			&& (color & 255) <= 4;
	}

	bool TestHighArtifactingAndCanaries(ATPortableTestContext& context, ATArtifactingEngine& engine) {
		alignas(16) SourceLine black {};
		alignas(16) std::array<uint32, ATArtifactingEngine::N * 2 + 2 * kGuardWords> ntscBlack, palBlack0, palBlack1;
		ntscBlack.fill(kCanary);
		palBlack0.fill(kCanary);
		palBlack1.fill(kCanary);
		engine.BeginFrame(false, true, true, false, false, false, false,
			false, false, false, false);
		engine.Artifact8(0, ntscBlack.data() + kGuardWords, black.data(), true, false, true);
		engine.BeginFrame(true, true, true, false, false, false, false,
			false, false, false, false);
		engine.Artifact8(0, palBlack0.data() + kGuardWords, black.data(), true, false, true);
		engine.Artifact8(1, palBlack1.data() + kGuardWords, black.data(), true, false, true);
		for(size_t i = 0; i < kGuardWords; ++i) {
			AT_PORTABLE_TEST_ASSERT(context, ntscBlack[i] == kCanary && ntscBlack[ntscBlack.size() - 1 - i] == kCanary);
			AT_PORTABLE_TEST_ASSERT(context, palBlack0[i] == kCanary && palBlack0[palBlack0.size() - 1 - i] == kCanary);
			AT_PORTABLE_TEST_ASSERT(context, palBlack1[i] == kCanary && palBlack1[palBlack1.size() - 1 - i] == kCanary);
		}
		for(size_t i = 0; i < ATArtifactingEngine::N * 2; ++i) {
			const uint32 ntscPixel = ntscBlack[i + kGuardWords];
			const uint32 palPixel0 = palBlack0[i + kGuardWords];
			const uint32 palPixel1 = palBlack1[i + kGuardWords];
			AT_PORTABLE_TEST_ASSERT(context, IsNearBlack(ntscPixel));
			AT_PORTABLE_TEST_ASSERT(context, IsNearBlack(palPixel0));
			AT_PORTABLE_TEST_ASSERT(context, IsNearBlack(palPixel1));
			AT_PORTABLE_TEST_ASSERT(context, NearRGB(palPixel0, ntscPixel, 4));
			AT_PORTABLE_TEST_ASSERT(context, NearRGB(palPixel1, ntscPixel, 4));
		}

		alignas(16) SourceLine src {};
		for(size_t x = 0; x < src.size(); ++x)
			src[x] = (uint8)(((x * 5) & 15) << 4 | ((x * 7 + x / 3) & 15));

		alignas(16) std::array<uint32, ATArtifactingEngine::N * 2 + 2 * kGuardWords> ntsc;
		ntsc.fill(kCanary);
		engine.BeginFrame(false, true, true, false, false, false, false,
			false, false, false, false);
		engine.Artifact8(0, ntsc.data() + kGuardWords, src.data(), true, false, true);
		for(size_t i = 0; i < kGuardWords; ++i)
			AT_PORTABLE_TEST_ASSERT(context, ntsc[i] == kCanary && ntsc[ntsc.size() - 1 - i] == kCanary);
		bool ntscVaries = false;
		for(size_t i = 1; i < ATArtifactingEngine::N * 2; ++i)
			ntscVaries |= (ntsc[i + kGuardWords] & 0xffffff) != (ntsc[kGuardWords] & 0xffffff);
		AT_PORTABLE_TEST_ASSERT(context, ntscVaries);

		alignas(16) std::array<uint32, ATArtifactingEngine::N * 2 + 2 * kGuardWords> palEven, palOdd;
		palEven.fill(kCanary);
		palOdd.fill(kCanary);
		engine.BeginFrame(true, true, true, false, false, false, false,
			false, false, false, false);
		engine.Artifact8(0, palEven.data() + kGuardWords, src.data(), true, false, true);
		engine.Artifact8(1, palOdd.data() + kGuardWords, src.data(), true, false, true);
		for(size_t i = 0; i < kGuardWords; ++i) {
			AT_PORTABLE_TEST_ASSERT(context, palEven[i] == kCanary && palEven[palEven.size() - 1 - i] == kCanary);
			AT_PORTABLE_TEST_ASSERT(context, palOdd[i] == kCanary && palOdd[palOdd.size() - 1 - i] == kCanary);
		}
		bool palPhaseDiffers = false;
		for(size_t i = 0; i < ATArtifactingEngine::N * 2; ++i)
			palPhaseDiffers |= (palEven[i + kGuardWords] & 0xffffff) != (palOdd[i + kGuardWords] & 0xffffff);
		AT_PORTABLE_TEST_ASSERT(context, palPhaseDiffers);
		return true;
	}

	bool TestFrameBlend(ATPortableTestContext& context, ATArtifactingEngine& engine,
		const ATColorPaletteGenerator& palette) {
		alignas(16) SourceLine src {};
		alignas(16) OutputLine output {};
		constexpr uint32 y = 7;
		constexpr uint8 a = 0x35;
		constexpr uint8 b = 0x9a;
		constexpr uint8 c = 0xd2;
		constexpr uint8 d = 0x47;

		src.fill(a);
		engine.BeginFrame(false, false, false, false, true, false, false,
			false, false, false, false);
		engine.Artifact8(y, output.data(), src.data(), true, false, true);
		for(uint32 px : output)
			AT_PORTABLE_TEST_ASSERT(context, NearPaletteRGB(px, palette.mPalette[a]));

		src.fill(b);
		engine.BeginFrame(false, false, false, true, true, false, false,
			false, false, false, false);
		engine.Artifact8(y, output.data(), src.data(), true, true, true);
		for(uint32 px : output)
			AT_PORTABLE_TEST_ASSERT(context, NearPaletteRGB(px, BlendRGB(palette.mPalette[a], palette.mPalette[b])));

		src.fill(c);
		engine.Artifact8(y, output.data(), src.data(), true, false, true);
		for(uint32 px : output)
			AT_PORTABLE_TEST_ASSERT(context, NearPaletteRGB(px, BlendRGB(palette.mPalette[a], palette.mPalette[c])));

		src.fill(d);
		engine.Artifact8(y, output.data(), src.data(), true, true, true);
		for(uint32 px : output)
			AT_PORTABLE_TEST_ASSERT(context, NearPaletteRGB(px, BlendRGB(palette.mPalette[c], palette.mPalette[d])));
		return true;
	}

	bool TestInterpolationAndDeinterlace(ATPortableTestContext& context, ATArtifactingEngine& engine) {
		alignas(16) std::array<uint32, 8> upper {}, lower {};
		const ATArtifactingParams originalArtifactingParams = engine.GetArtifactingParams();
		for(size_t i = 0; i < 8; ++i) {
			upper[i] = 0x00102030u + (uint32)i * 0x000b0703u;
			lower[i] = 0x00c0a080u - (uint32)i * 0x00090705u;
		}
		ATArtifactingParams artifactingParams = engine.GetArtifactingParams();
		for(float intensity : { 0.0f, 0.25f, 0.5f, 1.0f }) {
			artifactingParams.mScanlineIntensity = intensity;
			engine.SetArtifactingParams(artifactingParams);
			for(uint32 n = 0; n <= 7; ++n) {
				alignas(16) std::array<uint32, 10> guarded;
				guarded.fill(kCanary);
				engine.InterpolateScanlines(guarded.data() + 1, upper.data(), lower.data(), n);
				AT_PORTABLE_TEST_ASSERT(context, guarded[0] == kCanary);
				for(uint32 i = n + 1; i < guarded.size(); ++i)
					AT_PORTABLE_TEST_ASSERT(context, guarded[i] == kCanary);
				for(uint32 i = 0; i < n; ++i) {
					for(int shift : { 0, 8, 16, 24 }) {
						const uint32 a = (upper[i] >> shift) & 255;
						const uint32 b = (lower[i] >> shift) & 255;
						const uint32 avg = (a + b + 1) >> 1;
						const uint32 expected = (uint32)(avg * intensity + 0.5f);
						AT_PORTABLE_TEST_ASSERT(context, ((guarded[i + 1] >> shift) & 255) == expected);
					}
				}
			}
		}
		// Exercise the aligned SIMD path too. Its rounded fixed-point kernels
		// can differ from the scalar float oracle by one channel code.
		artifactingParams.mScanlineIntensity = 0.75f;
		engine.SetArtifactingParams(artifactingParams);
		alignas(16) std::array<uint32, 8> interpolated {};
		engine.InterpolateScanlines(interpolated.data(), upper.data(), lower.data(), 8);
		for(size_t i = 0; i < interpolated.size(); ++i) {
			for(int shift : { 0, 8, 16, 24 }) {
				const uint32 avg = (((upper[i] >> shift) & 255) + ((lower[i] >> shift) & 255) + 1) >> 1;
				const uint32 expected = (uint32)(avg * 0.75f + 0.5f);
				AT_PORTABLE_TEST_ASSERT(context, std::abs((int)((interpolated[i] >> shift) & 255) - (int)expected) <= 1);
			}
		}
		engine.SetArtifactingParams(originalArtifactingParams);

		alignas(16) const std::array<uint32, 2> stable { 0, 0 };
		alignas(16) const std::array<uint32, 2> changed { 0x00303030, 0x00303030 };
		alignas(16) const std::array<uint32, 2> current { 0x00101010, 0x00101010 };
		alignas(16) std::array<uint32, 2> output { kCanary, kCanary };

		// Deinterlace shares storage with 14 MHz frame blending. Initialize the
		// three history rows explicitly before asking it to compare against them.
		alignas(16) std::array<uint32, ATArtifactingEngine::N * 2> historyRow {};
		engine.BeginFrame(false, false, false, false, true, false, false,
			true, false, false, false);
		for(uint32 y = 0; y < 3; ++y)
			engine.Artifact32(y, historyRow.data(), (uint32)historyRow.size(), false, true);

		engine.Deinterlace(0, nullptr, current.data(), stable.data(), 2);
		engine.Deinterlace(1, nullptr, current.data(), stable.data(), 2);
		engine.Deinterlace(2, output.data(), current.data(), changed.data(), 2);
		AT_PORTABLE_TEST_ASSERT(context, output[0] == 0x00202020 && output[1] == 0x00202020);

		alignas(16) std::array<uint32, 2> settling { kCanary, kCanary };
		engine.Deinterlace(0, nullptr, current.data(), stable.data(), 2);
		engine.Deinterlace(1, nullptr, current.data(), stable.data(), 2);
		engine.Deinterlace(2, settling.data(), current.data(), stable.data(), 2);
		AT_PORTABLE_TEST_ASSERT(context, settling[0] == 0x00080808 && settling[1] == 0x00080808);

		alignas(16) std::array<uint32, 2> untouched { kCanary, kCanary };
		engine.Deinterlace(0, nullptr, current.data(), stable.data(), 2);
		engine.Deinterlace(1, nullptr, current.data(), stable.data(), 2);
		engine.Deinterlace(2, untouched.data(), current.data(), stable.data(), 2);
		AT_PORTABLE_TEST_ASSERT(context, untouched[0] == kCanary && untouched[1] == kCanary);
		return true;
	}

	bool TestColorCorrectionAndTint(ATPortableTestContext& context, ATArtifactingEngine& engine) {
		ATColorParams params = MakeColorParams();
		const vdfloat3x3 zeroMatrix = vdfloat3x3::zero();
		engine.SetColorParams(params, &zeroMatrix, nullptr, ATMonitorMode::Color, 0);
		AT_PORTABLE_TEST_ASSERT(context, (engine.CorrectSingleColor(0x00abcdef, true, false) & 0xffffff) == 0);
		AT_PORTABLE_TEST_ASSERT(context, (engine.CorrectSingleColor(0x00abcdef, true, true) & 0xffffff) == 0x404040);

		const nsVDVecMath::vdfloat32x3 tint = nsVDVecMath::vdfloat32x3::set(0.15f, 0.75f, 0.20f);
		engine.SetColorParams(params, nullptr, &tint, ATMonitorMode::MonoGreen, 0);
		engine.BeginFrame(false, false, false, false, false, false, false,
			true, false, false, false);
		alignas(16) SourceLine src {};
		alignas(16) OutputLine dst {};
		src.fill(0x8f);
		engine.Artifact8(0, dst.data(), src.data(), true, false, true);
		const uint32 color = dst[0];
		AT_PORTABLE_TEST_ASSERT(context, ((color >> 8) & 255) > ((color >> 16) & 255));
		AT_PORTABLE_TEST_ASSERT(context, ((color >> 8) & 255) > (color & 255));
		return true;
	}

	bool TestSRGBColorCorrection(ATPortableTestContext& context, ATArtifactingEngine& engine) {
		ATColorParams params = MakeColorParams();
		params.mColorMatchingMode = ATColorMatchingMode::SRGB;
		ATColorPaletteGenerator palette;
		palette.Generate(params, ATMonitorMode::Color);
		AT_PORTABLE_TEST_ASSERT(context, palette.mColorMatchingMatrix.has_value());
		const vdfloat3x3& matrix = *palette.mColorMatchingMatrix;
		engine.SetColorParams(params, &matrix, nullptr, ATMonitorMode::Color, 0);

		constexpr uint32 input = 0x003080d0;
		const uint32 uncorrected = engine.CorrectSingleColor(input, false, false);
		const uint32 corrected = engine.CorrectSingleColor(input, true, false);
		const uint32 signedCorrected = engine.CorrectSingleColor(input, true, true);
		AT_PORTABLE_TEST_ASSERT(context, (uncorrected & 0xffffff) == input);
		AT_PORTABLE_TEST_ASSERT(context, (corrected & 0xffffff) != input);
		AT_PORTABLE_TEST_ASSERT(context, (signedCorrected & 0xffffff) == (((corrected & 0xfefefe) >> 1) + 0x404040));
		return true;
	}

	void BeginArtifactFrame(ATArtifactingEngine& engine, bool pal, bool signedRange,
		bool blendIn, bool blendOut, bool monoPersistence, bool deinterlacing) {
		engine.BeginFrame(pal, true, true, blendIn, blendOut, false, monoPersistence,
			true, signedRange, signedRange, deinterlacing);
	}

	bool TestSuspendResumeContinuation(ATPortableTestContext& context, const ATColorParams& params) {
		// Exercise real state transfer between two independent production engines.
		// Two seed rows initialize the 14 MHz mono-persistence frame buffer before
		// the PAL continuation, and processing during suspension is temporary.
		for(bool signedRange : { false, true }) {
			std::unique_ptr<ATArtifactingEngine> reference(new ATArtifactingEngine);
			std::unique_ptr<ATArtifactingEngine> suspended(new ATArtifactingEngine);
			const nsVDVecMath::vdfloat32x3 tint = nsVDVecMath::vdfloat32x3::set(0.15f, 0.75f, 0.20f);
			reference->SetColorParams(params, nullptr, &tint, ATMonitorMode::MonoGreen, 0);
			suspended->SetColorParams(params, nullptr, &tint, ATMonitorMode::MonoGreen, 0);

			alignas(16) SourceLine src0 {}, src1 {}, alternate {};
			for(size_t x = 0; x < src0.size(); ++x) {
				src0[x] = (uint8)(((x * 5 + 3) & 15) << 4 | ((x * 7 + 2) & 15));
				src1[x] = (uint8)(((x * 3 + 9) & 15) << 4 | ((x * 11 + 5) & 15));
				alternate[x] = (uint8)(((x * 13 + 1) & 15) << 4 | ((x * 5 + 12) & 15));
			}
			alignas(16) std::array<uint32, ATArtifactingEngine::N * 2> scratch0 {}, scratch1 {};
			alignas(16) std::array<uint32, ATArtifactingEngine::N * 2> refLine {}, testLine {};
			alignas(16) std::array<uint32, ATArtifactingEngine::N * 2> alternateLine {};

			BeginArtifactFrame(*reference, true, signedRange, false, true, true, true);
			BeginArtifactFrame(*suspended, true, signedRange, false, true, true, true);
			reference->Artifact8(0, scratch0.data(), src0.data(), true, false, true);
			reference->Artifact8(1, scratch1.data(), src1.data(), true, false, true);
			suspended->Artifact8(0, scratch0.data(), src0.data(), true, false, true);
			suspended->Artifact8(1, scratch1.data(), src1.data(), true, false, true);

			BeginArtifactFrame(*reference, true, signedRange, true, true, true, true);
			BeginArtifactFrame(*suspended, true, signedRange, true, true, true, true);
			reference->Artifact8(0, refLine.data(), src0.data(), true, false, true);
			suspended->Artifact8(0, testLine.data(), src0.data(), true, false, true);
			for(size_t i = 0; i < refLine.size(); ++i)
				AT_PORTABLE_TEST_ASSERT(context, refLine[i] == testLine[i]);

			suspended->SuspendFrame();
			// Change signedness, PAL/NTSC, high artifact mode and persistence;
			// overwrite PAL delay history, then switch back before resuming.
			BeginArtifactFrame(*suspended, false, !signedRange, false, true, false, false);
			suspended->Artifact8(1, alternateLine.data(), alternate.data(), true, true, true);
			BeginArtifactFrame(*suspended, true, !signedRange, false, true, false, false);
			suspended->Artifact8(1, alternateLine.data(), alternate.data(), true, true, true);
			BeginArtifactFrame(*suspended, false, !signedRange, false, true, false, false);
			suspended->Artifact8(1, alternateLine.data(), alternate.data(), true, true, true);
			suspended->ResumeFrame();

			reference->Artifact8(1, refLine.data(), src1.data(), true, false, true);
			suspended->Artifact8(1, testLine.data(), src1.data(), true, false, true);
			for(size_t i = 0; i < refLine.size(); ++i)
				AT_PORTABLE_TEST_ASSERT(context, refLine[i] == testLine[i]);
		}
		return true;
	}
}

bool ATTestAltirraArtifactingEngine(ATPortableTestContext& context) {
	// The engine contains multi-megabyte filter tables; keep it off the test stack.
	std::unique_ptr<ATArtifactingEngine> engine(new ATArtifactingEngine);
	const ATColorParams colorParams = MakeColorParams();
	ATColorPaletteGenerator palette;
	palette.Generate(colorParams, ATMonitorMode::Color);
	Configure(*engine, colorParams);

	return TestIndexedPaletteAndExpansion(context, *engine, palette)
		&& TestNTSCAndPAL(context, *engine, palette)
		&& TestHighArtifactingAndCanaries(context, *engine)
		&& TestFrameBlend(context, *engine, palette)
		&& TestInterpolationAndDeinterlace(context, *engine)
		&& TestColorCorrectionAndTint(context, *engine)
		&& TestSRGBColorCorrection(context, *engine)
		&& TestSuspendResumeContinuation(context, colorParams);
}
