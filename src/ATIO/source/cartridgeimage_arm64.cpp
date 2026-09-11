//	Altirra - Atari 800/800XL/5200 emulator
//	I/O library - cartridge image ARM64 acceleration
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

#if VD_CPU_ARM64

#include <arm_neon.h>

uint32 ATComputeCartridgeImageByteSum32(const uint8 *src, size_t len) {
	uint64x2_t accumulator = vdupq_n_u64(0);

	while(len >= 16) {
		const uint16x8_t sums16 = vpaddlq_u8(vld1q_u8(src));
		const uint32x4_t sums32 = vpaddlq_u16(sums16);

		accumulator = vpadalq_u32(accumulator, sums32);
		src += 16;
		len -= 16;
	}

	uint64 sum = vaddvq_u64(accumulator);

	while(len--)
		sum += *src++;

	return static_cast<uint32>(sum);
}

#endif
