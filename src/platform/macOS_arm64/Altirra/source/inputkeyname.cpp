// macOS labels for semantic input codes (physical event translation is separate).
#include <vd2/system/VDString.h>
#include "inputdefs.h"
#include "inputkeyname.h"

void ATInputGetKeyName(uint32 code, VDStringW& name) {
	if ((code >= kATInputCode_KeyA && code <= kATInputCode_KeyZ) || (code >= kATInputCode_Key0 && code <= kATInputCode_Key9)) {
		name.assign(1, static_cast<wchar_t>(code));
		return;
	}
	if (code >= kATInputCode_KeyF1 && code <= kATInputCode_KeyF12) {
		name.sprintf(L"F%u", code - kATInputCode_KeyF1 + 1);
		return;
	}
	if (code >= kATInputCode_KeyNumpad0 && code <= kATInputCode_KeyNumpad9) {
		name.sprintf(L"Keypad %u", code - kATInputCode_KeyNumpad0);
		return;
	}
	switch (code) {
		case kATInputCode_KeyBack: name = L"Delete"; return;
		case kATInputCode_KeyTab: name = L"Tab"; return;
		case kATInputCode_KeyReturn: name = L"Return"; return;
		case kATInputCode_KeyEscape: name = L"Escape"; return;
		case kATInputCode_KeySpace: name = L"Space"; return;
		case kATInputCode_KeyPrior: name = L"Page Up"; return;
		case kATInputCode_KeyNext: name = L"Page Down"; return;
		case kATInputCode_KeyHome: name = L"Home"; return;
		case kATInputCode_KeyEnd: name = L"End"; return;
		case kATInputCode_KeyInsert: name = L"Help"; return;
		case kATInputCode_KeyDelete: name = L"Forward Delete"; return;
		case kATInputCode_KeyLeft: name = L"Left Arrow"; return;
		case kATInputCode_KeyRight: name = L"Right Arrow"; return;
		case kATInputCode_KeyUp: name = L"Up Arrow"; return;
		case kATInputCode_KeyDown: name = L"Down Arrow"; return;
		case kATInputCode_KeyMultiply: name = L"Keypad *"; return;
		case kATInputCode_KeyAdd: name = L"Keypad +"; return;
		case kATInputCode_KeySubtract: name = L"Keypad -"; return;
		case kATInputCode_KeyDecimal: name = L"Keypad ."; return;
		case kATInputCode_KeyDivide: name = L"Keypad /"; return;
		case kATInputCode_KeyNumpadEnter: name = L"Keypad Enter"; return;
		case kATInputCode_KeyLShift: name = L"Left Shift"; return;
		case kATInputCode_KeyRShift: name = L"Right Shift"; return;
		case kATInputCode_KeyLControl: name = L"Left Control"; return;
		case kATInputCode_KeyRControl: name = L"Right Control"; return;
		case kATInputCode_KeyOem1: name = L";"; return;
		case kATInputCode_KeyOemPlus: name = L"="; return;
		case kATInputCode_KeyOemComma: name = L","; return;
		case kATInputCode_KeyOemMinus: name = L"-"; return;
		case kATInputCode_KeyOemPeriod: name = L"."; return;
		case kATInputCode_KeyOem2: name = L"/"; return;
		case kATInputCode_KeyOem3: name = L"`"; return;
		case kATInputCode_KeyOem4: name = L"["; return;
		case kATInputCode_KeyOem5: name = L"\\"; return;
		case kATInputCode_KeyOem6: name = L"]"; return;
		case kATInputCode_KeyOem7: name = L"'"; return;
		default: name.sprintf(L"Key code %03X", code); return;
	}
}
