// Altirra - AppKit native windows, child views, lifecycle and event dispatch.

#import <AppKit/AppKit.h>
#import <dispatch/dispatch.h>
#import <Carbon/Carbon.h>
#import <IOKit/hidsystem/IOLLEvent.h>
#include <algorithm>
#include <vector>
#include <at/atnativeui/uinativewindow.h>
#include <vd2/system/refcount.h>

struct ATMacNativeWindowState {
	ATUINativeWindow *mpOwner = nullptr;
	NSView *mpView = nil; // Borrowed; the hierarchy owns child views.
	NSWindow *mpWindow = nil; // Owned alloc reference only for top-level windows.
	std::vector<std::weak_ptr<ATMacNativeWindowState>> mChildren;
	std::weak_ptr<ATMacNativeWindowState> mParent;
	bool mbCreating = true;
	bool mbDestroying = false;
	bool mbNativeRef = false;
	bool mbEnteredCreate = false;
	bool mbDestroyQueued = false;
	uint32 mPaintDepth = 0;
	bool IsEnding() const {
		if (mbDestroying || mbDestroyQueued) return true;
		const auto parent = mParent.lock();
		return parent && parent->IsEnding();
	}
	bool IsInteractive() const {
		if (!mpOwner || mbCreating || IsEnding() || !mpOwner->IsEnabled()) return false;
		const auto parent = mParent.lock();
		if (parent && !parent->IsInteractive()) return false;
		for(NSView *p = [mpView superview]; p; p = [p superview])
			if (!ATUINativeWindowProxy(reinterpret_cast<VDGUIHandle>(p)).IsEnabled()) return false;
		NSWindow *window = [mpView window];
		return !window || ATUINativeWindowProxy(reinterpret_cast<VDGUIHandle>(window)).IsEnabled();
	}
	bool IsPainting() const {
		if (mPaintDepth) return true;
		for(const auto& weak : mChildren)
			if (const auto c = weak.lock(); c && c->mParent.lock().get() == this && c->IsPainting()) return true;
		return false;
	}

	void Resize() {
		if (!mpOwner || mbCreating || IsEnding()) return;
		const vdrefptr<ATUINativeWindow> pin(mpOwner);
		try { pin->OnSize(); } catch (...) {}
	}
	bool Input(const ATNativeInputEvent& event) {
		if (!IsInteractive()) return true;
		const vdrefptr<ATUINativeWindow> pin(mpOwner);
		try { return pin->OnInput(event); } catch (...) { return true; }
	}
	void Focus(bool focused) {
		if (!mpOwner || mbCreating || IsEnding()) return;
		const vdrefptr<ATUINativeWindow> pin(mpOwner);
		try { if (focused) pin->OnSetFocus(); else pin->OnKillFocus(); } catch (...) {}
	}
	void Paint(CGContextRef context, const vdrect32& rect) {
		if (!mpOwner || mbCreating || IsEnding()) return;
		const vdrefptr<ATUINativeWindow> pin(mpOwner);
		++mPaintDepth;
		try { pin->OnPaint(reinterpret_cast<VDGUIHandle>(context), rect); } catch (...) {}
		--mPaintDepth;
	}
	ATUITouchMode TouchMode(const vdpoint32& point) const {
		if (!mpOwner || IsEnding()) return kATUITouchMode_Default;
		const vdrefptr<ATUINativeWindow> pin(mpOwner);
		try {
			if (pin->mTouchMode == kATUITouchMode_Dynamic || pin->mTouchMode == kATUITouchMode_MultiTouchDynamic)
				return pin->GetTouchModeAtPoint(point);
			return pin->mTouchMode;
		} catch (...) { return kATUITouchMode_Default; }
	}
};

@interface ATMacNativeHostView : NSView <NSTextInputClient> {
@public
	std::shared_ptr<ATMacNativeWindowState> _state;
	NSMutableAttributedString *_markedText;
	NSRange _selection;
	NSMutableDictionary *_touchIdentities;
	uintptr _nextTouchId;
	NSTrackingArea *_trackingArea;
}
- (void)atDestroyNativeWindow;
- (void)performClose:(id)sender;
@end
@interface ATMacNativeHostWindow : NSWindow {
@public
	std::shared_ptr<ATMacNativeWindowState> _state;
}
- (void)atDestroyNativeWindow;
@end

namespace {
	struct NativeReceiverPin {
		id mObject;
		explicit NativeReceiverPin(id object) : mObject([object retain]) {}
		~NativeReceiverPin() { [mObject release]; }
	};
	std::u32string DecodeText(NSString *string) {
		std::u32string result;
		for(NSUInteger i = 0; i < [string length]; ++i) {
			uint32 cp = [string characterAtIndex:i];
			if (cp >= 0xD800 && cp <= 0xDBFF && i + 1 < [string length]) {
				const uint32 low = [string characterAtIndex:i + 1];
				if (low >= 0xDC00 && low <= 0xDFFF) { ++i; cp = 0x10000 + ((cp - 0xD800) << 10) + low - 0xDC00; }
			}
			result.push_back(cp >= 0xD800 && cp <= 0xDFFF ? 0xFFFD : cp);
		}
		return result;
	}
	std::shared_ptr<ATMacNativeWindowState> StateFor(id object) {
		if ([object isKindOfClass:[ATMacNativeHostView class]]) return ((ATMacNativeHostView *)object)->_state;
		if ([object isKindOfClass:[ATMacNativeHostWindow class]]) return ((ATMacNativeHostWindow *)object)->_state;
		return {};
	}
	std::shared_ptr<ATMacNativeWindowState> ParentStateFor(NSView *view) {
		for(NSView *p = view; p; p = [p superview])
			if (auto s = StateFor(p)) return s;
		return {};
	}
	uint32 Modifiers(NSEvent *event) {
		const NSEventModifierFlags f = [event modifierFlags];
		return ((f & NSEventModifierFlagShift) ? kATNativeInputShift : 0)
			| ((f & NSEventModifierFlagControl) ? kATNativeInputControl : 0)
			| ((f & NSEventModifierFlagOption) ? kATNativeInputAlt : 0)
			| ((f & NSEventModifierFlagCommand) ? kATNativeInputCommand : 0)
			| ((f & NSEventModifierFlagCapsLock) ? kATNativeInputCapsLock : 0)
			| ((f & NSEventModifierFlagFunction) ? kATNativeInputFunction : 0);
	}
	bool ModifierPressed(NSEvent *event) {
		uint32 own = 0, group = 0;
		NSEventModifierFlags independent = 0;
		switch([event keyCode]) {
			case kVK_Shift: own = NX_DEVICELSHIFTKEYMASK; group = own | NX_DEVICERSHIFTKEYMASK; independent = NSEventModifierFlagShift; break;
			case kVK_RightShift: own = NX_DEVICERSHIFTKEYMASK; group = own | NX_DEVICELSHIFTKEYMASK; independent = NSEventModifierFlagShift; break;
			case kVK_Control: own = NX_DEVICELCTLKEYMASK; group = own | NX_DEVICERCTLKEYMASK; independent = NSEventModifierFlagControl; break;
			case kVK_RightControl: own = NX_DEVICERCTLKEYMASK; group = own | NX_DEVICELCTLKEYMASK; independent = NSEventModifierFlagControl; break;
			case kVK_Option: own = NX_DEVICELALTKEYMASK; group = own | NX_DEVICERALTKEYMASK; independent = NSEventModifierFlagOption; break;
			case kVK_RightOption: own = NX_DEVICERALTKEYMASK; group = own | NX_DEVICELALTKEYMASK; independent = NSEventModifierFlagOption; break;
			case kVK_Command: own = NX_DEVICELCMDKEYMASK; group = own | NX_DEVICERCMDKEYMASK; independent = NSEventModifierFlagCommand; break;
			case kVK_RightCommand: own = NX_DEVICERCMDKEYMASK; group = own | NX_DEVICELCMDKEYMASK; independent = NSEventModifierFlagCommand; break;
			case kVK_CapsLock: independent = NSEventModifierFlagCapsLock; break;
			case kVK_Function: independent = NSEventModifierFlagFunction; break;
		}
		const auto flags = [event modifierFlags];
		return (flags & group) ? (flags & own) != 0 : (flags & independent) != 0;
	}
	ATNativeInputEvent KeyEvent(NSEvent *event, ATNativeInputType type) {
		ATNativeInputEvent e;
		e.mType = type;
		e.mNativeKeyCode = [event keyCode];
		e.mModifiers = Modifiers(event);
		e.mbRepeat = [event type] == NSEventTypeKeyDown && [event isARepeat];
		return e;
	}
	ATNativeInputEvent PointerEvent(NSView *view, NSEvent *event, ATNativeInputType type) {
		ATNativeInputEvent e;
		e.mType = type;
		e.mModifiers = Modifiers(event);
		const NSPoint p = [view convertPoint:[event locationInWindow] fromView:nil];
		const NSRect bounds = [view bounds];
		e.mPosition = vdpoint32((sint32)(p.x - NSMinX(bounds)), (sint32)(p.y - NSMinY(bounds)));
		switch([event type]) {
			case NSEventTypeLeftMouseDown: case NSEventTypeLeftMouseUp:
			case NSEventTypeRightMouseDown: case NSEventTypeRightMouseUp:
			case NSEventTypeOtherMouseDown: case NSEventTypeOtherMouseUp:
				e.mButton = (uint32)[event buttonNumber];
				e.mClickCount = (uint32)[event clickCount];
				break;
			case NSEventTypeLeftMouseDragged: e.mButton = 0; break;
			case NSEventTypeRightMouseDragged: e.mButton = 1; break;
			case NSEventTypeOtherMouseDragged: e.mButton = (uint32)[event buttonNumber]; break;
			default: break;
		}
		return e;
	}
}

@implementation ATMacNativeHostView
- (BOOL)isFlipped { return YES; }
- (void)atBindParent {
	const auto s = _state;
	if (!s || s->mbDestroying || s->mpWindow) return;
	const auto parent = ParentStateFor([self superview]);
	if (parent != s->mParent.lock()) {
		s->mParent = parent;
		if (parent) parent->mChildren.push_back(s);
	}
}
- (void)viewDidMoveToSuperview { [super viewDidMoveToSuperview]; [self atBindParent]; }
- (void)viewDidMoveToWindow { [super viewDidMoveToWindow]; [self atBindParent]; }
- (void)updateTrackingAreas {
	[super updateTrackingAreas];
	if (_trackingArea) { [self removeTrackingArea:_trackingArea]; [_trackingArea release]; }
	_trackingArea = [[NSTrackingArea alloc] initWithRect:NSZeroRect
		options:NSTrackingMouseMoved | NSTrackingMouseEnteredAndExited | NSTrackingActiveInKeyWindow | NSTrackingInVisibleRect
		owner:self userInfo:nil];
	[self addTrackingArea:_trackingArea];
}
- (BOOL)acceptsFirstResponder {
	const auto s = _state;
	return s && s->IsInteractive();
}
- (NSView *)hitTest:(NSPoint)point {
	const auto s = _state;
	return s && s->IsInteractive() ? [super hitTest:point] : nil;
}
- (BOOL)canBecomeKeyView {
	const auto s = _state;
	return [self acceptsFirstResponder] && s->mpOwner->IsTabStop();
}
- (BOOL)becomeFirstResponder {
	const NativeReceiverPin receiver(self);
	const auto s = _state;
	if (![self acceptsFirstResponder]) return NO;
	if (s) s->Focus(true);
	return s && s->mpOwner && !s->mbDestroying;
}
- (BOOL)resignFirstResponder {
	const NativeReceiverPin receiver(self);
	const auto s = _state;
	if (s) s->Focus(false);
	return YES;
}
- (void)setFrameSize:(NSSize)size {
	const NativeReceiverPin receiver(self);
	[super setFrameSize:size];
	const auto s = _state;
	if (s) s->Resize();
}
- (void)setBoundsSize:(NSSize)size {
	const NativeReceiverPin receiver(self);
	[super setBoundsSize:size];
	const auto s = _state;
	if (s) s->Resize();
}
- (void)drawRect:(NSRect)rect {
	const NativeReceiverPin receiver(self);
	const auto s = _state;
	if (s) s->Paint([[NSGraphicsContext currentContext] CGContext],
		vdrect32((sint32)NSMinX(rect), (sint32)NSMinY(rect), (sint32)NSMaxX(rect), (sint32)NSMaxY(rect)));
}
- (void)atDestroyNativeWindow {
	const NativeReceiverPin receiver(self);
	const auto s = _state;
	if (s && s->mpOwner) s->mpOwner->Destroy();
}
- (void)performClose:(id)sender {
	const NativeReceiverPin receiver(self);
	const auto s = _state;
	if (s && s->mpOwner) s->mpOwner->Close();
}
- (void)keyDown:(NSEvent *)event {
	const NativeReceiverPin receiver(self);
	const auto s = _state;
	if (!s || !s->mpOwner) return;
	if (!s->Input(KeyEvent(event, ATNativeInputType::KeyDown)) && s->mpOwner && !s->mbDestroying)
		[self interpretKeyEvents:@[event]];
}
- (void)keyUp:(NSEvent *)event {
	const NativeReceiverPin receiver(self);
	const auto s = _state;
	if (s) s->Input(KeyEvent(event, ATNativeInputType::KeyUp));
}
- (void)flagsChanged:(NSEvent *)event {
	const NativeReceiverPin receiver(self);
	// Preserve native modifier transitions for the emulator's key mapper.
	const auto s = _state;
	if (s) s->Input(KeyEvent(event, ModifierPressed(event) ? ATNativeInputType::KeyDown : ATNativeInputType::KeyUp));
}
- (void)insertText:(id)text {
	const NativeReceiverPin receiver(self);
	NSString *string = [text isKindOfClass:[NSAttributedString class]] ? [text string] : text;
	const auto s = _state;
	[self unmarkText];
	for(const uint32 cp : DecodeText(string)) {
		ATNativeInputEvent e;
		e.mType = ATNativeInputType::Text;
		e.mCodePoint = cp;
		if (s) s->Input(e);
	}
}
- (void)insertText:(id)text replacementRange:(NSRange)range { [self insertText:text]; }
- (BOOL)hasMarkedText { return [_markedText length] != 0; }
- (NSRange)markedRange { return [self hasMarkedText] ? NSMakeRange(0, [_markedText length]) : NSMakeRange(NSNotFound, 0); }
- (NSRange)selectedRange { return [self hasMarkedText] ? _selection : NSMakeRange(0, 0); }
- (void)setMarkedText:(id)text selectedRange:(NSRange)selection replacementRange:(NSRange)range {
	const NativeReceiverPin receiver(self);
	NSMutableAttributedString *value = [text isKindOfClass:[NSAttributedString class]]
		? [text mutableCopy] : [[NSMutableAttributedString alloc] initWithString:text];
	[_markedText release];
	_markedText = value;
	_selection = NSMakeRange(std::min(selection.location, [value length]), 0);
	_selection.length = std::min(selection.length, [value length] - _selection.location);
	const auto s = _state;
	ATNativeInputEvent e;
	e.mType = ATNativeInputType::Composition;
	e.mComposition = DecodeText([value string]);
	if (s) s->Input(e);
}
- (void)unmarkText {
	const NativeReceiverPin receiver(self);
	if (!_markedText) return;
	[_markedText release];
	_markedText = nil;
	const auto s = _state;
	ATNativeInputEvent e;
	e.mType = ATNativeInputType::Composition;
	if (s) s->Input(e);
}
- (NSArray<NSAttributedStringKey> *)validAttributesForMarkedText { return @[]; }
- (NSAttributedString *)attributedSubstringForProposedRange:(NSRange)range actualRange:(NSRangePointer)actual {
	if (![self hasMarkedText] || range.location >= [_markedText length]) { if (actual) *actual = NSMakeRange(NSNotFound, 0); return nil; }
	const NSRange clipped = NSMakeRange(range.location, std::min(range.length, [_markedText length] - range.location));
	if (actual) *actual = clipped;
	return [_markedText attributedSubstringFromRange:clipped];
}
- (NSUInteger)characterIndexForPoint:(NSPoint)point { return 0; }
- (NSRect)firstRectForCharacterRange:(NSRange)range actualRange:(NSRangePointer)actual {
	if (actual) *actual = NSMakeRange(0, 0);
	return [[self window] convertRectToScreen:[self convertRect:NSMakeRect(0, 0, 1, 16) toView:nil]];
}
- (void)doCommandBySelector:(SEL)selector {
	if (selector == @selector(insertNewline:)) [self insertText:@"\r"];
	else if (selector == @selector(insertTab:)) [self insertText:@"\t"];
	else if (selector == @selector(deleteBackward:)) [self insertText:@"\b"];
}
- (void)mouseDown:(NSEvent *)event {
	const NativeReceiverPin receiver(self);
	const auto s = _state;
	if (s && s->mpOwner && s->mpOwner->IsEnabled()) {
		[[self window] makeFirstResponder:self];
		s->Input(PointerEvent(self, event, ATNativeInputType::PointerDown));
	}
}
- (void)mouseUp:(NSEvent *)event {
	const NativeReceiverPin receiver(self);
	const auto s = _state; if (s) s->Input(PointerEvent(self, event, ATNativeInputType::PointerUp));
}
- (void)rightMouseDown:(NSEvent *)event { [self mouseDown:event]; }
- (void)rightMouseUp:(NSEvent *)event { [self mouseUp:event]; }
- (void)otherMouseDown:(NSEvent *)event { [self mouseDown:event]; }
- (void)otherMouseUp:(NSEvent *)event { [self mouseUp:event]; }
- (void)mouseMoved:(NSEvent *)event {
	const NativeReceiverPin receiver(self);
	const auto s = _state; if (s) s->Input(PointerEvent(self, event, ATNativeInputType::PointerMove));
}
- (void)mouseDragged:(NSEvent *)event { [self mouseMoved:event]; }
- (void)mouseEntered:(NSEvent *)event { const NativeReceiverPin pin(self); const auto s = _state; if (s) s->Input(PointerEvent(self, event, ATNativeInputType::PointerEnter)); }
- (void)mouseExited:(NSEvent *)event { const NativeReceiverPin pin(self); const auto s = _state; if (s) s->Input(PointerEvent(self, event, ATNativeInputType::PointerLeave)); }
- (void)rightMouseDragged:(NSEvent *)event { [self mouseMoved:event]; }
- (void)otherMouseDragged:(NSEvent *)event { [self mouseMoved:event]; }
- (void)scrollWheel:(NSEvent *)event {
	const NativeReceiverPin receiver(self);
	const auto s = _state;
	ATNativeInputEvent e = PointerEvent(self, event, ATNativeInputType::Scroll);
	// Pointer deltas stay precise; do not manufacture legacy 120-unit wheel ticks.
	e.mScrollX = (float)[event scrollingDeltaX];
	e.mScrollY = (float)[event scrollingDeltaY];
	if (s && s->TouchMode(e.mPosition) == kATUITouchMode_VerticalPan) e.mScrollX = 0;
	if (s) s->Input(e);
}
- (void)atTouches:(NSEvent *)event phase:(uint8)phase {
	const NativeReceiverPin receiver(self);
	const auto s = _state;
	const NSTouchPhase phases[] {NSTouchPhaseBegan, NSTouchPhaseMoved, NSTouchPhaseEnded, NSTouchPhaseCancelled};
	for(NSTouch *touch in [event touchesMatchingPhase:phases[phase] inView:self]) {
		ATNativeInputEvent e;
		e.mType = ATNativeInputType::Touch;
		e.mTouchPhase = phase;
		if (!_touchIdentities) _touchIdentities = [[NSMutableDictionary alloc] init];
		// NSTouch identity objects are compared by value, not pointer address.
		NSNumber *identity = [_touchIdentities objectForKey:[touch identity]];
		if (!identity) { identity = @((uint64)++_nextTouchId); [_touchIdentities setObject:identity forKey:[touch identity]]; }
		e.mPointerId = (uintptr)[identity unsignedLongLongValue];
		e.mModifiers = Modifiers(event);
		if ([touch type] == NSTouchTypeDirect) {
			const NSPoint p = [touch locationInView:self];
			e.mPosition = vdpoint32((sint32)(p.x - NSMinX([self bounds])), (sint32)(p.y - NSMinY([self bounds])));
		} else {
			const NSPoint p = [touch normalizedPosition];
			e.mPosition = vdpoint32((sint32)(p.x * NSWidth([self bounds])), (sint32)((1 - p.y) * NSHeight([self bounds])));
		}
		if (s) s->Input(e);
		if (phase >= 2) [_touchIdentities removeObjectForKey:[touch identity]];
	}
}
- (void)touchesBeganWithEvent:(NSEvent *)event { [self atTouches:event phase:0]; }
- (void)touchesMovedWithEvent:(NSEvent *)event { [self atTouches:event phase:1]; }
- (void)touchesEndedWithEvent:(NSEvent *)event { [self atTouches:event phase:2]; }
- (void)touchesCancelledWithEvent:(NSEvent *)event { [self atTouches:event phase:3]; }
- (void)dealloc {
	const auto s = _state;
	if (s && s->mpView == self) {
		s->mpView = nil;
		if (s->mpOwner) s->mpOwner->Destroy();
	}
	_state.reset();
	[_markedText release];
	[_touchIdentities release];
	[_trackingArea release];
	[super dealloc];
}
@end

@implementation ATMacNativeHostWindow
- (void)atDestroyNativeWindow { const NativeReceiverPin pin(self); const auto s = _state; if (s && s->mpOwner) s->mpOwner->Destroy(); }
- (void)performClose:(id)sender { const NativeReceiverPin pin(self); const auto s = _state; if (s && s->mpOwner) s->mpOwner->Close(); }
- (void)close {
	const NativeReceiverPin receiver(self);
	const auto s = _state;
	if (s && s->mpOwner && !s->mbDestroying) s->mpOwner->Destroy();
	else [super close];
}
@end

ATUINativeWindow::ATUINativeWindow() : mRefCount(0) {}
ATUINativeWindow::~ATUINativeWindow() {}
int ATUINativeWindow::AddRef() { return ++mRefCount; }
int ATUINativeWindow::Release() { const int n = --mRefCount; if (!n) delete this; return n; }
void *ATUINativeWindow::AsInterface(uint32) { return nullptr; }

bool ATUINativeWindow::FinishCreate(const std::shared_ptr<ATMacNativeWindowState>& s, bool visible) {
	const vdrefptr<ATUINativeWindow> pin(this);
	s->mbEnteredCreate = true;
	bool success = false;
	try { success = OnCreate(); } catch (...) { Destroy(); throw; }
	if (!success || s->mbDestroying || !s->mpOwner) { Destroy(); return false; }
	s->mbCreating = false;
	SetTouchMode(mTouchMode);
	s->Resize();
	if (!s->mpOwner || s->mbDestroying) return false;
	SetVisible(visible);
	return true;
}
bool ATUINativeWindow::CreateChild(VDGUIHandle parentHandle, uint16 childId,
	int x, int y, int w, int h, bool visible, const wchar_t *text) {
	if (![NSThread isMainThread] || mpState || !parentHandle || w < 0 || h < 0
		|| (sint64)x + w > INT32_MAX || (sint64)y + h > INT32_MAX) return false;
	id object = reinterpret_cast<id>(parentHandle);
	NSView *parent = [object isKindOfClass:[NSWindow class]] ? [object contentView] : object;
	if (![parent isKindOfClass:[NSView class]]) return false;
	const auto parentState = StateFor(object) ? StateFor(object) : ParentStateFor(parent);
	if (parentState && parentState->mbDestroying) return false;
	const vdrefptr<ATUINativeWindow> pin(this);
	auto s = std::make_shared<ATMacNativeWindowState>();
	auto *view = [[ATMacNativeHostView alloc] initWithFrame:NSZeroRect];
	if (!view) return false;
	view->_state = s;
	s->mpOwner = this;
	s->mpView = view;
	s->mbNativeRef = true;
	AddRef();
	mpState = s;
	mhwnd = reinterpret_cast<VDGUIHandle>(view);
	[view setHidden:YES];
	[parent addSubview:view];
	[view release];
	SetWindowId(childId);
	SetArea({x, y, x + w, y + h});
	SetCaption(text);
	return FinishCreate(s, visible);
}
bool ATUINativeWindow::CreateWindow(const vdrect32& area, const wchar_t *title, bool visible) {
	if (![NSThread isMainThread] || mpState || area.right <= area.left || area.bottom <= area.top) return false;
	const vdrefptr<ATUINativeWindow> pin(this);
	[NSApplication sharedApplication];
	auto s = std::make_shared<ATMacNativeWindowState>();
	auto *window = [[ATMacNativeHostWindow alloc] initWithContentRect:NSMakeRect(0, 0, 100, 100)
		styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskResizable | NSWindowStyleMaskMiniaturizable
		backing:NSBackingStoreBuffered defer:YES];
	if (!window) return false;
	[window setReleasedWhenClosed:NO];
	[window setAcceptsMouseMovedEvents:YES];
	auto *view = [[ATMacNativeHostView alloc] initWithFrame:NSZeroRect];
	if (!view) { [window release]; return false; }
	view->_state = s;
	window->_state = s;
	s->mpOwner = this;
	s->mpView = view;
	s->mpWindow = window;
	s->mbNativeRef = true;
	AddRef();
	mpState = s;
	mhwnd = reinterpret_cast<VDGUIHandle>(window);
	[window setContentView:view];
	[view release];
	SetArea(area);
	SetCaption(title);
	return FinishCreate(s, visible);
}
VDGUIHandle ATUINativeWindow::GetClientViewHandle() const {
	return mpState ? reinterpret_cast<VDGUIHandle>(mpState->mpView) : nullptr;
}
ATUINativeWindow *ATUINativeWindow::FromNativeHandle(VDGUIHandle handle) {
	const auto s = StateFor(reinterpret_cast<id>(handle));
	return s ? s->mpOwner : nullptr;
}
void ATUINativeWindow::Close() {
	const auto s = mpState;
	if (!s || s->mbDestroying) return;
	const vdrefptr<ATUINativeWindow> pin(this);
	bool close = false;
	try { close = OnCloseRequest(); } catch (...) {}
	if (close && s->mpOwner && !s->mbDestroying) Destroy();
}
void ATUINativeWindow::Destroy() {
	const auto s = mpState;
	if (!s || s->mbDestroying) return;
	if (s->IsPainting()) {
		if (!s->mbDestroyQueued) {
			s->mbDestroyQueued = true;
			const std::weak_ptr<ATMacNativeWindowState> weak = s;
			dispatch_async(dispatch_get_main_queue(), ^{
				if (const auto state = weak.lock(); state && state->mpOwner) state->mpOwner->Destroy();
			});
		}
		return;
	}
	const vdrefptr<ATUINativeWindow> pin(this);
	s->mbDestroying = true;
	if (!s->mpView) mhwnd = nullptr; // Native deallocation already started.
	if (s->mbEnteredCreate) { try { OnDestroy(); } catch (...) {} }
	const auto children = std::move(s->mChildren);
	for(const auto& weak : children)
		if (const auto child = weak.lock(); child && child->mpOwner && child->mParent.lock() == s) child->mpOwner->Destroy();
	NSWindow *window = s->mpWindow;
	NSView *view = [s->mpView retain];
	if (window) {
		[window close];
		[window setContentView:nil];
		s->mpWindow = nil;
		[window autorelease]; // Keep an executing AppKit method's receiver alive.
	} else [view removeFromSuperview];
	s->mpView = nil;
	mhwnd = nullptr;
	s->mpOwner = nullptr;
	mpState.reset();
	[view autorelease];
	if (s->mbNativeRef) { s->mbNativeRef = false; Release(); }
}
void ATUINativeWindow::SetTouchMode(ATUITouchMode mode) {
	mTouchMode = mode;
	const auto s = mpState;
	if (!s || !s->mpView || s->mbDestroying) return;
	const bool multi = mode == kATUITouchMode_MultiTouch || mode == kATUITouchMode_MultiTouchImmediate || mode == kATUITouchMode_MultiTouchDynamic;
	[s->mpView setAllowedTouchTypes:multi ? NSTouchTypeMaskDirect | NSTouchTypeMaskIndirect : NSTouchTypeMaskDirect];
}
