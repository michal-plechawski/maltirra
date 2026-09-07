// Altirra system timing implementation for macOS ARM64

#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>

#include <CoreFoundation/CoreFoundation.h>
#include <mach/mach_time.h>
#include <vd2/system/time.h>

namespace {
	mach_timebase_info_data_t VDGetMachTimebase() {
		mach_timebase_info_data_t timebase {};
		mach_timebase_info(&timebase);
		return timebase;
	}

	uint64 VDGetContinuousNanoseconds() {
		static const mach_timebase_info_data_t timebase = VDGetMachTimebase();
		const unsigned __int128 scaledTicks =
			static_cast<unsigned __int128>(mach_continuous_time()) * timebase.numer;
		return static_cast<uint64>(scaledTicks / timebase.denom);
	}

	uint32 VDGetWaitMilliseconds(std::chrono::steady_clock::duration delay) {
		using namespace std::chrono;

		const auto nanosecondsLeft = duration_cast<nanoseconds>(delay).count();
		if (nanosecondsLeft <= 0)
			return 0;

		const uint64 milliseconds =
			(static_cast<uint64>(nanosecondsLeft) + UINT64_C(999999)) / UINT64_C(1000000);
		return milliseconds > (std::numeric_limits<uint32>::max)()
			? (std::numeric_limits<uint32>::max)()
			: static_cast<uint32>(milliseconds);
	}
}

uint32 VDGetCurrentTick() {
	return static_cast<uint32>(VDGetCurrentTick64());
}

uint64 VDGetCurrentTick64() {
	return VDGetContinuousNanoseconds() / UINT64_C(1000000);
}

uint64 VDGetPreciseTick() {
	return mach_absolute_time();
}

uint64 VDGetPreciseTicksPerSecondI() {
	static const uint64 ticksPerSecond = [] {
		const mach_timebase_info_data_t timebase = VDGetMachTimebase();
		const long double frequency = static_cast<long double>(UINT64_C(1000000000))
			* static_cast<long double>(timebase.denom)
			/ static_cast<long double>(timebase.numer);
		return static_cast<uint64>(frequency + 0.5L);
	}();

	return ticksPerSecond;
}

double VDGetPreciseTicksPerSecond() {
	static const double ticksPerSecond = [] {
		const mach_timebase_info_data_t timebase = VDGetMachTimebase();
		return 1000000000.0 * static_cast<double>(timebase.denom)
			/ static_cast<double>(timebase.numer);
	}();

	return ticksPerSecond;
}

double VDGetPreciseSecondsPerTick() {
	static const double secondsPerTick = 1.0 / VDGetPreciseTicksPerSecond();
	return secondsPerTick;
}

uint32 VDGetAccurateTick() {
	return VDGetCurrentTick();
}

///////////////////////////////////////////////////////////////////////////////

VDCallbackTimer::VDCallbackTimer()
	: mTimerAccuracy(0) {
}

VDCallbackTimer::~VDCallbackTimer() {
	Shutdown();
}

bool VDCallbackTimer::Init(IVDTimerCallback *pCB, uint32 period_ms) {
	const uint64 period100ns = static_cast<uint64>(period_ms) * 10000;
	if (period100ns > (std::numeric_limits<uint32>::max)())
		return false;

	return Init2(pCB, static_cast<uint32>(period100ns));
}

bool VDCallbackTimer::Init2(IVDTimerCallback *pCB, uint32 period_100ns) {
	return Init3(pCB, period_100ns, period_100ns >> 1, true);
}

bool VDCallbackTimer::Init3(
	IVDTimerCallback *pCB,
	uint32 period_100ns,
	uint32 accuracy_100ns,
	bool precise) {
	Shutdown();
	if (!pCB || !period_100ns)
		return false;
	while(msigExit.check())
		;

	mpCB = pCB;
	mbExit.store(false, std::memory_order_release);
	mbPrecise = precise;
	mTimerAccuracy = static_cast<uint32>(std::min<uint64>(
		10, std::max<uint64>(
			1, (static_cast<uint64>(accuracy_100ns) + 9999) / 10000)));
	mTimerPeriod = period_100ns;
	mTimerPeriodAdjustment.xchg(0);
	mTimerPeriodDelta.xchg(0);

	if (ThreadStart())
		return true;

	mTimerAccuracy = 0;
	mpCB = nullptr;
	return false;
}

void VDCallbackTimer::Shutdown() {
	if (isThreadActive()) {
		mbExit.store(true, std::memory_order_release);
		msigExit.signal();
		ThreadWait();
	}

	mTimerAccuracy = 0;
	mpCB = nullptr;
}

void VDCallbackTimer::SetRateDelta(int delta_100ns) {
	mTimerPeriodDelta.xchg(delta_100ns);
}

void VDCallbackTimer::AdjustRate(int adjustment_100ns) {
	mTimerPeriodAdjustment += adjustment_100ns;
}

bool VDCallbackTimer::IsTimerRunning() const {
	return const_cast<VDCallbackTimer *>(this)->isThreadActive();
}

void VDCallbackTimer::ThreadRun() {
	using namespace std::chrono;

	sint64 basePeriod100ns = mTimerPeriod;
	auto nextTime = steady_clock::now() + nanoseconds(basePeriod100ns * 100);

	while(!mbExit.load(std::memory_order_acquire)) {
		const auto now = steady_clock::now();
		const uint32 waitMilliseconds = VDGetWaitMilliseconds(nextTime - now);
		if (waitMilliseconds && msigExit.tryWait(waitMilliseconds))
			break;

		const auto callbackTime = steady_clock::now();
		if (callbackTime < nextTime)
			continue;

		if (mpCB)
			mpCB->TimerCallback();

		const int adjustment = mTimerPeriodAdjustment.xchg(0);
		const int delta = mTimerPeriodDelta.compareExchange(0, 0);
		basePeriod100ns = std::clamp<sint64>(
			basePeriod100ns + adjustment,
			1,
			(std::numeric_limits<uint32>::max)());
		const sint64 effectivePeriod100ns = std::clamp<sint64>(
			basePeriod100ns + delta,
			1,
			(std::numeric_limits<uint32>::max)());
		const auto effectivePeriod = nanoseconds(effectivePeriod100ns * 100);

		if (mbPrecise) {
			nextTime += effectivePeriod;
			if (callbackTime - nextTime > effectivePeriod * 5)
				nextTime = callbackTime + effectivePeriod;
		} else {
			nextTime = callbackTime + effectivePeriod;
		}
	}
}

///////////////////////////////////////////////////////////////////////////////

VDLazyTimer::VDLazyTimer()
	: mTimerId(0)
	, mbPeriodic(false)
	, mpThunk(nullptr) {
}

VDLazyTimer::~VDLazyTimer() {
	Stop();
}

void VDLazyTimer::SetOneShot(IVDTimerCallback *pCB, uint32 delay) {
	if (!pCB) {
		Stop();
		return;
	}

	SetOneShotFn([=]() { pCB->TimerCallback(); }, delay);
}

void VDLazyTimer::SetOneShotFn(const vdfunction<void()>& fn, uint32 delay) {
	Stop();

	mbPeriodic = false;
	mpFn = fn;

	CFRunLoopTimerContext context {
		0, this, nullptr, nullptr, nullptr
	};
	const CFAbsoluteTime fireTime =
		CFAbsoluteTimeGetCurrent() + static_cast<double>(delay) / 1000.0;
	CFRunLoopTimerRef timer = CFRunLoopTimerCreate(
		kCFAllocatorDefault,
		fireTime,
		0,
		0,
		0,
		[](CFRunLoopTimerRef, void *info) {
			auto *self = static_cast<VDLazyTimer *>(info);
			self->Stop();
			if (self->mpFn)
				self->mpFn();
		},
		&context);
	if (!timer)
		return;

	CFRunLoopRef runLoop = CFRunLoopGetCurrent();
	CFRetain(runLoop);
	mpNativeTimer = timer;
	mpNativeRunLoop = runLoop;
	mTimerId = 1;
	CFRunLoopAddTimer(runLoop, timer, kCFRunLoopCommonModes);
}

void VDLazyTimer::SetPeriodic(IVDTimerCallback *pCB, uint32 delay) {
	if (!pCB) {
		Stop();
		return;
	}

	SetPeriodicFn([=]() { pCB->TimerCallback(); }, delay);
}

void VDLazyTimer::SetPeriodicFn(const vdfunction<void()>& fn, uint32 delay) {
	Stop();

	mbPeriodic = true;
	mpFn = fn;
	const double interval = static_cast<double>(std::max<uint32>(delay, 1)) / 1000.0;

	CFRunLoopTimerContext context {
		0, this, nullptr, nullptr, nullptr
	};
	CFRunLoopTimerRef timer = CFRunLoopTimerCreate(
		kCFAllocatorDefault,
		CFAbsoluteTimeGetCurrent() + interval,
		interval,
		0,
		0,
		[](CFRunLoopTimerRef, void *info) {
			auto *self = static_cast<VDLazyTimer *>(info);
			if (self->mpFn)
				self->mpFn();
		},
		&context);
	if (!timer)
		return;

	CFRunLoopRef runLoop = CFRunLoopGetCurrent();
	CFRetain(runLoop);
	mpNativeTimer = timer;
	mpNativeRunLoop = runLoop;
	mTimerId = 1;
	CFRunLoopAddTimer(runLoop, timer, kCFRunLoopCommonModes);
}

void VDLazyTimer::Stop() {
	auto timer = static_cast<CFRunLoopTimerRef>(mpNativeTimer);
	auto runLoop = static_cast<CFRunLoopRef>(mpNativeRunLoop);
	mpNativeTimer = nullptr;
	mpNativeRunLoop = nullptr;
	mTimerId = 0;

	if (timer) {
		CFRunLoopTimerInvalidate(timer);
		CFRelease(timer);
	}

	if (runLoop) {
		CFRunLoopWakeUp(runLoop);
		CFRelease(runLoop);
	}
}
