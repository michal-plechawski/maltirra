//	Altirra - Atari 800/800XL/5200 emulator
//	Copyright (C) 2009-2026 Avery Lee and the Altirra contributors
//
//	This program is free software; you can redistribute it and/or modify
//	it under the terms of the GNU General Public License as published by
//	the Free Software Foundation; either version 2 of the License, or
//	(at your option) any later version.

#include <algorithm>
#include <new>

#include <vd2/system/math.h>
#include <vd2/system/time.h>
#include <vd2/system/vdstl.h>
#include <at/atcore/asyncdispatcher.h>
#include <at/atcore/internal/timerserviceimpl_macos.h>

namespace {
	char gATTimerServiceQueueKey;

	void ATTimerServiceDispatchNoop(void *) {
	}
}

ATTimerService::ATTimerService(IATAsyncDispatcher& dispatcher)
	: mpAsyncDispatcher(&dispatcher) {
	mTimerQueue = dispatch_queue_create(
		"com.virtualdub.altirra.timerservice",
		DISPATCH_QUEUE_SERIAL);
	if (!mTimerQueue)
		throw std::bad_alloc();

	dispatch_queue_set_specific(
		mTimerQueue,
		&gATTimerServiceQueueKey,
		this,
		nullptr);

	mTimer = dispatch_source_create(
		DISPATCH_SOURCE_TYPE_TIMER,
		0,
		0,
		mTimerQueue);
	if (!mTimer) {
		dispatch_release(mTimerQueue);
		mTimerQueue = nullptr;
		throw std::bad_alloc();
	}

	dispatch_set_context(mTimer, this);
	dispatch_source_set_event_handler_f(mTimer, StaticTimerCallback);
	dispatch_source_set_timer(
		mTimer,
		DISPATCH_TIME_FOREVER,
		DISPATCH_TIME_FOREVER,
		0);
	dispatch_resume(mTimer);
}

ATTimerService::~ATTimerService() {
	IATAsyncDispatcher *dispatcher =
		mpAsyncDispatcher.exchange(nullptr, std::memory_order_acq_rel);

	if (mTimer) {
		dispatch_source_cancel(mTimer);

		if (dispatch_get_specific(&gATTimerServiceQueueKey) != this)
			dispatch_sync_f(mTimerQueue, nullptr, ATTimerServiceDispatchNoop);
	}

	if (dispatcher)
		dispatcher->Cancel(&mRunToken);

	if (mTimer)
		dispatch_release(mTimer);
	if (mTimerQueue)
		dispatch_release(mTimerQueue);
}

void ATTimerService::Request(uint64 *token, float delay, Callback fn) {
	if (!fn)
		return;

	const uint64 t = VDGetCurrentTick64();
	const uint64 deadline =
		(t + VDRoundToInt64(std::clamp(delay, 0.0f, 60.0f) * 1000.0f)) / 50;

	vdsynchronized(mMutex) {
		uint32 slotIndex;
		if (mFreeSlots.empty()) {
			slotIndex = mSlots.size();
			mSlots.emplace_back();
		} else {
			slotIndex = mFreeSlots.back();
			mFreeSlots.pop_back();
		}

		Slot& slot = mSlots[slotIndex];
		VDASSERT(slot.mHeapIndex < 0);

		slot.mDeadline = deadline;
		slot.mCallback = std::move(fn);
		++slot.mSequenceNo;

		uint32 curPos = (uint32)mHeap.size();
		mHeap.push_back(slotIndex);

		while(curPos) {
			const uint32 parentPos = (curPos - 1) >> 1;
			const uint32 parentIdx = mHeap[parentPos];
			Slot& parentSlot = mSlots[parentIdx];

			if (parentSlot.mDeadline <= deadline)
				break;

			parentSlot.mHeapIndex = curPos;
			mHeap[curPos] = parentIdx;
			curPos = parentPos;
		}

		slot.mHeapIndex = curPos;
		mHeap[curPos] = slotIndex;

		if (curPos == 0) {
			const uint64 deadlineTick = deadline * 50;
			RearmTimerForTickDelay(deadlineTick > t ? deadlineTick - t : 0);
		}

		if (token) {
			InternalCancel(*token, fn);
			*token = ((uint64)slot.mSequenceNo << 32) + slotIndex + 1;
		}
	}
}

void ATTimerService::Cancel(uint64 *tokenPtr) {
	if (tokenPtr) {
		Callback cb;

		vdsynchronized(mMutex) {
			InternalCancel(*tokenPtr, cb);
			*tokenPtr = 0;
		}
	}
}

void ATTimerService::InternalCancel(uint64 token, Callback& cb) {
	const uint32 slotIdx = (uint32)(token - 1);
	if (slotIdx >= mSlots.size())
		return;

	Slot& slot = mSlots[slotIdx];
	if ((token >> 32) != slot.mSequenceNo || slot.mHeapIndex < 0)
		return;

	const uint32 pos = slot.mHeapIndex;
	slot.mHeapIndex = -1;
	cb = std::move(slot.mCallback);
	slot.mCallback = nullptr;
	mFreeSlots.push_back(slotIdx);
	++slot.mSequenceNo;

	const uint32 tailIdx = mHeap.back();
	mHeap.pop_back();

	if (slotIdx != tailIdx) {
		Slot& tailSlot = mSlots[tailIdx];
		VDASSERT(tailSlot.mHeapIndex == mHeap.size());

		uint32 pos2 = pos;
		while(pos2) {
			const uint32 parentPos = (pos2 - 1) >> 1;
			const uint32 parentIdx = mHeap[parentPos];
			Slot& parentSlot = mSlots[parentIdx];
			if (parentSlot.mDeadline <= tailSlot.mDeadline)
				break;

			parentSlot.mHeapIndex = pos2;
			mHeap[pos2] = parentIdx;
			pos2 = parentPos;
		}

		if (pos2 == pos)
			pos2 = Sink(pos, tailSlot.mDeadline);

		mHeap[pos2] = tailIdx;
		tailSlot.mHeapIndex = pos2;
	}
}

void ATTimerService::RunCallbacks() {
	static constexpr uint64 kEternity = ~UINT64_C(0);

	const uint64 t = VDGetCurrentTick64();
	const uint64 now = t / 50;

	Callback cb;
	uint64 slotDeadline;
	for(;;) {
		slotDeadline = kEternity;

		vdsynchronized(mMutex) {
			if (mHeap.empty())
				break;

			const uint32 slotIdx = mHeap.front();
			Slot& slot = mSlots[slotIdx];
			slotDeadline = slot.mDeadline;

			if (slotDeadline > now)
				break;

			cb = std::move(slot.mCallback);
			slot.mCallback = nullptr;
			VDASSERT(slot.mHeapIndex == 0);
			slot.mHeapIndex = -1;
			mFreeSlots.push_back(slotIdx);

			const uint32 tailIdx = mHeap.back();
			mHeap.pop_back();

			if (!mHeap.empty()) {
				Slot& tailSlot = mSlots[tailIdx];
				VDASSERT(tailSlot.mHeapIndex == mHeap.size());
				const uint32 pos = Sink(0, tailSlot.mDeadline);
				mHeap[pos] = tailIdx;
				tailSlot.mHeapIndex = pos;
			}
		}

		if (cb)
			cb();
	}

	if (slotDeadline != kEternity) {
		const uint64 deadlineTick = slotDeadline * 50;
		RearmTimerForTickDelay(deadlineTick > t ? deadlineTick - t : 0);
	}
}

uint32 ATTimerService::Sink(uint32 pos, uint64 val) {
	const uint32 heapCount = (uint32)mHeap.size();

	for(;;) {
		uint32 childPos = pos * 2 + 1;
		if (childPos >= heapCount)
			break;

		uint32 childIdx = mHeap[childPos];
		uint64 childVal = mSlots[childIdx].mDeadline;
		const uint32 rightPos = childPos + 1;
		if (rightPos < heapCount) {
			const uint32 rightIdx = mHeap[rightPos];
			const uint64 rightVal = mSlots[rightIdx].mDeadline;

			if (rightVal < childVal) {
				childVal = rightVal;
				childIdx = rightIdx;
				childPos = rightPos;
			}
		}

		if (val <= childVal)
			break;

		mSlots[childIdx].mHeapIndex = pos;
		mHeap[pos] = childIdx;
		pos = childPos;
	}

	return pos;
}

void ATTimerService::RearmTimerForTickDelay(uint64 ticks) {
	if (!mTimer)
		return;

	const uint64 nanoseconds = ticks * NSEC_PER_MSEC;
	dispatch_source_set_timer(
		mTimer,
		dispatch_time(DISPATCH_TIME_NOW, (int64_t)nanoseconds),
		DISPATCH_TIME_FOREVER,
		25 * NSEC_PER_MSEC);
}

void ATTimerService::StaticTimerCallback(void *context) {
	auto *service = static_cast<ATTimerService *>(context);
	IATAsyncDispatcher *dispatcher =
		service->mpAsyncDispatcher.load(std::memory_order_acquire);
	if (dispatcher) {
		dispatcher->Queue(
			&service->mRunToken,
			[service] { service->RunCallbacks(); });
	}
}

IATTimerService *ATCreateTimerService(IATAsyncDispatcher& dispatcher) {
	return new ATTimerService(dispatcher);
}
