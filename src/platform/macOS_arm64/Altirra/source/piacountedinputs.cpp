// Altirra - Atari 800/800XL/5200 emulator
// Copyright (C) 2009-2012 Avery Lee
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
//
// Apple Silicon PIA counted-input kernels.
#include <arm_neon.h>
#include <piacountedinputs.h>

namespace {
	uint8x16x2_t ExpandMask(uint32 mask) {
		static constexpr uint8 bitMask[16] { 1, 2, 4, 8, 16, 32, 64, 128, 1, 2, 4, 8, 16, 32, 64, 128 };
		const uint8x16_t bitMaskV = vld1q_u8(bitMask);
		const uint8x8_t bytes = vreinterpret_u8_u32(vmov_n_u32(mask));
		uint8x16x2_t out;
		out.val[0] = vceqq_u8(vandq_u8(vcombine_u8(vdup_lane_u8(bytes, 0), vdup_lane_u8(bytes, 1)), bitMaskV), vmovq_n_u8(0));
		out.val[1] = vceqq_u8(vandq_u8(vcombine_u8(vdup_lane_u8(bytes, 2), vdup_lane_u8(bytes, 3)), bitMaskV), vmovq_n_u8(0));
		return out;
	}
}

void ATPIAAddZeroBits(uint8 *counts, uint32 mask) {
	const uint8x16x2_t bits = ExpandMask(mask);
	vst1q_u8(counts, vsubq_u8(vld1q_u8(counts), bits.val[0]));
	vst1q_u8(counts + 16, vsubq_u8(vld1q_u8(counts + 16), bits.val[1]));
}

void ATPIARemoveZeroBits(uint8 *counts, uint32 mask) {
	const uint8x16x2_t bits = ExpandMask(mask);
	vst1q_u8(counts, vaddq_u8(vld1q_u8(counts), bits.val[0]));
	vst1q_u8(counts + 16, vaddq_u8(vld1q_u8(counts + 16), bits.val[1]));
}

uint32 ATPIAReadZeroMask(const uint8 *counts) {
	static constexpr uint8 bitMask[8] { 1, 2, 4, 8, 16, 32, 64, 128 };
	const uint8x8_t bitMaskV = vld1_u8(bitMask);
	const uint8x8_t zero = vmov_n_u8(0);
	const uint32 b0 = vaddv_u8(vand_u8(vceq_u8(vld1_u8(counts), zero), bitMaskV));
	const uint32 b1 = vaddv_u8(vand_u8(vceq_u8(vld1_u8(counts + 8), zero), bitMaskV));
	const uint32 b2 = vaddv_u8(vand_u8(vceq_u8(vld1_u8(counts + 16), zero), bitMaskV));
	const uint32 b3 = vaddv_u8(vand_u8(vceq_u8(vld1_u8(counts + 24), zero), bitMaskV));
	return b0 | (b1 << 8) | (b2 << 16) | (b3 << 24);
}
