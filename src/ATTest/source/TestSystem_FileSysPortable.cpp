// Altirra portable filesystem and path tests

#include <cstdio>
#include <cstring>
#include <cwchar>

#include <at/attest/portabletest.h>
#include <vd2/system/Error.h>
#include <vd2/system/date.h>
#include <vd2/system/filesys.h>
#include <vd2/system/text.h>
#include <vd2/system/thread.h>
#include <vd2/system/time.h>

namespace {
	class VDTestFileSystemSandbox {
	public:
		explicit VDTestFileSystemSandbox(const VDStringW& basePath)
			: mBasePath(basePath)
			, mChildPath(VDMakePath(basePath.c_str(), L"child"))
			, mSourcePath(VDMakePath(basePath.c_str(), L"alpha.txt"))
			, mDestinationPath(VDMakePath(basePath.c_str(), L"beta.txt")) {
		}

		~VDTestFileSystemSandbox() {
			try {
				VDFileSetAttributes(
					mSourcePath.c_str(), kVDFileAttr_ReadOnly, 0);
			} catch(...) {
			}
			try {
				VDFileSetAttributes(
					mDestinationPath.c_str(), kVDFileAttr_ReadOnly, 0);
			} catch(...) {
			}
			VDRemoveFile(mSourcePath.c_str());
			VDRemoveFile(mDestinationPath.c_str());
			try {
				VDRemoveDirectory(mChildPath.c_str());
			} catch(...) {
			}
			try {
				VDRemoveDirectory(mBasePath.c_str());
			} catch(...) {
			}
		}

		VDStringW mBasePath;
		VDStringW mChildPath;
		VDStringW mSourcePath;
		VDStringW mDestinationPath;
	};
}

bool ATTestSystemFileSys(ATPortableTestContext& context) {
	AT_PORTABLE_TEST_ASSERT(context, !strcmp(VDFileSplitFirstDir("one/two"), "two"));
	AT_PORTABLE_TEST_ASSERT(context, !wcscmp(VDFileSplitFirstDir(L"one\\two"), L"two"));
	AT_PORTABLE_TEST_ASSERT(context, !strcmp(VDFileSplitPath("one/two.bin"), "two.bin"));
	AT_PORTABLE_TEST_ASSERT(context, VDFileSplitPathLeft(VDStringA("one/two.bin")) == "one/");
	AT_PORTABLE_TEST_ASSERT(context, VDFileSplitPathRight(VDStringW(L"one\\two.bin")) == L"two.bin");
	AT_PORTABLE_TEST_ASSERT(context, !strcmp(VDFileSplitExt("one/two.bin"), ".bin"));
	AT_PORTABLE_TEST_ASSERT(context, VDFileSplitExtLeft(VDStringW(L"archive.tar.gz")) == L"archive.tar");
	AT_PORTABLE_TEST_ASSERT(context, VDFileSplitExtRight(VDStringA("archive.tar.gz")) == ".gz");
	AT_PORTABLE_TEST_ASSERT(context, VDFileSplitRoot(VDStringW(L"c:/folder")) == L"c:/");
	const VDStringSpanA narrowPath("one/two.bin");
	const VDStringSpanW widePath(L"one/two.bin");
	AT_PORTABLE_TEST_ASSERT(context, VDFileSplitPathLeftSpan(narrowPath) == "one/");
	AT_PORTABLE_TEST_ASSERT(context, VDFileSplitPathRightSpan(widePath) == L"two.bin");
	AT_PORTABLE_TEST_ASSERT(context, VDFileSplitExtLeftSpan(widePath) == L"one/two");
	AT_PORTABLE_TEST_ASSERT(context, VDFileSplitExtRightSpan(narrowPath) == ".bin");

	AT_PORTABLE_TEST_ASSERT(context, VDFileWildMatch("*.bin", "RANDOM.BIN"));
	AT_PORTABLE_TEST_ASSERT(context, VDFileWildMatch(L"ran*?*.bin", L"random.bin"));
	AT_PORTABLE_TEST_ASSERT(context, !VDFileWildMatch(L"*.rom", L"random.bin"));

#if defined(_WIN32)
	AT_PORTABLE_TEST_ASSERT(context,
		VDFileGetCanonicalPath(L"c:/one/./two/../three") == L"c:\\one\\three");
	AT_PORTABLE_TEST_ASSERT(context,
		VDFileGetRelativePath(L"c:/one/two", L"c:/one/three/file.bin", true)
			== L"..\\three\\file.bin");
	AT_PORTABLE_TEST_ASSERT(context,
		VDFileResolvePath(L"c:/one/two", L"../three") == L"c:\\one\\three");
	AT_PORTABLE_TEST_ASSERT(context, VDFileIsRelativePath(L"/one"));
	AT_PORTABLE_TEST_ASSERT(context, VDFileIsPathEqual(L"C:\\One\\", L"c:/one"));
#else
	AT_PORTABLE_TEST_ASSERT(context,
		VDFileGetCanonicalPath(L"/one/./two/../three") == L"/one/three");
	AT_PORTABLE_TEST_ASSERT(context,
		VDFileGetRelativePath(L"/one/two", L"/one/three/file.bin", true)
			== L"../three/file.bin");
	AT_PORTABLE_TEST_ASSERT(context,
		VDFileResolvePath(L"/one/two", L"../three") == L"/one/three");
	AT_PORTABLE_TEST_ASSERT(context, !VDFileIsRelativePath(L"/one"));
	AT_PORTABLE_TEST_ASSERT(context, VDFileIsPathEqual(L"/one//two/", L"\\one/two"));
	AT_PORTABLE_TEST_ASSERT(context, !VDFileIsPathEqual(L"/One", L"/one"));
#endif
	AT_PORTABLE_TEST_ASSERT(context,
		VDFileGetCanonicalPath(L"one/../../two")
#if defined(_WIN32)
			== L"..\\two"
#else
			== L"../two"
#endif
	);

	static uint32 sequence = 0;
	VDStringW baseName;
	baseName.sprintf(
		L"altirra-filesys-test-%u-%llu-%u",
		static_cast<unsigned>(VDGetCurrentProcessId()),
		static_cast<unsigned long long>(VDGetCurrentTick64()),
		static_cast<unsigned>(++sequence));
	VDTestFileSystemSandbox sandbox(baseName);

	AT_PORTABLE_TEST_ASSERT(context, !VDDoesPathExist(sandbox.mBasePath.c_str()));
	VDCreateDirectory(sandbox.mBasePath.c_str());
	AT_PORTABLE_TEST_ASSERT(context, VDDoesPathExist(sandbox.mBasePath.c_str()));
	AT_PORTABLE_TEST_ASSERT(context,
		(VDFileGetAttributes(sandbox.mBasePath.c_str()) & kVDFileAttr_Directory) != 0);
	AT_PORTABLE_TEST_ASSERT(context, VDGetDiskFreeSpace(sandbox.mBasePath.c_str()) > 0);

	bool duplicateCreateFailed = false;
	try {
		VDCreateDirectory(sandbox.mBasePath.c_str());
	} catch(const VDException&) {
		duplicateCreateFailed = true;
	}
	AT_PORTABLE_TEST_ASSERT(context, duplicateCreateFailed);

	VDCreateDirectory(sandbox.mChildPath.c_str());
	const VDStringA sourcePathUTF8 = VDTextWToU8(sandbox.mSourcePath);
	FILE *file = fopen(sourcePathUTF8.c_str(), "wb");
	AT_PORTABLE_TEST_ASSERT(context, file != nullptr);
	const char contents[] = "hello";
	AT_PORTABLE_TEST_ASSERT(context,
		fwrite(contents, 1, sizeof contents - 1, file) == sizeof contents - 1);
	AT_PORTABLE_TEST_ASSERT(context, fclose(file) == 0);

	const uint32 initialAttributes = VDFileGetAttributes(sandbox.mSourcePath.c_str());
	AT_PORTABLE_TEST_ASSERT(context, initialAttributes != kVDFileAttr_Invalid);
	AT_PORTABLE_TEST_ASSERT(context, !(initialAttributes & kVDFileAttr_Directory));
	AT_PORTABLE_TEST_ASSERT(context, VDFileGetLastWriteTime(sandbox.mSourcePath.c_str()) > 0);
	VDFileSetAttributes(
		sandbox.mSourcePath.c_str(), kVDFileAttr_ReadOnly, kVDFileAttr_ReadOnly);
	AT_PORTABLE_TEST_ASSERT(context,
		(VDFileGetAttributes(sandbox.mSourcePath.c_str()) & kVDFileAttr_ReadOnly) != 0);
	VDFileSetAttributes(sandbox.mSourcePath.c_str(), kVDFileAttr_ReadOnly, 0);
	AT_PORTABLE_TEST_ASSERT(context,
		(VDFileGetAttributes(sandbox.mSourcePath.c_str()) & kVDFileAttr_ReadOnly) == 0);
	VDFileSetAttributes(
		sandbox.mSourcePath.c_str(), kVDFileAttr_Hidden, kVDFileAttr_Hidden);
	AT_PORTABLE_TEST_ASSERT(context,
		(VDFileGetAttributes(sandbox.mSourcePath.c_str()) & kVDFileAttr_Hidden) != 0);
	VDFileSetAttributes(sandbox.mSourcePath.c_str(), kVDFileAttr_Hidden, 0);
	AT_PORTABLE_TEST_ASSERT(context,
		(VDFileGetAttributes(sandbox.mSourcePath.c_str()) & kVDFileAttr_Hidden) == 0);

	{
		VDDirectoryIterator iterator(
			VDMakePath(sandbox.mBasePath.c_str(), L"*.TXT").c_str());
		AT_PORTABLE_TEST_ASSERT(context, iterator.Next());
		AT_PORTABLE_TEST_ASSERT(context, !wcscmp(iterator.GetName(), L"alpha.txt"));
		AT_PORTABLE_TEST_ASSERT(context, !iterator.IsDirectory());
		AT_PORTABLE_TEST_ASSERT(context, iterator.GetSize() == 5);
		AT_PORTABLE_TEST_ASSERT(context, iterator.GetCreationDate().mTicks > 0);
		AT_PORTABLE_TEST_ASSERT(context, iterator.GetLastWriteDate().mTicks > 0);
		AT_PORTABLE_TEST_ASSERT(context,
			(iterator.GetAttributes() & kVDFileAttr_Directory) == 0);
		AT_PORTABLE_TEST_ASSERT(context, iterator.ResolveLinkSize());
		AT_PORTABLE_TEST_ASSERT(context, !iterator.Next());
	}

	VDMoveFile(sandbox.mSourcePath.c_str(), sandbox.mDestinationPath.c_str());
	AT_PORTABLE_TEST_ASSERT(context, !VDDoesPathExist(sandbox.mSourcePath.c_str()));
	AT_PORTABLE_TEST_ASSERT(context, VDDoesPathExist(sandbox.mDestinationPath.c_str()));
	const VDStringW fullDestination = VDGetFullPath(sandbox.mDestinationPath.c_str());
	AT_PORTABLE_TEST_ASSERT(context, !VDFileIsRelativePath(fullDestination.c_str()));
	AT_PORTABLE_TEST_ASSERT(context,
		VDGetLongPath(fullDestination.c_str()) == fullDestination);
	AT_PORTABLE_TEST_ASSERT(context,
		VDFileGetRelativePath(
			VDGetFullPath(sandbox.mBasePath.c_str()).c_str(),
			fullDestination.c_str(), true) == L"beta.txt");
	AT_PORTABLE_TEST_ASSERT(context,
		VDDoesPathExist(VDFileGetRootPath(fullDestination.c_str()).c_str()));

	vdvector<VDStringW> rootPaths;
	VDGetRootPaths(rootPaths);
	AT_PORTABLE_TEST_ASSERT(context, !rootPaths.empty());
	(void)VDGetRootVolumeLabel(rootPaths.front().c_str());
	AT_PORTABLE_TEST_ASSERT(context, !VDGetProgramFilePath().empty());
	AT_PORTABLE_TEST_ASSERT(context, VDDoesPathExist(VDGetProgramFilePath().c_str()));
	AT_PORTABLE_TEST_ASSERT(context, VDDoesPathExist(VDGetProgramPath().c_str()));
	AT_PORTABLE_TEST_ASSERT(context, VDDoesPathExist(VDGetLocalModulePath().c_str()));
	AT_PORTABLE_TEST_ASSERT(context, VDDoesPathExist(VDGetSystemPath().c_str()));
	VDSetDirectoryCreationTime(sandbox.mBasePath.c_str(), VDGetCurrentDate());

	AT_PORTABLE_TEST_ASSERT(context, VDRemoveFile(sandbox.mDestinationPath.c_str()));
	AT_PORTABLE_TEST_ASSERT(context, !VDRemoveFile(sandbox.mDestinationPath.c_str()));
	bool removeMissingFailed = false;
	try {
		VDRemoveFileEx(sandbox.mDestinationPath.c_str());
	} catch(const VDException&) {
		removeMissingFailed = true;
	}
	AT_PORTABLE_TEST_ASSERT(context, removeMissingFailed);

	VDStringW childWithSeparator(sandbox.mChildPath);
	VDFileFixDirPath(childWithSeparator);
	VDRemoveDirectory(childWithSeparator.c_str());
	AT_PORTABLE_TEST_ASSERT(context, !VDDoesPathExist(sandbox.mChildPath.c_str()));
	VDRemoveDirectory(sandbox.mBasePath.c_str());
	AT_PORTABLE_TEST_ASSERT(context, !VDDoesPathExist(sandbox.mBasePath.c_str()));
	return true;
}
