//	Altirra - Atari 800/800XL/5200 emulator
//	I/O library - x86 cassette analog decoder acceleration
//	Copyright (C) 2009-2017 Avery Lee
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

#include <stdafx.h>
#include <cmath>
#include <at/atio/internal/cassettedecoder.h>

#if VD_CPU_X86 || VD_CPU_X64

void ATCassetteDecoderFSKProcessSSE2(
	const sint16 *samples, uint32 n, uint32 *bitfield, uint32 bitoffset,
	float *analysis, bool enableAnalysis,
	sint32 accumulator[4], uint32& index, sint16 history[24],
	float markGain, const sint16 rotationTable[24][4]) {
	uint32 bitaccum = 0;
	uint32 bitcounter = 32 - bitoffset;
	__m128i acc01 = _mm_loadu_si128((const __m128i *)accumulator);
	const __m128i zero128 = _mm_setzero_si128();

	do {
		const sint32 x1 = *samples;
		samples += 2;

		const uint32 historyPosition = index++;
		if (index == 24)
			index = 0;

		const sint32 x0 = history[historyPosition];
		const __m128i x01 = _mm_shuffle_epi32(
			_mm_insert_epi16(
				_mm_insert_epi16(zero128, (uint16)x1, 0),
				(uint16)x0, 1),
			0);
		history[historyPosition] = x1;

		const __m128i rotations = _mm_loadl_epi64(
			(const __m128i *)rotationTable[historyPosition]);
		const __m128i signedRotations = _mm_unpacklo_epi16(
			rotations, _mm_sub_epi16(zero128, rotations));
		acc01 = _mm_add_epi32(
			acc01, _mm_madd_epi16(x01, signedRotations));

		__m128 response = _mm_cvtepi32_ps(acc01);
		response = _mm_mul_ps(response, response);
		response = _mm_add_ps(
			response,
			_mm_shuffle_ps(response, response, 0b0'10'11'00'01));

		const float space = _mm_cvtss_f32(response);
		const float mark =
			_mm_cvtss_f32(_mm_movehl_ps(response, response)) * markGain;

		if (enableAnalysis) {
			analysis[0] = (float)history[
				historyPosition >= 12
					? historyPosition - 12
					: historyPosition + 12] * (1.0f / 32767.0f);
			analysis[1] = std::sqrt(space) *
				(1.0f / 32767.0f / 4096.0f / 12.0f);
			analysis[2] = std::sqrt(mark) *
				(1.0f / 32767.0f / 4096.0f / 12.0f);
			analysis[3] = mark > space ? 0.8f : -0.8f;
			analysis += 6;
		}

		bitaccum += bitaccum;
		if (mark >= space)
			++bitaccum;

		if (!--bitcounter) {
			bitcounter = 32;
			*bitfield++ |= bitaccum;
		}
	} while(--n);

	if (bitcounter < 32)
		*bitfield |= bitaccum << bitcounter;

	_mm_storeu_si128((__m128i *)accumulator, acc01);
}

float ATCassetteDecoderFIRProcess(
	ATCassetteDecoderFIRState& state, float sample) {
	__m128 hpf0 = _mm_load_ps(state.mValues + 0);
	__m128 hpf1 = _mm_load_ps(state.mValues + 4);
	__m128 hpf2 = _mm_load_ps(state.mValues + 8);
	__m128 hpf3 = _mm_load_ps(state.mValues + 12);
	const __m128 sample4 = _mm_set1_ps(sample);

	hpf0 = _mm_add_ps(
		hpf0,
		_mm_mul_ps(_mm_load_ps(kATCassetteDecoderHPFKernel + 0), sample4));
	hpf1 = _mm_add_ps(
		hpf1,
		_mm_mul_ps(_mm_load_ps(kATCassetteDecoderHPFKernel + 4), sample4));
	hpf2 = _mm_add_ps(
		hpf2,
		_mm_mul_ps(_mm_load_ps(kATCassetteDecoderHPFKernel + 8), sample4));
	hpf3 = _mm_add_ps(
		hpf3,
		_mm_mul_ps(_mm_load_ps(kATCassetteDecoderHPFKernel + 12), sample4));

	const float result = _mm_cvtss_f32(hpf0);
	hpf0 = _mm_move_ss(hpf0, hpf1);
	hpf1 = _mm_move_ss(hpf1, hpf2);
	hpf2 = _mm_move_ss(hpf2, hpf3);
	hpf0 = _mm_shuffle_ps(hpf0, hpf0, 0b0'00'11'10'01);
	hpf1 = _mm_shuffle_ps(hpf1, hpf1, 0b0'00'11'10'01);
	hpf2 = _mm_shuffle_ps(hpf2, hpf2, 0b0'00'11'10'01);
	hpf3 = _mm_castsi128_ps(_mm_srli_si128(_mm_castps_si128(hpf3), 4));

	_mm_store_ps(state.mValues + 0, hpf0);
	_mm_store_ps(state.mValues + 4, hpf1);
	_mm_store_ps(state.mValues + 8, hpf2);
	_mm_store_ps(state.mValues + 12, hpf3);
	return result;
}

#endif
