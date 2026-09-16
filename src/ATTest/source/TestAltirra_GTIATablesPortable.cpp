// Portable reference tests for GTIA priority, PAL phase, and luma tables.

#include <array>
#include <cmath>

#include <at/attest/portabletest.h>
#include <gtiatables.h>

namespace {
	uint8 ReferencePriority(uint8 prior, uint8 input) {
		using namespace ATGTIA;
		static constexpr uint8 kPlayfieldPriority[8] { 0, 1, 2, 2, 4, 4, 4, 4 };
		const uint8 playfield = kPlayfieldPriority[input & 7];
		const bool pf0 = (playfield & 1) != 0;
		const bool pf1 = (playfield & 2) != 0;
		const bool pf2 = (playfield & 4) != 0;
		const bool pf3 = (input & PF3) != 0;
		const bool p0 = (input & P0) != 0;
		const bool p1 = (input & P1) != 0;
		const bool p2 = (input & P2) != 0;
		const bool p3 = (input & P3) != 0;
		const bool multi = (prior & 16) != 0;
		const bool pri0 = (prior & 1) != 0;
		const bool pri1 = (prior & 2) != 0;
		const bool pri2 = (prior & 4) != 0;
		const bool pri3 = (prior & 8) != 0;
		const bool p01 = p0 || p1;
		const bool p23 = p2 || p3;
		const bool pf01 = pf0 || pf1;
		const bool pf23 = pf2 || pf3;
		const bool sp0 = p0 && !(pf01 && (pri2 || pri3)) && !(pri2 && pf23);
		const bool sp1 = p1 && !(pf01 && (pri2 || pri3)) && !(pri2 && pf23) && (!p0 || multi);
		const bool sp2 = p2 && !p01 && !(pf23 && (pri1 || pri2)) && !(pf01 && !pri0);
		const bool sp3 = p3 && !p01 && !(pf23 && (pri1 || pri2)) && !(pf01 && !pri0) && (!p2 || multi);
		const bool sf3 = pf3 && !(p23 && (pri0 || pri3)) && !(p01 && !pri2);
		const bool sf2 = pf2 && !(p23 && (pri0 || pri3)) && !(p01 && !pri2) && !sf3;
		const bool sf1 = pf1 && !(p23 && pri0) && !(p01 && (pri0 || pri1)) && !sf3;
		const bool sf0 = pf0 && !(p23 && pri0) && !(p01 && (pri0 || pri1)) && !sf3;

		const uint16 visible = (sf0 ? 0x001 : 0) | (sf1 ? 0x002 : 0)
			| (sf2 ? 0x004 : 0) | (sf3 ? 0x008 : 0)
			| (sp0 ? 0x010 : 0) | (sp1 ? 0x020 : 0)
			| (sp2 ? 0x040 : 0) | (sp3 ? 0x080 : 0);
		switch(visible) {
			case 0x000: return input ? kColorBlack : kColorBAK;
			case 0x001: return kColorPF0;
			case 0x002: return kColorPF1;
			case 0x004: return kColorPF2;
			case 0x008: return kColorPF3;
			case 0x010: return kColorP0;
			case 0x011: return kColorPF0P0;
			case 0x012: return kColorPF1P0;
			case 0x020: return kColorP1;
			case 0x021: return kColorPF0P1;
			case 0x022: return kColorPF1P1;
			case 0x030: return kColorP0P1;
			case 0x031: return kColorPF0P0P1;
			case 0x032: return kColorPF1P0P1;
			case 0x040: return kColorP2;
			case 0x044: return kColorPF2P2;
			case 0x048: return kColorPF3P2;
			case 0x080: return kColorP3;
			case 0x084: return kColorPF2P3;
			case 0x088: return kColorPF3P3;
			case 0x0c0: return kColorP2P3;
			case 0x0c4: return kColorPF2P2P3;
			case 0x0c8: return kColorPF3P2P3;
			default: return 0xff;
		}
	}
}

bool ATTestAltirraGTIATables(ATPortableTestContext& context) {
	using namespace ATGTIA;
	static constexpr std::array<uint8, 24> kExpectedAnalysisColors {
		0x1a, 0x5a, 0x7a, 0x9a, 0x03, 0x07, 0x0b, 0x0f,
		0x01, 0x00, 0x3a, 0x8a, 0x13, 0x53, 0x33, 0x17,
		0x57, 0x37, 0x1b, 0x5b, 0x3b, 0x1f, 0x5f, 0x3f
	};
	for(size_t i = 0; i < kExpectedAnalysisColors.size(); ++i)
		AT_PORTABLE_TEST_ASSERT(context, kATAnalysisColorTable[i] == kExpectedAnalysisColors[i]);

	static constexpr ATPALPhaseInfo kExpectedPALPhases[15] {
		{  0,  1,  0,  1 }, {  1,  1,  1,  1 }, { -6, -1,  2,  1 },
		{ -5, -1, -5, -1 }, { -4, -1, -4, -1 }, { -3, -1, -3, -1 },
		{ -1, -1, -1, -1 }, {  0, -1,  0, -1 }, {  1, -1,  1, -1 },
		{ -6,  1,  2, -1 }, { -4,  1, -4,  1 }, { -3,  1, -3,  1 },
		{ -2,  1, -2,  1 }, { -1,  1, -1,  1 }, {  0,  1,  0,  1 }
	};
	for(size_t i = 0; i < 15; ++i) {
		AT_PORTABLE_TEST_ASSERT(context, kATPALPhaseLookup[i].mEvenPhase == kExpectedPALPhases[i].mEvenPhase);
		AT_PORTABLE_TEST_ASSERT(context, kATPALPhaseLookup[i].mEvenInvert == kExpectedPALPhases[i].mEvenInvert);
		AT_PORTABLE_TEST_ASSERT(context, kATPALPhaseLookup[i].mOddPhase == kExpectedPALPhases[i].mOddPhase);
		AT_PORTABLE_TEST_ASSERT(context, kATPALPhaseLookup[i].mOddInvert == kExpectedPALPhases[i].mOddInvert);
	}

	uint8 priorities[32][256] {};
	ATInitGTIAPriorityTables(priorities);
	for(uint32 prior = 0; prior < 32; ++prior) {
		for(uint32 input = 0; input < 256; ++input) {
			AT_PORTABLE_TEST_ASSERT(context, priorities[prior][input] == ReferencePriority((uint8)prior, (uint8)input));
			AT_PORTABLE_TEST_ASSERT(context, priorities[prior][input] < kExpectedAnalysisColors.size());
		}
	}
	AT_PORTABLE_TEST_ASSERT(context, priorities[0][P0 | P1] == kColorP0);
	AT_PORTABLE_TEST_ASSERT(context, priorities[16][P0 | P1] == kColorP0P1);
	AT_PORTABLE_TEST_ASSERT(context, priorities[0][PF0 | PF1] == kColorPF1);

	float linear[16] {};
	float xl[16] {};
	ATComputeLumaRamp(kATLumaRampMode_Linear, linear);
	ATComputeLumaRamp(kATLumaRampMode_XL, xl);
	static constexpr float kExpectedXL[16] {
		0.0f, 0.0658340f, 0.1435022f, 0.2093362f,
		0.2750246f, 0.3408586f, 0.4185267f, 0.4843608f,
		0.5156392f, 0.5814733f, 0.6591414f, 0.7249754f,
		0.7906638f, 0.8564978f, 0.9341660f, 1.0f
	};
	for(size_t i = 0; i < 16; ++i) {
		AT_PORTABLE_TEST_ASSERT(context, std::abs(linear[i] - (float)i / 15.0f) < 1e-7f);
		AT_PORTABLE_TEST_ASSERT(context, xl[i] == kExpectedXL[i]);
		if (i) {
			AT_PORTABLE_TEST_ASSERT(context, linear[i] > linear[i - 1]);
			AT_PORTABLE_TEST_ASSERT(context, xl[i] > xl[i - 1]);
		}
	}

	return true;
}
