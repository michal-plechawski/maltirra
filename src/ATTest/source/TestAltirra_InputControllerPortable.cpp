// Controller integration with the production PIA, GTIA, POKEY and ANTIC cores.
#include <array>
#include <memory>
#include <utility>
#include <vector>
#include <at/attest/portabletest.h>
#include "portmanagerportablefixture.h"
#include <inputcontroller.h>

bool ATTestAltirraInputRouting(ATPortableTestContext& context) {
	std::vector<std::pair<uint32, bool>> events;
	ATConsoleController console([&](uint32 id, bool state) { events.emplace_back(id, state); });
	console.SetDigitalTrigger(kATInputTrigger_Button0, true);
	console.SetDigitalTrigger(kATInputTrigger_Button0, false);
	AT_PORTABLE_TEST_ASSERT(context, events.size() == 2 && events[0].first == kATInputTrigger_Button0 && events[0].second && !events[1].second);
	events.clear();
	ATInputStateController flags([&](uint32 id, bool state) { events.emplace_back(id, state); }, 12);
	flags.SetDigitalTrigger(kATInputTrigger_Flag0, true);
	flags.SetDigitalTrigger(kATInputTrigger_Flag0 + 1, false);
	flags.SetDigitalTrigger(kATInputTrigger_Button0, true);
	AT_PORTABLE_TEST_ASSERT(context, events.size() == 2 && events[0].first == 12 && events[0].second && events[1].first == 13 && !events[1].second);
	ATConsoleController noConsole(vdfunction<void(uint32, bool)>{});
	ATInputStateController noFlags(vdfunction<void(uint32, bool)>{}, 0);
	noConsole.SetDigitalTrigger(0, true); noFlags.SetDigitalTrigger(kATInputTrigger_Flag0, true);
	return true;
}

bool ATTestAltirraInput5200(ATPortableTestContext& context) {
	auto f = std::make_unique<PortManagerPortableFixture>();
	f->mPOKEY.Set5200Mode(true);
	f->mPOKEY.WriteByte(0x0F, 2); // 5200 keypad scans with debounce disabled
	AT5200ControllerController joystick(0, false, f->mPOKEY);
	joystick.Attach(f->mPorts, 0);
	AT_PORTABLE_TEST_ASSERT(context, !joystick.Select5200Controller(1, false));
	AT_PORTABLE_TEST_ASSERT(context, f->Pot(0) == 228 && f->Pot(1) == 228);
	AT_PORTABLE_TEST_ASSERT(context, joystick.Select5200Controller(0, true));
	AT_PORTABLE_TEST_ASSERT(context, f->Pot(0) == 114 && f->Pot(1) == 114);
	joystick.ApplyAnalogInput(kATInputTrigger_Axis0, -65536);
	joystick.ApplyAnalogInput(kATInputTrigger_Axis0 + 1, 65536);
	// Five ticks settle the short calibration jitter used for large movements.
	for (int i = 0; i < 6; ++i) joystick.Tick();
	AT_PORTABLE_TEST_ASSERT(context, f->Pot(0) == 1 && f->Pot(1) == 227);
	joystick.SetDigitalTrigger(kATInputTrigger_Left, true);
	joystick.SetDigitalTrigger(kATInputTrigger_Right, true);
	for (int i = 0; i < 6; ++i) joystick.Tick();
	AT_PORTABLE_TEST_ASSERT(context, f->Pot(0) == 114);
	joystick.SetDigitalTrigger(kATInputTrigger_Button0, true);
	AT_PORTABLE_TEST_ASSERT(context, f->Trigger(0) == 0);
	joystick.SetDigitalTrigger(kATInputTrigger_Button0, false);
	AT_PORTABLE_TEST_ASSERT(context, f->Trigger(0) == 1);
	// The keypad duplicates each contact on two neighboring scan positions and
	// in both halves of the 64-contact POKEY matrix. Observe actual KBCODE/IRQ.
	constexpr std::array<uint32, 15> keys { kATInputTrigger_5200_0, kATInputTrigger_5200_1, kATInputTrigger_5200_2,
		kATInputTrigger_5200_3, kATInputTrigger_5200_4, kATInputTrigger_5200_5, kATInputTrigger_5200_6,
		kATInputTrigger_5200_7, kATInputTrigger_5200_8, kATInputTrigger_5200_9, kATInputTrigger_5200_Star,
		kATInputTrigger_5200_Pound, kATInputTrigger_5200_Start, kATInputTrigger_5200_Pause, kATInputTrigger_5200_Reset };
	constexpr std::array<uint8, 15> contacts { 2,15,14,13,11,10,9,7,6,5,3,1,12,8,4 };
	for (size_t i = 0; i < keys.size(); ++i) {
		f->mPOKEY.WriteByte(0x0E, 0); f->mPOKEY.WriteByte(0x0E, 0x40);
		joystick.SetDigitalTrigger(keys[i], true);
		f->mVideo.Tick(160 * 114);
		AT_PORTABLE_TEST_ASSERT(context, !(f->mPOKEY.ReadByte(0x0E) & 0x40));
		AT_PORTABLE_TEST_ASSERT(context, (f->mPOKEY.ReadByte(9) & 0x1E) == contacts[i] * 2);
		joystick.SetDigitalTrigger(keys[i], false);
		f->mVideo.Tick(160 * 114);
		AT_PORTABLE_TEST_ASSERT(context, f->mPOKEY.ReadByte(0x0F) & 4);
	}
	joystick.SetDigitalTrigger(kATInputTrigger_Button0 + 1, true);
	f->mVideo.Tick(80 * 114);
	AT_PORTABLE_TEST_ASSERT(context, !(f->mPOKEY.ReadByte(0x0F) & 8));
	joystick.Detach();
	f->mVideo.Tick(160 * 114);
	AT_PORTABLE_TEST_ASSERT(context, f->mPOKEY.ReadByte(0x0F) & 8);
	AT_PORTABLE_TEST_ASSERT(context, f->Pot(0) == 228);
	AT5200ControllerController trackball(0, true, f->mPOKEY);
	trackball.Attach(f->mPorts, 0); trackball.Select5200Controller(0, true);
	for (int i = 0; i < 6; ++i) {
		trackball.ApplyImpulse(kATInputTrigger_Axis0, 65536);
		trackball.ApplyImpulse(kATInputTrigger_Axis0 + 1, -65536);
		trackball.Tick();
	}
	AT_PORTABLE_TEST_ASSERT(context, f->Pot(0) == 189 && f->Pot(1) == 38);
	for (int i = 0; i < 6; ++i) trackball.Tick();
	AT_PORTABLE_TEST_ASSERT(context, f->Pot(0) == 114 && f->Pot(1) == 114);
	trackball.Select5200Controller(1, false);
	AT_PORTABLE_TEST_ASSERT(context, f->Pot(0) == 114 && f->Pot(1) == 114);
	return true;
}

bool ATTestAltirraInputLightPen(ATPortableTestContext& context) {
	auto f = std::make_unique<PortManagerPortableFixture>();
	f->mPIA.WriteByte(2, 4);
	f->mLightPen.SetImmediateUpdateEnabled(true);
	using Type = ATLightPenController::Type;
	for (Type type : {Type::LightPen, Type::LightPenStack, Type::LightGun}) {
		auto controller = std::make_unique<ATLightPenController>(type);
		controller->Init(&f->mVideo.mScheduler, &f->mLightPen);
		controller->Attach(f->mPorts, 0);
		AT_PORTABLE_TEST_ASSERT(context, !controller->IsActive());
		controller->SetDigitalTrigger(kATInputTrigger_Button0, true);
		const uint8 expected = type == Type::LightPen ? 14 : 15;
		AT_PORTABLE_TEST_ASSERT(context, (f->mPIA.ReadByte(0) & 15) == expected);
		controller->SetDigitalTrigger(kATInputTrigger_Button0 + 2, true);
		controller->Tick();
		AT_PORTABLE_TEST_ASSERT(context, controller->IsActive() && f->Trigger(0) == 0);
		// The centered pointer maps to color-clock (128,128); device delays add
		// (24,1) for a pen and (43,3) for a gun before ANTIC's Y divide by two.
		AT_PORTABLE_TEST_ASSERT(context, f->mVideo.mAntic.ReadByte(0x0C) == (type == Type::LightGun ? 171 : 152));
		AT_PORTABLE_TEST_ASSERT(context, f->mVideo.mAntic.ReadByte(0x0D) == (type == Type::LightGun ? 65 : 64));
		vdfastvector<ATInputPointerInfo> pointers;
		controller->GetPointers(pointers);
		AT_PORTABLE_TEST_ASSERT(context, pointers.size() == 1 && pointers[0].mbPrimary && pointers[0].mbPressed && pointers[0].mRadius < 0);
		AT_PORTABLE_TEST_ASSERT(context, pointers[0].mPos.x == 128 && pointers[0].mPos.y == 128 && pointers[0].mCoordSpace == ATInputPointerCoordinateSpace::Beam);
		f->mVideo.Tick(16);
		AT_PORTABLE_TEST_ASSERT(context, f->Trigger(0) == 0);
		f->mVideo.Tick(1);
		AT_PORTABLE_TEST_ASSERT(context, f->Trigger(0) == 1);
		controller->ApplyAnalogInput(kATInputTrigger_Axis0, 131072);
		controller->ApplyAnalogInput(kATInputTrigger_Axis0 + 1, -131072);
		pointers.clear(); controller->GetPointers(pointers);
		AT_PORTABLE_TEST_ASSERT(context, pointers[0].mPos.x == 222 && pointers[0].mPos.y > 7 && pointers[0].mPos.y < 9);
		controller->Tick(); // Destroy with the 17-cycle trigger-release event pending.
		controller.reset(); f->mVideo.Tick(100);
		AT_PORTABLE_TEST_ASSERT(context, f->Trigger(0) == 1 && (f->mPIA.ReadByte(0) & 15) == 15);
	}
	// Exercise registration/unregistration of the GTIA sensor callback as well.
	f->mLightPen.SetImmediateUpdateEnabled(false);
	{
		ATLightPenController pen(Type::LightPen);
		pen.Init(&f->mVideo.mScheduler, &f->mLightPen); pen.Attach(f->mPorts, 0);
		pen.SetDigitalTrigger(kATInputTrigger_Button0 + 2, true); pen.Tick();
		pen.Detach(); pen.Attach(f->mPorts, 0); pen.Tick();
	}
	f->mVideo.Tick(40000);
	return true;
}

bool ATTestAltirraInputLifecycle(ATPortableTestContext& context) {
	auto f = std::make_unique<PortManagerPortableFixture>();
	f->mPIA.WriteByte(2, 4);
	ATJoystickController a, b;
	a.Attach(f->mPorts, 0, 0); b.Attach(f->mPorts, 0, 1);
	a.SetDigitalTrigger(kATInputTrigger_Up, true); b.SetDigitalTrigger(kATInputTrigger_Right, true);
	a.SelectMultiJoy(1); b.SelectMultiJoy(1);
	AT_PORTABLE_TEST_ASSERT(context, (f->mPIA.ReadByte(0) & 15) == 14);
	a.SelectMultiJoy(2); b.SelectMultiJoy(2);
	AT_PORTABLE_TEST_ASSERT(context, (f->mPIA.ReadByte(0) & 15) == 7);
	a.Detach(); b.Detach();
	AT_PORTABLE_TEST_ASSERT(context, (f->mPIA.ReadByte(0) & 15) == 15);
	// Mouse/trackball destruction must remove every scheduler callback while the
	// derived object is still alive. The base destructor cannot dispatch OnDetach.
	for (int i = 0; i < 8; ++i) {
		{
			ATMouseController mouse(i & 1); mouse.Init(&f->mVideo.mScheduler);
			mouse.Attach(f->mPorts, 0); mouse.ApplyImpulse(kATInputTrigger_Axis0, -65536);
		}
		{
			ATTrackballController ball; ball.Init(&f->mVideo.mScheduler);
			ball.Attach(f->mPorts, 0); ball.ApplyImpulse(kATInputTrigger_Axis0, 65536);
			ball.Detach(); ball.Attach(f->mPorts, 0);
		}
		f->mVideo.Tick(1024);
		AT_PORTABLE_TEST_ASSERT(context, f->Trigger(0) == 1 && (f->mPIA.ReadByte(0) & 15) == 15);
	}
	return true;
}
