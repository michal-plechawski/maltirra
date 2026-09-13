// Altirra portable virtual disk directory watcher tests

#include <cstring>

#include <vd2/system/file.h>
#include <vd2/system/filesys.h>
#include <vd2/system/thread.h>
#include <vd2/system/time.h>

#include <at/attest/portabletest.h>
#include <directorywatcher.h>

namespace {
	class DirectoryWatcherTestSandbox {
	public:
		explicit DirectoryWatcherTestSandbox(const VDStringW& path)
			: mPath(path)
			, mDirectFile(VDMakePath(path.c_str(), L"direct.txt"))
			, mNestedDir(VDMakePath(path.c_str(), L"nested"))
			, mNestedFile(VDMakePath(mNestedDir.c_str(), L"nested.txt")) {
		}

		~DirectoryWatcherTestSandbox() {
			VDRemoveFile(mNestedFile.c_str());
			try {
				VDRemoveDirectory(mNestedDir.c_str());
			} catch(...) {
			}
			VDRemoveFile(mDirectFile.c_str());
			try {
				VDRemoveDirectory(mPath.c_str());
			} catch(...) {
			}
		}

		VDStringW mPath;
		VDStringW mDirectFile;
		VDStringW mNestedDir;
		VDStringW mNestedFile;
	};

	void WriteWatcherTestFile(const wchar_t *path) {
		VDFileStream stream(path, nsVDFile::kWrite | nsVDFile::kCreateAlways);
		static const char kContents[] = "directory watcher test";
		stream.Write(kContents, static_cast<sint32>(strlen(kContents)));
	}

	bool WaitForAnyChange(ATDirectoryWatcher& watcher) {
		const uint64 deadline = VDGetCurrentTick64() + 5000;
		do {
			if (watcher.CheckForChanges())
				return true;
			VDThreadSleep(25);
		} while(VDGetCurrentTick64() < deadline);
		return false;
	}

	bool WaitForRecursiveChange(ATDirectoryWatcher& watcher) {
		const uint64 deadline = VDGetCurrentTick64() + 5000;
		vdfastvector<wchar_t> changedDirs;
		do {
			if (watcher.CheckForChanges(changedDirs) || !changedDirs.empty())
				return true;
			VDThreadSleep(25);
		} while(VDGetCurrentTick64() < deadline);
		return false;
	}
}

bool ATTestAltirraDirectoryWatcher(ATPortableTestContext& context) {
	VDStringW baseName;
	baseName.sprintf(
		L"altirra-directorywatcher-test-%u-%llu",
		static_cast<unsigned>(VDGetCurrentProcessId()),
		static_cast<unsigned long long>(VDGetCurrentTick64()));
	DirectoryWatcherTestSandbox sandbox(VDGetFullPath(baseName.c_str()));
	VDCreateDirectory(sandbox.mPath.c_str());
	VDCreateDirectory(sandbox.mNestedDir.c_str());

	ATDirectoryWatcher::SetShouldUsePolling(false);
	ATDirectoryWatcher watcher;
	watcher.Init(sandbox.mPath.c_str(), false);
	VDThreadSleep(100);
	WriteWatcherTestFile(sandbox.mDirectFile.c_str());
	AT_PORTABLE_TEST_ASSERT(context, WaitForAnyChange(watcher));
	watcher.Shutdown();
	AT_PORTABLE_TEST_ASSERT(context, !watcher.CheckForChanges());

	watcher.Init(sandbox.mPath.c_str(), true);
	VDThreadSleep(100);
	WriteWatcherTestFile(sandbox.mNestedFile.c_str());
	AT_PORTABLE_TEST_ASSERT(context, WaitForRecursiveChange(watcher));
	watcher.Shutdown();
	return true;
}
