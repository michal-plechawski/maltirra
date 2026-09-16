//	Altirra - Atari 800/800XL emulator
//	Copyright (C) 2008-2010 Avery Lee
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

#ifndef f_AT_GTIATABLES_H
#define f_AT_GTIATABLES_H

#include <vd2/system/vdtypes.h>
#include <at/atcore/enumparse.h>

namespace ATGTIA {
	const uint8 PF0  = 0x01;
	const uint8 PF1  = 0x02;
	const uint8 PF01 = 0x03;
	const uint8 PF2  = 0x04;
	const uint8 PF3  = 0x08;
	const uint8 PF23 = 0x0c;
	const uint8 PF   = 0x0f;
	const uint8 P0   = 0x10;
	const uint8 P1   = 0x20;
	const uint8 P01  = 0x30;
	const uint8 P2   = 0x40;
	const uint8 P3   = 0x80;
	const uint8 P23  = 0xc0;

	enum {
		kColorP0,
		kColorP1,
		kColorP2,
		kColorP3,
		kColorPF0,
		kColorPF1,
		kColorPF2,
		kColorPF3,
		kColorBAK,
		kColorBlack,
		kColorP0P1,
		kColorP2P3,
		kColorPF0P0,
		kColorPF0P1,
		kColorPF0P0P1,
		kColorPF1P0,
		kColorPF1P1,
		kColorPF1P0P1,
		kColorPF2P2,
		kColorPF2P3,
		kColorPF2P2P3,
		kColorPF3P2,
		kColorPF3P3,
		kColorPF3P2P3
	};
}

extern const VDALIGN(16) uint8 kATAnalysisColorTable[24];
struct ATPALPhaseInfo {
	float mEvenPhase;		// delays relative to even line colorburst
	float mEvenInvert;		// even line color signal inversion
	float mOddPhase;		// delays relative to odd line colorburst
	float mOddInvert;		// odd line color signal inversion
};

extern const ATPALPhaseInfo kATPALPhaseLookup[15];

void ATInitGTIAPriorityTables(uint8 priorityTables[32][256]);

enum ATLumaRampMode : uint8 {
	kATLumaRampMode_Linear,
	kATLumaRampMode_XL,
	kATLumaRampModeCount
};

AT_DECLARE_ENUM_TABLE(ATLumaRampMode);

void ATComputeLumaRamp(ATLumaRampMode mode, float lumaRamp[16]);

#endif
