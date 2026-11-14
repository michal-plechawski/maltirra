// Host input events routed through the production input manager and chip cores.
#include <array>
#include <cwchar>
#include <memory>
#include <at/attest/portabletest.h>
#include <inputkeyname.h>
#include <inputmanager.h>
#include <inputmap.h>
#include "portmanagerportablefixture.h"

namespace {
	void Init(ATInputManager& manager, PortManagerPortableFixture& f) {
		manager.Init(&f.mVideo.mScheduler, &f.mVideo.mScheduler, f.mPOKEY, f.mPorts, &f.mLightPen);
		f.mPIA.WriteByte(2, 4);
	}
	void Enable(ATInputManager& manager, ATInputMap& map) {
		manager.AddInputMap(&map); manager.ActivateInputMap(&map, true);
	}
}

bool ATTestAltirraInputManagerAnalog(ATPortableTestContext& context) {
	auto f = std::make_unique<PortManagerPortableFixture>();
	ATInputManager manager; Init(manager, *f);
	vdrefptr<ATInputMap> map(new ATInputMap);
	map->AddController(kATInputControllerType_Paddle, 0);
	map->AddController(kATInputControllerType_Paddle, 2);
	map->AddMapping(kATInputCode_JoyHoriz1, 0, kATInputTrigger_Axis0);
	map->AddMapping(kATInputCode_JoyHoriz1, 1, kATInputTrigger_Axis0 | kATInputTriggerMode_Inverted);
	Enable(manager, *map);
	manager.OnAxisInput(0, kATInputCode_JoyHoriz1, 0, 0);
	AT_PORTABLE_TEST_ASSERT(context, f->Pot(0) == 114 && f->Pot(2) == 114);
	manager.OnAxisInput(0, kATInputCode_JoyHoriz1, 65536, 65536);
	AT_PORTABLE_TEST_ASSERT(context, f->Pot(0) == 1 && f->Pot(2) == 228);
	manager.OnAxisInput(0, kATInputCode_JoyHoriz1, -65536, -65536);
	AT_PORTABLE_TEST_ASSERT(context, f->Pot(0) == 228 && f->Pot(2) == 1);
	manager.RemoveAllInputMaps();
	map->Clear();
	map->AddController(kATInputControllerType_STMouse, 0);
	map->AddMapping(kATInputCode_MouseHoriz, 0, kATInputTrigger_Axis0);
	map->AddMapping(kATInputCode_MouseVert, 0, kATInputTrigger_Axis0 + 1);
	Enable(manager, *map);
	AT_PORTABLE_TEST_ASSERT(context, manager.IsMouseMapped() && !manager.IsMouseAbsoluteMode());
	manager.OnMouseMove(0, 16, -16); // +/-one quadrature pixel after the two input scale factors
	f->mVideo.Tick(256);
	AT_PORTABLE_TEST_ASSERT(context, (f->mPIA.ReadByte(0) & 15) == 9);
	manager.Shutdown();
	f->mVideo.Tick(1000);
	AT_PORTABLE_TEST_ASSERT(context, (f->mPIA.ReadByte(0) & 15) == 15);
	map->Clear();
	map->AddController(kATInputControllerType_Paddle, 0);
	map->AddMapping(kATInputCode_JoyHoriz1, 0, kATInputTrigger_Axis0 | kATInputTriggerMode_Relative | (5 << kATInputTriggerSpeed_Shift));
	Enable(manager, *map);
	manager.OnAxisInput(0, kATInputCode_JoyHoriz1, 65536, 65536);
	manager.Poll(1.0f / 60);
	// Speed index 5 is one normalized unit/second. A frame emits 1092
	// fixed-point units; 228.5 - 1092*113/65536 gives POT=226.
	AT_PORTABLE_TEST_ASSERT(context, f->Pot(0) == 226);
	manager.OnAxisInput(0, kATInputCode_JoyHoriz1, 0, 0);
	manager.Poll(1.0f / 60);
	AT_PORTABLE_TEST_ASSERT(context, f->Pot(0) == 226);
	manager.Shutdown();
	map->Clear();
	map->AddController(kATInputControllerType_STMouse, 0);
	map->AddMapping(kATInputCode_MouseHoriz, 0, kATInputTrigger_Axis0);
	{
		ATInputManager scoped; Init(scoped, *f); Enable(scoped, *map);
		scoped.OnMouseMove(0, 160, 0); // leave several quadrature events pending
	} // Destructor must release controllers before their scheduler and ports.
	f->mVideo.Tick(1000);
	AT_PORTABLE_TEST_ASSERT(context, f->Trigger(0) == 1 && (f->mPIA.ReadByte(0) & 15) == 15);
	return true;
}

bool ATTestAltirraInputManagerPointers(ATPortableTestContext& context) {
	auto f = std::make_unique<PortManagerPortableFixture>();
	ATInputManager manager; Init(manager, *f);
	vdrefptr<ATInputMap> map(new ATInputMap);
	map->AddController(kATInputControllerType_PowerPad, 0);
	map->AddMapping(kATInputCode_MousePadX, 0, kATInputTrigger_Axis0);
	map->AddMapping(kATInputCode_MousePadY, 0, kATInputTrigger_Axis0 + 1);
	map->AddMapping(kATInputCode_MouseLMB, 0, kATInputTrigger_Button0);
	Enable(manager, *map);
	AT_PORTABLE_TEST_ASSERT(context, manager.HasAbsMousePointer() && manager.HasNonBeamPointer() && manager.IsMouseAbsoluteMode());
	manager.SetMousePadPos(-65536, 65536);
	manager.OnButtonDown(0, kATInputCode_MouseLMB);
	vdfastvector<ATInputPointerInfo> pointers; manager.GetPointers(pointers);
	AT_PORTABLE_TEST_ASSERT(context, pointers.size() == 1 && pointers[0].mbPressed);
	AT_PORTABLE_TEST_ASSERT(context, pointers[0].mPos.x < -0.98f && pointers[0].mPos.y > 0.98f);
	manager.RemoveAllInputMaps(); map->Clear();
	map->AddController(kATInputControllerType_LightPen, 0);
	map->AddMapping(kATInputCode_MouseBeamX, 0, kATInputTrigger_Axis0);
	map->AddMapping(kATInputCode_MouseBeamY, 0, kATInputTrigger_Axis0 + 1);
	map->AddMapping(kATInputCode_MouseRMB, 0, kATInputTrigger_Button0 + 2);
	Enable(manager, *map); f->mLightPen.SetImmediateUpdateEnabled(true);
	AT_PORTABLE_TEST_ASSERT(context, manager.HasAbsMousePointer() && !manager.HasNonBeamPointer());
	manager.SetMouseBeamPos(65536, 0);
	manager.Poll(1.0f / 60);
	AT_PORTABLE_TEST_ASSERT(context, !manager.IsMouseActiveTarget());
	manager.OnButtonDown(0, kATInputCode_MouseRMB); manager.Poll(1.0f / 60);
	AT_PORTABLE_TEST_ASSERT(context, manager.IsMouseActiveTarget() && f->Trigger(0) == 0);
	pointers.clear(); manager.GetPointers(pointers);
	AT_PORTABLE_TEST_ASSERT(context, pointers.size() == 1 && pointers[0].mPos.x == 222 && pointers[0].mPos.y == 128 && pointers[0].mbPressed);
	AT_PORTABLE_TEST_ASSERT(context, f->mVideo.mAntic.ReadByte(0x0C) == 18 && f->mVideo.mAntic.ReadByte(0x0D) == 65);
	manager.Shutdown(); f->mVideo.Tick(100);
	AT_PORTABLE_TEST_ASSERT(context, !manager.HasAbsMousePointer() && !manager.IsMouseMapped() && f->Trigger(0) == 1);
	return true;
}

bool ATTestAltirraInputManagerWheel(ATPortableTestContext& context) {
	auto f = std::make_unique<PortManagerPortableFixture>();
	ATInputManager manager; Init(manager, *f);
	vdrefptr<ATInputMap> map(new ATInputMap);
	map->AddController(kATInputControllerType_Joystick, 0);
	map->AddMapping(kATInputCode_MouseWheelDown, 0, kATInputTrigger_Down);
	map->AddMapping(kATInputCode_MouseHWheelRight, 0, kATInputTrigger_Right);
	Enable(manager, *map);
	AT_PORTABLE_TEST_ASSERT(context, manager.OnMouseHWheel(0, 1));
	manager.Poll(1.0f / 60);
	AT_PORTABLE_TEST_ASSERT(context, (f->mPIA.ReadByte(0) & 15) == 7); // horizontal must not activate vertical mapping
	manager.Poll(1.0f / 60);
	AT_PORTABLE_TEST_ASSERT(context, (f->mPIA.ReadByte(0) & 15) == 15);
	AT_PORTABLE_TEST_ASSERT(context, manager.OnMouseWheel(0, 1));
	manager.Poll(1.0f / 60);
	AT_PORTABLE_TEST_ASSERT(context, (f->mPIA.ReadByte(0) & 15) == 13);
	manager.Poll(1.0f / 60);
	AT_PORTABLE_TEST_ASSERT(context, (f->mPIA.ReadByte(0) & 15) == 15);
	return true;
}

bool ATTestAltirraInputKeyNames(ATPortableTestContext& context) {
	ATInputManager manager;
	VDStringW name;
	manager.GetNameForInputCode(kATInputCode_KeyA, name);
	AT_PORTABLE_TEST_ASSERT(context, !wcscmp(name.c_str(), L"Key: A"));
	// Host-specific names may be localized. Every declared keyboard code has a
	// display label, including keypad Enter (which is distinct from Return).
	for (uint32 key : {kATInputCode_KeyBack, kATInputCode_KeyTab, kATInputCode_KeyReturn,
		kATInputCode_KeyEscape, kATInputCode_KeySpace, kATInputCode_KeyPrior, kATInputCode_KeyNext,
		kATInputCode_KeyHome, kATInputCode_KeyEnd, kATInputCode_KeyInsert, kATInputCode_KeyDelete,
		kATInputCode_KeyUp, kATInputCode_KeyDown, kATInputCode_KeyLeft, kATInputCode_KeyRight,
		kATInputCode_KeyNumpadEnter, kATInputCode_KeyLShift, kATInputCode_KeyRControl,
		kATInputCode_KeyOem1, kATInputCode_KeyOemPlus, kATInputCode_KeyOemComma,
		kATInputCode_KeyOemMinus, kATInputCode_KeyOemPeriod, kATInputCode_KeyOem2,
		kATInputCode_KeyOem3, kATInputCode_KeyOem4, kATInputCode_KeyOem5,
		kATInputCode_KeyOem6, kATInputCode_KeyOem7}) {
		manager.GetNameForInputCode(key, name);
		AT_PORTABLE_TEST_ASSERT(context, name.size() > 5);
	}
	manager.GetNameForInputCode(kATInputCode_KeyReturn, name);
	VDStringW enter; manager.GetNameForInputCode(kATInputCode_KeyNumpadEnter, enter);
	AT_PORTABLE_TEST_ASSERT(context, name != enter);
	return true;
}
