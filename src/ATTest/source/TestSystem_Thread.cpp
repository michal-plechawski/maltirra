// Altirra portable threading and synchronization tests

#include <atomic>
#include <chrono>
#include <cstring>
#include <thread>
#include <vector>

#include <at/attest/portabletest.h>
#include <vd2/system/thread.h>
#include <vd2/system/tls.h>

namespace {
	std::atomic<int> gThreadHookAttachCount;
	std::atomic<int> gThreadHookDetachCount;
	std::atomic<bool> gThreadHookNameMatched;

	void VDTestThreadHook(bool attach, const char *threadName) {
		if (attach) {
			++gThreadHookAttachCount;
			gThreadHookNameMatched.store(
				threadName && !strcmp(threadName, "portable-worker"));
		} else {
			++gThreadHookDetachCount;
		}
	}

	class VDTestWorker final : public VDThread {
	public:
		VDTestWorker()
			: VDThread("portable-worker") {
		}

		void ThreadRun() override {
			mObservedThreadID.store(VDGetCurrentThreadID());
			mbWasCurrentThread.store(IsCurrentThread());
			++mRunCount;
			mStarted.signal();
			mRelease.wait();
		}

		VDSignal mStarted;
		VDSignal mRelease;
		std::atomic<int> mRunCount { 0 };
		std::atomic<VDThreadID> mObservedThreadID { 0 };
		std::atomic<bool> mbWasCurrentThread { false };
	};
}

bool ATTestSystemThread(ATPortableTestContext& context) {
	const VDThreadID mainThreadID = VDGetCurrentThreadID();
	AT_PORTABLE_TEST_ASSERT(context, mainThreadID != 0);
	AT_PORTABLE_TEST_ASSERT(context, VDGetCurrentThreadID() == mainThreadID);
	AT_PORTABLE_TEST_ASSERT(context, VDGetCurrentProcessId() != 0);
	AT_PORTABLE_TEST_ASSERT(context, VDGetLogicalProcessorCount() >= 1);
	VDSetThreadDebugName(mainThreadID, "portable-test-main");

	const auto sleepStart = std::chrono::steady_clock::now();
	VDThreadSleep(15);
	const auto sleepDuration = std::chrono::steady_clock::now() - sleepStart;
	AT_PORTABLE_TEST_ASSERT(context,
		sleepDuration >= std::chrono::milliseconds(5));
	AT_PORTABLE_TEST_ASSERT(context,
		sleepDuration < std::chrono::seconds(5));

	gThreadHookAttachCount = 0;
	gThreadHookDetachCount = 0;
	gThreadHookNameMatched = false;
	VDSetThreadInitHook(VDTestThreadHook);
	VDTestWorker worker;
	AT_PORTABLE_TEST_ASSERT(context, worker.ThreadStart());
	AT_PORTABLE_TEST_ASSERT(context, worker.isThreadAttached());
	AT_PORTABLE_TEST_ASSERT(context, worker.mStarted.tryWait(5000));
	AT_PORTABLE_TEST_ASSERT(context, worker.isThreadActive());
	AT_PORTABLE_TEST_ASSERT(context, !worker.IsCurrentThread());
	AT_PORTABLE_TEST_ASSERT(context,
		worker.mObservedThreadID.load() == worker.getThreadID());
	AT_PORTABLE_TEST_ASSERT(context, worker.getThreadID() != mainThreadID);
	AT_PORTABLE_TEST_ASSERT(context, worker.mbWasCurrentThread.load());
	worker.mRelease.signal();
	worker.ThreadWait();
	AT_PORTABLE_TEST_ASSERT(context, !worker.isThreadAttached());
	AT_PORTABLE_TEST_ASSERT(context, !worker.isThreadActive());
	AT_PORTABLE_TEST_ASSERT(context, worker.mRunCount.load() == 1);
	AT_PORTABLE_TEST_ASSERT(context, gThreadHookAttachCount.load() == 1);
	AT_PORTABLE_TEST_ASSERT(context, gThreadHookDetachCount.load() == 1);
	AT_PORTABLE_TEST_ASSERT(context, gThreadHookNameMatched.load());
	worker.ThreadCancelSynchronousIo();
	VDSetThreadInitHook(nullptr);

	VDCriticalSection criticalSection;
	criticalSection.Lock();
	criticalSection.Lock();
	criticalSection.Unlock();
	criticalSection.Unlock();
	int protectedCounter = 0;
	std::vector<std::thread> counterThreads;
	for(int threadIndex = 0; threadIndex < 4; ++threadIndex) {
		counterThreads.emplace_back([&criticalSection, &protectedCounter] {
			for(int index = 0; index < 1000; ++index) {
				vdsynchronized(criticalSection) {
					++protectedCounter;
				}
			}
		});
	}
	for(auto& thread : counterThreads)
		thread.join();
	AT_PORTABLE_TEST_ASSERT(context, protectedCounter == 4000);

	VDSignal automaticSignal;
	AT_PORTABLE_TEST_ASSERT(context, !automaticSignal.check());
	automaticSignal.signal();
	AT_PORTABLE_TEST_ASSERT(context, automaticSignal.check());
	AT_PORTABLE_TEST_ASSERT(context, !automaticSignal.check());
	automaticSignal.signal();
	automaticSignal.signal();
	AT_PORTABLE_TEST_ASSERT(context, automaticSignal.check());
	AT_PORTABLE_TEST_ASSERT(context, !automaticSignal.check());
	AT_PORTABLE_TEST_ASSERT(context, !automaticSignal.tryWait(10));

	VDSignal secondSignal;
	secondSignal.signal();
	AT_PORTABLE_TEST_ASSERT(context, automaticSignal.wait(&secondSignal) == 1);
	VDSignal thirdSignal;
	thirdSignal.signal();
	AT_PORTABLE_TEST_ASSERT(context,
		automaticSignal.wait(&secondSignal, &thirdSignal) == 2);
	const VDSignalBase *signalArray[] = {
		&automaticSignal, &secondSignal, &thirdSignal
	};
	secondSignal.signal();
	AT_PORTABLE_TEST_ASSERT(context,
		VDSignalBase::waitMultiple(signalArray, 3) == 1);

	std::thread delayedSignaler([&automaticSignal] {
		VDThreadSleep(10);
		automaticSignal.signal();
	});
	AT_PORTABLE_TEST_ASSERT(context, automaticSignal.tryWait(5000));
	delayedSignaler.join();

	VDSignalPersistent persistentSignal;
	AT_PORTABLE_TEST_ASSERT(context, !persistentSignal.check());
	persistentSignal.signal();
	AT_PORTABLE_TEST_ASSERT(context, persistentSignal.check());
	AT_PORTABLE_TEST_ASSERT(context, persistentSignal.check());
	persistentSignal.wait();
	persistentSignal.unsignal();
	AT_PORTABLE_TEST_ASSERT(context, !persistentSignal.check());

	VDSemaphore semaphore(2);
	AT_PORTABLE_TEST_ASSERT(context, semaphore.TryWait());
	AT_PORTABLE_TEST_ASSERT(context, semaphore.TryWait());
	AT_PORTABLE_TEST_ASSERT(context, !semaphore.TryWait());
	AT_PORTABLE_TEST_ASSERT(context, !semaphore.Wait(10));
	semaphore.Post();
	AT_PORTABLE_TEST_ASSERT(context, semaphore.Wait(100));
	semaphore.Reset(3);
	AT_PORTABLE_TEST_ASSERT(context, semaphore.TryWait());
	AT_PORTABLE_TEST_ASSERT(context, semaphore.TryWait());
	AT_PORTABLE_TEST_ASSERT(context, semaphore.TryWait());
	AT_PORTABLE_TEST_ASSERT(context, !semaphore.TryWait());
	std::thread semaphorePoster([&semaphore] {
		VDThreadSleep(10);
		semaphore.Post();
	});
	semaphore.Wait();
	semaphorePoster.join();

	VDRWLock conditionLock;
	VDConditionVariable condition;
	bool conditionReady = false;
	conditionLock.LockExclusive();
	std::thread notifier([&conditionLock, &condition, &conditionReady] {
		conditionLock.LockExclusive();
		conditionReady = true;
		conditionLock.UnlockExclusive();
		condition.NotifyOne();
	});
	while(!conditionReady)
		condition.Wait(conditionLock);
	conditionLock.UnlockExclusive();
	notifier.join();
	AT_PORTABLE_TEST_ASSERT(context, conditionReady);

	VDRWLock broadcastLock;
	VDConditionVariable broadcastCondition;
	int broadcastWaiters = 0;
	bool broadcastReady = false;
	std::atomic<int> broadcastWakeCount { 0 };
	auto broadcastWaiter = [&] {
		broadcastLock.LockExclusive();
		++broadcastWaiters;
		while(!broadcastReady)
			broadcastCondition.Wait(broadcastLock);
		broadcastLock.UnlockExclusive();
		++broadcastWakeCount;
	};
	std::thread firstWaiter(broadcastWaiter);
	std::thread secondWaiter(broadcastWaiter);
	for(;;) {
		broadcastLock.LockExclusive();
		const bool allWaiting = broadcastWaiters == 2;
		broadcastLock.UnlockExclusive();
		if (allWaiting)
			break;
		VDThreadSleep(1);
	}
	{
		vdsyncexclusive(broadcastLock) {
			broadcastReady = true;
		}
	}
	broadcastCondition.NotifyAll();
	firstWaiter.join();
	secondWaiter.join();
	AT_PORTABLE_TEST_ASSERT(context, broadcastWakeCount.load() == 2);

	return true;
}
