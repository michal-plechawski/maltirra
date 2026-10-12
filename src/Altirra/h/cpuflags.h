//	Altirra - Atari 800/800XL emulator
//	Copyright (C) 2008 Avery Lee
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

#ifndef AT_CPUFLAGS_H
#define AT_CPUFLAGS_H

namespace AT6502 {
	enum {
		kFlagN = 0x80,
		kFlagV = 0x40,
		kFlagM = 0x20,		// 65C816 native mode only
		kFlagX = 0x10,		// 65C816 native mode only
		kFlagB = 0x10,
		kFlagD = 0x08,
		kFlagI = 0x04,
		kFlagZ = 0x02,
		kFlagC = 0x01
	};
}

#endif
