//	Altirra - Atari 800/800XL/5200 emulator
//	Copyright (C) 2009-2010 Avery Lee
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
#include "bkptmanager.h"
#include "cpu.h"
#include "simulator.h"

void ATBreakpointManager::Init(ATCPUEmulator *cpu, ATMemoryManager *memmgr, ATSimulator *sim) {
	ATBreakpointManagerConnections connections;
	connections.mpSetBreakpointManager = [cpu](ATBreakpointManager *manager) { cpu->SetBreakpointManager(manager); };
	connections.mpSetBreakpoint = [cpu](uint16 pc) { cpu->SetBreakpoint(pc); };
	connections.mpClearBreakpoint = [cpu](uint16 pc) { cpu->ClearBreakpoint(pc); };
	connections.mpSetAllBreakpoints = [cpu] { cpu->SetAllBreakpoints(); };
	connections.mpClearAllBreakpoints = [cpu] { cpu->ClearAllBreakpoints(); };
	connections.mpGetXPC = [cpu] { return cpu->GetXPC(); };
	connections.mpPostInterruptingEvent = [sim](ATSimulatorEvent ev) { sim->PostInterruptingEvent(ev); };
	Init(std::move(connections), memmgr);
}
