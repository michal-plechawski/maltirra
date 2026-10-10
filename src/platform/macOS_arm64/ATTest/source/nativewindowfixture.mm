// Real AppKit views, without opening windows or starting NSApplication.

#import <AppKit/AppKit.h>
#include <at/attest/nativewindowfixture.h>

namespace {
	class Fixture final : public IATNativeWindowProxyFixture {
	public:
		Fixture() {
			mpParent = [[NSView alloc] initWithFrame:NSMakeRect(40, 60, 500, 400)];
			// Nonzero, unflipped parent bounds catch assumptions about both axes.
			[mpParent setBoundsOrigin:NSMakePoint(17, 29)];
			mpChild = [[NSTextField alloc] initWithFrame:NSZeroRect];
			[mpChild setBordered:NO];
			mpSibling = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, 10, 10)];
			[mpParent addSubview:mpChild];
			[mpParent addSubview:mpSibling];
			mProxy = ATUINativeWindowProxy(reinterpret_cast<VDGUIHandle>(mpChild));
			mSibling = ATUINativeWindowProxy(reinterpret_cast<VDGUIHandle>(mpSibling));
		}
		~Fixture() {
			[mpChild release];
			[mpSibling release];
			[mpParent release];
		}
		ATUINativeWindowProxy& GetProxy() override { return mProxy; }
		ATUINativeWindowProxy& GetSibling() override { return mSibling; }
		bool IsAboveSibling() const override {
			NSArray *views = [mpParent subviews];
			return [views indexOfObjectIdenticalTo:mpChild] > [views indexOfObjectIdenticalTo:mpSibling];
		}
	private:
		NSView *mpParent;
		NSTextField *mpChild;
		NSView *mpSibling;
		ATUINativeWindowProxy mProxy, mSibling;
	};
}
std::unique_ptr<IATNativeWindowProxyFixture> ATCreateNativeWindowProxyFixture() {
	return std::make_unique<Fixture>();
}
