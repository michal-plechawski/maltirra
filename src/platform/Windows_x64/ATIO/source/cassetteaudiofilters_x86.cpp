//	Altirra - Atari 800/800XL/5200 emulator
//	Copyright (C) 2024 Avery Lee
//
//	This program is free software; you can redistribute it and/or modify
//	it under the terms of the GNU General Public License as published by
//	the Free Software Foundation; either version 2 of the License, or
//	(at your option) any later version.
//
//	This program is distributed in the hope that it will be useful,
//	but WITHOUT ANY WARRANTY; without even the implied warranty of
//	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//	GNU General Public License for more details.
//
//	You should have received a copy of the GNU General Public License along
//	with this program. If not, see <http://www.gnu.org/licenses/>.

#include <stdafx.h>
#include <at/atio/internal/cassetteaudiofilters.h>

#if VD_CPU_X86 || VD_CPU_X64

void ATCassetteAudioMinMax16x2_SSE2(
	const sint16 *VDRESTRICT src, uint32 n,
	sint32& minvL, sint32& maxvL, sint32& minvR, sint32& maxvR) {
	// We do unaligned loads from this array, so it's important that we
	// avoid data cache unit (DCU) split penalties on older CPUs.
	alignas(64) static const uint64 windowTable[6] = {
		0, 0, (uint64)0 - 1, (uint64)0 - 1, 0, 0
	};

	const __m128i *VDRESTRICT src128 =
		(const __m128i *)((uintptr)src & ~(uintptr)15);
	const __m128i *VDRESTRICT srcend128 =
		(const __m128i *)((uintptr)(src + n*2) & ~(uintptr)15);
	const ptrdiff_t leftOffset = (ptrdiff_t)((uintptr)src & 15);
	const __m128i leftMask = _mm_loadu_si128(
		(const __m128i *)((const char *)windowTable + 16 - leftOffset));
	const ptrdiff_t rightOffset =
		(ptrdiff_t)((uintptr)(src + n * 2) & 15);
	const __m128i rightMask = _mm_loadu_si128(
		(const __m128i *)((const char *)windowTable + 32 - rightOffset));

	__m128i minAcc = _mm_insert_epi16(_mm_cvtsi32_si128(minvL), minvR, 1);
	__m128i maxAcc = _mm_insert_epi16(_mm_cvtsi32_si128(maxvL), maxvR, 1);

	if (src128 != srcend128) {
		__m128i vleft = _mm_and_si128(*src128++, leftMask);
		minAcc = _mm_min_epi16(minAcc, vleft);
		maxAcc = _mm_max_epi16(maxAcc, vleft);

		while(src128 != srcend128) {
			__m128i vmid = *src128++;

			minAcc = _mm_min_epi16(minAcc, vmid);
			maxAcc = _mm_max_epi16(maxAcc, vmid);
		}

		if (rightOffset) {
			__m128i vright = _mm_and_si128(*src128, rightMask);
			minAcc = _mm_min_epi16(minAcc, vright);
			maxAcc = _mm_max_epi16(maxAcc, vright);
		}
	} else {
		__m128i v = _mm_and_si128(
			src128[0], _mm_and_si128(leftMask, rightMask));

		minAcc = _mm_min_epi16(minAcc, v);
		maxAcc = _mm_max_epi16(maxAcc, v);
	}

	// Fold four stereo accumulators into one.
	minAcc = _mm_min_epi16(minAcc, _mm_shuffle_epi32(minAcc, 0xEE));
	maxAcc = _mm_max_epi16(maxAcc, _mm_shuffle_epi32(maxAcc, 0xEE));
	minAcc = _mm_min_epi16(minAcc, _mm_shuffle_epi32(minAcc, 0x55));
	maxAcc = _mm_max_epi16(maxAcc, _mm_shuffle_epi32(maxAcc, 0x55));

	minvL = (sint16)_mm_extract_epi16(minAcc, 0);
	minvR = (sint16)_mm_extract_epi16(minAcc, 1);
	maxvL = (sint16)_mm_extract_epi16(maxAcc, 0);
	maxvR = (sint16)_mm_extract_epi16(maxAcc, 1);
}

uint64 ATCassetteAudioResample16x2_SSE2(
	sint16 *d, const sint16 *s, uint32 count, uint64 accum, sint64 inc,
	const sint16 (*kernel)[8]) {
	__m128i round = _mm_set1_epi32(0x2000);

	do {
		const __m128i *VDRESTRICT s2 =
			(const __m128i *)(s + (size_t)(accum >> 32)*2);
		const __m128i *VDRESTRICT f =
			(const __m128i *)kernel[(uint32)accum >> 27];

		__m128i frac = _mm_shufflelo_epi16(
			_mm_cvtsi32_si128((accum >> 12) & 0x7FFF), 0);
		__m128i cdiff = _mm_mulhi_epi16(
			_mm_sub_epi16(f[1], f[0]), _mm_shuffle_epi32(frac, 0));
		__m128i coeff16 =
			_mm_add_epi16(f[0], _mm_add_epi16(cdiff, cdiff));

		accum += inc;

		__m128i x0 = _mm_loadu_si128(s2);
		__m128i x1 = _mm_loadu_si128(s2 + 1);

		__m128i y0 =
			_mm_shufflehi_epi16(_mm_shufflelo_epi16(x0, 0xd8), 0xd8);
		__m128i y1 =
			_mm_shufflehi_epi16(_mm_shufflelo_epi16(x1, 0xd8), 0xd8);

		__m128i z0 =
			_mm_madd_epi16(y0, _mm_shuffle_epi32(coeff16, 0x50));
		__m128i z1 =
			_mm_madd_epi16(y1, _mm_shuffle_epi32(coeff16, 0xfa));

		__m128i a = _mm_add_epi32(z0, z1);
		__m128i b = _mm_add_epi32(a, _mm_shuffle_epi32(a, 0xee));
		__m128i r = _mm_srai_epi32(_mm_add_epi32(b, round), 14);

		__m128i result = _mm_packs_epi32(r, r);

		*(int *)d = _mm_cvtsi128_si32(result);
		d += 2;
	} while(--count);

	return accum;
}

#endif
