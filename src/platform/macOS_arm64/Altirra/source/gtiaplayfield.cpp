#include <arm_neon.h>
#include <gtiaplayfield.h>
#include <gtiatables.h>

void ATGTIAUnpackPlayfield160(uint8 *dst, const uint8 *src, uint32 n) {
	while (n >= 16) {
		const uint8x16_t input = vld1q_u8(src);
		uint8x16x2_t output;
		output.val[0] = vshrq_n_u8(input, 4);
		output.val[1] = vandq_u8(input, vdupq_n_u8(15));
		vst2q_u8(dst, output);
		src += 16; dst += 32; n -= 16;
	}
	while (n--) {
		const uint8 value = *src++;
		*dst++ = value >> 4;
		*dst++ = value & 15;
	}
}

void ATGTIAUnpackPlayfield320(uint8 *priority, uint8 *hires, const uint8 *src, uint32 x, uint32 n) {
	if (!n) return;
	priority += x; hires += x;
	while (n >= 16) {
		const uint8x16_t input = vld1q_u8(src);
		uint8x16x2_t output;
		output.val[0] = vandq_u8(vshrq_n_u8(input, 2), vdupq_n_u8(3));
		output.val[1] = vandq_u8(input, vdupq_n_u8(3));
		vst2q_u8(hires, output);
		vst1q_u8(priority, vdupq_n_u8(ATGTIA::PF2));
		vst1q_u8(priority + 16, vdupq_n_u8(ATGTIA::PF2));
		src += 16; priority += 32; hires += 32; n -= 16;
	}
	while (n--) {
		const uint8 value = *src++;
		*priority++ = ATGTIA::PF2; *priority++ = ATGTIA::PF2;
		*hires++ = (value >> 2) & 3;
		*hires++ = value & 3;
	}
}
