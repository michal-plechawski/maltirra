#import <AppKit/AppKit.h>
#include <console_macos.h>
#include <at/atnativeui/uinativewindow.h>
#include <vd2/system/refcount.h>

namespace {
	class ATMacConsoleWindow final : public ATUINativeWindow, public IATUIDebuggerConsoleWindow {
	public:
		bool HasOutput() const { return IsValid() && mpTextView && !mbClosing; }
		VDGUIHandle GetTextView() const { return reinterpret_cast<VDGUIHandle>(mpTextView); }
		void Write(const char *s) override {
			if (!HasOutput()) return;
			// NSTextStorage notifications can synchronously close the window.
			vdrefptr<ATMacConsoleWindow> pin(this);
			NSTextView *view = [mpTextView retain];
			NSString *text = [[NSString alloc] initWithUTF8String:s];
			// Preserve legacy 8-bit diagnostic messages instead of dropping them.
			if (!text) text = [[NSString alloc] initWithCString:s encoding:NSISOLatin1StringEncoding];
			if (text) {
				NSAttributedString *addition = [[NSAttributedString alloc] initWithString:text
					attributes:@{NSFontAttributeName:[view font], NSForegroundColorAttributeName:[NSColor textColor]}];
				[[view textStorage] appendAttributedString:addition];
				[addition release];
				[text release];
			}
			[view release];
		}
		void ShowEnd() override {
			if (HasOutput()) [mpTextView scrollRangeToVisible:NSMakeRange([[mpTextView string] length], 0)];
		}
	protected:
		bool OnCreate() override {
			NSView *host = reinterpret_cast<NSView *>(GetClientViewHandle());
			mpScrollView = [[NSScrollView alloc] initWithFrame:[host bounds]];
			[mpScrollView setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
			[mpScrollView setHasVerticalScroller:YES];
			[mpScrollView setHasHorizontalScroller:YES];
			mpTextView = [[NSTextView alloc] initWithFrame:[host bounds]];
			[mpTextView setEditable:NO];
			[mpTextView setSelectable:YES];
			[mpTextView setRichText:NO];
			[mpTextView setFont:[NSFont monospacedSystemFontOfSize:12 weight:NSFontWeightRegular]];
			[mpTextView setVerticallyResizable:YES];
			[mpTextView setHorizontallyResizable:YES];
			[mpTextView setMinSize:NSMakeSize(0, 0)];
			[mpTextView setMaxSize:NSMakeSize(CGFLOAT_MAX, CGFLOAT_MAX)];
			[[mpTextView textContainer] setContainerSize:NSMakeSize(CGFLOAT_MAX, CGFLOAT_MAX)];
			[[mpTextView textContainer] setWidthTracksTextView:NO];
			[mpScrollView setDocumentView:mpTextView];
			[host addSubview:mpScrollView];
			return true;
		}
		void OnDestroy() override {
			mbClosing = true;
			[mpScrollView removeFromSuperview];
			[mpScrollView release]; mpScrollView = nil;
			[mpTextView release]; mpTextView = nil;
		}
	private:
		NSScrollView *mpScrollView = nil;
		NSTextView *mpTextView = nil;
		bool mbClosing = false;
	};
	vdrefptr<ATMacConsoleWindow> g_console;
}

IATUIDebuggerConsoleWindow *ATConsoleGetNativeTextOutput() {
	return g_console && g_console->HasOutput() ? g_console.get() : nullptr;
}

bool ATConsoleGetNativeBeamPosition(ATConsoleBeamPosition&) {
	// No running ANTIC has been wired into the native application yet. The
	// shared provider API will supply real machine timing at that integration.
	return false;
}

bool ATConsoleCreateMac(bool visible) {
	if (![NSThread isMainThread]) return false;
	if (ATConsoleGetNativeTextOutput()) {
		if (visible) g_console->Show();
		return true;
	}
	vdrefptr<ATMacConsoleWindow> window(new ATMacConsoleWindow);
	if (!window->CreateWindow({100, 100, 900, 500}, L"Altirra Console", visible)) return false;
	g_console = window;
	return true;
}

void ATOpenConsole() { ATConsoleCreateMac(true); }
void ATShowConsole() { if (ATConsoleCreateMac(true)) g_console->Activate(); }
void ATCloseConsole() {
	vdrefptr<ATMacConsoleWindow> window(g_console);
	g_console = nullptr;
	if (window) window->Destroy();
}
bool ATIsDebugConsoleActive() { return ATConsoleGetNativeTextOutput() != nullptr; }
VDGUIHandle ATConsoleGetWindowHandleMac() { return ATIsDebugConsoleActive() ? g_console->GetWindowHandle() : nullptr; }
VDGUIHandle ATConsoleGetTextViewHandleMac() { return ATIsDebugConsoleActive() ? g_console->GetTextView() : nullptr; }
