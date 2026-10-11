// Portable console text output tests.

#include <cstring>
#include <string>
#include <utility>

#include <at/attest/portabletest.h>
#include <consoletext.h>
#include <vd2/system/error.h>
#include <vd2/system/file.h>
#include <vd2/system/filesys.h>
#include <vd2/system/time.h>

namespace {
	class CapturingConsole final : public IATUIDebuggerConsoleWindow {
	public:
		void Write(const char *s) override {
			mText += s;
			++mWriteCount;
		}

		void ShowEnd() override { ++mShowEndCount; }

		std::string mText;
		uint32 mWriteCount = 0;
		uint32 mShowEndCount = 0;
	};

	class ConsoleTextTestScope {
	public:
		explicit ConsoleTextTestScope(IATUIDebuggerConsoleWindow *window)
			: mpPreviousWindow(ATConsoleSetTextOutput(window))
			, mPreviousBeamProvider(ATConsoleSetBeamPositionProvider({})) {
		}

		~ConsoleTextTestScope() {
			if (mbOwnsLog)
				ATConsoleCloseLogFileNT();
			ATConsoleSetBeamPositionProvider(std::move(mPreviousBeamProvider));
			ATConsoleSetTextOutput(mpPreviousWindow);
		}

		void OpenLog(const wchar_t *path) {
			ATConsoleOpenLogFile(path);
			mbOwnsLog = true;
		}

		void CloseLog() {
			ATConsoleCloseLogFile();
			mbOwnsLog = false;
		}

	private:
		IATUIDebuggerConsoleWindow *mpPreviousWindow;
		vdfunction<bool(ATConsoleBeamPosition&)> mPreviousBeamProvider;
		bool mbOwnsLog = false;
	};

	class ConsoleTextTestFile {
	public:
		ConsoleTextTestFile() {
			static uint32 sequence = 0;
			VDStringW name;
			name.sprintf(L"altirra-console-text-test-%u-%llu-%u.log",
				static_cast<unsigned>(VDGetCurrentProcessId()),
				static_cast<unsigned long long>(VDGetCurrentTick64()),
				static_cast<unsigned>(++sequence));
			mPath = VDGetFullPath(name.c_str());
		}

		~ConsoleTextTestFile() { VDRemoveFile(mPath.c_str()); }

		VDStringW mPath;
	};
}

bool ATTestAltirraConsoleText(ATPortableTestContext& context) {
	ConsoleTextTestFile testFile;
	CapturingConsole sink;
	ConsoleTextTestScope hooks(&sink);

	ATConsoleWrite("plain write");
	AT_PORTABLE_TEST_ASSERT(context, sink.mText == "plain write");
	AT_PORTABLE_TEST_ASSERT(context, sink.mWriteCount == 1 && sink.mShowEndCount == 1);

	ATConsolePrintf("; value=%d/%s", 37, "formatted");
	AT_PORTABLE_TEST_ASSERT(context, sink.mText == "plain write; value=37/formatted");
	AT_PORTABLE_TEST_ASSERT(context, sink.mWriteCount == 2 && sink.mShowEndCount == 2);

	ATConsoleSetBeamPositionProvider([](ATConsoleBeamPosition& position) {
		position.mFrame = 12;
		position.mVPos = 34;
		position.mHPos = 56;
		return true;
	});
	ATConsoleTaggedPrintf("tagged %u", 9u);
	AT_PORTABLE_TEST_ASSERT(context, sink.mText == "plain write; value=37/formatted( 12: 34, 56) tagged 9");
	AT_PORTABLE_TEST_ASSERT(context, sink.mWriteCount == 3 && sink.mShowEndCount == 3);

	ATConsoleSetBeamPositionProvider([](ATConsoleBeamPosition& position) {
		position.mFrame = 0xffffffffu;
		position.mVPos = 0xffffffffu;
		position.mHPos = 0xffffffffu;
		return true;
	});
	sink.mText.clear();
	ATConsoleTaggedPrintf("max");
	AT_PORTABLE_TEST_ASSERT(context, sink.mText == "(4294967295:4294967295,4294967295) max");

	ATConsoleSetBeamPositionProvider([](ATConsoleBeamPosition&) { return false; });
	sink.mText.clear();
	ATConsoleTaggedPrintf("untimed %d", 4);
	AT_PORTABLE_TEST_ASSERT(context, sink.mText == "untimed 4");

	std::string accepted(3071, 'a');
	std::string rejected(3072, 'b');
	sink.mText.clear();
	const uint32 writesBeforeLimits = sink.mWriteCount;
	const uint32 showsBeforeLimits = sink.mShowEndCount;
	ATConsolePrintf("%s", accepted.c_str());
	AT_PORTABLE_TEST_ASSERT(context, sink.mText == accepted);
	AT_PORTABLE_TEST_ASSERT(context, sink.mWriteCount == writesBeforeLimits + 1);
	ATConsolePrintf("%s", rejected.c_str());
	AT_PORTABLE_TEST_ASSERT(context, sink.mText == accepted);
	AT_PORTABLE_TEST_ASSERT(context, sink.mWriteCount == writesBeforeLimits + 1);
	AT_PORTABLE_TEST_ASSERT(context, sink.mShowEndCount == showsBeforeLimits + 1);

	// The tag shares the same fixed-size buffer with its prefix, so the
	// accepted payload limit is correspondingly smaller.
	ATConsoleSetBeamPositionProvider([](ATConsoleBeamPosition& position) {
		position.mFrame = 12;
		position.mVPos = 34;
		position.mHPos = 56;
		return true;
	});
	const std::string taggedAccepted(3057, 'c');
	const std::string taggedRejected(3058, 'd');
	sink.mText.clear();
	ATConsoleTaggedPrintf("%s", taggedAccepted.c_str());
	AT_PORTABLE_TEST_ASSERT(context, sink.mText == std::string("( 12: 34, 56) ") + taggedAccepted);
	const std::string acceptedTaggedText = sink.mText;
	ATConsoleTaggedPrintf("%s", taggedRejected.c_str());
	AT_PORTABLE_TEST_ASSERT(context, sink.mText == acceptedTaggedText);

	// Logging must remain functional without a GUI sink. The text stream owns
	// CRLF conversion for each newline, and CloseLogFile flushes before closing.
	const uint32 writesBeforeLog = sink.mWriteCount;
	ATConsoleSetTextOutput(nullptr);
	ATConsoleSetBeamPositionProvider([](ATConsoleBeamPosition&) { return false; });
	hooks.OpenLog(testFile.mPath.c_str());
	ATConsoleWrite("first\nsecond\n");
	ATConsolePrintf("formatted=%d\n", 3);
	ATConsoleTaggedPrintf("untimed=%s\n", "yes");
	ATConsoleWrite("last");
	hooks.CloseLog();
	AT_PORTABLE_TEST_ASSERT(context, sink.mWriteCount == writesBeforeLog);
	const char expectedLog[] = "first\r\nsecond\r\nformatted=3\r\nuntimed=yes\r\nlast";
	{
		VDFileStream file(testFile.mPath.c_str(), nsVDFile::kRead | nsVDFile::kOpenExisting);
		AT_PORTABLE_TEST_ASSERT(context, file.Length() == sizeof expectedLog - 1);
		char contents[sizeof expectedLog - 1];
		file.Read(contents, sizeof contents);
		AT_PORTABLE_TEST_ASSERT(context, !std::memcmp(contents, expectedLog, sizeof contents));
	}

	// A failed replacement must leave the console usable and must not strand
	// the file resources from the previous (already closed) log.
	VDStringW impossiblePath = VDMakePath(testFile.mPath.c_str(), L"child.log");
	bool openFailed = false;
	try {
		ATConsoleOpenLogFile(impossiblePath.c_str());
	} catch(const VDException&) {
		openFailed = true;
	}
	AT_PORTABLE_TEST_ASSERT(context, openFailed);
	hooks.OpenLog(testFile.mPath.c_str());
	ATConsoleWrite("reopened");
	hooks.CloseLog();
	AT_PORTABLE_TEST_ASSERT(context, VDDoesPathExist(testFile.mPath.c_str()));
	return true;
}
