// Altirra - native AppKit implementation of nativewindowproxy.cpp.

#import <AppKit/AppKit.h>
#import <objc/runtime.h>
#include <algorithm>
#include <cmath>
#include <at/atnativeui/nativewindowproxy.h>
#include <vd2/system/text.h>

@interface ATMacWindowProxyMetadata : NSObject {
@public
	uint16 windowId;
	BOOL enabled;
	BOOL tabStop;
}
@end
@implementation ATMacWindowProxyMetadata
- (instancetype)init {
	self = [super init];
	if (self) enabled = YES;
	return self;
}
@end

namespace {
	char kMetadataKey;
	id Object(VDGUIHandle handle) { return reinterpret_cast<id>(handle); }
	NSView *View(id object) { return [object isKindOfClass:[NSView class]] ? object : nil; }
	NSWindow *Window(id object) {
		return [object isKindOfClass:[NSWindow class]] ? object : [View(object) window];
	}
	NSView *Client(id object) {
		return View(object) ?: [Window(object) contentView];
	}
	ATMacWindowProxyMetadata *Metadata(id object, bool create = false) {
		if (!object) return nil;
		auto *state = (ATMacWindowProxyMetadata *)objc_getAssociatedObject(object, &kMetadataKey);
		if (!state && create) {
			state = [[[ATMacWindowProxyMetadata alloc] init] autorelease];
			objc_setAssociatedObject(object, &kMetadataKey, state, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
		}
		return state;
	}
	sint32 Integer(CGFloat value) {
		if (!std::isfinite(value)) return 0;
		return (sint32)std::clamp<double>(std::round(value), INT32_MIN, INT32_MAX);
	}
	// Keep the desktop origin at the primary display's upper-left, as on Win32.
	// NSScreen.screens[0] is primary (not mainScreen, which follows keyboard
	// focus). Displays above/left of it consequently retain negative coordinates.
	CGFloat ScreenTop() { return NSMaxY([[[NSScreen screens] firstObject] frame]); }
	vdrect32 ScreenRect(NSRect r) {
		const CGFloat top = ScreenTop();
		return { Integer(NSMinX(r)), Integer(top - NSMaxY(r)), Integer(NSMaxX(r)), Integer(top - NSMinY(r)) };
	}
	NSPoint LocalPoint(NSView *view, const vdpoint32& point) {
		const NSRect bounds = [view bounds];
		return NSMakePoint(NSMinX(bounds) + point.x,
			[view isFlipped] ? NSMinY(bounds) + point.y : NSMaxY(bounds) - point.y);
	}
	vdpoint32 ClientPoint(NSView *view, NSPoint point) {
		const NSRect bounds = [view bounds];
		return { Integer(point.x - NSMinX(bounds)),
			Integer([view isFlipped] ? point.y - NSMinY(bounds) : NSMaxY(bounds) - point.y) };
	}
}

uint16 ATUINativeWindowProxy::GetWindowId() const {
	auto *state = Metadata(Object(mhwnd));
	return state ? state->windowId : 0;
}
void ATUINativeWindowProxy::SetWindowId(uint16 id) {
	if (auto *state = Metadata(Object(mhwnd), true)) state->windowId = id;
}
bool ATUINativeWindowProxy::IsVisible() const {
	const id object = Object(mhwnd);
	if (NSView *view = View(object)) return ![view isHidden];
	return [Window(object) isVisible];
}
void ATUINativeWindowProxy::SetVisible(bool visible) { if (visible) Show(); else Hide(); }
void ATUINativeWindowProxy::Show() {
	const id object = Object(mhwnd);
	if (NSView *view = View(object)) [view setHidden:NO];
	else {
		NSWindow *window = Window(object);
		if ([window isMiniaturized]) [window deminiaturize:nil];
		[window orderFront:nil];
	}
}
void ATUINativeWindowProxy::Hide() {
	const id object = Object(mhwnd);
	if (NSView *view = View(object)) [view setHidden:YES];
	else [Window(object) orderOut:nil];
}
void ATUINativeWindowProxy::Activate() {
	Show();
	[Window(Object(mhwnd)) makeKeyAndOrderFront:nil];
	Focus();
}
void ATUINativeWindowProxy::Focus() {
	if (IsEnabled()) [Window(Object(mhwnd)) makeFirstResponder:Client(Object(mhwnd))];
}
void ATUINativeWindowProxy::Close() {
	const id object = Object(mhwnd);
	if (NSView *view = View(object)) {
		if ([view respondsToSelector:@selector(performClose:)]) [(id)view performClose:nil];
		else Destroy();
	} else [Window(object) performClose:nil];
}
void ATUINativeWindowProxy::Destroy() {
	const id object = Object(mhwnd);
	mhwnd = nullptr;
	if (NSView *view = View(object)) [view removeFromSuperview];
	else [Window(object) close];
}
bool ATUINativeWindowProxy::IsEnabled() const {
	const id object = Object(mhwnd);
	if (!object) return false;
	if ([object isKindOfClass:[NSControl class]]) return [(NSControl *)object isEnabled];
	auto *state = Metadata(object);
	return !state || state->enabled;
}
void ATUINativeWindowProxy::SetEnabled(bool enabled) {
	const id object = Object(mhwnd);
	if (auto *state = Metadata(object, true)) state->enabled = enabled;
	if ([object isKindOfClass:[NSControl class]]) [(NSControl *)object setEnabled:enabled];
	else if ([object isKindOfClass:[NSWindow class]]) [(NSWindow *)object setIgnoresMouseEvents:!enabled];
}
bool ATUINativeWindowProxy::IsTabStop() const {
	auto *state = Metadata(Object(mhwnd));
	return state && state->tabStop;
}
void ATUINativeWindowProxy::SetTabStop(bool enabled) {
	const id object = Object(mhwnd);
	if (auto *state = Metadata(object, true)) state->tabStop = enabled;
	if ([object isKindOfClass:[NSControl class]]) [(NSControl *)object setRefusesFirstResponder:!enabled];
}
vdrect32 ATUINativeWindowProxy::GetArea() const {
	const id object = Object(mhwnd);
	if (!object) return {0, 0, 0, 0};
	if (NSView *view = View(object)) {
		const NSRect frame = [view frame];
		NSView *parent = [view superview];
		const NSRect bounds = parent ? [parent bounds] : NSZeroRect;
		const CGFloat left = NSMinX(frame) - NSMinX(bounds);
		const CGFloat top = parent && ![parent isFlipped]
			? NSMaxY(bounds) - NSMaxY(frame) : NSMinY(frame) - NSMinY(bounds);
		return { Integer(left), Integer(top), Integer(left + NSWidth(frame)), Integer(top + NSHeight(frame)) };
	}
	return ScreenRect([Window(object) frame]);
}
void ATUINativeWindowProxy::SetArea(const vdrect32& area) {
	const id object = Object(mhwnd);
	if (!object) return;
	const double width = std::max<double>(0, (double)area.right - area.left);
	const double height = std::max<double>(0, (double)area.bottom - area.top);
	if (NSView *view = View(object)) {
		NSView *parent = [view superview];
		const NSRect bounds = parent ? [parent bounds] : NSZeroRect;
		const CGFloat y = parent && ![parent isFlipped]
			? NSMaxY(bounds) - area.top - height : NSMinY(bounds) + area.top;
		[view setFrame:NSMakeRect(NSMinX(bounds) + area.left, y, width, height)];
	} else {
		[Window(object) setFrame:NSMakeRect(area.left, ScreenTop() - area.top - height, width, height) display:YES];
	}
}
vdsize32 ATUINativeWindowProxy::GetSize() const { const auto r = GetArea(); return {r.width(), r.height()}; }
vdpoint32 ATUINativeWindowProxy::GetPosition() const { return GetArea().top_left(); }
void ATUINativeWindowProxy::SetPosition(const vdpoint32& p) {
	const auto r = GetArea();
	SetArea({p.x, p.y, p.x + r.width(), p.y + r.height()});
}
void ATUINativeWindowProxy::SetSize(const vdsize32& size) {
	const auto p = GetPosition();
	SetArea({p.x, p.y, p.x + std::max<sint32>(0, size.w), p.y + std::max<sint32>(0, size.h)});
}
vdsize32 ATUINativeWindowProxy::GetClientSize() const {
	const NSRect bounds = [Client(Object(mhwnd)) bounds];
	return { Integer(NSWidth(bounds)), Integer(NSHeight(bounds)) };
}
vdrect32 ATUINativeWindowProxy::GetClientArea() const {
	const auto size = GetClientSize(); return {0, 0, size.w, size.h};
}
vdrect32 ATUINativeWindowProxy::GetWindowArea() const {
	const id object = Object(mhwnd);
	NSWindow *window = Window(object);
	if (!window) return {0, 0, 0, 0};
	if (NSView *view = View(object))
		return ScreenRect([window convertRectToScreen:[view convertRect:[view bounds] toView:nil]]);
	return ScreenRect([window frame]);
}
vdpoint32 ATUINativeWindowProxy::TransformClientToScreen(const vdpoint32& p) const {
	const id object = Object(mhwnd);
	NSWindow *window = Window(object);
	NSView *view = Client(object);
	if (!window || !view) return p;
	const NSPoint point = [window convertPointToScreen:[view convertPoint:LocalPoint(view, p) toView:nil]];
	return {Integer(point.x), Integer(ScreenTop() - point.y)};
}
vdpoint32 ATUINativeWindowProxy::TransformScreenToClient(const vdpoint32& p) const {
	const id object = Object(mhwnd);
	NSWindow *window = Window(object);
	NSView *view = Client(object);
	if (!window || !view) return p;
	const NSPoint point = [window convertPointFromScreen:NSMakePoint(p.x, ScreenTop() - p.y)];
	return ClientPoint(view, [view convertPoint:point fromView:nil]);
}
vdrect32 ATUINativeWindowProxy::TransformClientToScreen(const vdrect32& r) const {
	const auto a = TransformClientToScreen(r.top_left()), b = TransformClientToScreen(r.bottom_right());
	return {a.x, a.y, b.x, b.y};
}
vdrect32 ATUINativeWindowProxy::TransformScreenToClient(const vdrect32& r) const {
	const auto a = TransformScreenToClient(r.top_left()), b = TransformScreenToClient(r.bottom_right());
	return {a.x, a.y, b.x, b.y};
}
void ATUINativeWindowProxy::BringToFront() {
	const id object = Object(mhwnd);
	if (NSView *view = View(object)) [[view superview] addSubview:view positioned:NSWindowAbove relativeTo:nil];
	else [Window(object) orderFront:nil];
}
void ATUINativeWindowProxy::InsertBelow(VDGUIHandle reference) {
	const id object = Object(mhwnd), other = Object(reference);
	if (!object || !other || object == other) return;
	if (NSView *view = View(object)) {
		NSView *sibling = View(other);
		NSView *parent = [view superview];
		if (parent && [sibling superview] == parent)
			[parent addSubview:view positioned:NSWindowBelow relativeTo:sibling];
	} else if (!View(other)) {
		[Window(object) orderWindow:NSWindowBelow relativeTo:[Window(other) windowNumber]];
	}
}
VDStringW ATUINativeWindowProxy::GetCaption() const {
	const id object = Object(mhwnd);
	NSString *text = View(object) ? [View(object) accessibilityLabel] : [Window(object) title];
	if ([object isKindOfClass:[NSControl class]]) text = [(NSControl *)object stringValue];
	return text ? VDTextU8ToW(VDStringSpanA([text UTF8String])) : VDStringW();
}
void ATUINativeWindowProxy::SetCaption(const wchar_t *caption) {
	const id object = Object(mhwnd);
	const VDStringA utf8 = VDTextWToU8(VDStringSpanW(caption ? caption : L""));
	NSString *text = [NSString stringWithUTF8String:utf8.c_str()] ?: @"";
	if (NSView *view = View(object)) {
		[view setAccessibilityLabel:text];
		if ([view isKindOfClass:[NSControl class]]) [(NSControl *)view setStringValue:text];
	} else [Window(object) setTitle:text];
}
void ATUINativeWindowProxy::Invalidate() { [Client(Object(mhwnd)) setNeedsDisplay:YES]; }
void ATUINativeWindowProxy::InvalidateArea(const vdrect32& r) {
	if (r.right <= r.left || r.bottom <= r.top) return;
	NSView *view = Client(Object(mhwnd));
	const NSPoint origin = LocalPoint(view, {r.left, [view isFlipped] ? r.top : r.bottom});
	[view setNeedsDisplayInRect:NSMakeRect(origin.x, origin.y, (double)r.right - r.left, (double)r.bottom - r.top)];
}
