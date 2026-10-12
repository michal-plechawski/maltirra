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

#ifndef AT_CPUMODE_H
#define AT_CPUMODE_H

#include <vd2/system/vdtypes.h>
#include <at/atcore/enumparse.h>

enum ATCPUMode : uint8 {
	kATCPUMode_6502,
	kATCPUMode_65C02,
	kATCPUMode_65C816,
	kATCPUModeCount
};

AT_DECLARE_ENUM_TABLE(ATCPUMode);

#endif
