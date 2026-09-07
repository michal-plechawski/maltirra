//	VirtualDub - Video processing and capture application
//	System library component
//	Copyright (C) 1998-2004 Avery Lee, All Rights Reserved.
//
//	Beginning with 1.6.0, the VirtualDub system library is licensed
//	differently than the remainder of VirtualDub.  This particular file is
//	thus licensed as follows (the "zlib" license):
//
//	This software is provided 'as-is', without any express or implied
//	warranty.  In no event will the authors be held liable for any
//	damages arising from the use of this software.
//
//	Permission is granted to anyone to use this software for any purpose,
//	including commercial applications, and to alter it and redistribute it
//	freely, subject to the following restrictions:
//
//	1.	The origin of this software must not be misrepresented; you must
//		not claim that you wrote the original software. If you use this
//		software in a product, an acknowledgment in the product
//		documentation would be appreciated but is not required.
//	2.	Altered source versions must be plainly marked as such, and must
//		not be misrepresented as being the original software.
//	3.	This notice may not be removed or altered from any source
//		distribution.

#ifndef f_VD2_SYSTEM_TIME_H
#define f_VD2_SYSTEM_TIME_H

#include <atomic>

#include <vd2/system/vdtypes.h>
#include <vd2/system/atomic.h>
#include <vd2/system/function.h>
#include <vd2/system/thread.h>

class VDFunctionThunkInfo;

// VDGetCurrentTick: Retrieve current process timer, in milliseconds.  Should only
// be used for sparsing updates/checks, and not for precision timing.  Approximate
// resolution is 55ms under Win9x and 10-15ms under WinNT. The advantage of this
// call is that it is usually extremely fast (just reading from the PEB).
uint32 VDGetCurrentTick();
uint64 VDGetCurrentTick64();

// VDGetPreciseTick: Retrieves the platform high-performance monotonic timer.
// Convert deltas with VDGetPreciseTicksPerSecond() or
// VDGetPreciseSecondsPerTick(); the absolute epoch is platform-specific.
uint64 VDGetPreciseTick();
uint64 VDGetPreciseTicksPerSecondI();
double VDGetPreciseTicksPerSecond();
double VDGetPreciseSecondsPerTick();

// VDGetAccurateTick: Reads a timer with good precision and accuracy, in
// milliseconds. On Win9x, it has 1ms precision; on WinNT, it may have anywhere
// from 1ms to 10-15ms, although 1ms can be forced with timeBeginPeriod().
uint32 VDGetAccurateTick();

// VDCallbackTimer is a high-accuracy periodic timer backed by a dedicated thread.
// It is relatively expensive to instantiate and is intended for critical timing
// needs such as multimedia. The callback should execute as quickly as possible.

class VDINTERFACE IVDTimerCallback {
public:
	virtual void TimerCallback() = 0;
};

class VDCallbackTimer : private VDThread {
public:
	VDCallbackTimer();
	~VDCallbackTimer();

	bool Init(IVDTimerCallback *pCB, uint32 period_ms);
	bool Init2(IVDTimerCallback *pCB, uint32 period_100ns);
	bool Init3(IVDTimerCallback *pCB, uint32 period_100ns, uint32 accuracy_100ns, bool precise);
	void Shutdown();

	void SetRateDelta(int delta_100ns);
	void AdjustRate(int adjustment_100ns);

	bool IsTimerRunning() const;

private:
	void ThreadRun();

	IVDTimerCallback *mpCB;
	unsigned		mTimerAccuracy;
	uint32			mTimerPeriod;
	VDAtomicInt		mTimerPeriodDelta;
	VDAtomicInt		mTimerPeriodAdjustment;

	VDSignal		msigExit;

	std::atomic<bool> mbExit { false };
	bool			mbPrecise;
};


class VDLazyTimer {
	VDLazyTimer(const VDLazyTimer&) = delete;
	VDLazyTimer& operator=(const VDLazyTimer&) = delete;
public:
	VDLazyTimer();
	~VDLazyTimer();

	void SetOneShot(IVDTimerCallback *pCB, uint32 delay);
	void SetOneShotFn(const vdfunction<void()>& fn, uint32 delay);
	void SetPeriodic(IVDTimerCallback *pCB, uint32 delay);
	void SetPeriodicFn(const vdfunction<void()>& fn, uint32 delay);
	void Stop();

protected:
	void StaticTimeCallback(void *nativeWindow, uint32 message, uintptr timerId, uint32 time);

	uint32				mTimerId;
	bool				mbPeriodic;
	VDFunctionThunkInfo	*mpThunk;
	void			*mpNativeTimer = nullptr;
	void			*mpNativeRunLoop = nullptr;
	vdfunction<void()>	mpFn;
};

#endif
