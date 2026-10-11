// Altirra - artifacting color correction, SSE2 acceleration.
// Copyright (C) 2009 Avery Lee
// SPDX-License-Identifier: GPL-2.0-or-later
#include <vd2/system/vdtypes.h>
#include <artifacting_colorcorrect.h>
#if defined(VD_CPU_X86) || defined(VD_CPU_X64)
#include <intrin.h>

void ATArtifactColorCorrect_SSE2(uint8 *VDRESTRICT dst8, uint32 n,
	const sint16 linearTab[256], const uint8 gammaTab[1024], const sint16 matrix[3][3]) {
	// A matrix row contains three lanes, not four: don't overread the last row.
	const __m128i m0 = _mm_set_epi16(0, 0, 0, 0, 0, matrix[0][2], matrix[0][1], matrix[0][0]);
	const __m128i m1 = _mm_set_epi16(0, 0, 0, 0, 0, matrix[1][2], matrix[1][1], matrix[1][0]);
	const __m128i m2 = _mm_set_epi16(0, 0, 0, 0, 0, matrix[2][2], matrix[2][1], matrix[2][0]);
	const __m128i zero = _mm_setzero_si128();
	const __m128i limit = _mm_set_epi16(0, 0, 0, 0, 0, 1023, 1023, 1023);
	for(uint32 i = 0; i < n; ++i) {
		const __m128i r = _mm_cvtsi32_si128((uint16)linearTab[dst8[2]]);
		const __m128i g = _mm_cvtsi32_si128((uint16)linearTab[dst8[1]]);
		const __m128i b = _mm_cvtsi32_si128((uint16)linearTab[dst8[0]]);
		__m128i rgb = _mm_mulhi_epi16(_mm_shufflelo_epi16(r, 0), m0);
		rgb = _mm_add_epi16(rgb, _mm_mulhi_epi16(_mm_shufflelo_epi16(g, 0), m1));
		rgb = _mm_add_epi16(rgb, _mm_mulhi_epi16(_mm_shufflelo_epi16(b, 0), m2));
		const __m128i indices = _mm_max_epi16(zero, _mm_min_epi16(limit, _mm_srai_epi16(rgb, 1)));
		dst8[2] = gammaTab[(uint32)_mm_extract_epi16(indices, 0)];
		dst8[1] = gammaTab[(uint32)_mm_extract_epi16(indices, 1)];
		dst8[0] = gammaTab[(uint32)_mm_extract_epi16(indices, 2)];
		dst8 += 4;
	}
}
#endif
