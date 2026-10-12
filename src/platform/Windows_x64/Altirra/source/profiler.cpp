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
#include <at/atcore/address.h>
#include <at/atcore/scheduler.h>
#include "cpu.h"
#include "cpumemory.h"
#include "console.h"
#include "profiler.h"

ATCPUProfiler::ATCPUProfiler()
	: mpTSDProvider(nullptr)
	, mpCPU(nullptr)
	, mpMemory(nullptr)
	, mpCallbacks(nullptr)
	, mpFastScheduler(nullptr)
	, mpSlowScheduler(nullptr)
	, mpUpdateEvent(nullptr)
{
}

ATCPUProfiler::~ATCPUProfiler() {
}

void ATCPUProfiler::SetBoundaryRule(ATProfileBoundaryRule rule, uint32 param, uint32 param2) {
	mBuilder.SetBoundaryRule(rule, param, param2);
}

void ATCPUProfiler::SetGlobalAddressesEnabled(bool enable) {
	mBuilder.SetGlobalAddressesEnabled(enable);
}

void ATCPUProfiler::Init(ATCPUEmulator *cpu, ATCPUEmulatorMemory *mem, ATCPUEmulatorCallbacks *callbacks, ATScheduler *scheduler, ATScheduler *slowScheduler, IATCPUTimestampDecoderProvider *tsdprovider) {
	mpTSDProvider = tsdprovider;
	mpCPU = cpu;
	mpMemory = mem;
	mpCallbacks = callbacks;
	mpFastScheduler = scheduler;
	mpSlowScheduler = slowScheduler;
}

void ATCPUProfiler::Start(ATProfileMode mode, ATProfileCounterMode c1, ATProfileCounterMode c2) {
	mBuilder.Init(mode, c1, c2);
	mBuilder.SetS(mpCPU->GetS());

	mProfileMode = mode;
	mLastHistoryCounter = mpCPU->GetHistoryCounter();
	mbDropFirstSample = true;

	if (mode == kATProfileMode_BasicLines)
		mpUpdateEvent = mpSlowScheduler->AddEvent(2, this, 2);
	else
		mpUpdateEvent = mpSlowScheduler->AddEvent(32, this, 1);

	OpenFrame();
}

void ATCPUProfiler::BeginFrame() {
	Update();
	AdvanceFrame(true);
}

void ATCPUProfiler::EndFrame() {
	Update();
	AdvanceFrame(false);
}

void ATCPUProfiler::End() {
	Update();

	if (mpUpdateEvent) {
		mpSlowScheduler->RemoveEvent(mpUpdateEvent);
		mpUpdateEvent = NULL;
	}

	CloseFrame();

	mBuilder.Finalize();
}

void ATCPUProfiler::GetSession(ATProfileSession& session) {
	mBuilder.TakeSession(session);
}

void ATCPUProfiler::OnScheduledEvent(uint32 id) {
	if (id == 1) {
		mpUpdateEvent = mpSlowScheduler->AddEvent(32, this, 1);

		Update();
	} else if (id == 2) {
		mpUpdateEvent = mpSlowScheduler->AddEvent(2, this, 1);

		Update();
	}
}

void ATCPUProfiler::Update() {
	uint32 nextHistoryCounter = mpCPU->GetHistoryCounter();
	uint32 count = (nextHistoryCounter - mLastHistoryCounter) & (mpCPU->GetHistoryLength() - 1);
	mLastHistoryCounter = nextHistoryCounter;

	if (!count)
		return;

	// If we have not taken any samples yet, toss the first sample. This is because we
	// always need to have one additional sample due to differencing for timestamps, so
	// we can only process one less than the total number of available samples.
	if (mbDropFirstSample) {
		mbDropFirstSample = false;

		--count;
	}

	const ATCPUHistoryEntry *heptrs[256];
	const ATCPUTimestampDecoder& tsDecoder = mpTSDProvider->GetTimestampDecoder();
	uint32 lineNo = 0;

	if (mProfileMode == kATProfileMode_BasicLines) {
		uint32 lineAddr = (uint32)mpMemory->DebugReadByte(0x8A) + ((uint32)mpMemory->DebugReadByte(0x8B) << 8);
		lineNo = (uint32)mpMemory->DebugReadByte(lineAddr) + ((uint32)mpMemory->DebugReadByte(lineAddr + 1) << 8);
	}

	const bool useGlobalAddrs = (mpCPU->GetCPUMode() == kATCPUMode_6502);

	while(count) {
		// Note that we are constrained to always overlap by one entry, so we consume
		// one less entry than we read.
		const uint32 toRead = std::min<uint32>((uint32)vdcountof(heptrs) - 1, count);

		for(uint32 i = 0; i <= toRead; ++i)
			heptrs[i] = &mpCPU->GetHistory(count - i);

		if (mProfileMode == kATProfileMode_BasicLines)
			mBuilder.UpdateBasicLines(tsDecoder, lineNo, heptrs, toRead);
		else
			mBuilder.Update(tsDecoder, heptrs, toRead, useGlobalAddrs);

		count -= toRead;
	}
}

void ATCPUProfiler::AdvanceFrame(bool enableCollection) {
	mBuilder.AdvanceFrame(mpCallbacks->CPUGetCycle(), mpCallbacks->CPUGetUnhaltedCycle(), enableCollection, mpTSDProvider->GetTimestampDecoder());
}

void ATCPUProfiler::OpenFrame() {
	mBuilder.OpenFrame(mpCallbacks->CPUGetCycle(), mpCallbacks->CPUGetUnhaltedCycle(), mpTSDProvider->GetTimestampDecoder());
}

void ATCPUProfiler::CloseFrame() {
	mBuilder.CloseFrame(mpCallbacks->CPUGetCycle(), mpCallbacks->CPUGetUnhaltedCycle(), true);
}
