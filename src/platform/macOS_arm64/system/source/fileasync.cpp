// VirtualDub/Altirra asynchronous file services for macOS

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <condition_variable>
#include <cstring>
#include <deque>
#include <exception>
#include <limits>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <vd2/system/Error.h>
#include <vd2/system/fileasync.h>
#include <vd2/system/text.h>
#include <vd2/system/VDString.h>

namespace {
	[[noreturn]] void VDThrowAsyncFileError(
		const char *operation,
		const std::string& filename,
		int errorCode) {
		throw MyError(
			"Unable to %s file \"%s\": %s",
			operation,
			filename.c_str(),
			strerror(errorCode));
	}

	int VDFileHandleToDescriptor(VDFileHandle handle) {
		return handle
			? static_cast<int>(reinterpret_cast<sintptr>(handle) - 1)
			: -1;
	}

	struct VDFileAsyncWriteRequest {
		sint64 mPosition = 0;
		std::vector<uint8> mData;
	};

	class VDFileAsyncMac final : public IVDFileAsync {
	public:
		explicit VDFileAsyncMac(Mode mode)
			: mMode(mode >= kModeSynchronous && mode < kModeCount ? mode : kModeSynchronous)
			, mbBackground(mMode == kModeThreaded || mMode == kModeAsynchronous)
			, mbSyncWrites(mMode == kModeSynchronous)
			, mbSyncOnDrain(mMode != kModeBuffered) {
		}

		~VDFileAsyncMac() override {
			Close();
		}

		void SetPreemptiveExtend(bool enabled) override {
			mbPreemptiveExtend.store(enabled, std::memory_order_relaxed);
		}

		bool IsPreemptiveExtendActive() override {
			return mbPreemptiveExtend.load(std::memory_order_relaxed);
		}

		bool IsOpen() override {
			return mFileDescriptor >= 0;
		}

		void Open(const wchar_t *filename, uint32 count, uint32 bufferSize) override {
			Close();

			const VDStringA filenameUTF8 = VDTextWToU8(filename ? filename : L"", -1);
			const int creationFlags = mMode == kModeAsynchronous ? 0 : O_TRUNC;
			int fileDescriptor;
			do {
				fileDescriptor = open(
					filenameUTF8.c_str(),
					O_WRONLY | O_CREAT | O_CLOEXEC | creationFlags,
					0666);
			} while(fileDescriptor < 0 && errno == EINTR);

			if (fileDescriptor < 0)
				VDThrowAsyncFileError("open", filenameUTF8.c_str(), errno);

			try {
				OpenDescriptor(fileDescriptor, filenameUTF8.c_str(), count, bufferSize);
			} catch(...) {
				close(fileDescriptor);
				throw;
			}
		}

		void Open(VDFileHandle handle, uint32 count, uint32 bufferSize) override {
			Close();

			const int sourceDescriptor = VDFileHandleToDescriptor(handle);
			int fileDescriptor;
			do {
				fileDescriptor = sourceDescriptor >= 0
					? fcntl(sourceDescriptor, F_DUPFD_CLOEXEC, 0)
					: -1;
			} while(fileDescriptor < 0 && errno == EINTR);

			if (fileDescriptor < 0)
				VDThrowAsyncFileError("duplicate", "<file handle>", sourceDescriptor < 0 ? EBADF : errno);

			try {
				OpenDescriptor(fileDescriptor, "<file handle>", count, bufferSize);
			} catch(...) {
				close(fileDescriptor);
				throw;
			}
		}

		void Close() override {
			if (mWorker.joinable()) {
				{
					std::lock_guard lock(mMutex);
					mbStopping = true;
					mRequests.clear();
					mQueuedBytes = 0;
				}
				mWorkAvailable.notify_all();
				mSpaceAvailable.notify_all();
				mDrained.notify_all();
				mWorker.join();
			}

			if (mFileDescriptor >= 0) {
				close(mFileDescriptor);
				mFileDescriptor = -1;
			}

			std::lock_guard lock(mMutex);
			mRequests.clear();
			mQueuedBytes = 0;
			mbWorkerWriting = false;
			mbStopping = false;
			mWorkerError = nullptr;
			mClientFastPointer = 0;
		}

		void FastWrite(const void *data, uint32 bytes) override {
			EnsureOpen();
			if (!bytes)
				return;
			if (mClientFastPointer < 0
				|| static_cast<uint64>(mClientFastPointer) + bytes
					> static_cast<uint64>((std::numeric_limits<sint64>::max)()))
				VDThrowAsyncFileError("write", mFilename, EINVAL);

			if (!mbBackground) {
				WriteDirect(mClientFastPointer, data, bytes);
				mClientFastPointer += bytes;
				return;
			}

			uint32 offset = 0;
			while(offset < bytes) {
				const uint32 requestSize = std::min(mBlockSize, bytes - offset);
				VDFileAsyncWriteRequest request;
				request.mPosition = mClientFastPointer + offset;
				request.mData.resize(requestSize);
				if (data)
					memcpy(request.mData.data(), static_cast<const uint8 *>(data) + offset, requestSize);
				else
					memset(request.mData.data(), 0, requestSize);

				std::unique_lock lock(mMutex);
				mSpaceAvailable.wait(lock, [&] {
					return mWorkerError
						|| mbStopping
						|| mQueuedBytes + requestSize <= mBufferCapacity;
				});
				RethrowWorkerErrorLocked();
				if (mbStopping)
					VDThrowAsyncFileError("write", mFilename, ECANCELED);

				mQueuedBytes += requestSize;
				mRequests.push_back(std::move(request));
				lock.unlock();
				mWorkAvailable.notify_one();
				offset += requestSize;
			}

			mClientFastPointer += bytes;
		}

		void FastWriteEnd() override {
			EnsureOpen();
			if (mbBackground)
				DrainWorker();
			else if (mbSyncOnDrain)
				SyncFile();
		}

		void Write(sint64 position, const void *data, uint32 bytes) override {
			EnsureOpen();
			CheckWorkerError();
			if (bytes && !data)
				VDThrowAsyncFileError("write", mFilename, EINVAL);
			WriteAllAt(position, data, bytes);
			if (mbSyncWrites)
				SyncFile();
		}

		bool Extend(sint64 position) override {
			if (mFileDescriptor < 0 || position < 0)
				return false;

			int result;
			do {
				result = ftruncate(mFileDescriptor, static_cast<off_t>(position));
			} while(result && errno == EINTR);
			return !result;
		}

		void Truncate(sint64 position) override {
			EnsureOpen();
			if (!Extend(position))
				VDThrowAsyncFileError("truncate", mFilename, position < 0 ? EINVAL : errno);
		}

		void SafeTruncateAndClose(sint64 position) override {
			if (!IsOpen())
				return;

			try {
				FastWriteEnd();
				Truncate(position);
				Close();
			} catch(...) {
				Close();
				throw;
			}
		}

		sint64 GetFastWritePos() override {
			return mClientFastPointer;
		}

		sint64 GetSize() override {
			EnsureOpen();
			struct stat info {};
			if (fstat(mFileDescriptor, &info))
				VDThrowAsyncFileError("query size of", mFilename, errno);
			return static_cast<sint64>(info.st_size);
		}

	private:
		void OpenDescriptor(
			int fileDescriptor,
			const std::string& filename,
			uint32 count,
			uint32 bufferSize) {
			if (!count || !bufferSize
				|| static_cast<size_t>(count)
					> (std::numeric_limits<size_t>::max)() / bufferSize
				|| static_cast<size_t>(count) * bufferSize
					> static_cast<size_t>((std::numeric_limits<sint64>::max)()))
				VDThrowAsyncFileError("configure", filename, EINVAL);

			std::string filenameCopy(filename);
			mFilename = std::move(filenameCopy);
			mBlockSize = bufferSize;
			mBufferCapacity = static_cast<size_t>(count) * bufferSize;
			mClientFastPointer = 0;
			mFileDescriptor = fileDescriptor;

			if (mbBackground) {
				(void)fcntl(mFileDescriptor, F_NOCACHE, 1);
				try {
					mWorker = std::thread(&VDFileAsyncMac::WorkerMain, this);
				} catch(...) {
					mFileDescriptor = -1;
					throw;
				}
			}
		}

		void EnsureOpen() const {
			if (mFileDescriptor < 0)
				VDThrowAsyncFileError("access", mFilename, EBADF);
		}

		void WriteDirect(sint64 position, const void *data, uint32 bytes) {
			if (data) {
				WriteAllAt(position, data, bytes);
			} else {
				static constexpr size_t kZeroBufferSize = 64 * 1024;
				const std::vector<uint8> zeroBuffer(kZeroBufferSize);
				uint32 offset = 0;
				while(offset < bytes) {
					const uint32 chunk = std::min<uint32>(bytes - offset, kZeroBufferSize);
					WriteAllAt(position + offset, zeroBuffer.data(), chunk);
					offset += chunk;
				}
			}

			if (mbSyncWrites)
				SyncFile();
		}

		void WriteAllAt(sint64 position, const void *data, size_t bytes) {
			if (position < 0
				|| static_cast<uint64>(position) + bytes
					> static_cast<uint64>((std::numeric_limits<off_t>::max)()))
				VDThrowAsyncFileError("write", mFilename, EINVAL);

			const uint8 *source = static_cast<const uint8 *>(data);
			size_t offset = 0;
			while(offset < bytes) {
				ssize_t actual;
				do {
					actual = pwrite(
						mFileDescriptor,
						source + offset,
						bytes - offset,
						static_cast<off_t>(static_cast<uint64>(position) + offset));
				} while(actual < 0 && errno == EINTR);

				if (actual <= 0)
					VDThrowAsyncFileError("write", mFilename, actual ? errno : EIO);
				offset += static_cast<size_t>(actual);
			}
		}

		void SyncFile() {
			int result;
			do {
				result = fsync(mFileDescriptor);
			} while(result && errno == EINTR);
			if (result)
				VDThrowAsyncFileError("flush", mFilename, errno);
		}

		void CheckWorkerError() {
			std::lock_guard lock(mMutex);
			RethrowWorkerErrorLocked();
		}

		void RethrowWorkerErrorLocked() {
			if (mWorkerError)
				std::rethrow_exception(mWorkerError);
		}

		void DrainWorker() {
			std::unique_lock lock(mMutex);
			mDrained.wait(lock, [&] {
				return mWorkerError || (mRequests.empty() && !mbWorkerWriting);
			});
			RethrowWorkerErrorLocked();
			lock.unlock();

			if (mbSyncOnDrain)
				SyncFile();
		}

		void WorkerMain() noexcept {
			for(;;) {
				VDFileAsyncWriteRequest request;
				{
					std::unique_lock lock(mMutex);
					mWorkAvailable.wait(lock, [&] {
						return mbStopping || !mRequests.empty();
					});
					if (mbStopping)
						break;

					request = std::move(mRequests.front());
					mRequests.pop_front();
					mQueuedBytes -= request.mData.size();
					mbWorkerWriting = true;
				}
				mSpaceAvailable.notify_all();

				try {
					PreemptivelyExtend(request);
					WriteAllAt(request.mPosition, request.mData.data(), request.mData.size());
				} catch(...) {
					std::lock_guard lock(mMutex);
					mWorkerError = std::current_exception();
					mRequests.clear();
					mQueuedBytes = 0;
					mbWorkerWriting = false;
					mbStopping = true;
					mSpaceAvailable.notify_all();
					mDrained.notify_all();
					return;
				}

				{
					std::lock_guard lock(mMutex);
					mbWorkerWriting = false;
					if (mRequests.empty())
						mDrained.notify_all();
				}
			}
		}

		void PreemptivelyExtend(const VDFileAsyncWriteRequest& request) {
			if (!mbPreemptiveExtend.load(std::memory_order_relaxed))
				return;

			struct stat info {};
			const uint64 maximumPosition =
				static_cast<uint64>((std::numeric_limits<off_t>::max)());
			const uint64 position = static_cast<uint64>(request.mPosition);
			if (position > maximumPosition
				|| request.mData.size() > maximumPosition - position
				|| mBufferCapacity
					> maximumPosition - position - request.mData.size()) {
				mbPreemptiveExtend.store(false, std::memory_order_relaxed);
				return;
			}
			const uint64 target = position + request.mData.size() + mBufferCapacity;

			if (fstat(mFileDescriptor, &info)
				|| (static_cast<uint64>(info.st_size) < target
					&& ftruncate(mFileDescriptor, static_cast<off_t>(target))))
				mbPreemptiveExtend.store(false, std::memory_order_relaxed);
		}

		const Mode mMode;
		const bool mbBackground;
		const bool mbSyncWrites;
		const bool mbSyncOnDrain;
		std::atomic<bool> mbPreemptiveExtend { false };
		int mFileDescriptor = -1;
		std::string mFilename;
		uint32 mBlockSize = 0;
		size_t mBufferCapacity = 0;
		sint64 mClientFastPointer = 0;

		std::thread mWorker;
		std::mutex mMutex;
		std::condition_variable mWorkAvailable;
		std::condition_variable mSpaceAvailable;
		std::condition_variable mDrained;
		std::deque<VDFileAsyncWriteRequest> mRequests;
		size_t mQueuedBytes = 0;
		bool mbWorkerWriting = false;
		bool mbStopping = false;
		std::exception_ptr mWorkerError;
	};
}

IVDFileAsync *VDCreateFileAsync(IVDFileAsync::Mode mode) {
	return new VDFileAsyncMac(mode);
}
