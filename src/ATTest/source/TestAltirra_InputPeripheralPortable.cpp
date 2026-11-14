// Controller peripherals connected to production input chips.
#include <array>
#include <cmath>
#include <memory>
#include <vector>
#include <at/attest/portabletest.h>
#include "portmanagerportablefixture.h"
#include "inputcontroller.h"

bool ATTestAltirraInputJoystick(ATPortableTestContext& context) {
	auto f = std::make_unique<PortManagerPortableFixture>(); f->mPIA.WriteByte(2, 4);
	ATJoystickController controller; controller.Attach(f->mPorts, 0);
	for (const auto [trigger, pins] : std::array<std::pair<uint32, uint8>, 4>{{
		{kATInputTrigger_Up,14}, {kATInputTrigger_Down,13}, {kATInputTrigger_Left,11}, {kATInputTrigger_Right,7}}}) {
		controller.SetDigitalTrigger(trigger, true);
		AT_PORTABLE_TEST_ASSERT(context, (f->mPIA.ReadByte(0) & 15) == pins);
		controller.SetDigitalTrigger(trigger, false);
	}
	controller.SetDigitalTrigger(kATInputTrigger_Up, true); controller.SetDigitalTrigger(kATInputTrigger_Left, true);
	AT_PORTABLE_TEST_ASSERT(context, (f->mPIA.ReadByte(0) & 15) == 10);
	controller.SetDigitalTrigger(kATInputTrigger_Down, true); controller.SetDigitalTrigger(kATInputTrigger_Right, true);
	AT_PORTABLE_TEST_ASSERT(context, (f->mPIA.ReadByte(0) & 15) == 15); // opposite directions cancel independently
	controller.SetDigitalTrigger(kATInputTrigger_Button0, true);
	AT_PORTABLE_TEST_ASSERT(context, f->Trigger(0) == 0);
	controller.SetDigitalTrigger(kATInputTrigger_Button0, false);
	AT_PORTABLE_TEST_ASSERT(context, f->Trigger(0) == 1);
	return true;
}

bool ATTestAltirraInputTablet(ATPortableTestContext& context) {
	auto f = std::make_unique<PortManagerPortableFixture>(); f->mPIA.WriteByte(2, 4);
	for (bool invert : {false, true}) {
		ATTabletController controller(229, invert); controller.Attach(f->mPorts, 0);
		controller.ApplyAnalogInput(kATInputTrigger_Axis0, 65536);
		controller.ApplyAnalogInput(kATInputTrigger_Axis0 + 1, 65536);
		AT_PORTABLE_TEST_ASSERT(context, f->Pot(0) == 228 && f->Pot(1) == (invert ? 1 : 228));
		controller.ApplyImpulse(kATInputTrigger_Left, 65536);
		AT_PORTABLE_TEST_ASSERT(context, f->Pot(0) == 114);
		controller.SetDigitalTrigger(kATInputTrigger_Button0, true);
		AT_PORTABLE_TEST_ASSERT(context, (f->mPIA.ReadByte(0) & 15) == 14 && f->Trigger(0) == 1);
		controller.SetDigitalTrigger(kATInputTrigger_Button0 + 1, true);
		controller.SetDigitalTrigger(kATInputTrigger_Button0 + 2, true);
		AT_PORTABLE_TEST_ASSERT(context, (f->mPIA.ReadByte(0) & 15) == 2);
		controller.SetDigitalTrigger(kATInputTrigger_Button0 + 3, true);
		AT_PORTABLE_TEST_ASSERT(context, f->Pot(0) == 228 && f->Pot(1) == 228);
		controller.SetDigitalTrigger(kATInputTrigger_Button0 + 3, false);
		AT_PORTABLE_TEST_ASSERT(context, f->Pot(0) == 114 && f->Pot(1) == (invert ? 1 : 228));
	}
	return true;
}

bool ATTestAltirraInputKeypad(ATPortableTestContext& context) {
	auto f = std::make_unique<PortManagerPortableFixture>(); f->mPIA.WriteByte(2, 4);
	ATKeypadController controller; controller.Attach(f->mPorts, 0);
	constexpr std::array<uint8,17> codes {6,5,4,14,13,12,10,9,8,3,2,1,0,7,11,15,19};
	for (size_t i = 0; i < codes.size(); ++i) {
		controller.SetDigitalTrigger(kATInputTrigger_Button0 + i, true);
		AT_PORTABLE_TEST_ASSERT(context, (f->mPIA.ReadByte(0) & 15) == (15 ^ (codes[i] & 15)));
		AT_PORTABLE_TEST_ASSERT(context, f->Trigger(0) == 0 && f->Pot(1) == ((codes[i] & 16) ? 228 : 1));
		controller.SetDigitalTrigger(kATInputTrigger_Button0 + i, false);
		AT_PORTABLE_TEST_ASSERT(context, f->Trigger(0) == 1);
	}
	return true;
}

bool ATTestAltirraInputKeyboard(ATPortableTestContext& context) {
	auto f = std::make_unique<PortManagerPortableFixture>();
	ATKeyboardController controller; controller.Attach(f->mPorts, 0);
	f->mPIA.WriteByte(2, 0); f->mPIA.WriteByte(0, 15); f->mPIA.WriteByte(2, 4); f->mPIA.WriteByte(0, 15);
	for (int key = 0; key < 12; ++key) {
		controller.SetDigitalTrigger(kATInputTrigger_Button0 + key, true);
		for (int row = 0; row < 4; ++row) {
			f->mPIA.WriteByte(0, 15 ^ (1 << row));
			const bool active = key / 3 == row;
			AT_PORTABLE_TEST_ASSERT(context, f->Trigger(0) == (active && key % 3 == 2 ? 0 : 1));
			AT_PORTABLE_TEST_ASSERT(context, (f->Pot(0) == 228) == (active && key % 3 == 1));
			AT_PORTABLE_TEST_ASSERT(context, (f->Pot(1) == 228) == (active && key % 3 == 0));
		}
		controller.SetDigitalTrigger(kATInputTrigger_Button0 + key, false);
	}
	// With no diodes, keys 1,2,4 connect both columns to row 2 through row 1.
	for (int key : {0,1,3}) controller.SetDigitalTrigger(kATInputTrigger_Button0 + key, true);
	f->mPIA.WriteByte(0, 13);
	AT_PORTABLE_TEST_ASSERT(context, f->Pot(0) == 228 && f->Pot(1) == 228 && f->Trigger(0) == 1);
	controller.Detach();
	AT_PORTABLE_TEST_ASSERT(context, f->Pot(0) == 228 && f->Trigger(0) == 1);
	return true;
}

bool ATTestAltirraInputPowerPad(ATPortableTestContext& context) {
	auto f = std::make_unique<PortManagerPortableFixture>();
	ATPowerPadController controller; controller.Attach(f->mPorts, 0);
	f->mPIA.WriteByte(2, 0); f->mPIA.WriteByte(0, 6); f->mPIA.WriteByte(2, 4); f->mPIA.WriteByte(0, 6);
	auto scan = [&]() {
		f->mPIA.WriteByte(0, 6); f->mPIA.WriteByte(0, 4); // clear falling: latch next switch
		uint32 packet = 0;
		for (int bit = 0; bit < 20; ++bit) {
			packet |= static_cast<uint32>((~f->mPIA.ReadByte(0)) & 1) << bit;
			f->mPIA.WriteByte(0, 0); f->mPIA.WriteByte(0, 4); // clock falling: shift
		}
		return packet;
	};
	AT_PORTABLE_TEST_ASSERT(context, scan() == 2 && scan() == 2); // fixed switch at (0,0)
	controller.ApplyAnalogInput(kATInputTrigger_Axis0, -65536);
	controller.ApplyAnalogInput(kATInputTrigger_Axis0 + 1, 65536);
	controller.SetDigitalTrigger(kATInputTrigger_Button0, true);
	// Independently rasterize the 2.5-switch-radius touch at the lower-left corner.
	controller.ColdReset();
	std::vector<uint32> expected {2};
	for (int x = 117; x < 120; ++x)
		for (int y = 117; y < 120; ++y)
			if ((x-119)*(x-119) + (y-119)*(y-119) < 6.25f)
				expected.push_back(2 + y * 4 + x * 512);
	for (uint32 packet : expected) AT_PORTABLE_TEST_ASSERT(context, scan() == packet);
	AT_PORTABLE_TEST_ASSERT(context, scan() == 2); // scan wraps to marker
	vdfastvector<ATInputPointerInfo> pointers; controller.GetPointers(pointers);
	AT_PORTABLE_TEST_ASSERT(context, pointers.size() == 1 && pointers[0].mbPrimary && pointers[0].mbPressed);
	AT_PORTABLE_TEST_ASSERT(context, pointers[0].mCoordSpace == ATInputPointerCoordinateSpace::Normalized && pointers[0].mRadius > 0);
	controller.SetDigitalTrigger(kATInputTrigger_Button0, false);
	// ColdReset restarts traversal; touch state remains a property of input.
	controller.ColdReset();
	AT_PORTABLE_TEST_ASSERT(context, scan() == 2 && scan() == 2);
	return true;
}
