// Altirra portable asynchronous file tests

#include <cstring>
#include <memory>
#include <vector>

#include <at/attest/portabletest.h>
#include <vd2/system/Error.h>
#include <vd2/system/file.h>
#include <vd2/system/fileasync.h>
#include <vd2/system/filesys.h>
#include <vd2/system/thread.h>
#include <vd2/system/time.h>

namespace {
	class VDTestAsyncFileSandbox {
	public:
		explicit VDTestAsyncFileSandbox(const VDStringW& basePath)
			: mBasePath(basePath) {
		}

		~VDTestAsyncFileSandbox() {
			for(uint32 mode = 0; mode < IVDFileAsync::kModeCount; ++mode) {
				VDRemoveFile(GetModePath(mode, false).c_str());
				VDRemoveFile(GetModePath(mode, true).c_str());
			}
			VDRemoveFile(VDMakePath(mBasePath.c_str(), L"handle.bin").c_str());
			try {
				VDRemoveDirectory(mBasePath.c_str());
			} catch(...) {
			}
		}

		VDStringW GetModePath(uint32 mode, bool safe) const {
			VDStringW filename;
			filename.sprintf(L"mode-%u%s.bin", mode, safe ? L"-safe" : L"");
			return VDMakePath(mBasePath.c_str(), filename.c_str());
		}

		VDStringW mBasePath;
	};

	bool VDAsyncFileHasContents(
		const wchar_t *path,
		const void *expected,
		size_t expectedSize) {
		VDFile file(path, nsVDFile::kRead | nsVDFile::kOpenExisting);
		if (file.size() != static_cast<sint64>(expectedSize))
			return false;

		std::vector<uint8> buffer(expectedSize);
		file.read(buffer.data(), static_cast<long>(expectedSize));
		return !memcmp(buffer.data(), expected, expectedSize);
	}
}

bool ATTestSystemFileAsync(ATPortableTestContext& context) {
	static uint32 sequence = 0;
	VDStringW baseName;
	baseName.sprintf(
		L"altirra-fileasync-test-%u-%llu-%u",
		static_cast<unsigned>(VDGetCurrentProcessId()),
		static_cast<unsigned long long>(VDGetCurrentTick64()),
		static_cast<unsigned>(++sequence));
	VDTestAsyncFileSandbox sandbox(baseName);
	VDCreateDirectory(sandbox.mBasePath.c_str());

	static constexpr char kExpected[] = "abcDEFxyz";
	static constexpr char kInitial[] = "abc";
	static constexpr char kTail[] = "xyz";
	static constexpr char kPatch[] = "DEF";

	for(uint32 modeValue = 0; modeValue < IVDFileAsync::kModeCount; ++modeValue) {
		const auto mode = static_cast<IVDFileAsync::Mode>(modeValue);
		const VDStringW path = sandbox.GetModePath(modeValue, false);
		std::unique_ptr<IVDFileAsync> file(VDCreateFileAsync(mode));

		AT_PORTABLE_TEST_ASSERT(context, file != nullptr);
		AT_PORTABLE_TEST_ASSERT(context, !file->IsOpen());
		AT_PORTABLE_TEST_ASSERT(context, !file->IsPreemptiveExtendActive());
		file->SetPreemptiveExtend(true);
		AT_PORTABLE_TEST_ASSERT(context, file->IsPreemptiveExtendActive());
		file->SetPreemptiveExtend(false);
		AT_PORTABLE_TEST_ASSERT(context, !file->IsPreemptiveExtendActive());
		file->SetPreemptiveExtend(true);

		file->Open(path.c_str(), 2, 4096);
		AT_PORTABLE_TEST_ASSERT(context, file->IsOpen());
		AT_PORTABLE_TEST_ASSERT(context, file->GetFastWritePos() == 0);
		AT_PORTABLE_TEST_ASSERT(context, file->GetSize() == 0);
		AT_PORTABLE_TEST_ASSERT(context, file->Extend(64));
		AT_PORTABLE_TEST_ASSERT(context, file->GetSize() == 64);
		file->Truncate(0);
		AT_PORTABLE_TEST_ASSERT(context, file->GetSize() == 0);

		file->FastWrite(kInitial, sizeof kInitial - 1);
		file->FastWrite(nullptr, 3);
		file->FastWrite(kTail, sizeof kTail - 1);
		AT_PORTABLE_TEST_ASSERT(context, file->GetFastWritePos() == 9);
		file->FastWriteEnd();
		AT_PORTABLE_TEST_ASSERT(context, file->IsPreemptiveExtendActive());
		AT_PORTABLE_TEST_ASSERT(context, file->GetSize() >= 9);
		file->Write(3, kPatch, sizeof kPatch - 1);
		file->Truncate(9);
		AT_PORTABLE_TEST_ASSERT(context, file->GetSize() == 9);
		file->Close();
		AT_PORTABLE_TEST_ASSERT(context, !file->IsOpen());
		file->Close();
		AT_PORTABLE_TEST_ASSERT(context,
			VDAsyncFileHasContents(path.c_str(), kExpected, sizeof kExpected - 1));

		if (mode == IVDFileAsync::kModeThreaded
			|| mode == IVDFileAsync::kModeAsynchronous) {
			std::vector<uint8> bulkData(2 * 4096 + 137);
			for(size_t i = 0; i < bulkData.size(); ++i)
				bulkData[i] = static_cast<uint8>((i * 29 + 7) & 0xFF);

			AT_PORTABLE_TEST_ASSERT(context, VDRemoveFile(path.c_str()));
			file->SetPreemptiveExtend(false);
			file->Open(path.c_str(), 2, 4096);
			file->FastWrite(bulkData.data(), static_cast<uint32>(bulkData.size()));
			AT_PORTABLE_TEST_ASSERT(context,
				file->GetFastWritePos() == static_cast<sint64>(bulkData.size()));
			file->FastWriteEnd();
			file->Truncate(static_cast<sint64>(bulkData.size()));
			file->Close();
			AT_PORTABLE_TEST_ASSERT(context,
				VDAsyncFileHasContents(path.c_str(), bulkData.data(), bulkData.size()));
		}

		const VDStringW safePath = sandbox.GetModePath(modeValue, true);
		file->Open(safePath.c_str(), 2, 4096);
		file->FastWrite("truncate-me", 11);
		file->SafeTruncateAndClose(7);
		AT_PORTABLE_TEST_ASSERT(context, !file->IsOpen());
		VDFile safeFile(safePath.c_str(), nsVDFile::kRead | nsVDFile::kOpenExisting);
		AT_PORTABLE_TEST_ASSERT(context, safeFile.size() == 7);
	}

	const VDStringW handlePath = VDMakePath(sandbox.mBasePath.c_str(), L"handle.bin");
	VDFile handleFile(
		handlePath.c_str(),
		nsVDFile::kReadWrite | nsVDFile::kDenyNone | nsVDFile::kCreateAlways);
	std::unique_ptr<IVDFileAsync> handleWriter(
		VDCreateFileAsync(IVDFileAsync::kModeBuffered));
	handleWriter->Open(handleFile.getRawHandle(), 2, 4096);
	handleWriter->FastWrite("handle", 6);
	handleWriter->Truncate(6);
	handleWriter->Close();
	AT_PORTABLE_TEST_ASSERT(context, handleFile.isOpen());
	handleFile.seek(6);
	handleFile.write("!", 1);
	handleFile.close();
	AT_PORTABLE_TEST_ASSERT(context,
		VDAsyncFileHasContents(handlePath.c_str(), "handle!", 7));

	std::unique_ptr<IVDFileAsync> invalidHandleWriter(
		VDCreateFileAsync(IVDFileAsync::kModeSynchronous));
	bool invalidHandleFailed = false;
	try {
		invalidHandleWriter->Open(static_cast<VDFileHandle>(nullptr), 2, 4096);
	} catch(const VDException&) {
		invalidHandleFailed = true;
	}
	AT_PORTABLE_TEST_ASSERT(context, invalidHandleFailed);
	AT_PORTABLE_TEST_ASSERT(context, !invalidHandleWriter->IsOpen());

	const VDStringW missingDirectory =
		VDMakePath(sandbox.mBasePath.c_str(), L"missing-directory");
	const VDStringW missingPath = VDMakePath(missingDirectory.c_str(), L"file.bin");
	bool missingDirectoryFailed = false;
	try {
		invalidHandleWriter->Open(missingPath.c_str(), 2, 4096);
	} catch(const VDException&) {
		missingDirectoryFailed = true;
	}
	AT_PORTABLE_TEST_ASSERT(context, missingDirectoryFailed);
	AT_PORTABLE_TEST_ASSERT(context, !invalidHandleWriter->IsOpen());

	return true;
}
