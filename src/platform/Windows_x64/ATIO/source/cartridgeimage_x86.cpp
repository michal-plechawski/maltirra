//	Altirra - Atari 800/800XL/5200 emulator
//	I/O library - cartridge image x86 acceleration
//	Copyright (C) 2009-2016 Avery Lee
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
//	You should have received a copy of the GNU General Public License
//	along with this program; if not, write to the Free Software
//	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

#include <stdafx.h>
#include <at/atio/internal/cartridgeimage.h>

#if VD_CPU_X86 || VD_CPU_X64

uint32 ATComputeCartridgeImageByteSum32(const uint8 *src, size_t len) {
	uint32 sum = 0;

	if (len >= 1024) {
		uint32 align = (0U - (uint32)(uintptr)src) & 15;
		len -= align;

		while(align--)
			sum += *src++;

		uint32 blocks = len >> 6;
		len &= 0x3f;

		__m128i zero = _mm_setzero_si128();
		__m128i acc0 = zero;
		__m128i acc1 = zero;
		while(blocks--) {
			__m128i x0 = _mm_load_si128((const __m128i *)(src +  0));
			__m128i x1 = _mm_load_si128((const __m128i *)(src + 16));
			__m128i x2 = _mm_load_si128((const __m128i *)(src + 32));
			__m128i x3 = _mm_load_si128((const __m128i *)(src + 48));
			src += 64;

			acc0 = _mm_add_epi32(acc0, _mm_sad_epu8(x0, zero));
			acc1 = _mm_add_epi32(acc1, _mm_sad_epu8(x1, zero));
			acc0 = _mm_add_epi32(acc0, _mm_sad_epu8(x2, zero));
			acc1 = _mm_add_epi32(acc1, _mm_sad_epu8(x3, zero));
		}

		__m128i acc = _mm_add_epi32(acc0, acc1);
		__m128i acchi = _mm_castps_si128(
			_mm_movehl_ps(_mm_undefined_ps(), _mm_castsi128_ps(acc)));

		sum += (uint32)_mm_cvtsi128_si32(_mm_add_epi32(acc, acchi));
	}

	while(len--)
		sum += *src++;

	return sum;
}

#endif
