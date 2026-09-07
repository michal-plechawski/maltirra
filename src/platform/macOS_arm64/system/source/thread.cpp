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

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <exception>
#include <limits>
#include <mutex>
#include <new>

#include <errno.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>

#include <vd2/system/thread.h>
#include <vd2/system/tls.h>

namespace {
	struct VDMacThreadHandle {
		explicit VDMacThreadHandle(VDThread *thread)
			: mpThread(thread) {
		}

		VDThread *mpThread;
		pthread_t mThread {};
		std::atomic<bool> mbActive { true };
		std::mutex mStartMutex;
		std::condition_variable mStartCondition;
		bool mbStartReady = false;
	};

	void VDMacThreadCleanup(void *argument) {
		auto *handle = static_cast<VDMacThreadHandle *>(argument);
		handle->mbActive.store(false, std::memory_order_release);
	}

	void *VDMacThreadStartThunk(void *argument) {
		auto *handle = static_cast<VDMacThreadHandle *>(argument);

		pthread_cleanup_push(VDMacThreadCleanup, handle);
		{
			std::unique_lock lock(handle->mStartMutex);
			handle->mStartCondition.wait(lock, [handle] { return handle->mbStartReady; });
		}

		VDThread::StaticThreadStart(handle->mpThread);
		pthread_cleanup_pop(1);

		return nullptr;
	}

	struct VDMacSignal {
		explicit VDMacSignal(bool persistent)
			: mbPersistent(persistent) {
		}

		std::mutex mMutex;
		std::condition_variable mCondition;
		bool mbSignaled = false;
		const bool mbPersistent;
		uint64 mGeneration = 0;
	};

	struct VDMacSignalCoordinator {
		std::mutex mMutex;
		std::condition_variable mCondition;
		uint64 mGeneration = 0;
	};

	VDMacSignalCoordinator& VDGetSignalCoordinator() {
		static VDMacSignalCoordinator coordinator;
		return coordinator;
	}

	bool VDTryConsumeSignal(VDMacSignal& signal) {
		std::lock_guard lock(signal.mMutex);
		if (!signal.mbSignaled)
			return false;

		if (!signal.mbPersistent)
			signal.mbSignaled = false;

		return true;
	}

	struct VDMacSemaphore {
		explicit VDMacSemaphore(int count)
			: mCount(count > 0 ? count : 0) {
		}

		std::mutex mMutex;
		std::condition_variable mCondition;
		int mCount;
	};

	struct VDMacRWLock {
		std::mutex mMutex;
	};

	struct VDMacConditionVariable {
		std::condition_variable mCondition;
	};

	template<typename T>
	T *VDGetOrCreateNative(void *&storage) noexcept {
		static std::mutex initializationMutex;
		std::lock_guard lock(initializationMutex);

		if (!storage)
			storage = new(std::nothrow) T;

		if (!storage)
			std::terminate();

		return static_cast<T *>(storage);
	}

	void VDSetCurrentThreadDebugName(const char *name) {
		if (!name)
			return;

		char truncatedName[64];
		size_t length = 0;
		while(length + 1 < sizeof truncatedName && name[length]) {
			truncatedName[length] = name[length];
			++length;
		}
		truncatedName[length] = 0;

		pthread_setname_np(truncatedName);
	}
}

VDThreadID VDGetCurrentThreadID() {
	return (VDThreadID)pthread_mach_thread_np(pthread_self());
}

VDProcessId VDGetCurrentProcessId() {
	return (VDProcessId)getpid();
}

uint32 VDGetLogicalProcessorCount() {
	const long processorCount = sysconf(_SC_NPROCESSORS_ONLN);
	if (processorCount <= 0)
		return 1;

	return processorCount > (long)std::numeric_limits<uint32>::max()
		? std::numeric_limits<uint32>::max()
		: (uint32)processorCount;
}

void VDSetThreadDebugName(VDThreadID threadID, const char *name) {
	if (threadID == VDGetCurrentThreadID())
		VDSetCurrentThreadDebugName(name);
}

void VDThreadSleep(int milliseconds) {
	if (milliseconds <= 0)
		return;

	struct timespec delay {
		milliseconds / 1000,
		(long)(milliseconds % 1000) * 1000000L
	};

	while(nanosleep(&delay, &delay) && errno == EINTR)
		;
}

///////////////////////////////////////////////////////////////////////////////

VDThread::VDThread(const char *debugName)
	: mpszDebugName(debugName)
	, mhThread(nullptr)
	, mThreadID(0) {
}

VDThread::~VDThread() throw() {
	if (isThreadAttached())
		ThreadWait();
}

bool VDThread::ThreadStart() {
	VDASSERT(!isThreadAttached());
	if (isThreadAttached())
		return false;

	auto *handle = new(std::nothrow) VDMacThreadHandle(this);
	if (!handle)
		return false;

	mhThread = handle;
	const int result = pthread_create(
		&handle->mThread, nullptr, VDMacThreadStartThunk, handle);
	if (result) {
		mhThread = nullptr;
		delete handle;
		return false;
	}

	mThreadID = (VDThreadID)pthread_mach_thread_np(handle->mThread);
	{
		std::lock_guard lock(handle->mStartMutex);
		handle->mbStartReady = true;
	}
	handle->mStartCondition.notify_one();
	return true;
}

void VDThread::ThreadDetach() {
	if (!isThreadAttached())
		return;

	auto *handle = static_cast<VDMacThreadHandle *>(mhThread);
	pthread_join(handle->mThread, nullptr);
	delete handle;
	mhThread = nullptr;
	mThreadID = 0;
}

void VDThread::ThreadWait() {
	ThreadDetach();
}

void VDThread::ThreadCancelSynchronousIo() {
	if (isThreadAttached()) {
		auto *handle = static_cast<VDMacThreadHandle *>(mhThread);
		pthread_cancel(handle->mThread);
	}
}

bool VDThread::isThreadActive() {
	if (!isThreadAttached())
		return false;

	auto *handle = static_cast<VDMacThreadHandle *>(mhThread);
	if (handle->mbActive.load(std::memory_order_acquire))
		return true;

	ThreadDetach();
	return false;
}

bool VDThread::IsCurrentThread() const {
	return mThreadID && VDGetCurrentThreadID() == mThreadID;
}

unsigned VDThread::StaticThreadStart(void *threadAsVoid) {
	auto *thread = static_cast<VDThread *>(threadAsVoid);
	if (thread->mpszDebugName)
		VDSetCurrentThreadDebugName(thread->mpszDebugName);

	VDInitThreadData(thread->mpszDebugName);
	struct ThreadDataScope {
		~ThreadDataScope() {
			VDDeinitThreadData();
		}
	} threadDataScope;

	thread->ThreadRun();
	return 0;
}

///////////////////////////////////////////////////////////////////////////////

VDCriticalSection::VDCriticalSection() {
	static_assert(sizeof(mNativeStorage) >= sizeof(void *));
	auto **storage = reinterpret_cast<std::recursive_mutex **>(mNativeStorage);
	*storage = new std::recursive_mutex;
}

VDCriticalSection::~VDCriticalSection() {
	auto **storage = reinterpret_cast<std::recursive_mutex **>(mNativeStorage);
	delete *storage;
	*storage = nullptr;
}

void VDCriticalSection::operator++() {
	Lock();
}

void VDCriticalSection::operator--() {
	Unlock();
}

void VDCriticalSection::Lock() {
	auto *mutex = *reinterpret_cast<std::recursive_mutex **>(mNativeStorage);
	mutex->lock();
}

void VDCriticalSection::Unlock() {
	auto *mutex = *reinterpret_cast<std::recursive_mutex **>(mNativeStorage);
	mutex->unlock();
}

///////////////////////////////////////////////////////////////////////////////

VDSignal::VDSignal() {
	hEvent = new VDMacSignal(false);
}

VDSignalPersistent::VDSignalPersistent() {
	hEvent = new VDMacSignal(true);
}

VDSignalBase::~VDSignalBase() {
	delete static_cast<VDMacSignal *>(hEvent);
}

void VDSignalBase::signal() {
	auto& signal = *static_cast<VDMacSignal *>(hEvent);
	{
		std::lock_guard lock(signal.mMutex);
		signal.mbSignaled = true;
		++signal.mGeneration;
	}

	if (signal.mbPersistent)
		signal.mCondition.notify_all();
	else
		signal.mCondition.notify_one();

	auto& coordinator = VDGetSignalCoordinator();
	{
		std::lock_guard lock(coordinator.mMutex);
		++coordinator.mGeneration;
	}
	coordinator.mCondition.notify_all();
}

void VDSignalBase::wait() {
	auto& signal = *static_cast<VDMacSignal *>(hEvent);
	std::unique_lock lock(signal.mMutex);

	if (signal.mbPersistent) {
		const uint64 generation = signal.mGeneration;
		signal.mCondition.wait(lock, [&signal, generation] {
			return signal.mbSignaled || signal.mGeneration != generation;
		});
	} else {
		signal.mCondition.wait(lock, [&signal] { return signal.mbSignaled; });
		signal.mbSignaled = false;
	}
}

bool VDSignalBase::check() {
	return VDTryConsumeSignal(*static_cast<VDMacSignal *>(hEvent));
}

int VDSignalBase::wait(VDSignalBase *second) {
	const VDSignalBase *signals[] = { this, second };
	return waitMultiple(signals, 2);
}

int VDSignalBase::wait(VDSignalBase *second, VDSignalBase *third) {
	const VDSignalBase *signals[] = { this, second, third };
	return waitMultiple(signals, 3);
}

int VDSignalBase::waitMultiple(const VDSignalBase *const *signals, int count) {
	if (!signals || count <= 0)
		return -1;

	auto& coordinator = VDGetSignalCoordinator();
	uint64 observedGeneration;
	{
		std::lock_guard lock(coordinator.mMutex);
		observedGeneration = coordinator.mGeneration;
	}

	for(;;) {
		for(int index = 0; index < count; ++index) {
			if (signals[index] && signals[index]->hEvent
				&& VDTryConsumeSignal(
					*static_cast<VDMacSignal *>(signals[index]->hEvent)))
			{
				return index;
			}
		}

		std::unique_lock lock(coordinator.mMutex);
		coordinator.mCondition.wait(lock, [&coordinator, &observedGeneration] {
			return coordinator.mGeneration != observedGeneration;
		});
		observedGeneration = coordinator.mGeneration;
	}
}

bool VDSignalBase::tryWait(uint32 timeoutMilliseconds) {
	auto& signal = *static_cast<VDMacSignal *>(hEvent);
	std::unique_lock lock(signal.mMutex);

	bool ready;
	if (signal.mbPersistent) {
		const uint64 generation = signal.mGeneration;
		ready = signal.mCondition.wait_for(
			lock,
			std::chrono::milliseconds(timeoutMilliseconds),
			[&signal, generation] {
				return signal.mbSignaled || signal.mGeneration != generation;
			});
	} else {
		ready = signal.mCondition.wait_for(
			lock,
			std::chrono::milliseconds(timeoutMilliseconds),
			[&signal] { return signal.mbSignaled; });
		if (ready)
			signal.mbSignaled = false;
	}

	return ready;
}

void VDSignalPersistent::unsignal() {
	auto& signal = *static_cast<VDMacSignal *>(hEvent);
	std::lock_guard lock(signal.mMutex);
	signal.mbSignaled = false;
}

///////////////////////////////////////////////////////////////////////////////

VDSemaphore::VDSemaphore(int initial)
	: mKernelSema(new VDMacSemaphore(initial)) {
}

VDSemaphore::~VDSemaphore() {
	delete static_cast<VDMacSemaphore *>(mKernelSema);
}

void VDSemaphore::Reset(int count) {
	auto& semaphore = *static_cast<VDMacSemaphore *>(mKernelSema);
	{
		std::lock_guard lock(semaphore.mMutex);
		semaphore.mCount = count > 0 ? count : 0;
	}

	if (count > 0)
		semaphore.mCondition.notify_all();
}

void VDSemaphore::Wait() {
	auto& semaphore = *static_cast<VDMacSemaphore *>(mKernelSema);
	std::unique_lock lock(semaphore.mMutex);
	semaphore.mCondition.wait(lock, [&semaphore] { return semaphore.mCount > 0; });
	--semaphore.mCount;
}

bool VDSemaphore::Wait(int timeout) {
	if (timeout < 0) {
		Wait();
		return true;
	}

	auto& semaphore = *static_cast<VDMacSemaphore *>(mKernelSema);
	std::unique_lock lock(semaphore.mMutex);
	if (!semaphore.mCondition.wait_for(
		lock,
		std::chrono::milliseconds(timeout),
		[&semaphore] { return semaphore.mCount > 0; }))
	{
		return false;
	}

	--semaphore.mCount;
	return true;
}

bool VDSemaphore::TryWait() {
	auto& semaphore = *static_cast<VDMacSemaphore *>(mKernelSema);
	std::lock_guard lock(semaphore.mMutex);
	if (semaphore.mCount <= 0)
		return false;

	--semaphore.mCount;
	return true;
}

void VDSemaphore::Post() {
	auto& semaphore = *static_cast<VDMacSemaphore *>(mKernelSema);
	{
		std::lock_guard lock(semaphore.mMutex);
		if (semaphore.mCount < 0x0fffffff)
			++semaphore.mCount;
	}
	semaphore.mCondition.notify_one();
}

///////////////////////////////////////////////////////////////////////////////

VDRWLock::~VDRWLock() {
	delete static_cast<VDMacRWLock *>(mpSRWLock);
}

void VDRWLock::LockExclusive() noexcept {
	VDGetOrCreateNative<VDMacRWLock>(mpSRWLock)->mMutex.lock();
}

void VDRWLock::UnlockExclusive() noexcept {
	VDGetOrCreateNative<VDMacRWLock>(mpSRWLock)->mMutex.unlock();
}

///////////////////////////////////////////////////////////////////////////////

VDConditionVariable::~VDConditionVariable() {
	delete static_cast<VDMacConditionVariable *>(mpCondVar);
}

void VDConditionVariable::Wait(VDRWLock& rwLock) noexcept {
	auto *condition = VDGetOrCreateNative<VDMacConditionVariable>(mpCondVar);
	auto *lock = VDGetOrCreateNative<VDMacRWLock>(rwLock.mpSRWLock);
	std::unique_lock nativeLock(lock->mMutex, std::adopt_lock);
	condition->mCondition.wait(nativeLock);
	nativeLock.release();
}

void VDConditionVariable::NotifyOne() noexcept {
	VDGetOrCreateNative<VDMacConditionVariable>(mpCondVar)->mCondition.notify_one();
}

void VDConditionVariable::NotifyAll() noexcept {
	VDGetOrCreateNative<VDMacConditionVariable>(mpCondVar)->mCondition.notify_all();
}
