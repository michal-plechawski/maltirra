// Altirra portable timer service tests

#include <chrono>
#include <memory>
#include <thread>

#include <at/attest/portabletest.h>
#include <at/atcore/asyncdispatcherimpl.h>
#include <at/atcore/timerservice.h>

namespace {
	template<typename TPredicate>
	bool ATPumpUntil(
		ATAsyncDispatcher& dispatcher,
		TPredicate&& predicate,
		std::chrono::milliseconds timeout) {
		const auto deadline = std::chrono::steady_clock::now() + timeout;
		for(;;) {
			dispatcher.RunCallbacks();
			if (predicate())
				return true;
			if (std::chrono::steady_clock::now() >= deadline)
				return false;
			std::this_thread::sleep_for(std::chrono::milliseconds(2));
		}
	}

	void ATPumpFor(
		ATAsyncDispatcher& dispatcher,
		std::chrono::milliseconds duration) {
		const auto deadline = std::chrono::steady_clock::now() + duration;
		while(std::chrono::steady_clock::now() < deadline) {
			dispatcher.RunCallbacks();
			std::this_thread::sleep_for(std::chrono::milliseconds(2));
		}
		dispatcher.RunCallbacks();
	}
}

bool ATTestCoreTimerService(ATPortableTestContext& context) {
	ATAsyncDispatcher dispatcher;
	auto timerService = std::unique_ptr<IATTimerService>(
		ATCreateTimerService(dispatcher));
	AT_PORTABLE_TEST_ASSERT(context, timerService != nullptr);

	const std::thread::id dispatchThread = std::this_thread::get_id();
	std::thread::id callbackThread;
	int immediateCount = 0;
	uint64 immediateToken = 0;
	timerService->Request(&immediateToken, 0.0f, [&] {
		callbackThread = std::this_thread::get_id();
		++immediateCount;
	});
	AT_PORTABLE_TEST_ASSERT(context, immediateToken != 0);
	AT_PORTABLE_TEST_ASSERT(context,
		ATPumpUntil(dispatcher, [&] { return immediateCount == 1; },
			std::chrono::seconds(2)));
	AT_PORTABLE_TEST_ASSERT(context, callbackThread == dispatchThread);
	timerService->Cancel(&immediateToken);
	AT_PORTABLE_TEST_ASSERT(context, immediateToken == 0);

	int cancelledCount = 0;
	uint64 cancelledToken = 0;
	timerService->Request(&cancelledToken, 0.05f, [&] { ++cancelledCount; });
	AT_PORTABLE_TEST_ASSERT(context, cancelledToken != 0);
	std::this_thread::sleep_for(std::chrono::milliseconds(150));
	timerService->Cancel(&cancelledToken);
	AT_PORTABLE_TEST_ASSERT(context, cancelledToken == 0);
	dispatcher.RunCallbacks();
	AT_PORTABLE_TEST_ASSERT(context, cancelledCount == 0);

	int replacementValue = 0;
	uint64 replacementToken = 0;
	timerService->Request(&replacementToken, 0.25f, [&] {
		replacementValue = 1;
	});
	const uint64 firstReplacementToken = replacementToken;
	timerService->Request(&replacementToken, 0.02f, [&] {
		replacementValue += 10;
	});
	AT_PORTABLE_TEST_ASSERT(context,
		replacementToken != 0 && replacementToken != firstReplacementToken);
	AT_PORTABLE_TEST_ASSERT(context,
		ATPumpUntil(dispatcher, [&] { return replacementValue != 0; },
			std::chrono::seconds(2)));
	AT_PORTABLE_TEST_ASSERT(context, replacementValue == 10);
	ATPumpFor(dispatcher, std::chrono::milliseconds(350));
	AT_PORTABLE_TEST_ASSERT(context, replacementValue == 10);

	int multipleCount = 0;
	timerService->Request(nullptr, 0.12f, [&] { ++multipleCount; });
	timerService->Request(nullptr, 0.03f, [&] { ++multipleCount; });
	timerService->Request(nullptr, 0.08f, [&] { ++multipleCount; });
	timerService->Request(nullptr, 0.18f, [&] { ++multipleCount; });
	AT_PORTABLE_TEST_ASSERT(context,
		ATPumpUntil(dispatcher, [&] { return multipleCount == 4; },
			std::chrono::seconds(2)));

	int destroyedCount = 0;
	timerService->Request(nullptr, 0.03f, [&] { ++destroyedCount; });
	std::this_thread::sleep_for(std::chrono::milliseconds(150));
	timerService.reset();
	dispatcher.RunCallbacks();
	AT_PORTABLE_TEST_ASSERT(context, destroyedCount == 0);

	return true;
}
