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

#include <stdafx.h>
#include <utility>
#include "cpu.h"
#include "verifier.h"
#include "simulator.h"

void ATCPUVerifier::Init(ATCPUEmulator *cpu, ATCPUEmulatorMemory *mem, ATSimulator *sim, ATSimulatorEventManager *simevmgr) {
	(void)mem; // Legacy parameter was stored but never read.
	ATCPUVerifierConnections connections;
	connections.mpReadCPU = [cpu] {
		ATCPUVerifierCPUState state;
		state.mA = cpu->GetA();
		state.mX = cpu->GetX();
		state.mY = cpu->GetY();
		state.mP = cpu->GetP();
		state.mS = cpu->GetS();
		state.mK = cpu->GetK();
		state.mS16 = cpu->GetS16();
		state.mPC = cpu->GetPC();
		state.mInsnPC = cpu->GetInsnPC();
		state.mbEmulation = cpu->GetEmulationFlag();
		state.mMode = cpu->GetCPUMode();
		return state;
	};
	connections.mpIsKernelROM = [sim](uint16 addr) { return sim->IsKernelROMLocation(addr); };
	connections.mpDebugReadByte = [sim](uint16 addr) { return sim->DebugReadByte(addr); };
	connections.mpDebugReadWord = [sim](uint16 addr) { return sim->DebugReadWord(addr); };
	connections.mpGetDisplayListPtr = [sim] { return sim->GetAntic().GetDisplayListPtr(); };
	connections.mpIsDisplayListEnabled = [sim] { return sim->GetAntic().IsDisplayListEnabled(); };
	Init(std::move(connections), simevmgr);
}

void ATCPUVerifier::VerifyInsn(const ATCPUEmulator& cpu, uint8 opcode, uint16 target) {
	(void)cpu; // Preserve the original API's observations of the bound CPU.
	VerifyInsn(opcode, target);
}
