// Preserve Windows keyboard-layout naming for the shared input manager.
#include <stdafx.h>
#include <vd2/Dita/accel.h>
#include <vd2/system/VDString.h>
#include "inputdefs.h"
#include "inputkeyname.h"

void ATInputGetKeyName(uint32 code, VDStringW& name) {
	VDUIAccelerator accel;
	accel.mModifiers = 0;

	switch(code) {
		case kATInputCode_KeyNumpadEnter:
			code = kATInputCode_KeyReturn;
			accel.mModifiers |= VDUIAccelerator::kModExtended;
			break;
		case kATInputCode_KeyInsert:
		case kATInputCode_KeyDelete:
		case kATInputCode_KeyHome:
		case kATInputCode_KeyEnd:
		case kATInputCode_KeyNext:
		case kATInputCode_KeyPrior:
		case kATInputCode_KeyLeft:
		case kATInputCode_KeyRight:
		case kATInputCode_KeyUp:
		case kATInputCode_KeyDown:
			accel.mModifiers |= VDUIAccelerator::kModExtended;
			break;
	}

	accel.mVirtKey = code & 0xff;
	VDUIGetAcceleratorString(accel, name);
}
