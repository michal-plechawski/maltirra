#ifndef f_AT_CONSOLETEXT_H
#define f_AT_CONSOLETEXT_H

#include <stdio.h>

#include <vd2/system/function.h>
#include <vd2/system/vdtypes.h>

class IATUIDebuggerConsoleWindow {
public:
	virtual void Write(const char *s) = 0;
	virtual void ShowEnd() = 0;
};

struct ATConsoleBeamPosition {
	uint32 mFrame;
	uint32 mVPos;
	uint32 mHPos;
};

IATUIDebuggerConsoleWindow *ATConsoleGetNativeTextOutput();
bool ATConsoleGetNativeBeamPosition(ATConsoleBeamPosition& position);

IATUIDebuggerConsoleWindow *ATConsoleSetTextOutput(IATUIDebuggerConsoleWindow *output);
vdfunction<bool(ATConsoleBeamPosition&)> ATConsoleSetBeamPositionProvider(vdfunction<bool(ATConsoleBeamPosition&)> provider);

void ATConsoleOpenLogFile(const wchar_t *path);
void ATConsoleCloseLogFileNT();
void ATConsoleCloseLogFile();

void ATConsoleWrite(const char *s);
void ATConsolePrintfImpl(const char *format, ...);
void ATConsoleTaggedPrintfImpl(const char *format, ...);

#define ATConsolePrintf(format, ...) ATConsolePrintfImpl(((void)sizeof printf(format __VA_OPT__(,) __VA_ARGS__), (format)) __VA_OPT__(,) __VA_ARGS__)
#define ATConsoleTaggedPrintf(format, ...) ATConsoleTaggedPrintfImpl(((void)sizeof printf(format __VA_OPT__(,) __VA_ARGS__), (format)) __VA_OPT__(,) __VA_ARGS__)

// Formatters use a 3072-byte buffer including the terminator. If the formatted
// output does not fit, the whole message is discarded, matching legacy behavior.

#endif
