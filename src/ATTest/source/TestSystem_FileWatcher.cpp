// Altirra portable filesystem watcher tests

#include <cstring>

#include <vd2/system/Error.h>
#include <vd2/system/file.h>
#include <vd2/system/filesys.h>
#include <vd2/system/filewatcher.h>
#include <vd2/system/thread.h>
#include <vd2/system/time.h>

#include <at/attest/portabletest.h>

#if defined(__APPLE__)
#include <CoreFoundation/CoreFoundation.h>
#endif

namespace {
	class VDFileWatcherTestSandbox {
	public:
		explicit VDFileWatcherTestSandbox(const VDStringW& path)
			: mPath(path)
			, mFilePath(VDMakePath(path.c_str(), L"watched.txt"))
			, mCreatedPath(VDMakePath(path.c_str(), L"created.txt"))
			, mNestedPath(VDMakePath(path.c_str(), L"nested"))
			, mNestedFilePath(VDMakePath(mNestedPath.c_str(), L"nested.txt")) {
		}

		~VDFileWatcherTestSandbox() {
			VDRemoveFile(mNestedFilePath.c_str());
			try {
				VDRemoveDirectory(mNestedPath.c_str());
			} catch(...) {
			}
			VDRemoveFile(mCreatedPath.c_str());
			VDRemoveFile(mFilePath.c_str());
			try {
				VDRemoveDirectory(mPath.c_str());
			} catch(...) {
			}
		}

		VDStringW mPath;
		VDStringW mFilePath;
		VDStringW mCreatedPath;
		VDStringW mNestedPath;
		VDStringW mNestedFilePath;
	};

	void VDWriteWatcherTestFile(const wchar_t *path, const char *text) {
		VDFileStream stream(path, nsVDFile::kWrite | nsVDFile::kCreateAlways);
		stream.Write(text, static_cast<sint32>(strlen(text)));
	}

	bool VDInitDirFails(VDFileWatcher& watcher, const wchar_t *path) {
		try {
			watcher.InitDir(path, false, nullptr);
		} catch(const VDException&) {
			return true;
		}
		return false;
	}

#if defined(__APPLE__)
	class VDFileWatcherTestCallback final : public IVDFileWatcherCallback {
	public:
		bool OnFileUpdated(const wchar_t *path) override {
			mPath = path;
			++mCallCount;
			return mCallCount >= 2;
		}

		int mCallCount = 0;
		VDStringW mPath;
	};

	bool VDPumpFileWatcherCallback(VDFileWatcherTestCallback& callback) {
		const uint64 deadline = VDGetCurrentTick64() + 4000;
		while(callback.mCallCount < 2 && VDGetCurrentTick64() < deadline) {
			CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.05, true);
		}
		return callback.mCallCount >= 2;
	}
#endif
}

bool ATTestSystemFileWatcher(ATPortableTestContext& context) {
	static uint32 sequence = 0;
	VDStringW baseName;
	baseName.sprintf(
		L"altirra-filewatcher-test-%u-%llu-%u",
		static_cast<unsigned>(VDGetCurrentProcessId()),
		static_cast<unsigned long long>(VDGetCurrentTick64()),
		static_cast<unsigned>(++sequence));
	VDFileWatcherTestSandbox sandbox(VDGetFullPath(baseName.c_str()));
	VDCreateDirectory(sandbox.mPath.c_str());
	VDWriteWatcherTestFile(sandbox.mFilePath.c_str(), "initial");

	VDFileWatcher watcher;
	AT_PORTABLE_TEST_ASSERT(context, !watcher.IsActive());
	AT_PORTABLE_TEST_ASSERT(context, !watcher.Wait(0));

	watcher.Init(sandbox.mFilePath.c_str(), nullptr);
	AT_PORTABLE_TEST_ASSERT(context, watcher.IsActive());
	AT_PORTABLE_TEST_ASSERT(context, !watcher.Wait(0));
	VDWriteWatcherTestFile(sandbox.mFilePath.c_str(), "updated-content");
	AT_PORTABLE_TEST_ASSERT(context, watcher.Wait(5000));
	watcher.Shutdown();
	AT_PORTABLE_TEST_ASSERT(context, !watcher.IsActive());
	AT_PORTABLE_TEST_ASSERT(context, !watcher.Wait(0));

#if defined(__APPLE__)
	VDFileWatcherTestCallback callback;
	watcher.Init(sandbox.mFilePath.c_str(), &callback);
	VDWriteWatcherTestFile(sandbox.mFilePath.c_str(), "callback-update");
	AT_PORTABLE_TEST_ASSERT(context, VDPumpFileWatcherCallback(callback));
	AT_PORTABLE_TEST_ASSERT(context, callback.mCallCount == 2);
	AT_PORTABLE_TEST_ASSERT(context,
		VDFileIsPathEqual(callback.mPath.c_str(), sandbox.mFilePath.c_str()));
	watcher.Shutdown();
#endif

	watcher.Init(sandbox.mCreatedPath.c_str(), nullptr);
	VDWriteWatcherTestFile(sandbox.mCreatedPath.c_str(), "created");
	AT_PORTABLE_TEST_ASSERT(context, watcher.Wait(5000));
	watcher.Shutdown();

	watcher.InitDir(sandbox.mPath.c_str(), false, nullptr);
	VDWriteWatcherTestFile(sandbox.mCreatedPath.c_str(), "directory-change");
	AT_PORTABLE_TEST_ASSERT(context, watcher.Wait(5000));
	watcher.Shutdown();

	VDCreateDirectory(sandbox.mNestedPath.c_str());
	watcher.InitDir(sandbox.mPath.c_str(), true, nullptr);
	VDWriteWatcherTestFile(sandbox.mNestedFilePath.c_str(), "nested-change");
	AT_PORTABLE_TEST_ASSERT(context, watcher.Wait(5000));
	watcher.Shutdown();

	VDStringW missingDirectory(sandbox.mPath);
	missingDirectory += L"-missing";
	AT_PORTABLE_TEST_ASSERT(context, VDInitDirFails(watcher, missingDirectory.c_str()));
	AT_PORTABLE_TEST_ASSERT(context, !watcher.IsActive());
	return true;
}
