#import <AppKit/AppKit.h>
#include <console_macos.h>
#include <at/attest/portabletest.h>
#include <utility>
#include <string>

namespace {
	struct ConsoleScope {
		IATUIDebuggerConsoleWindow *mpOutput = ATConsoleSetTextOutput(nullptr);
		vdfunction<bool(ATConsoleBeamPosition&)> mBeam = ATConsoleSetBeamPositionProvider({});
		~ConsoleScope() {
			ATCloseConsole();
			ATConsoleSetTextOutput(mpOutput);
			ATConsoleSetBeamPositionProvider(std::move(mBeam));
		}
	};
}

bool ATTestAltirraConsoleMac(ATPortableTestContext& context) {
	@autoreleasepool {
		ConsoleScope scope;
		AT_PORTABLE_TEST_ASSERT(context, !ATIsDebugConsoleActive());
		ATConsoleWrite("no window should be created");
		AT_PORTABLE_TEST_ASSERT(context, !ATConsoleGetWindowHandleMac());
		AT_PORTABLE_TEST_ASSERT(context, ATConsoleCreateMac());
		NSWindow *window = reinterpret_cast<NSWindow *>(ATConsoleGetWindowHandleMac());
		NSTextView *text = reinterpret_cast<NSTextView *>(ATConsoleGetTextViewHandleMac());
		AT_PORTABLE_TEST_ASSERT(context, window && text && ![window isVisible]);
		AT_PORTABLE_TEST_ASSERT(context, ![text isEditable] && [text isSelectable] && ![text isRichText]);
		AT_PORTABLE_TEST_ASSERT(context, [[text font] isFixedPitch]);
		ATConsoleWrite("Zażółć gęślą jaźń 😀\n");
		ATConsolePrintf("value=%d\n", 42);
		ATConsoleTaggedPrintf("untimed\n");
		AT_PORTABLE_TEST_ASSERT(context, [[text string] isEqualToString:@"Zażółć gęślą jaźń 😀\nvalue=42\nuntimed\n"]);
		ATConsoleSetBeamPositionProvider([](ATConsoleBeamPosition& p) { p = {1, 2, 3}; return true; });
		ATConsoleTaggedPrintf("beam");
		AT_PORTABLE_TEST_ASSERT(context, [[text string] hasSuffix:@"(  1:  2,  3) beam"]);
		ATConsoleWrite("\xff");
		AT_PORTABLE_TEST_ASSERT(context, [[text string] hasSuffix:@"beamÿ"]);
		NSScrollView *scroll = [text enclosingScrollView];
		AT_PORTABLE_TEST_ASSERT(context, [scroll hasVerticalScroller] && [scroll hasHorizontalScroller]);
		[window setContentSize:NSMakeSize(600, 240)];
		AT_PORTABLE_TEST_ASSERT(context, NSEqualSizes([scroll frame].size, [[window contentView] bounds].size));
		std::string lines;
		for (int i = 0; i < 100; ++i) lines += "\nscroll test";
		ATConsoleWrite(lines.c_str());
		AT_PORTABLE_TEST_ASSERT(context, [scroll documentVisibleRect].origin.y > 0);
		[window performClose:nil];
		AT_PORTABLE_TEST_ASSERT(context, !ATIsDebugConsoleActive() && !ATConsoleGetNativeTextOutput());
		ATCloseConsole(); ATCloseConsole();
		AT_PORTABLE_TEST_ASSERT(context, ATConsoleCreateMac());
		text = reinterpret_cast<NSTextView *>(ATConsoleGetTextViewHandleMac());
		AT_PORTABLE_TEST_ASSERT(context, [[text string] length] == 0);
		// A native text-storage observer may tear down the console during Write.
		id observer = [[NSNotificationCenter defaultCenter] addObserverForName:NSTextStorageDidProcessEditingNotification
			object:[text textStorage] queue:nil usingBlock:^(NSNotification *) { ATCloseConsole(); }];
		ATConsoleWrite("close in callback");
		[[NSNotificationCenter defaultCenter] removeObserver:observer];
		AT_PORTABLE_TEST_ASSERT(context, !ATIsDebugConsoleActive());
		AT_PORTABLE_TEST_ASSERT(context, ATConsoleCreateMac());
		ATConsoleWrite("reopened");
		text = reinterpret_cast<NSTextView *>(ATConsoleGetTextViewHandleMac());
		AT_PORTABLE_TEST_ASSERT(context, [[text string] isEqualToString:@"reopened"]);
	}
	return true;
}
