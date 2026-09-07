// Altirra portable function thunk tests

#include <atomic>
#include <thread>
#include <vector>

#include <at/attest/portabletest.h>
#include <vd2/system/thunk.h>

namespace {
	using VDTestTimerFn = void (*)(void *, unsigned, uint64, unsigned long);
	using VDTestWindowFn = sint64 (*)(void *, unsigned, uint64, sint64);
	using VDTestHookFn = sint64 (*)(int, uint64, sint64);

	class VDTestThunkTarget {
	public:
		void OnTimer(void *window, unsigned message, uint64 timerId, unsigned long tick) {
			mpWindow = window;
			mMessage = message;
			mTimerId = timerId;
			mValue = static_cast<sint64>(tick);
			++mCallCount;
		}

		sint64 OnWindow(void *window, unsigned message, uint64 first, sint64 second) {
			mpWindow = window;
			mMessage = message;
			mTimerId = first;
			mValue = second;
			++mCallCount;
			return static_cast<sint64>(message) + static_cast<sint64>(first) + second;
		}

		sint64 OnHook(int code, uint64 first, sint64 second) {
			mMessage = static_cast<unsigned>(code);
			mTimerId = first;
			mValue = second;
			++mCallCount;
			return static_cast<sint64>(code) - static_cast<sint64>(first) + second;
		}

		void *mpWindow = nullptr;
		unsigned mMessage = 0;
		uint64 mTimerId = 0;
		sint64 mValue = 0;
		int mCallCount = 0;
	};
}

bool ATTestSystemThunk(ATPortableTestContext& context) {
	AT_PORTABLE_TEST_ASSERT(context, VDInitThunkAllocator());
	AT_PORTABLE_TEST_ASSERT(context, VDInitThunkAllocator());

	VDTestThunkTarget target;
	void *const expectedWindow = reinterpret_cast<void *>(static_cast<uintptr>(0x1234));

	VDFunctionThunkInfo *timerThunk = VDCreateFunctionThunkFromMethod(
		&target, &VDTestThunkTarget::OnTimer, true);
	AT_PORTABLE_TEST_ASSERT(context, timerThunk != nullptr);
	VDGetThunkFunction<VDTestTimerFn>(timerThunk)(expectedWindow, 7, UINT64_C(0x123456789), 99);
	AT_PORTABLE_TEST_ASSERT(context, target.mpWindow == expectedWindow);
	AT_PORTABLE_TEST_ASSERT(context, target.mMessage == 7);
	AT_PORTABLE_TEST_ASSERT(context, target.mTimerId == UINT64_C(0x123456789));
	AT_PORTABLE_TEST_ASSERT(context, target.mValue == 99);
	AT_PORTABLE_TEST_ASSERT(context, target.mCallCount == 1);
	VDDestroyFunctionThunk(timerThunk);

	VDFunctionThunkInfo *windowThunk = VDCreateFunctionThunkFromMethod(
		&target, &VDTestThunkTarget::OnWindow, true);
	AT_PORTABLE_TEST_ASSERT(context, windowThunk != nullptr);
	const sint64 windowResult = VDGetThunkFunction<VDTestWindowFn>(windowThunk)(
		expectedWindow, 11, 13, -5);
	AT_PORTABLE_TEST_ASSERT(context, windowResult == 19);
	AT_PORTABLE_TEST_ASSERT(context, target.mMessage == 11);
	AT_PORTABLE_TEST_ASSERT(context, target.mTimerId == 13);
	AT_PORTABLE_TEST_ASSERT(context, target.mValue == -5);
	VDDestroyFunctionThunk(windowThunk);

	VDFunctionThunkInfo *hookThunk = VDCreateFunctionThunkFromMethod(
		&target, &VDTestThunkTarget::OnHook, true);
	AT_PORTABLE_TEST_ASSERT(context, hookThunk != nullptr);
	const sint64 hookResult = VDGetThunkFunction<VDTestHookFn>(hookThunk)(17, 5, -3);
	AT_PORTABLE_TEST_ASSERT(context, hookResult == 9);
	AT_PORTABLE_TEST_ASSERT(context, target.mMessage == 17);
	AT_PORTABLE_TEST_ASSERT(context, target.mTimerId == 5);
	AT_PORTABLE_TEST_ASSERT(context, target.mValue == -3);
	VDDestroyFunctionThunk(hookThunk);

	std::vector<VDTestThunkTarget> targets(64);
	std::vector<VDFunctionThunkInfo *> thunks;
	thunks.reserve(targets.size());
	for(size_t i = 0; i < targets.size(); ++i) {
		VDFunctionThunkInfo *thunk = VDCreateFunctionThunkFromMethod(
			&targets[i], &VDTestThunkTarget::OnTimer, true);
		AT_PORTABLE_TEST_ASSERT(context, thunk != nullptr);
		thunks.push_back(thunk);
	}
	for(size_t i = 0; i < thunks.size(); ++i) {
		VDGetThunkFunction<VDTestTimerFn>(thunks[i])(
			expectedWindow, static_cast<unsigned>(i), i * 3, static_cast<unsigned long>(i * 5));
		AT_PORTABLE_TEST_ASSERT(context, targets[i].mMessage == i);
		AT_PORTABLE_TEST_ASSERT(context, targets[i].mTimerId == i * 3);
		AT_PORTABLE_TEST_ASSERT(context, targets[i].mValue == static_cast<sint64>(i * 5));
	}

#if !VD_USE_DYNAMIC_THUNKS
	VDTestThunkTarget overflowTarget;
	AT_PORTABLE_TEST_ASSERT(context,
		VDCreateFunctionThunkFromMethod(
			&overflowTarget, &VDTestThunkTarget::OnTimer, true) == nullptr);
#endif

	for(VDFunctionThunkInfo *thunk : thunks)
		VDDestroyFunctionThunk(thunk);

	VDFunctionThunkInfo *reusedThunk = VDCreateFunctionThunkFromMethod(
		&target, &VDTestThunkTarget::OnTimer, true);
	AT_PORTABLE_TEST_ASSERT(context, reusedThunk != nullptr);
	VDGetThunkFunction<VDTestTimerFn>(reusedThunk)(nullptr, 23, 29, 31);
	AT_PORTABLE_TEST_ASSERT(context, target.mMessage == 23);
	AT_PORTABLE_TEST_ASSERT(context, target.mTimerId == 29);
	AT_PORTABLE_TEST_ASSERT(context, target.mValue == 31);
	VDDestroyFunctionThunk(reusedThunk);

	std::atomic<bool> concurrencyPassed { true };
	std::vector<std::thread> workers;
	for(int workerIndex = 0; workerIndex < 8; ++workerIndex) {
		workers.emplace_back([workerIndex, &concurrencyPassed] {
			for(int iteration = 0; iteration < 100; ++iteration) {
				VDTestThunkTarget workerTarget;
				VDFunctionThunkInfo *thunk = VDCreateFunctionThunkFromMethod(
					&workerTarget, &VDTestThunkTarget::OnTimer, true);
				if (!thunk) {
					concurrencyPassed = false;
					continue;
				}

				VDGetThunkFunction<VDTestTimerFn>(thunk)(
					nullptr, static_cast<unsigned>(workerIndex), iteration, 1);
				if (workerTarget.mMessage != static_cast<unsigned>(workerIndex)
					|| workerTarget.mTimerId != static_cast<uint64>(iteration)
					|| workerTarget.mCallCount != 1)
					concurrencyPassed = false;

				VDDestroyFunctionThunk(thunk);
			}
		});
	}
	for(std::thread& worker : workers)
		worker.join();
	AT_PORTABLE_TEST_ASSERT(context, concurrencyPassed.load());

	VDShutdownThunkAllocator();
	VDShutdownThunkAllocator();
	return true;
}
