// Windows ANTIC kernels: SSE2 on x86/x64, bounded scalar fallback on ARM64.
#include <cstring>
#include <antickernels.h>

#if VD_CPU_X86 || VD_CPU_X64
#include <intrin.h>
#include <at/atcore/intrin_sse2.h>

uint32 ATAnticDecodeMode2(uint8 *dst, const uint8 *src, const uint8 *chdata,
	uint32 x, uint32 limit, uint8 invMask, uint8 blinkMask) {
	if (x >= limit)
		return x;

	const uint32 delta = limit - x;
	const uint32 n = delta / 2 + (delta & 1);
	const uint32 endX = x + n * 2;
	const __m128i zero = _mm_setzero_si128();
	const __m128i inv = _mm_set1_epi8((char)invMask);
	const __m128i blink = _mm_set1_epi8((char)blinkMask);
	const __m128i all = _mm_set1_epi8((char)0xff);
	const __m128i lowNibble = _mm_set1_epi8(0x0f);

	uint32 left = n;
	while (left >= 8) {
		const __m128i names = _mm_loadl_epi64((const __m128i *)src);
		const __m128i chars = _mm_loadl_epi64((const __m128i *)chdata);
		const __m128i hi = _mm_cmpgt_epi8(zero, names);
		const __m128i preserve = _mm_or_si128(_mm_andnot_si128(hi, all), blink);
		__m128i data = _mm_and_si128(chars, preserve);
		data = _mm_xor_si128(data, _mm_and_si128(hi, inv));
		const __m128i loBits = _mm_and_si128(data, lowNibble);
		const __m128i hiBits = _mm_and_si128(_mm_srli_epi16(data, 4), lowNibble);
		_mm_storeu_si128((__m128i *)dst, _mm_unpacklo_epi8(hiBits, loBits));
		src += 8;
		chdata += 8;
		dst += 16;
		left -= 8;
	}

	while (left--) {
		const uint8 name = *src++;
		uint8 data = *chdata++;
		const uint8 hi = (name & 0x80) ? 0xff : 0;
		data &= (uint8)(~hi | blinkMask);
		data ^= (uint8)(hi & invMask);
		*dst++ = data >> 4;
		*dst++ = data & 15;
	}

	return endX;
}
void ATAnticSetDMACycles(void *dmaPattern0, uint32 start, uint32 end,
	uint8 cyclePattern, uint8 dmaMask) {
	if (end > 115)
		end = 115;
	if (end <= start)
		return;

	uint8 *const dmaPattern = (uint8 *)dmaPattern0;
	const __m128i starts = _mm_set1_epi8((char)start);
	const __m128i ends = _mm_set1_epi8((char)end);
	const __m128i mask = _mm_set1_epi8((char)dmaMask);
	const __m128i pat = _mm_set1_epi8((char)cyclePattern);
	const __m128i all = _mm_set1_epi8((char)0xff);
	const __m128i idxStep = _mm_set1_epi8(16);
	const __m128i indices0 = _mm_setr_epi8(0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15);
	const __m128i bits = _mm_setr_epi8(1,2,4,8,16,32,64,(char)128,
		1,2,4,8,16,32,64,(char)128);
	__m128i indices = indices0;
	for (uint32 block = 0; block < 8; ++block) {
		const __m128i valid = _mm_and_si128(_mm_cmpgt_epi8(ends, indices),
			_mm_andnot_si128(_mm_cmpgt_epi8(starts, indices), all));
		const __m128i selected = _mm_andnot_si128(
			_mm_cmpeq_epi8(_mm_and_si128(pat, bits), _mm_setzero_si128()), all);
		const __m128i add = _mm_and_si128(_mm_and_si128(valid, selected), mask);
		__m128i *p = (__m128i *)(dmaPattern + block * 16);
		_mm_store_si128(p, _mm_or_si128(_mm_load_si128(p), add));
		indices = _mm_add_epi8(indices, idxStep);
	}
}

void ATAnticSetRefreshCycles(uint8 *dmaPattern) {
	const __m128i bit = _mm_set1_epi8(1);
	const __m128i one = _mm_set1_epi32(1);
	__m128i a = _mm_loadu_si128((const __m128i *)(dmaPattern + 25));
	__m128i b = _mm_loadu_si128((const __m128i *)(dmaPattern + 41));
	a = _mm_or_si128(a, _mm_and_si128(_mm_add_epi32(_mm_cmpeq_epi8(_mm_and_si128(a, bit), bit), one), bit));
	b = _mm_or_si128(b, _mm_and_si128(_mm_add_epi32(_mm_cmpeq_epi8(_mm_and_si128(b, bit), bit), one), bit));
	_mm_storeu_si128((__m128i *)(dmaPattern + 25), a);
	_mm_storeu_si128((__m128i *)(dmaPattern + 41), b);

	const __m128i *p = (const __m128i *)dmaPattern + 3;
	uint32 mask = 0x1ff;
	for (int i = 3; i < 8; ++i) {
		const uint32 freeMask = (uint32)~(_mm_movemask_epi8(_mm_slli_epi16(*p, 7)) | mask) & 0xffff;
		if (freeMask) {
			const uint32 lane = _tzcnt_u32(freeMask);
			const uint32 cycle = (uint32)((const uint8 *)p - dmaPattern) + lane;
			if (cycle < 107)
				((uint8 *)p)[lane] |= 1;
			break;
		}
		mask = 0;
		++p;
	}
}

void ATAnticClearDMACycles(uint8 *dmaPattern) {
	const __m128i zero = _mm_setzero_si128();
	__m128i *p = (__m128i *)dmaPattern;
	for (uint32 i = 0; i < 8; ++i)
		_mm_store_si128(p + i, zero);
}
#else

uint32 ATAnticDecodeMode2(uint8 *dst, const uint8 *src, const uint8 *chdata,
	uint32 x, uint32 limit, uint8 invMask, uint8 blinkMask) {
	if (x >= limit)
		return x;

	const uint32 delta = limit - x;
	const uint32 n = delta / 2 + (delta & 1);
	for (uint32 i = 0; i < n; ++i) {
		const uint8 hi = (src[i] & 0x80) ? 0xff : 0;
		uint8 data = chdata[i];
		data &= (uint8)(~hi | blinkMask);
		data ^= (uint8)(hi & invMask);
		dst[2 * i] = data >> 4;
		dst[2 * i + 1] = data & 15;
	}
	return x + n * 2;
}

void ATAnticSetDMACycles(void *dmaPattern0, uint32 start, uint32 end,
	uint8 cyclePattern, uint8 dmaMask) {
	if (end > 115)
		end = 115;
	if (end <= start)
		return;

	uint8 *const dmaPattern = (uint8 *)dmaPattern0;
	for (uint32 i = start; i < end; ++i) {
		if (cyclePattern & (1u << (i & 7)))
			dmaPattern[i] |= dmaMask;
	}
}

void ATAnticSetRefreshCycles(uint8 *dmaPattern) {
	int r = 24;
	for (int x = 25; x < 61; x += 4) {
		if (r >= x)
			continue;
		r = x;
		while (r < 107) {
			if (!(dmaPattern[r] & 1)) {
				dmaPattern[r] |= 1;
				++r;
				break;
			}
			++r;
		}
	}
}

void ATAnticClearDMACycles(uint8 *dmaPattern) {
	memset(dmaPattern, 0, 128);
}
#endif
