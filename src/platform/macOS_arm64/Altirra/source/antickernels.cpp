// Bounded ANTIC kernels for Apple Silicon.
#include <arm_neon.h>
#include <cstring>
#include <antickernels.h>

uint32 ATAnticDecodeMode2(uint8 *dst, const uint8 *src, const uint8 *chdata,
	uint32 x, uint32 limit, uint8 invMask, uint8 blinkMask) {
	if (x >= limit)
		return x;

	const uint32 delta = limit - x;
	const uint32 n = delta / 2 + (delta & 1);
	uint32 i = 0;
	const uint8x8_t inv = vdup_n_u8(invMask);
	const uint8x8_t blink = vdup_n_u8(blinkMask);
	const uint8x8_t all = vdup_n_u8(0xff);
	const uint8x8_t lowNibble = vdup_n_u8(15);
	for (; n - i >= 8; i += 8) {
		const uint8x8_t names = vld1_u8(src + i);
		const uint8x8_t chars = vld1_u8(chdata + i);
		const uint8x8_t hi = vshr_n_u8(names, 7);
		const uint8x8_t hiMask = vsub_u8(vdup_n_u8(0), hi);
		const uint8x8_t preserve = vorr_u8(vbic_u8(all, hiMask), blink);
		const uint8x8_t data = veor_u8(vand_u8(chars, preserve), vand_u8(hiMask, inv));
		uint8x8x2_t output;
		output.val[0] = vand_u8(vshr_n_u8(data, 4), lowNibble);
		output.val[1] = vand_u8(data, lowNibble);
		vst2_u8(dst + i * 2, output);
	}
	for (; i < n; ++i) {
		const uint8 name = src[i];
		const uint8 hi = (name & 0x80) ? 0xff : 0;
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
				dmaPattern[r++] |= 1;
				break;
			}
			++r;
		}
	}
}

void ATAnticClearDMACycles(uint8 *dmaPattern) {
	memset(dmaPattern, 0, 128);
}
