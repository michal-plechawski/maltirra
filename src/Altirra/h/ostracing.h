//	Altirra - Atari 800/800XL/5200 emulator
//	Copyright (C) 2025 Avery Lee
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

#ifndef f_AT_OSTRACING_H
#define f_AT_OSTRACING_H

// Initialize OS-level tracing. This emits events into the system via
// the native platform tracing facility. Backends may keep the calls as no-ops
// when tracing support is unavailable.
void ATInitOSTracing();
void ATShutdownOSTracing();
bool ATIsOSTracingEnabled();

void ATOSTraceSimulateBegin();
void ATOSTraceSimulateEnd();

#endif
