// Altirra portable monotonic clock and timer tests

#include <atomic>
#include <cmath>
#include <limits>

#if defined(__APPLE__)
	#include <CoreFoundation/CoreFoundation.h>
#endif

#include <at/attest/portabletest.h>
#include <vd2/system/thread.h>
#include <vd2/system/time.h>

namespace {
	class VDTestTimerCallback final : public IVDTimerCallback {
	public:
		void TimerCallback() override {
			const int count = ++mCount;
			if (count >= mSignalAt)
				mSignal.signal();
		}

		std::atomic<int> mCount { 0 };
		int mSignalAt = 1;
		VDSignalPersistent mSignal;
	};

#if defined(__APPLE__)
	bool VDRunCurrentRunLoopUntil(const std::atomic<int>& value, int target, double timeoutSeconds) {
		const CFAbsoluteTime deadline = CFAbsoluteTimeGetCurrent() + timeoutSeconds;
		while(value.load() < target && CFAbsoluteTimeGetCurrent() < deadline)
			CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.05, true);

		return value.load() >= target;
	}

	void VDRunCurrentRunLoopFor(double seconds) {
		const CFAbsoluteTime deadline = CFAbsoluteTimeGetCurrent() + seconds;
		while(CFAbsoluteTimeGetCurrent() < deadline)
			CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.01, false);
	}
#endif
}

bool ATTestSystemTime(ATPortableTestContext& context) {
	const uint64 frequencyI = VDGetPreciseTicksPerSecondI();
	const double frequency = VDGetPreciseTicksPerSecond();
	const double secondsPerTick = VDGetPreciseSecondsPerTick();
	AT_PORTABLE_TEST_ASSERT(context, frequencyI > 0);
	AT_PORTABLE_TEST_ASSERT(context, std::isfinite(frequency) && frequency > 0);
	AT_PORTABLE_TEST_ASSERT(context, std::isfinite(secondsPerTick) && secondsPerTick > 0);
	AT_PORTABLE_TEST_ASSERT(context,
		std::abs(frequency - static_cast<double>(frequencyI)) / frequency < 0.000001);
	AT_PORTABLE_TEST_ASSERT(context, std::abs(frequency * secondsPerTick - 1.0) < 0.000001);

	const uint32 currentStart = VDGetCurrentTick();
	const uint64 current64Start = VDGetCurrentTick64();
	const uint32 accurateStart = VDGetAccurateTick();
	const uint64 preciseStart = VDGetPreciseTick();
	VDThreadSleep(25);
	const uint32 currentElapsed = VDGetCurrentTick() - currentStart;
	const uint64 current64Elapsed = VDGetCurrentTick64() - current64Start;
	const uint32 accurateElapsed = VDGetAccurateTick() - accurateStart;
	const uint64 preciseElapsed = VDGetPreciseTick() - preciseStart;
	const double preciseSeconds = static_cast<double>(preciseElapsed) * secondsPerTick;
	AT_PORTABLE_TEST_ASSERT(context, currentElapsed >= 5 && currentElapsed < 5000);
	AT_PORTABLE_TEST_ASSERT(context, current64Elapsed >= 5 && current64Elapsed < 5000);
	AT_PORTABLE_TEST_ASSERT(context, accurateElapsed >= 5 && accurateElapsed < 5000);
	AT_PORTABLE_TEST_ASSERT(context, preciseSeconds >= 0.005 && preciseSeconds < 5.0);

	VDTestTimerCallback callback;
	callback.mSignalAt = 3;
	VDCallbackTimer callbackTimer;
	AT_PORTABLE_TEST_ASSERT(context, !callbackTimer.Init(nullptr, 10));
	AT_PORTABLE_TEST_ASSERT(context, !callbackTimer.Init(&callback, 0));
	AT_PORTABLE_TEST_ASSERT(context,
		!callbackTimer.Init(&callback, std::numeric_limits<uint32>::max()));
	AT_PORTABLE_TEST_ASSERT(context, callbackTimer.Init(&callback, 5));
	AT_PORTABLE_TEST_ASSERT(context, callbackTimer.IsTimerRunning());
	callbackTimer.SetRateDelta(1000);
	callbackTimer.AdjustRate(-500);
	AT_PORTABLE_TEST_ASSERT(context, callback.mSignal.tryWait(5000));
	callbackTimer.Shutdown();
	AT_PORTABLE_TEST_ASSERT(context, !callbackTimer.IsTimerRunning());
	const int stoppedCount = callback.mCount.load();
	VDThreadSleep(20);
	AT_PORTABLE_TEST_ASSERT(context, callback.mCount.load() == stoppedCount);

	callback.mCount = 0;
	callback.mSignal.unsignal();
	callback.mSignalAt = 2;
	AT_PORTABLE_TEST_ASSERT(context,
		callbackTimer.Init3(&callback, 50000, 25000, false));
	AT_PORTABLE_TEST_ASSERT(context, callback.mSignal.tryWait(5000));
	callbackTimer.Shutdown();

	for(int iteration = 0; iteration < 10; ++iteration) {
		callback.mCount = 0;
		callback.mSignal.unsignal();
		callback.mSignalAt = 1;
		AT_PORTABLE_TEST_ASSERT(context, callbackTimer.Init(&callback, 2));
		AT_PORTABLE_TEST_ASSERT(context, callback.mSignal.tryWait(5000));
		callbackTimer.Shutdown();
	}

	VDLazyTimer lazyTimer;
	std::atomic<int> lazyCount { 0 };
	lazyTimer.SetOneShot(nullptr, 1);
	lazyTimer.SetPeriodic(nullptr, 1);
	lazyTimer.SetOneShotFn([&lazyCount] { ++lazyCount; }, 10);
#if defined(__APPLE__)
	AT_PORTABLE_TEST_ASSERT(context, VDRunCurrentRunLoopUntil(lazyCount, 1, 5.0));
	VDRunCurrentRunLoopFor(0.03);
	AT_PORTABLE_TEST_ASSERT(context, lazyCount.load() == 1);

	lazyCount = 0;
	lazyTimer.SetPeriodicFn([&lazyCount] { ++lazyCount; }, 5);
	AT_PORTABLE_TEST_ASSERT(context, VDRunCurrentRunLoopUntil(lazyCount, 3, 5.0));
	lazyTimer.Stop();
	const int lazyStoppedCount = lazyCount.load();
	VDRunCurrentRunLoopFor(0.03);
	AT_PORTABLE_TEST_ASSERT(context, lazyCount.load() == lazyStoppedCount);
#else
	// Windows SetTimer callbacks require the owning thread's message pump. The
	// portable console runner verifies creation and cancellation here; UI tests
	// exercise delivery through the native message loop.
	lazyTimer.Stop();
	AT_PORTABLE_TEST_ASSERT(context, lazyCount.load() == 0);
#endif

	lazyTimer.SetPeriodicFn([&lazyCount] { ++lazyCount; }, 1);
	lazyTimer.Stop();
	return true;
}
