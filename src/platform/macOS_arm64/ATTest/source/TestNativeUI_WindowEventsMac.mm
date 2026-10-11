// Actual AppKit selectors: input/IME/focus, callback teardown, and reparenting.

#import <AppKit/AppKit.h>
#import <IOKit/hidsystem/IOLLEvent.h>
#include <vector>
#include <stdexcept>
#include <at/attest/portabletest.h>
#include <at/atnativeui/uinativewindow.h>
#include <vd2/system/function.h>
#include <vd2/system/refcount.h>

namespace {
	struct Counts {
		int mCreated = 0, mDestroyed = 0, mDeleted = 0, mFocus = 0, mPaint = 0;
		std::vector<ATNativeInputEvent> mEvents;
	};
	class TestWindow final : public ATUINativeWindow {
	public:
		explicit TestWindow(Counts& counts) : mCounts(counts) {}
		~TestWindow() override { ++mCounts.mDeleted; }
		vdfunction<void()> mOnInput, mOnPaint, mOnDestroy;
		bool mbHandleKeys = true;
		bool mbThrowCreate = false;
	protected:
		bool OnCreate() override {
			++mCounts.mCreated;
			if (mbThrowCreate) throw std::runtime_error("create failure");
			return true;
		}
		void OnDestroy() override { ++mCounts.mDestroyed; if (mOnDestroy) mOnDestroy(); }
		void OnSetFocus() override { ++mCounts.mFocus; }
		bool OnInput(const ATNativeInputEvent& event) override {
			mCounts.mEvents.push_back(event);
			if (mOnInput) mOnInput();
			return mbHandleKeys;
		}
		void OnPaint(VDGUIHandle context, const vdrect32&) override {
			++mCounts.mPaint;
			if (mOnPaint) mOnPaint();
			CGContextSetRGBFillColor(reinterpret_cast<CGContextRef>(context), 1, 0, 0, 1);
			CGContextFillRect(reinterpret_cast<CGContextRef>(context), CGRectMake(0, 0, 10, 10));
		}
	private:
		Counts& mCounts;
	};
	struct DestroyWindow {
		vdrefptr<TestWindow>& mWindow;
		~DestroyWindow() { if (mWindow) mWindow->Destroy(); }
	};
	struct ReleaseView {
		NSView *mpView;
		~ReleaseView() { [mpView release]; }
	};
	NSEvent *Key(NSEventType type, uint16 key = 0, NSEventModifierFlags flags = 0) {
		return [NSEvent keyEventWithType:type location:NSZeroPoint modifierFlags:flags timestamp:0
			windowNumber:0 context:nil characters:@"a" charactersIgnoringModifiers:@"a" isARepeat:NO keyCode:key];
	}
	void Draw(NSView *view) {
		uint32 pixels[100] {};
		CGColorSpaceRef colorSpace = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
		CGContextRef context = CGBitmapContextCreate(pixels, 10, 10, 8, 40, colorSpace,
			(CGBitmapInfo)((uint32)kCGBitmapByteOrder32Little | (uint32)kCGImageAlphaNoneSkipFirst));
		CGColorSpaceRelease(colorSpace);
		[NSGraphicsContext saveGraphicsState];
		[NSGraphicsContext setCurrentContext:[NSGraphicsContext graphicsContextWithCGContext:context flipped:YES]];
		[view drawRect:NSMakeRect(0, 0, 10, 10)];
		[NSGraphicsContext restoreGraphicsState];
		CGContextRelease(context);
	}
}

bool ATTestNativeUIWindowEventsMac(ATPortableTestContext& context) {
	@autoreleasepool {
		Counts rootCounts, childCounts;
		vdrefptr<TestWindow> root(new TestWindow(rootCounts)), child(new TestWindow(childCounts));
		DestroyWindow rootGuard { root }, childGuard { child };
		AT_PORTABLE_TEST_ASSERT(context, root->CreateWindow({100, 120, 420, 340}, L"Altirra native test"));
		AT_PORTABLE_TEST_ASSERT(context, child->CreateChild(root->GetClientViewHandle(), 19, 11, 23, 93, 61));
		NSView *view = reinterpret_cast<NSView *>(child->GetClientViewHandle());
		child->SetTabStop(true);
		AT_PORTABLE_TEST_ASSERT(context, [view acceptsFirstResponder] && [view canBecomeKeyView]);
		child->Focus();
		AT_PORTABLE_TEST_ASSERT(context, childCounts.mFocus > 0);
		[view keyDown:Key(NSEventTypeKeyDown, 0, NSEventModifierFlagShift | NSEventModifierFlagOption)];
		AT_PORTABLE_TEST_ASSERT(context, childCounts.mEvents.back().mType == ATNativeInputType::KeyDown);
		AT_PORTABLE_TEST_ASSERT(context, childCounts.mEvents.back().mModifiers == (kATNativeInputShift | kATNativeInputAlt));
		[view keyUp:Key(NSEventTypeKeyUp)];
		AT_PORTABLE_TEST_ASSERT(context, childCounts.mEvents.back().mType == ATNativeInputType::KeyUp);
		// Releasing left Shift while right Shift is held must be a key-up, even
		// though the aggregate Shift modifier remains on.
		[view flagsChanged:Key(NSEventTypeFlagsChanged, 0x38, NSEventModifierFlagShift | NX_DEVICERSHIFTKEYMASK)];
		AT_PORTABLE_TEST_ASSERT(context, childCounts.mEvents.back().mType == ATNativeInputType::KeyUp);
		AT_PORTABLE_TEST_ASSERT(context, childCounts.mEvents.back().mNativeKeyCode == 0x38);
		[view flagsChanged:Key(NSEventTypeFlagsChanged, 0x3C, NSEventModifierFlagShift | NX_DEVICERSHIFTKEYMASK)];
		AT_PORTABLE_TEST_ASSERT(context, childCounts.mEvents.back().mType == ATNativeInputType::KeyDown);

		id<NSTextInputClient> text = (id<NSTextInputClient>)view;
		AT_PORTABLE_TEST_ASSERT(context, [view inputContext] != nil);
		[text setMarkedText:@"\u0105\U0001F680" selectedRange:NSMakeRange(1, 2) replacementRange:NSMakeRange(NSNotFound, 0)];
		AT_PORTABLE_TEST_ASSERT(context, [text hasMarkedText] && [text markedRange].length == 3);
		AT_PORTABLE_TEST_ASSERT(context, childCounts.mEvents.back().mComposition == U"\u0105\U0001F680");
		[text insertText:@"X\U0001F680" replacementRange:NSMakeRange(NSNotFound, 0)];
		AT_PORTABLE_TEST_ASSERT(context, ![text hasMarkedText]);
		const auto n = childCounts.mEvents.size();
		AT_PORTABLE_TEST_ASSERT(context, n >= 3 && childCounts.mEvents[n - 3].mType == ATNativeInputType::Composition);
		AT_PORTABLE_TEST_ASSERT(context, childCounts.mEvents[n - 3].mComposition.empty());
		AT_PORTABLE_TEST_ASSERT(context, childCounts.mEvents[n - 2].mCodePoint == 'X' && childCounts.mEvents[n - 1].mCodePoint == 0x1F680);
		const NSPoint position = [view convertPoint:NSMakePoint(9, 13) toView:nil];
		NSEvent *mouse = [NSEvent mouseEventWithType:NSEventTypeLeftMouseDown location:position
			modifierFlags:0 timestamp:0 windowNumber:[[view window] windowNumber] context:nil eventNumber:1 clickCount:2 pressure:1];
		[view mouseDown:mouse];
		const auto& pointer = childCounts.mEvents.back();
		AT_PORTABLE_TEST_ASSERT(context, pointer.mType == ATNativeInputType::PointerDown && pointer.mClickCount == 2);
		AT_PORTABLE_TEST_ASSERT(context, pointer.mPosition.x == 9 && pointer.mPosition.y == 13 && pointer.mButton == 0);
		CGEventRef scrollCG = CGEventCreateScrollWheelEvent(nullptr, kCGScrollEventUnitPixel, 2, 3, 4);
		AT_PORTABLE_TEST_ASSERT(context, scrollCG != nullptr);
		NSEvent *scroll = [NSEvent eventWithCGEvent:scrollCG];
		CFRelease(scrollCG);
		[view scrollWheel:scroll];
		AT_PORTABLE_TEST_ASSERT(context, childCounts.mEvents.back().mType == ATNativeInputType::Scroll);
		AT_PORTABLE_TEST_ASSERT(context, childCounts.mEvents.back().mScrollX != 0 && childCounts.mEvents.back().mScrollY != 0);
		child->SetTouchMode(kATUITouchMode_VerticalPan);
		[view scrollWheel:scroll];
		AT_PORTABLE_TEST_ASSERT(context, childCounts.mEvents.back().mScrollX == 0 && childCounts.mEvents.back().mScrollY != 0);
		child->SetEnabled(false);
		const auto disabledCount = childCounts.mEvents.size();
		[view keyDown:Key(NSEventTypeKeyDown)];
		[view mouseDown:mouse];
		AT_PORTABLE_TEST_ASSERT(context, ![view acceptsFirstResponder] && childCounts.mEvents.size() == disabledCount);
		child->SetEnabled(true);
		root->SetEnabled(false);
		[view keyDown:Key(NSEventTypeKeyDown)];
		AT_PORTABLE_TEST_ASSERT(context, ![view acceptsFirstResponder] && childCounts.mEvents.size() == disabledCount);
		root->SetEnabled(true);
		child->SetTouchMode(kATUITouchMode_MultiTouch);
		AT_PORTABLE_TEST_ASSERT(context, ([view allowedTouchTypes] & NSTouchTypeMaskIndirect) != 0);
		child->SetTouchMode(kATUITouchMode_Direct);
		AT_PORTABLE_TEST_ASSERT(context, [view allowedTouchTypes] == NSTouchTypeMaskDirect);
		// The borrowed proxy must trigger the same native lifecycle teardown.
		ATUINativeWindowProxy borrowed(child->GetWindowHandle());
		borrowed.Destroy();
		AT_PORTABLE_TEST_ASSERT(context, childCounts.mDestroyed == 1 && !child->IsValid());
		child = nullptr;
		AT_PORTABLE_TEST_ASSERT(context, childCounts.mDeleted == 1);

		Counts inputCounts;
		vdrefptr<TestWindow> input(new TestWindow(inputCounts));
		DestroyWindow inputGuard { input };
		AT_PORTABLE_TEST_ASSERT(context, input->CreateChild(root->GetClientViewHandle(), 1, 0, 0, 10, 10));
		NSView *inputView = [reinterpret_cast<NSView *>(input->GetClientViewHandle()) retain];
		ReleaseView inputViewGuard { inputView };
		input->mbHandleKeys = false;
		input->mOnInput = [&] { @autoreleasepool { input->Destroy(); input = nullptr; } };
		[inputView keyDown:Key(NSEventTypeKeyDown)];
		AT_PORTABLE_TEST_ASSERT(context, inputCounts.mDestroyed == 1 && inputCounts.mDeleted == 1);
		AT_PORTABLE_TEST_ASSERT(context, inputCounts.mEvents.size() == 1); // No text fallback after destruction.
		AT_PORTABLE_TEST_ASSERT(context, ATUINativeWindow::FromNativeHandle(reinterpret_cast<VDGUIHandle>(inputView)) == nullptr);

		Counts paintCounts;
		vdrefptr<TestWindow> paint(new TestWindow(paintCounts));
		DestroyWindow paintGuard { paint };
		AT_PORTABLE_TEST_ASSERT(context, paint->CreateChild(root->GetClientViewHandle(), 2, 0, 0, 10, 10));
		NSView *paintView = [reinterpret_cast<NSView *>(paint->GetClientViewHandle()) retain];
		ReleaseView paintViewGuard { paintView };
		paint->mOnPaint = [&] { paint->Destroy(); paint = nullptr; };
		Draw(paintView);
		AT_PORTABLE_TEST_ASSERT(context, paintCounts.mPaint == 1 && paintCounts.mDestroyed == 0 && paintCounts.mDeleted == 0);
		CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.01, false);
		AT_PORTABLE_TEST_ASSERT(context, paintCounts.mDestroyed == 1 && paintCounts.mDeleted == 1);

		// Reparented children belong to the new native root, not a stale list on
		// the old root. Generic AppKit containers must not break this ownership.
		Counts otherCounts, movedCounts;
		vdrefptr<TestWindow> other(new TestWindow(otherCounts)), moved(new TestWindow(movedCounts));
		DestroyWindow otherGuard { other }, movedGuard { moved };
		AT_PORTABLE_TEST_ASSERT(context, other->CreateWindow({120, 130, 440, 350}, L"other"));
		NSView *container = [[[NSView alloc] initWithFrame:NSMakeRect(0, 0, 100, 100)] autorelease];
		[reinterpret_cast<NSView *>(root->GetClientViewHandle()) addSubview:container];
		AT_PORTABLE_TEST_ASSERT(context, moved->CreateChild(reinterpret_cast<VDGUIHandle>(container), 3, 0, 0, 10, 10));
		NSView *movedView = reinterpret_cast<NSView *>(moved->GetClientViewHandle());
		[reinterpret_cast<NSView *>(other->GetClientViewHandle()) addSubview:movedView];
		root->Destroy();
		AT_PORTABLE_TEST_ASSERT(context, moved->IsValid() && movedCounts.mDestroyed == 0);
		moved->mOnDestroy = [&] { moved->Destroy(); };
		other->Destroy();
		AT_PORTABLE_TEST_ASSERT(context, !moved->IsValid() && movedCounts.mDestroyed == 1);

		Counts failedCounts;
		vdrefptr<TestWindow> failed(new TestWindow(failedCounts));
		DestroyWindow failedGuard { failed };
		failed->mbThrowCreate = true;
		bool threw = false;
		try { failed->CreateWindow({100, 120, 420, 340}); }
		catch (const std::runtime_error&) { threw = true; }
		AT_PORTABLE_TEST_ASSERT(context, threw && failedCounts.mCreated == 1 && failedCounts.mDestroyed == 1 && !failed->IsValid());
		failed->mbThrowCreate = false;
		AT_PORTABLE_TEST_ASSERT(context, failed->CreateWindow({100, 120, 420, 340}));
		failed->Destroy();
		AT_PORTABLE_TEST_ASSERT(context, failedCounts.mDestroyed == 2);

		// Destroying an ancestor is also deferred while any descendant paints.
		Counts parentPaintCounts, nestedPaintCounts;
		vdrefptr<TestWindow> parentPaint(new TestWindow(parentPaintCounts)), nestedPaint(new TestWindow(nestedPaintCounts));
		DestroyWindow parentPaintGuard { parentPaint }, nestedPaintGuard { nestedPaint };
		AT_PORTABLE_TEST_ASSERT(context, parentPaint->CreateWindow({100, 120, 420, 340}));
		AT_PORTABLE_TEST_ASSERT(context, nestedPaint->CreateChild(parentPaint->GetClientViewHandle(), 4, 0, 0, 10, 10));
		NSView *nestedView = [reinterpret_cast<NSView *>(nestedPaint->GetClientViewHandle()) retain];
		ReleaseView nestedGuard { nestedView };
		nestedPaint->mOnPaint = [&] { parentPaint->Destroy(); };
		Draw(nestedView);
		AT_PORTABLE_TEST_ASSERT(context, parentPaintCounts.mDestroyed == 0 && nestedPaintCounts.mDestroyed == 0);
		CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.01, false);
		AT_PORTABLE_TEST_ASSERT(context, parentPaintCounts.mDestroyed == 1 && nestedPaintCounts.mDestroyed == 1);
	}
	return true;
}
