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

#include <stdafx.h>
#include <at/atcpu/history.h>
#include <at/atdebugger/symbols.h>
#include "cpu.h"
#include "simulator.h"
#include "console.h"
#include "debugger.h"
#include "disasm.h"

extern ATSimulator g_sim;

namespace {
	ATDisasmServices GetDisasmServices() {
		ATDisasmServices services;
		services.mpLookupSymbol = [](uint32 addr, uint32 flags, ATSymbol& symbol) {
			return ATGetDebuggerSymbolLookup()->LookupSymbol(addr, flags, symbol);
		};
		services.mpIsPathStart = [](uint16 addr) {
			const auto& cpu = g_sim.GetCPU();
			return cpu.IsPathfindingEnabled() && cpu.IsPathStart(addr);
		};
		return services;
	}
}

ATDisasmResult ATDisassembleInsn(VDStringA& line,
	IATDebugTarget *target,
	ATDebugDisasmMode disasmMode,
	const ATCPUHistoryEntry& hent,
	bool decodeReferences,
	bool decodeRefsHistory,
	bool showPCAddress,
	bool showCodeBytes,
	bool showLabels,
	bool lowercaseOps,
	bool wideOpcode,
	bool showLabelNamespaces,
	bool showSymbols,
	bool showGlobalPC)
{
	return ATDisassembleInsn(line, GetDisasmServices(), target, disasmMode, hent,
		decodeReferences, decodeRefsHistory, showPCAddress, showCodeBytes,
		showLabels, lowercaseOps, wideOpcode, showLabelNamespaces, showSymbols,
		showGlobalPC);
}

void ATDisassembleCaptureRegisterContext(ATCPUHistoryEntry& hent) {
	ATCPUEmulator& cpu = g_sim.GetCPU();
	hent.mP = cpu.GetP();
	hent.mX = cpu.GetX();
	hent.mExt.mXH = cpu.GetXH();
	hent.mY = cpu.GetY();
	hent.mExt.mYH = cpu.GetYH();
	hent.mD = cpu.GetD();
	hent.mS = cpu.GetS();
	hent.mExt.mSH = cpu.GetSH();
	hent.mB = cpu.GetB();
	hent.mK = cpu.GetK();
	hent.mbEmulation = cpu.GetEmulationFlag();
}

void ATDisassembleCaptureInsnContext(uint16 addr, uint8 bank, ATCPUHistoryEntry& hent) {
	uint32 addr24 = addr + ((uint32)bank << 16);
	uint8 opcode = g_sim.DebugGlobalReadByte(addr24);
	uint8 byte1 = g_sim.DebugGlobalReadByte((addr24+1) & 0xffffff);
	uint8 byte2 = g_sim.DebugGlobalReadByte((addr24+2) & 0xffffff);
	uint8 byte3 = g_sim.DebugGlobalReadByte((addr24+3) & 0xffffff);

	hent.mPC = addr;
	hent.mK = bank;
	hent.mOpcode[0] = opcode;
	hent.mOpcode[1] = byte1;
	hent.mOpcode[2] = byte2;
	hent.mOpcode[3] = byte3;
}

void ATDisassembleCaptureInsnContext(uint32 globalAddr, ATCPUHistoryEntry& hent) {
	const uint32 bankSpace = globalAddr & 0xFFFF0000;
	uint8 opcode = g_sim.DebugGlobalReadByte(globalAddr);
	uint8 byte1 = g_sim.DebugGlobalReadByte(bankSpace + ((globalAddr+1) & 0xffff));
	uint8 byte2 = g_sim.DebugGlobalReadByte(bankSpace + ((globalAddr+2) & 0xffff));
	uint8 byte3 = g_sim.DebugGlobalReadByte(bankSpace + ((globalAddr+3) & 0xffff));

	hent.mPC = (uint16)globalAddr;
	hent.mK = (uint8)(globalAddr >> 16);
	hent.mGlobalPCBase = bankSpace;
	hent.mOpcode[0] = opcode;
	hent.mOpcode[1] = byte1;
	hent.mOpcode[2] = byte2;
	hent.mOpcode[3] = byte3;
}

uint16 ATDisassembleInsn(VDStringA& line, uint16 addr, bool decodeReferences) {
	ATCPUHistoryEntry hent;
	ATDisassembleCaptureRegisterContext(hent);
	ATDisassembleCaptureInsnContext(addr, hent.mK, hent);

	const ATCPUEmulator& cpu = g_sim.GetCPU();
	return ATDisassembleInsn(line, g_sim.GetDebugTarget(), cpu.GetDisasmMode(), hent, decodeReferences, false, true, true, true).mNextPC;
}

uint16 ATDisassembleInsn(uint16 addr, uint8 bank) {
	ATCPUHistoryEntry hent;
	ATDisassembleCaptureRegisterContext(hent);
	ATDisassembleCaptureInsnContext(addr, bank, hent);

	const ATCPUEmulator& cpu = g_sim.GetCPU();

	VDStringA buf;
	addr = ATDisassembleInsn(buf, g_sim.GetDebugTarget(), cpu.GetDisasmMode(), hent, true, false, true, true, true).mNextPC;
	buf += '\n';
	ATConsoleWrite(buf.c_str());

	return addr;
}

int ATGetOpcodeLength(uint8 opcode) {
	return ATGetOpcodeLength(opcode, g_sim.GetCPU().GetCPUSubMode());
}

int ATGetOpcodeLength(uint8 opcode, uint8 p, bool emuMode) {
	ATCPUEmulator& cpu = g_sim.GetCPU();

	return ATGetOpcodeLength(opcode, p, emuMode, cpu.GetDisasmMode());
}
