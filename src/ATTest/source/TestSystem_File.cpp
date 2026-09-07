// Altirra portable native file tests

#include <cstring>
#include <cwchar>
#include <initializer_list>
#include <utility>

#include <at/attest/portabletest.h>
#include <vd2/system/Error.h>
#include <vd2/system/date.h>
#include <vd2/system/file.h>
#include <vd2/system/filesys.h>
#include <vd2/system/text.h>
#include <vd2/system/thread.h>
#include <vd2/system/time.h>

namespace {
	class VDTestNativeFileSandbox {
	public:
		explicit VDTestNativeFileSandbox(const VDStringW& basePath)
			: mBasePath(basePath)
			, mFilePath(VDMakePath(basePath.c_str(), L"native-file.bin")) {
		}

		~VDTestNativeFileSandbox() {
			VDRemoveFile(mFilePath.c_str());
			try {
				VDRemoveDirectory(mBasePath.c_str());
			} catch(...) {
			}
		}

		VDStringW mBasePath;
		VDStringW mFilePath;
	};

	bool VDDateIsNear(const VDDate& left, const VDDate& right, float toleranceSeconds) {
		return (left - right).Abs()
			<= VDDateInterval::FromSeconds(toleranceSeconds);
	}
}

bool ATTestSystemFile(ATPortableTestContext& context) {
	static uint32 sequence = 0;
	VDStringW baseName;
	baseName.sprintf(
		L"altirra-file-test-%u-%llu-%u",
		static_cast<unsigned>(VDGetCurrentProcessId()),
		static_cast<unsigned long long>(VDGetCurrentTick64()),
		static_cast<unsigned>(++sequence));
	VDTestNativeFileSandbox sandbox(baseName);

	VDFile unopened;
	AT_PORTABLE_TEST_ASSERT(context, !unopened.isOpen());
	AT_PORTABLE_TEST_ASSERT(context, unopened.getRawHandle() == nullptr);
	AT_PORTABLE_TEST_ASSERT(context, unopened.getAttributes() == 0);
	AT_PORTABLE_TEST_ASSERT(context, unopened.getCreationTime().mTicks == 0);
	AT_PORTABLE_TEST_ASSERT(context, unopened.getLastWriteTime().mTicks == 0);
	AT_PORTABLE_TEST_ASSERT(context, !unopened.seekNT(0));
	AT_PORTABLE_TEST_ASSERT(context, !unopened.truncateNT());
	AT_PORTABLE_TEST_ASSERT(context, !unopened.flushNT());
	AT_PORTABLE_TEST_ASSERT(context, unopened.closeNT());

	AT_PORTABLE_TEST_ASSERT(context,
		!unopened.openNT(sandbox.mFilePath.c_str(),
			nsVDFile::kRead | nsVDFile::kOpenExisting));
	AT_PORTABLE_TEST_ASSERT(context,
		!unopened.tryOpen(sandbox.mFilePath.c_str(),
			nsVDFile::kRead | nsVDFile::kOpenExisting));
	bool missingOpenFailed = false;
	try {
		unopened.open(
			sandbox.mFilePath.c_str(),
			nsVDFile::kRead | nsVDFile::kOpenExisting);
	} catch(const VDException&) {
		missingOpenFailed = true;
	}
	AT_PORTABLE_TEST_ASSERT(context, missingOpenFailed);
	AT_PORTABLE_TEST_ASSERT(context, !unopened.isOpen());

	VDCreateDirectory(sandbox.mBasePath.c_str());
	AT_PORTABLE_TEST_ASSERT(context,
		!unopened.openNT(sandbox.mBasePath.c_str(),
			nsVDFile::kRead | nsVDFile::kOpenExisting));

	VDFile file;
	AT_PORTABLE_TEST_ASSERT(context,
		!file.openAlways(
			sandbox.mFilePath.c_str(),
			nsVDFile::kReadWrite | nsVDFile::kDenyAll | nsVDFile::kOpenAlways));
	AT_PORTABLE_TEST_ASSERT(context, file.isOpen());
	AT_PORTABLE_TEST_ASSERT(context, file.getRawHandle() != nullptr);
	AT_PORTABLE_TEST_ASSERT(context,
		!wcscmp(file.getFilenameForError(), L"native-file.bin"));
	AT_PORTABLE_TEST_ASSERT(context, file.tell() == 0);
	AT_PORTABLE_TEST_ASSERT(context, file.size() == 0);
	AT_PORTABLE_TEST_ASSERT(context, file.writeData("", 0) == 0);

	static constexpr char kContents[] = "0123456789";
	file.write(kContents, sizeof kContents - 1);
	AT_PORTABLE_TEST_ASSERT(context, file.tell() == 10);
	AT_PORTABLE_TEST_ASSERT(context, file.size() == 10);
#if defined(__APPLE__)
	AT_PORTABLE_TEST_ASSERT(context, file.extendValidNT(10));
	AT_PORTABLE_TEST_ASSERT(context, !file.extendValidNT(11));
	AT_PORTABLE_TEST_ASSERT(context, file.size() == 10);
#endif
	AT_PORTABLE_TEST_ASSERT(context, file.flushNT());
	file.flush();
	AT_PORTABLE_TEST_ASSERT(context,
		(file.getAttributes() & kVDFileAttr_Directory) == 0);
	AT_PORTABLE_TEST_ASSERT(context, file.getCreationTime().mTicks > 0);
	AT_PORTABLE_TEST_ASSERT(context, file.getLastWriteTime().mTicks > 0);

	const VDDate adjustedTime =
		VDGetCurrentDate() - VDDateInterval::FromSeconds(120.0f);
	file.setLastWriteTime(adjustedTime);
	AT_PORTABLE_TEST_ASSERT(context,
		VDDateIsNear(file.getLastWriteTime(), adjustedTime, 2.0f));
	file.setCreationTime(adjustedTime);
	AT_PORTABLE_TEST_ASSERT(context,
		VDDateIsNear(file.getCreationTime(), adjustedTime, 2.0f));

	VDFile moved(std::move(file));
	AT_PORTABLE_TEST_ASSERT(context, !file.isOpen());
	AT_PORTABLE_TEST_ASSERT(context, moved.isOpen());
	AT_PORTABLE_TEST_ASSERT(context, moved.tell() == 10);
	VDFile assigned;
	assigned = std::move(moved);
	AT_PORTABLE_TEST_ASSERT(context, !moved.isOpen());
	AT_PORTABLE_TEST_ASSERT(context, assigned.isOpen());
	AT_PORTABLE_TEST_ASSERT(context, assigned.tell() == 10);
	assigned.close();
	AT_PORTABLE_TEST_ASSERT(context, !assigned.isOpen());
	AT_PORTABLE_TEST_ASSERT(context, assigned.closeNT());

	AT_PORTABLE_TEST_ASSERT(context,
		file.openAlways(
			sandbox.mFilePath.c_str(),
			nsVDFile::kReadWrite | nsVDFile::kDenyAll | nsVDFile::kOpenAlways));
	file.close();

	bool createExistingFailed = false;
	try {
		file.open(
			sandbox.mFilePath.c_str(),
			nsVDFile::kWrite | nsVDFile::kCreateNew);
	} catch(const VDException&) {
		createExistingFailed = true;
	}
	AT_PORTABLE_TEST_ASSERT(context, createExistingFailed);

	VDFile reader(
		sandbox.mFilePath.c_str(),
		nsVDFile::kRead | nsVDFile::kDenyWrite | nsVDFile::kOpenExisting);
	char buffer[16] {};
	AT_PORTABLE_TEST_ASSERT(context, reader.readData(buffer, 4) == 4);
	AT_PORTABLE_TEST_ASSERT(context, !memcmp(buffer, "0123", 4));
	AT_PORTABLE_TEST_ASSERT(context, reader.tell() == 4);
	reader.skip(2);
	AT_PORTABLE_TEST_ASSERT(context, reader.tell() == 6);
	reader.read(buffer, 1);
	AT_PORTABLE_TEST_ASSERT(context, buffer[0] == '6');
	AT_PORTABLE_TEST_ASSERT(context, reader.skipNT(-2));
	AT_PORTABLE_TEST_ASSERT(context, reader.tell() == 5);
	reader.read(buffer, 1);
	AT_PORTABLE_TEST_ASSERT(context, buffer[0] == '5');
	reader.seek(-1, nsVDFile::kSeekEnd);
	reader.read(buffer, 1);
	AT_PORTABLE_TEST_ASSERT(context, buffer[0] == '9');
	reader.seek(0);
	reader.read(buffer, 10);
	AT_PORTABLE_TEST_ASSERT(context, !memcmp(buffer, kContents, 10));
	AT_PORTABLE_TEST_ASSERT(context, reader.readData(buffer, 0) == 0);
	bool prematureReadFailed = false;
	try {
		reader.read(buffer, 1);
	} catch(const VDException&) {
		prematureReadFailed = true;
	}
	AT_PORTABLE_TEST_ASSERT(context, prematureReadFailed);

	VDFile secondReader(
		sandbox.mFilePath.c_str(),
		nsVDFile::kRead | nsVDFile::kDenyWrite | nsVDFile::kOpenExisting);
	bool deniedWriterFailed = false;
	try {
		VDFile deniedWriter(
			sandbox.mFilePath.c_str(),
			nsVDFile::kWrite | nsVDFile::kDenyNone | nsVDFile::kOpenExisting);
	} catch(const VDException&) {
		deniedWriterFailed = true;
	}
	AT_PORTABLE_TEST_ASSERT(context, deniedWriterFailed);
	secondReader.close();
	reader.close();

	VDFile writer(
		sandbox.mFilePath.c_str(),
		nsVDFile::kWrite | nsVDFile::kDenyNone | nsVDFile::kOpenExisting);
	writer.seek(6);
	writer.truncate();
	AT_PORTABLE_TEST_ASSERT(context, writer.size() == 6);
	writer.close();

	writer.open(
		sandbox.mFilePath.c_str(),
		nsVDFile::kWrite | nsVDFile::kTruncateExisting);
	AT_PORTABLE_TEST_ASSERT(context, writer.size() == 0);
	writer.write("data", 4);
	writer.close();

	const VDStringA narrowPath = VDTextWToU8(sandbox.mFilePath);
	VDFile narrowFile(
		narrowPath.c_str(),
		nsVDFile::kRead | nsVDFile::kOpenExisting);
	AT_PORTABLE_TEST_ASSERT(context, narrowFile.size() == 4);
	narrowFile.close();

	for(const uint32 hint : {
		nsVDFile::kSequential,
		nsVDFile::kRandomAccess,
		nsVDFile::kUnbuffered,
		nsVDFile::kWriteThrough
	}) {
		VDFile hintedFile(
			sandbox.mFilePath.c_str(),
			nsVDFile::kRead | nsVDFile::kOpenExisting | hint);
		AT_PORTABLE_TEST_ASSERT(context, hintedFile.isOpen());
	}

	void *unbuffer = VDFile::AllocUnbuffer(8192);
	AT_PORTABLE_TEST_ASSERT(context, unbuffer != nullptr);
	AT_PORTABLE_TEST_ASSERT(context,
		(reinterpret_cast<uintptr>(unbuffer) & 4095) == 0);
	memset(unbuffer, 0xA5, 8192);
	VDFile::FreeUnbuffer(unbuffer);
	AT_PORTABLE_TEST_ASSERT(context, VDFile::AllocUnbuffer(0) == nullptr);

#if defined(__APPLE__)
	AT_PORTABLE_TEST_ASSERT(context, VDFile::enableExtendValid());
#else
	(void)VDFile::enableExtendValid();
#endif

	AT_PORTABLE_TEST_ASSERT(context, VDRemoveFile(sandbox.mFilePath.c_str()));
	VDRemoveDirectory(sandbox.mBasePath.c_str());
	return true;
}
