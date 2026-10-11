// Platform-independent payloads from native UI event hosts.
#ifndef f_AT_ATNATIVEUI_NATIVEINPUT_H
#define f_AT_ATNATIVEUI_NATIVEINPUT_H

#include <vd2/system/vectors.h>
#include <string>

enum class ATNativeInputType : uint8 {
	KeyDown, KeyUp, Text, Composition, PointerDown, PointerUp, PointerMove, PointerEnter, PointerLeave, Scroll, Touch
};
enum ATNativeInputModifiers : uint32 {
	kATNativeInputShift = 1, kATNativeInputControl = 2,
	kATNativeInputAlt = 4, kATNativeInputCommand = 8, kATNativeInputCapsLock = 16, kATNativeInputFunction = 32
};
struct ATNativeInputEvent {
	ATNativeInputType mType = ATNativeInputType::KeyDown;
	uint32 mModifiers = 0;
	// Physical, platform-specific key code, kept separate from Unicode text.
	uint32 mNativeKeyCode = 0;
	uint32 mCodePoint = 0;
	std::u32string mComposition; // Current IME preedit; empty means cleared.
	vdpoint32 mPosition {0, 0}; // Top-left client coordinates in logical units.
	uint32 mButton = 0; // 0=left, 1=right, 2=middle, >=3=auxiliary.
	uint32 mClickCount = 0;
	float mScrollX = 0, mScrollY = 0;
	bool mbRepeat = false;
	uintptr mPointerId = 0;
	uint8 mTouchPhase = 0; // 0=began, 1=moved, 2=ended, 3=cancelled.
};

#endif
