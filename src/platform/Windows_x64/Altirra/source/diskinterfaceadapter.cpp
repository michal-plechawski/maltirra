//	Altirra - Atari 800/800XL/5200 emulator
//	Copyright (C) 2017 Avery Lee
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
#include "diskinterface.h"
#include "simulator.h"
#include "uirender.h"

extern ATSimulator g_sim;

void ATDiskInterface::Init(uint32 index, IATUIRenderer *renderer) {
	const uint32 errorSource = renderer->AllocateErrorSourceId();
	ATDiskInterfaceConnections c;
	c.mpSetDiskErrorState = [renderer](uint32 i, bool v) { renderer->SetDiskErrorState(i, v); };
	c.mpSetDiskMotorActivity = [renderer](uint32 i, bool v) { renderer->SetDiskMotorActivity(i, v); };
	c.mpSetStatusCounter = [renderer](uint32 i, uint32 v) { renderer->SetStatusCounter(i, v); };
	c.mpSetStatusFlags = [renderer](uint32 mask) { renderer->SetStatusFlags(mask); };
	c.mpResetStatusFlags = [renderer](uint32 mask, uint32 hold) { renderer->ResetStatusFlags(mask, hold); };
	c.mpSetDiskLEDState = [renderer](uint32 i, sint32 v) { renderer->SetDiskLEDState(i, v); };
	c.mpReportError = [renderer, errorSource](const wchar_t *s) { renderer->ReportError(errorSource, s); };
	c.mpOnSectorBreakpoint = [] { g_sim.PostInterruptingEvent(kATSimEvent_DiskSectorBreakpoint); };
	Init(index, std::move(c));
}
