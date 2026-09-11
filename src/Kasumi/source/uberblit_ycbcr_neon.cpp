//	VirtualDub - Video processing and capture application
//	Graphics support library
//	Copyright (C) 1998-2019 Avery Lee
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

#if VD_CPU_ARM64
#include <arm_neon.h>
#include "uberblit_ycbcr_accel.h"

namespace {
	int32x4_t VDPixmapConvertLow(
		int16x8_t b, int16x8_t g, int16x8_t r,
		int16 bCoefficient, int16 gCoefficient, int16 rCoefficient,
		sint32 bias) {
		int32x4_t result = vmull_n_s16(vget_low_s16(b), bCoefficient);
		result = vmlal_n_s16(result, vget_low_s16(g), gCoefficient);
		result = vmlal_n_s16(result, vget_low_s16(r), rCoefficient);
		return vshrq_n_s32(vaddq_s32(result, vdupq_n_s32(bias)), 15);
	}

	int32x4_t VDPixmapConvertHigh(
		int16x8_t b, int16x8_t g, int16x8_t r,
		int16 bCoefficient, int16 gCoefficient, int16 rCoefficient,
		sint32 bias) {
		int32x4_t result = vmull_n_s16(vget_high_s16(b), bCoefficient);
		result = vmlal_n_s16(result, vget_high_s16(g), gCoefficient);
		result = vmlal_n_s16(result, vget_high_s16(r), rCoefficient);
		return vshrq_n_s32(vaddq_s32(result, vdupq_n_s32(bias)), 15);
	}

	uint8x8_t VDPixmapPackChannel(int32x4_t low, int32x4_t high) {
		return vqmovn_u16(vcombine_u16(vqmovun_s32(low), vqmovun_s32(high)));
	}

	uint8 VDPixmapConvertPixelChannel(sint32 value) {
		value >>= 15;

		if (value < 0)
			return 0;
		if (value > 255)
			return 255;

		return static_cast<uint8>(value);
	}
}

void VDPixmapGenRGB32ToYCbCr709_Accel::Compute(void *dst0, sint32 y) {
	uint8 *VDRESTRICT dstCr = static_cast<uint8 *>(dst0);
	uint8 *VDRESTRICT dstY = dstCr + mWindowPitch;
	uint8 *VDRESTRICT dstCb = dstY + mWindowPitch;
	const uint8 *VDRESTRICT srcRGB =
		static_cast<const uint8 *>(mpSrc->GetRow(y, mSrcIndex));

	sint32 remaining = mWidth;
	while(remaining >= 8) {
		const uint8x8x4_t bgra = vld4_u8(srcRGB);
		srcRGB += 32;
		remaining -= 8;

		const int16x8_t b = vreinterpretq_s16_u16(vmovl_u8(bgra.val[0]));
		const int16x8_t g = vreinterpretq_s16_u16(vmovl_u8(bgra.val[1]));
		const int16x8_t r = vreinterpretq_s16_u16(vmovl_u8(bgra.val[2]));

		const int32x4_t yLow =
			VDPixmapConvertLow(b, g, r, 2032, 20127, 5983, 0x084000);
		const int32x4_t yHigh =
			VDPixmapConvertHigh(b, g, r, 2032, 20127, 5983, 0x084000);
		const int32x4_t cbLow =
			VDPixmapConvertLow(b, g, r, 14392, -11094, -3298, 0x404000);
		const int32x4_t cbHigh =
			VDPixmapConvertHigh(b, g, r, 14392, -11094, -3298, 0x404000);
		const int32x4_t crLow =
			VDPixmapConvertLow(b, g, r, -1320, -13073, 14392, 0x404000);
		const int32x4_t crHigh =
			VDPixmapConvertHigh(b, g, r, -1320, -13073, 14392, 0x404000);

		vst1_u8(dstY, VDPixmapPackChannel(yLow, yHigh));
		vst1_u8(dstCb, VDPixmapPackChannel(cbLow, cbHigh));
		vst1_u8(dstCr, VDPixmapPackChannel(crLow, crHigh));
		dstY += 8;
		dstCb += 8;
		dstCr += 8;
	}

	while(remaining-- > 0) {
		const sint32 b = srcRGB[0];
		const sint32 g = srcRGB[1];
		const sint32 r = srcRGB[2];
		srcRGB += 4;

		*dstY++ = VDPixmapConvertPixelChannel(
			2032 * b + 20127 * g + 5983 * r + 0x084000);
		*dstCb++ = VDPixmapConvertPixelChannel(
			14392 * b - 11094 * g - 3298 * r + 0x404000);
		*dstCr++ = VDPixmapConvertPixelChannel(
			-1320 * b - 13073 * g + 14392 * r + 0x404000);
	}
}
#endif
