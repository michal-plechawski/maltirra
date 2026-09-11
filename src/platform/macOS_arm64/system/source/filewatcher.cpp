// Altirra filesystem change monitoring for macOS ARM64

#include <atomic>
#include <chrono>
#include <cstring>
#include <mutex>
#include <string>

#include <CoreServices/CoreServices.h>
#include <dispatch/dispatch.h>
#include <pthread.h>

#include <vd2/system/Error.h>
#include <vd2/system/filewatcher.h>
#include <vd2/system/filesys.h>
#include <vd2/system/text.h>

struct VDFileWatcherState {
	FSEventStreamRef mpStream = nullptr;
	dispatch_queue_t mpEventQueue = nullptr;
	dispatch_semaphore_t mpEventSemaphore = nullptr;
	dispatch_source_t mpCallbackTimer = nullptr;
	std::atomic<uint64> mEventGeneration { 0 };
	std::atomic<bool> mbActive { false };
	std::mutex mChangeMutex;
	uint64 mWaitGeneration = 0;
	uint64 mCallbackGeneration = 0;
	uint64 mLastWriteTime = 0;
	bool mbStreamStarted = false;
	bool mbWatchDir = false;
	bool mbWatchSubdirs = false;
	bool mbRepeatRequested = false;
	std::string mWatchRoot;
	std::string mTargetPath;
	VDStringW mPath;
	IVDFileWatcherCallback *mpCallback = nullptr;
};

namespace {
	char gVDFileWatcherQueueKey;

	void VDDispatchNoop(void *) {
	}

	void VDTrimTrailingSeparators(std::string& path) {
		while(path.size() > 1 && path.back() == '/')
			path.pop_back();
	}

	bool VDPathIsEqualOrChild(const std::string& root, const char *path) {
		const size_t rootLength = root.size();
		if (strncmp(root.c_str(), path, rootLength))
			return false;
		return !path[rootLength]
			|| root == "/"
			|| path[rootLength] == '/';
	}

	bool VDIsRelevantEvent(
		const VDFileWatcherState& state,
		const char *path,
		FSEventStreamEventFlags flags) {
		if (flags & (kFSEventStreamEventFlagMustScanSubDirs
			| kFSEventStreamEventFlagUserDropped
			| kFSEventStreamEventFlagKernelDropped
			| kFSEventStreamEventFlagRootChanged))
			return true;

		if (!state.mbWatchDir)
			return state.mTargetPath == path;

		if (!VDPathIsEqualOrChild(state.mWatchRoot, path))
			return false;
		if (state.mbWatchSubdirs || state.mWatchRoot == path)
			return true;

		const char *relativePath = path + state.mWatchRoot.size();
		if (*relativePath == '/')
			++relativePath;
		return !strchr(relativePath, '/');
	}

	void VDFileWatcherEventCallback(
		ConstFSEventStreamRef,
		void *context,
		size_t eventCount,
		void *eventPaths,
		const FSEventStreamEventFlags eventFlags[],
		const FSEventStreamEventId[]) {
		auto *state = static_cast<VDFileWatcherState *>(context);
		if (!state->mbActive.load(std::memory_order_acquire))
			return;

		auto **paths = static_cast<char **>(eventPaths);
		for(size_t index = 0; index < eventCount; ++index) {
			if (VDIsRelevantEvent(*state, paths[index], eventFlags[index])) {
				state->mEventGeneration.fetch_add(1, std::memory_order_release);
				dispatch_semaphore_signal(state->mpEventSemaphore);
				break;
			}
		}
	}

	bool VDFileWatcherHasChanged(VDFileWatcherState& state) {
		if (state.mbWatchDir)
			return true;

		std::lock_guard<std::mutex> lock(state.mChangeMutex);
		const uint64 lastWriteTime = VDFileGetLastWriteTime(state.mPath.c_str());
		if (lastWriteTime == state.mLastWriteTime)
			return false;
		state.mLastWriteTime = lastWriteTime;
		return true;
	}

	void VDFileWatcherTimerCallback(void *context) {
		auto *state = static_cast<VDFileWatcherState *>(context);
		if (!state->mbActive.load(std::memory_order_acquire) || !state->mpCallback)
			return;

		if (state->mbRepeatRequested) {
			state->mbRepeatRequested = !state->mpCallback->OnFileUpdated(state->mPath.c_str());
			return;
		}

		const uint64 generation = state->mEventGeneration.load(std::memory_order_acquire);
		if (generation == state->mCallbackGeneration)
			return;
		state->mCallbackGeneration = generation;
		if (VDFileWatcherHasChanged(*state))
			state->mbRepeatRequested = !state->mpCallback->OnFileUpdated(state->mPath.c_str());
	}

	void VDDestroyFileWatcherState(VDFileWatcherState *state) {
		if (!state)
			return;

		state->mbActive.store(false, std::memory_order_release);
		if (state->mpCallbackTimer) {
			dispatch_source_cancel(state->mpCallbackTimer);
			if (!pthread_main_np())
				dispatch_sync_f(dispatch_get_main_queue(), nullptr, VDDispatchNoop);
			dispatch_release(state->mpCallbackTimer);
		}

		if (state->mpStream && state->mbStreamStarted)
			FSEventStreamStop(state->mpStream);
		if (state->mpStream)
			FSEventStreamInvalidate(state->mpStream);
		if (state->mpEventQueue
			&& dispatch_get_specific(&gVDFileWatcherQueueKey) != state)
			dispatch_sync_f(state->mpEventQueue, nullptr, VDDispatchNoop);
		if (state->mpStream)
			FSEventStreamRelease(state->mpStream);
		if (state->mpEventSemaphore)
			dispatch_release(state->mpEventSemaphore);
		if (state->mpEventQueue)
			dispatch_release(state->mpEventQueue);
		delete state;
	}

	[[noreturn]] void VDThrowFileWatcherError(
		const wchar_t *path,
		bool directory,
		const wchar_t *detail) {
		throw VDException(
			directory
				? L"Unable to monitor path: %ls (%ls)"
				: L"Unable to monitor file: %ls (%ls)",
			path ? path : L"",
			detail);
	}

	VDFileWatcherState *VDCreateFileWatcherState(
		const wchar_t *path,
		bool directory,
		bool subdirs,
		IVDFileWatcherCallback *callback) {
		if (!path || !*path)
			VDThrowFileWatcherError(path, directory, L"empty path");

		const VDStringW fullPath = VDGetFullPath(path);
		const VDStringW watchRoot = directory ? fullPath : VDFileSplitPathLeft(fullPath);
		const uint32 rootAttributes = VDFileGetAttributes(watchRoot.c_str());
		if (rootAttributes == kVDFileAttr_Invalid || !(rootAttributes & kVDFileAttr_Directory))
			VDThrowFileWatcherError(path, directory, L"watch root is not a directory");

		auto *state = new VDFileWatcherState;
		state->mbWatchDir = directory;
		state->mbWatchSubdirs = subdirs;
		state->mPath = fullPath;
		state->mpCallback = callback;
		state->mLastWriteTime = directory ? 0 : VDFileGetLastWriteTime(fullPath.c_str());
		state->mWatchRoot = VDTextWToU8(watchRoot.c_str(), -1).c_str();
		state->mTargetPath = directory ? std::string() : VDTextWToU8(fullPath.c_str(), -1).c_str();
		VDTrimTrailingSeparators(state->mWatchRoot);
		VDTrimTrailingSeparators(state->mTargetPath);

		state->mpEventSemaphore = dispatch_semaphore_create(0);
		state->mpEventQueue = dispatch_queue_create(
			"com.virtualdub.altirra.filewatcher",
			DISPATCH_QUEUE_SERIAL);
		if (!state->mpEventSemaphore || !state->mpEventQueue) {
			VDDestroyFileWatcherState(state);
			VDThrowFileWatcherError(path, directory, L"dispatch setup failed");
		}
		dispatch_queue_set_specific(
			state->mpEventQueue,
			&gVDFileWatcherQueueKey,
			state,
			nullptr);

		CFStringRef rootString = CFStringCreateWithCString(
			kCFAllocatorDefault,
			state->mWatchRoot.c_str(),
			kCFStringEncodingUTF8);
		const void *rootValue = rootString;
		CFArrayRef roots = rootString
			? CFArrayCreate(kCFAllocatorDefault, &rootValue, 1, &kCFTypeArrayCallBacks)
			: nullptr;
		if (roots) {
			FSEventStreamContext streamContext { 0, state, nullptr, nullptr, nullptr };
			state->mpStream = FSEventStreamCreate(
				kCFAllocatorDefault,
				VDFileWatcherEventCallback,
				&streamContext,
				roots,
				kFSEventStreamEventIdSinceNow,
				0.05,
				kFSEventStreamCreateFlagFileEvents
					| kFSEventStreamCreateFlagNoDefer
					| kFSEventStreamCreateFlagWatchRoot);
		}
		if (roots)
			CFRelease(roots);
		if (rootString)
			CFRelease(rootString);

		if (!state->mpStream) {
			VDDestroyFileWatcherState(state);
			VDThrowFileWatcherError(path, directory, L"FSEvent stream creation failed");
		}

		state->mbActive.store(true, std::memory_order_release);
		FSEventStreamSetDispatchQueue(state->mpStream, state->mpEventQueue);
		if (!FSEventStreamStart(state->mpStream)) {
			VDDestroyFileWatcherState(state);
			VDThrowFileWatcherError(path, directory, L"FSEvent stream start failed");
		}
		state->mbStreamStarted = true;

		if (callback) {
			state->mpCallbackTimer = dispatch_source_create(
				DISPATCH_SOURCE_TYPE_TIMER,
				0,
				0,
				dispatch_get_main_queue());
			if (!state->mpCallbackTimer) {
				VDDestroyFileWatcherState(state);
				VDThrowFileWatcherError(path, directory, L"callback timer creation failed");
			}
			dispatch_set_context(state->mpCallbackTimer, state);
			dispatch_source_set_event_handler_f(state->mpCallbackTimer, VDFileWatcherTimerCallback);
			dispatch_source_set_timer(
				state->mpCallbackTimer,
				dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC),
				NSEC_PER_SEC,
				NSEC_PER_MSEC * 50);
			dispatch_resume(state->mpCallbackTimer);
		}

		return state;
	}
}

VDFileWatcher::VDFileWatcher()
	: mpState(nullptr) {
}

VDFileWatcher::~VDFileWatcher() {
	Shutdown();
}

bool VDFileWatcher::IsActive() const {
	return mpState && mpState->mbActive.load(std::memory_order_acquire);
}

void VDFileWatcher::Init(const wchar_t *file, IVDFileWatcherCallback *callback) {
	Shutdown();
	mpState = VDCreateFileWatcherState(file, false, false, callback);
}

void VDFileWatcher::InitDir(
	const wchar_t *path,
	bool subdirs,
	IVDFileWatcherCallback *callback) {
	Shutdown();
	mpState = VDCreateFileWatcherState(path, true, subdirs, callback);
}

void VDFileWatcher::Shutdown() {
	VDFileWatcherState *state = mpState;
	mpState = nullptr;
	VDDestroyFileWatcherState(state);
}

bool VDFileWatcher::Wait(uint32 delay) {
	VDFileWatcherState *state = mpState;
	if (!state || !state->mbActive.load(std::memory_order_acquire))
		return false;

	const bool infinite = delay == 0xFFFFFFFFU;
	const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(delay);
	for(;;) {
		const uint64 generation = state->mEventGeneration.load(std::memory_order_acquire);
		if (generation != state->mWaitGeneration) {
			state->mWaitGeneration = generation;
			if (VDFileWatcherHasChanged(*state))
				return true;
		}

		dispatch_time_t timeout = DISPATCH_TIME_FOREVER;
		if (!infinite) {
			const auto now = std::chrono::steady_clock::now();
			if (now >= deadline)
				return false;
			const auto remaining = std::chrono::duration_cast<std::chrono::nanoseconds>(deadline - now);
			timeout = dispatch_time(DISPATCH_TIME_NOW, remaining.count());
		}

		if (dispatch_semaphore_wait(state->mpEventSemaphore, timeout))
			return false;
	}
}
