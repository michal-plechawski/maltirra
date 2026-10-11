// Altirra - Atari 800/800XL/5200 emulator
// Copyright (C) 2026 Altirra contributors
// SPDX-License-Identifier: GPL-2.0-or-later

#include <arm_neon.h>

namespace {
	constexpr uint8 kGTIAColorIndex[16] { 8, 4, 5, 5, 6, 6, 6, 6, 7, 7, 7, 7, 7, 7, 7, 7 };
}

// Only access the requested span, including unaligned starts and short tails.
static void atasm_gtia_render_lores_fast_neon(void *output, const uint8 *src,
	uint32 n, const uint8 *colors) {
	uint8 *dst = static_cast<uint8 *>(output);
	const uint8x16_t palette = vqtbl1q_u8(vld1q_u8(colors), vld1q_u8(kGTIAColorIndex));
	while(n >= 16) {
		const uint8x16_t pixels = vqtbl1q_u8(palette, vld1q_u8(src));
		const uint8x16x2_t doubled { { pixels, pixels } };
		vst2q_u8(dst, doubled);
		src += 16;
		dst += 32;
		n -= 16;
	}
	while(n--) {
		const uint8 pixel = colors[kGTIAColorIndex[*src++]];
		*dst++ = pixel;
		*dst++ = pixel;
	}
}

static void atasm_gtia_render_mode8_fast_neon(void *output, const uint8 *src,
	const uint8 *lumaSrc, uint32 n, const uint8 *colors) {
	uint8 *dst = static_cast<uint8 *>(output);
	const uint8x16_t palette = vqtbl1q_u8(vld1q_u8(colors), vld1q_u8(kGTIAColorIndex));
	const uint8x16_t pf1Luma = vdupq_n_u8(colors[5] & 15);
	while(n >= 16) {
		const uint8x16_t base = vqtbl1q_u8(palette, vld1q_u8(src));
		const uint8x16_t luma = vld1q_u8(lumaSrc);
		const uint8x16_t lit = vorrq_u8(vandq_u8(base, vdupq_n_u8(0xf0)), pf1Luma);
		const uint8x16x2_t pixels { {
			vbslq_u8(vtstq_u8(luma, vdupq_n_u8(2)), lit, base),
			vbslq_u8(vtstq_u8(luma, vdupq_n_u8(1)), lit, base)
		} };
		vst2q_u8(dst, pixels);
		src += 16;
		lumaSrc += 16;
		dst += 32;
		n -= 16;
	}
	while(n--) {
		const uint8 base = colors[kGTIAColorIndex[*src++]];
		const uint8 lit = (base & 0xf0) | (colors[5] & 15);
		const uint8 luma = *lumaSrc++;
		*dst++ = luma & 2 ? lit : base;
		*dst++ = luma & 1 ? lit : base;
	}
}
