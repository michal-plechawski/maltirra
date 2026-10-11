#include <consoletext.h>

#include <stdarg.h>
#include <string.h>
#include <utility>

#include <vd2/system/file.h>
#include <vd2/system/vdalloc.h>

namespace {
	VDFileStream *g_pLogFile = nullptr;
	VDTextOutputStream *g_pLogOutput = nullptr;
	IATUIDebuggerConsoleWindow *g_pTextOutputOverride = nullptr;
	vdfunction<bool(ATConsoleBeamPosition&)> g_beamPositionProviderOverride;

	IATUIDebuggerConsoleWindow *GetTextOutput() {
		return g_pTextOutputOverride ? g_pTextOutputOverride : ATConsoleGetNativeTextOutput();
	}

	bool HasOutput() {
		return GetTextOutput() || g_pLogOutput;
	}

	bool GetBeamPosition(ATConsoleBeamPosition& position) {
		if (g_beamPositionProviderOverride)
			return g_beamPositionProviderOverride(position);

		return ATConsoleGetNativeBeamPosition(position);
	}
}

IATUIDebuggerConsoleWindow *ATConsoleSetTextOutput(IATUIDebuggerConsoleWindow *output) {
	IATUIDebuggerConsoleWindow *previous = g_pTextOutputOverride;
	g_pTextOutputOverride = output;
	return previous;
}

vdfunction<bool(ATConsoleBeamPosition&)> ATConsoleSetBeamPositionProvider(vdfunction<bool(ATConsoleBeamPosition&)> provider) {
	vdfunction<bool(ATConsoleBeamPosition&)> previous(std::move(g_beamPositionProviderOverride));
	g_beamPositionProviderOverride = std::move(provider);
	return previous;
}

void ATConsoleOpenLogFile(const wchar_t *path) {
	ATConsoleCloseLogFile();

	vdautoptr<VDFileStream> fs(new VDFileStream(path, nsVDFile::kWrite | nsVDFile::kDenyAll | nsVDFile::kCreateAlways));
	vdautoptr<VDTextOutputStream> tos(new VDTextOutputStream(fs));

	g_pLogFile = fs.release();
	g_pLogOutput = tos.release();
}

void ATConsoleCloseLogFileNT() {
	vdsafedelete <<= g_pLogOutput;
	vdsafedelete <<= g_pLogFile;
}

void ATConsoleCloseLogFile() {
	try {
		if (g_pLogOutput)
			g_pLogOutput->Flush();

		if (g_pLogFile)
			g_pLogFile->close();
	} catch(...) {
		ATConsoleCloseLogFileNT();
		throw;
	}

	ATConsoleCloseLogFileNT();
}

void ATConsoleWrite(const char *s) {
	if (IATUIDebuggerConsoleWindow *output = GetTextOutput()) {
		output->Write(s);
		// A native notification during Write may destroy or replace the sink.
		if (GetTextOutput() == output)
			output->ShowEnd();
	}

	if (g_pLogOutput) {
		for(;;) {
			const char *lbreak = strchr(s, '\n');

			if (!lbreak) {
				g_pLogOutput->Write(s);
				break;
			}

			g_pLogOutput->PutLine(s, (int)(lbreak - s));

			s = lbreak+1;
		}
	}
}

void ATConsolePrintfImpl(const char *format, ...) {
	if (!HasOutput())
		return;

	char buf[3072];
	va_list val;

	va_start(val, format);
	const int result = vsnprintf(buf, sizeof buf, format, val);
	va_end(val);

	if (result >= 0 && (size_t)result < sizeof buf)
		ATConsoleWrite(buf);
}

void ATConsoleTaggedPrintfImpl(const char *format, ...) {
	if (!HasOutput())
		return;

	char buf[3072];
	size_t prefixLen = 0;
	ATConsoleBeamPosition position {};

	if (GetBeamPosition(position)) {
		const int result = snprintf(buf, sizeof buf, "(%3u:%3u,%3u) ", position.mFrame, position.mVPos, position.mHPos);
		if (result < 0 || (size_t)result >= sizeof buf)
			return;

		prefixLen = (size_t)result;
	}

	va_list val;
	va_start(val, format);
	const int result = vsnprintf(buf + prefixLen, sizeof buf - prefixLen, format, val);
	va_end(val);

	if (result >= 0 && (size_t)result < sizeof buf - prefixLen)
		ATConsoleWrite(buf);
}
