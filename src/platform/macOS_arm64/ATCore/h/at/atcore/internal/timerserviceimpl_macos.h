//	Altirra - Atari 800/800XL/5200 emulator
//	Copyright (C) 2009-2026 Avery Lee and the Altirra contributors
//
//	This program is free software; you can redistribute it and/or modify
//	it under the terms of the GNU General Public License as published by
//	the Free Software Foundation; either version 2 of the License, or
//	(at your option) any later version.

#ifndef f_AT_ATCORE_TIMERSERVICEIMPL_MACOS_H
#define f_AT_ATCORE_TIMERSERVICEIMPL_MACOS_H

#include <atomic>
#include <dispatch/dispatch.h>
#include <vd2/system/thread.h>
#include <vd2/system/vdstl.h>
#include <at/atcore/timerservice.h>

class IATAsyncDispatcher;

class ATTimerService final : public IATTimerService
{
public:
	ATTimerService(IATAsyncDispatcher& dispatcher);
	~ATTimerService();

public:
	using Callback = vdfunction<void()>;
	void Request(uint64 *token, float delay, Callback fn) override;
	void Cancel(uint64 *token) override;

private:
	void InternalCancel(uint64 token, Callback& cb);
	void RunCallbacks();
	uint32 Sink(uint32 pos, uint64 val);

	void RearmTimerForTickDelay(uint64 ticks);
	static void StaticTimerCallback(void *context);

	VDCriticalSection mMutex;

	dispatch_queue_t mTimerQueue = nullptr;
	dispatch_source_t mTimer = nullptr;
	uint64 mRunToken = 0;
	std::atomic<IATAsyncDispatcher *> mpAsyncDispatcher;

	struct Slot {
		uint64 mDeadline = 0;
		uint32 mSequenceNo = 0x1234ABCD;
		sint32 mHeapIndex = -1;
		Callback mCallback;
	};

	vdfastvector<uint32> mHeap;
	vdvector<Slot> mSlots;
	vdfastvector<uint32> mFreeSlots;
};

#endif
