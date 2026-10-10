// Real hidden NSWindow integration: frame/client/screen geometry and close veto.

#import <AppKit/AppKit.h>
#include <at/attest/portabletest.h>
#include <at/atnativeui/nativewindowproxy.h>
#include <vd2/VDDisplay/display_macos.h>
#include <vd2/VDDisplay/displayview_macos.h>

@interface ATMacProxyTestDelegate : NSObject <NSWindowDelegate> {
@public
	BOOL allowClose;
	int closeRequests;
	int closes;
}
@end
@implementation ATMacProxyTestDelegate
- (BOOL)windowShouldClose:(NSWindow *)sender { ++closeRequests; return allowClose; }
- (void)windowWillClose:(NSNotification *)notification { ++closes; }
@end

namespace {
	struct WindowScope {
		NSWindow *mpWindow;
		~WindowScope() { [mpWindow setDelegate:nil]; [mpWindow close]; [mpWindow release]; }
	};
	struct DisplayScope {
		IVDVideoDisplay *mpDisplay;
		~DisplayScope() { if (mpDisplay) mpDisplay->Destroy(); }
	};
}

bool ATTestNativeUIWindowProxyMac(ATPortableTestContext& context) {
	@autoreleasepool {
		[NSApplication sharedApplication];
		NSWindow *window = [[NSWindow alloc] initWithContentRect:NSMakeRect(100, 120, 320, 200)
			styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskResizable
			backing:NSBackingStoreBuffered defer:YES];
		AT_PORTABLE_TEST_ASSERT(context, window != nil);
		[window setReleasedWhenClosed:NO];
		WindowScope windowScope { window };
		auto *delegate = [[[ATMacProxyTestDelegate alloc] init] autorelease];
		[window setDelegate:delegate];
		ATUINativeWindowProxy proxy(reinterpret_cast<VDGUIHandle>(window));
		AT_PORTABLE_TEST_ASSERT(context, !proxy.IsVisible());
		const auto before = proxy.GetArea();
		AT_PORTABLE_TEST_ASSERT(context, before.left == 100);
		AT_PORTABLE_TEST_ASSERT(context, before.top == (sint32)(NSMaxY([[[NSScreen screens] firstObject] frame]) - NSMaxY([window frame])));
		AT_PORTABLE_TEST_ASSERT(context, proxy.GetClientSize().w == 320 && proxy.GetClientSize().h == 200);
		AT_PORTABLE_TEST_ASSERT(context, proxy.GetSize().h > proxy.GetClientSize().h);
		proxy.SetSize({410, 290});
		AT_PORTABLE_TEST_ASSERT(context, proxy.GetPosition().x == before.left && proxy.GetPosition().y == before.top);
		AT_PORTABLE_TEST_ASSERT(context, proxy.GetSize().w == 410 && proxy.GetSize().h == 290);
		proxy.SetPosition({-90, -170});
		AT_PORTABLE_TEST_ASSERT(context, proxy.GetPosition().x == -90 && proxy.GetPosition().y == -170);
		proxy.SetPosition({130, 150});
		AT_PORTABLE_TEST_ASSERT(context, proxy.GetWindowArea().left == 130 && proxy.GetWindowArea().top == 150);
		AT_PORTABLE_TEST_ASSERT(context, proxy.GetSize().w == 410 && proxy.GetSize().h == 290);
		proxy.SetCaption(L"Altirra \u0105\u03A9 \U0001F680");
		AT_PORTABLE_TEST_ASSERT(context, proxy.GetCaption() == L"Altirra \u0105\u03A9 \U0001F680");

		// Attach the production video view to a real window. Its top-left geometry
		// and screen transforms must work under an unflipped content view.
		VDGUIHandle viewHandle = VDCreateDisplayViewMac();
		AT_PORTABLE_TEST_ASSERT(context, viewHandle != nullptr);
		NSView *view = reinterpret_cast<NSView *>(viewHandle);
		[[window contentView] addSubview:view];
		VDDestroyDisplayViewMac(viewHandle); // The hierarchy now owns the view.
		ATUINativeWindowProxy child(viewHandle);
		child.SetArea({11, 23, 104, 84});
		AT_PORTABLE_TEST_ASSERT(context, child.GetPosition().x == 11 && child.GetPosition().y == 23);
		const auto windowOrigin = proxy.TransformClientToScreen(vdpoint32(0, 0));
		const auto childOrigin = child.TransformClientToScreen(vdpoint32(0, 0));
		AT_PORTABLE_TEST_ASSERT(context, childOrigin.x == windowOrigin.x + 11 && childOrigin.y == windowOrigin.y + 23);
		const auto local = child.TransformScreenToClient(child.TransformClientToScreen(vdpoint32(-7, 15)));
		AT_PORTABLE_TEST_ASSERT(context, local.x == -7 && local.y == 15);
		const auto screen = child.GetWindowArea();
		AT_PORTABLE_TEST_ASSERT(context, screen.left == childOrigin.x && screen.top == childOrigin.y);
		AT_PORTABLE_TEST_ASSERT(context, screen.width() == 93 && screen.height() == 61);
		DisplayScope display { VDCreateVideoDisplayMac(viewHandle) };
		AT_PORTABLE_TEST_ASSERT(context, display.mpDisplay != nullptr);
		display.mpDisplay->SetSourceSolidColor(0x00112233);
		bool captured = false;
		display.mpDisplay->RequestCapture([&](const VDPixmap *px) {
			captured = px && px->w > 0 && px->h > 0
				&& (px->GetPixelRow<uint32>(0)[0] & 0x00FFFFFF) == 0x00112233;
		});
		AT_PORTABLE_TEST_ASSERT(context, captured);

		// Closing a child must never close the enclosing native window.
		child.Close();
		AT_PORTABLE_TEST_ASSERT(context, [view superview] == nil && delegate->closes == 0);
		// Vetoable close requests and unconditional destruction are distinct.
		proxy.Close();
		AT_PORTABLE_TEST_ASSERT(context, delegate->closeRequests == 1 && delegate->closes == 0);
		proxy.Destroy();
		AT_PORTABLE_TEST_ASSERT(context, !proxy.IsValid());
		AT_PORTABLE_TEST_ASSERT(context, delegate->closeRequests == 1 && delegate->closes == 1);
	}
	return true;
}
