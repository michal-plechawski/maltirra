// Altirra portable stream tests

#include <cstring>
#include <cwchar>
#include <string>

#include <at/attest/portabletest.h>
#include <vd2/system/Error.h>
#include <vd2/system/file.h>
#include <vd2/system/filesys.h>
#include <vd2/system/thread.h>
#include <vd2/system/time.h>

namespace {
	class VDTestFileStreamSandbox {
	public:
		explicit VDTestFileStreamSandbox(const VDStringW& basePath)
			: mBasePath(basePath)
			, mFilePath(VDMakePath(basePath.c_str(), L"stream.txt")) {
		}

		~VDTestFileStreamSandbox() {
			VDRemoveFile(mFilePath.c_str());
			try {
				VDRemoveDirectory(mBasePath.c_str());
			} catch(...) {
			}
		}

		VDStringW mBasePath;
		VDStringW mFilePath;
	};

	bool VDExpectException(void (*operation)(void *), void *context) {
		try {
			operation(context);
		} catch(const VDException&) {
			return true;
		}
		return false;
	}
}

bool ATTestSystemFileStream(ATPortableTestContext& context) {
	static constexpr char kData[] = "0123456789abcdef";
	char buffer[64] {};

	VDMemoryStream memory(kData, sizeof kData - 1);
	AT_PORTABLE_TEST_ASSERT(context, !wcscmp(memory.GetNameForError(), L"memory stream"));
	AT_PORTABLE_TEST_ASSERT(context, memory.Pos() == 0);
	AT_PORTABLE_TEST_ASSERT(context, memory.Length() == 16);
	AT_PORTABLE_TEST_ASSERT(context, memory.ReadData(buffer, 0) == 0);
	AT_PORTABLE_TEST_ASSERT(context, memory.ReadData(buffer, -1) == 0);
	memory.Read(buffer, 4);
	AT_PORTABLE_TEST_ASSERT(context, !memcmp(buffer, "0123", 4));
	AT_PORTABLE_TEST_ASSERT(context, memory.Pos() == 4);
	memory.Seek(14);
	AT_PORTABLE_TEST_ASSERT(context, memory.ReadData(buffer, 8) == 2);
	AT_PORTABLE_TEST_ASSERT(context, !memcmp(buffer, "ef", 2));
	AT_PORTABLE_TEST_ASSERT(context,
		VDExpectException(
			[](void *stream) { static_cast<VDMemoryStream *>(stream)->Read(nullptr, 1); },
			&memory));
	AT_PORTABLE_TEST_ASSERT(context,
		VDExpectException(
			[](void *stream) { static_cast<VDMemoryStream *>(stream)->Seek(-1); },
			&memory));
	AT_PORTABLE_TEST_ASSERT(context,
		VDExpectException(
			[](void *stream) { static_cast<VDMemoryStream *>(stream)->Seek(17); },
			&memory));
	AT_PORTABLE_TEST_ASSERT(context,
		VDExpectException(
			[](void *stream) { static_cast<VDMemoryStream *>(stream)->Write("x", 1); },
			&memory));

	VDMemoryBufferStream memoryBuffer;
	AT_PORTABLE_TEST_ASSERT(context,
		!wcscmp(memoryBuffer.GetNameForError(), L"memory buffer stream"));
	AT_PORTABLE_TEST_ASSERT(context, memoryBuffer.Pos() == 0);
	AT_PORTABLE_TEST_ASSERT(context, memoryBuffer.Length() == 0);
	AT_PORTABLE_TEST_ASSERT(context, memoryBuffer.GetBuffer().empty());
	memoryBuffer.Write("abcdef", 6);
	memoryBuffer.Write("ignored", 0);
	AT_PORTABLE_TEST_ASSERT(context, memoryBuffer.Pos() == 6);
	AT_PORTABLE_TEST_ASSERT(context, memoryBuffer.Length() == 6);
	memoryBuffer.Seek(2);
	memoryBuffer.Write("XY", 2);
	memoryBuffer.Seek(0);
	memoryBuffer.Read(buffer, 6);
	AT_PORTABLE_TEST_ASSERT(context, !memcmp(buffer, "abXYef", 6));
	AT_PORTABLE_TEST_ASSERT(context, memoryBuffer.ReadData(buffer, 1) == 0);
	AT_PORTABLE_TEST_ASSERT(context,
		VDExpectException(
			[](void *stream) {
				static_cast<VDMemoryBufferStream *>(stream)->Read(nullptr, 1);
			},
			&memoryBuffer));
	AT_PORTABLE_TEST_ASSERT(context,
		VDExpectException(
			[](void *stream) { static_cast<VDMemoryBufferStream *>(stream)->Seek(7); },
			&memoryBuffer));
	memoryBuffer.Clear();
	AT_PORTABLE_TEST_ASSERT(context, memoryBuffer.Pos() == 0);
	AT_PORTABLE_TEST_ASSERT(context, memoryBuffer.Length() == 0);

	VDMemoryStream bufferedSource(kData, 16);
	VDBufferedStream buffered(&bufferedSource, 3);
	AT_PORTABLE_TEST_ASSERT(context,
		!wcscmp(buffered.GetNameForError(), bufferedSource.GetNameForError()));
	buffered.Read(buffer, 2);
	AT_PORTABLE_TEST_ASSERT(context, !memcmp(buffer, "01", 2));
	AT_PORTABLE_TEST_ASSERT(context, buffered.Pos() == 2);
	buffered.Read(buffer, 8);
	AT_PORTABLE_TEST_ASSERT(context, !memcmp(buffer, "23456789", 8));
	buffered.Skip(3);
	AT_PORTABLE_TEST_ASSERT(context, buffered.Pos() == 13);
	AT_PORTABLE_TEST_ASSERT(context, buffered.ReadData(buffer, 8) == 3);
	AT_PORTABLE_TEST_ASSERT(context, !memcmp(buffer, "def", 3));
	AT_PORTABLE_TEST_ASSERT(context,
		VDExpectException(
			[](void *stream) { static_cast<VDBufferedStream *>(stream)->Read(nullptr, 1); },
			&buffered));
	AT_PORTABLE_TEST_ASSERT(context,
		VDExpectException(
			[](void *stream) { static_cast<VDBufferedStream *>(stream)->Write("x", 1); },
			&buffered));

	VDMemoryStream zeroBufferedSource(kData, 16);
	VDBufferedStream zeroBuffered(&zeroBufferedSource, 0);
	zeroBuffered.Read(buffer, 4);
	AT_PORTABLE_TEST_ASSERT(context, !memcmp(buffer, "0123", 4));

	VDMemoryStream randomSource(kData, 16);
	VDBufferedRandomAccessStream random(&randomSource, 4);
	AT_PORTABLE_TEST_ASSERT(context, random.Length() == 16);
	random.Read(buffer, 3);
	AT_PORTABLE_TEST_ASSERT(context, !memcmp(buffer, "012", 3));
	random.Seek(1);
	random.Read(buffer, 4);
	AT_PORTABLE_TEST_ASSERT(context, !memcmp(buffer, "1234", 4));
	random.Seek(12);
	random.Read(buffer, 2);
	AT_PORTABLE_TEST_ASSERT(context, !memcmp(buffer, "cd", 2));
	random.Skip(-4);
	random.Read(buffer, 2);
	AT_PORTABLE_TEST_ASSERT(context, !memcmp(buffer, "ab", 2));
	AT_PORTABLE_TEST_ASSERT(context,
		VDExpectException(
			[](void *stream) {
				static_cast<VDBufferedRandomAccessStream *>(stream)->Write("x", 1);
			},
			&random));

	VDMemoryBufferStream writeDestination;
	{
		VDBufferedWriteStream output(&writeDestination, 4);
		AT_PORTABLE_TEST_ASSERT(context,
			!wcscmp(output.GetNameForError(), writeDestination.GetNameForError()));
		output.Write("ab", 2);
		AT_PORTABLE_TEST_ASSERT(context, output.Pos() == 2);
		AT_PORTABLE_TEST_ASSERT(context, output.Length() == 2);
		AT_PORTABLE_TEST_ASSERT(context, writeDestination.Length() == 0);
		output.Write("cdef", 4);
		AT_PORTABLE_TEST_ASSERT(context, output.Pos() == 6);
		AT_PORTABLE_TEST_ASSERT(context, output.Length() == 6);
		output.Flush();
		AT_PORTABLE_TEST_ASSERT(context, writeDestination.Length() == 6);
		output.Seek(2);
		output.Write("XY", 2);
		output.Skip(1);
		output.Write("Z", 1);
		output.Flush();
		AT_PORTABLE_TEST_ASSERT(context, output.Pos() == 6);
		AT_PORTABLE_TEST_ASSERT(context,
			VDExpectException(
				[](void *stream) {
					static_cast<VDBufferedWriteStream *>(stream)->Read(nullptr, 1);
				},
				&output));
		AT_PORTABLE_TEST_ASSERT(context, output.ReadData(nullptr, 1) == -1);
	}
	AT_PORTABLE_TEST_ASSERT(context, writeDestination.Length() == 6);
	writeDestination.Seek(0);
	writeDestination.Read(buffer, 6);
	AT_PORTABLE_TEST_ASSERT(context, !memcmp(buffer, "abXYeZ", 6));

	VDMemoryBufferStream destructorDestination;
	{
		VDBufferedWriteStream destructorOutput(&destructorDestination, 8);
		destructorOutput.Write("flushed", 7);
	}
	AT_PORTABLE_TEST_ASSERT(context, destructorDestination.Length() == 7);

	const std::string longLine(5000, 'L');
	const std::string textData = "alpha\r\nbeta\n\rgamma\rdelta\n" + longLine + "\nlast";
	VDMemoryStream textMemory(textData.data(), static_cast<uint32>(textData.size()));
	VDTextStream text(&textMemory);
	AT_PORTABLE_TEST_ASSERT(context, !strcmp(text.GetNextLine(), "alpha"));
	AT_PORTABLE_TEST_ASSERT(context, !strcmp(text.GetNextLine(), "beta"));
	AT_PORTABLE_TEST_ASSERT(context, !strcmp(text.GetNextLine(), "gamma"));
	AT_PORTABLE_TEST_ASSERT(context, !strcmp(text.GetNextLine(), "delta"));
	const char *readLongLine = text.GetNextLine();
	AT_PORTABLE_TEST_ASSERT(context, readLongLine != nullptr);
	AT_PORTABLE_TEST_ASSERT(context, strlen(readLongLine) == longLine.size());
	AT_PORTABLE_TEST_ASSERT(context, !memcmp(readLongLine, longLine.data(), longLine.size()));
	AT_PORTABLE_TEST_ASSERT(context, !strcmp(text.GetNextLine(), "last"));
	AT_PORTABLE_TEST_ASSERT(context, text.GetNextLine() == nullptr);
	AT_PORTABLE_TEST_ASSERT(context, text.GetNextLine() == nullptr);

	VDMemoryBufferStream textDestination;
	{
		VDTextOutputStream textOutput(&textDestination);
		textOutput.Write("prefix");
		textOutput.Write("-ignored", 0);
		textOutput.PutLine(" line");
		textOutput.Format("value=%d", 42);
		textOutput.FormatLine("/%s", "ok");
		textOutput.Format("%s", longLine.c_str());
		AT_PORTABLE_TEST_ASSERT(context,
			textOutput.Pos() == 26 + static_cast<sint64>(longLine.size()));
		textOutput.Flush();
	}
	AT_PORTABLE_TEST_ASSERT(context,
		textDestination.Length() == 26 + static_cast<sint64>(longLine.size()));
	textDestination.Seek(0);
	std::string formatted(static_cast<size_t>(textDestination.Length()), '\0');
	textDestination.Read(formatted.data(), static_cast<sint32>(formatted.size()));
	AT_PORTABLE_TEST_ASSERT(context,
		formatted.substr(0, 26) == "prefix line\r\nvalue=42/ok\r\n");
	AT_PORTABLE_TEST_ASSERT(context,
		formatted.substr(26) == longLine);

	static uint32 sequence = 0;
	VDStringW baseName;
	baseName.sprintf(
		L"altirra-stream-test-%u-%llu-%u",
		static_cast<unsigned>(VDGetCurrentProcessId()),
		static_cast<unsigned long long>(VDGetCurrentTick64()),
		static_cast<unsigned>(++sequence));
	VDTestFileStreamSandbox sandbox(baseName);
	VDCreateDirectory(sandbox.mBasePath.c_str());
	{
		VDFileStream fileStream(
			sandbox.mFilePath.c_str(),
			nsVDFile::kReadWrite | nsVDFile::kCreateAlways);
		AT_PORTABLE_TEST_ASSERT(context,
			!wcscmp(fileStream.GetNameForError(), L"stream.txt"));
		fileStream.Write("one\r\ntwo", 8);
		AT_PORTABLE_TEST_ASSERT(context, fileStream.Pos() == 8);
		AT_PORTABLE_TEST_ASSERT(context, fileStream.Length() == 8);
		fileStream.Seek(0);
		AT_PORTABLE_TEST_ASSERT(context, fileStream.ReadData(buffer, 3) == 3);
		AT_PORTABLE_TEST_ASSERT(context, !memcmp(buffer, "one", 3));
	}
	{
		VDTextInputFile inputFile(sandbox.mFilePath.c_str());
		AT_PORTABLE_TEST_ASSERT(context, !strcmp(inputFile.GetNextLine(), "one"));
		AT_PORTABLE_TEST_ASSERT(context, !strcmp(inputFile.GetNextLine(), "two"));
		AT_PORTABLE_TEST_ASSERT(context, inputFile.GetNextLine() == nullptr);
	}

	AT_PORTABLE_TEST_ASSERT(context, VDRemoveFile(sandbox.mFilePath.c_str()));
	VDRemoveDirectory(sandbox.mBasePath.c_str());
	return true;
}
