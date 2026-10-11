// Native diagnostic text output (not yet a debugger command interpreter).
#ifndef f_AT_CONSOLE_MACOS_H
#define f_AT_CONSOLE_MACOS_H

#include <consoletext.h>
#include <vd2/system/vdtypes.h>

// All console UI and shared output/hooks are serialized on the main thread.
// Hidden creation is useful for embedding/tests; output never creates a window.
bool ATConsoleCreateMac(bool visible = false);
void ATOpenConsole();
void ATShowConsole();
void ATCloseConsole();
bool ATIsDebugConsoleActive();
// Borrowed native handles; invalid after window closure.
VDGUIHandle ATConsoleGetWindowHandleMac();
VDGUIHandle ATConsoleGetTextViewHandleMac();

#endif
