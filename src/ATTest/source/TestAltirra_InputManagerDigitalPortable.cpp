// Real input-map routing through the production input manager and controller chips.

#include <memory>
#include <vector>

#include <at/attest/portabletest.h>
#include <inputdefs.h>
#include <inputmap.h>
#include <inputmanager.h>
#include "portmanagerportablefixture.h"

namespace {
	struct InputManagerScope {
		ATInputManager mManager;

		explicit InputManagerScope(PortManagerPortableFixture& f) {
			mManager.Init(&f.mVideo.mScheduler, &f.mVideo.mScheduler, f.mPOKEY, f.mPorts, &f.mLightPen);
			mManager.SetRestrictedMode(false);
		}
		~InputManagerScope() { mManager.Shutdown(); }
	};

	struct ConsoleEvent {
		uint32 mId;
		bool mbState;
	};

	struct ConsoleEventSink final : IATInputConsoleCallback {
		void SetConsoleTrigger(uint32 id, bool state) override { mEvents.push_back({ id, state }); }
		std::vector<ConsoleEvent> mEvents;
	};

	vdrefptr<ATInputMap> MakeMap(ATInputManager& manager, ATInputControllerType type, uint32 port,
		std::initializer_list<ATInputMap::Mapping> mappings, int specificUnit = -1) {
		vdrefptr<ATInputMap> map(new ATInputMap);
		map->SetSpecificInputUnit(specificUnit);
		map->AddController(type, port);
		map->AddMappings(mappings);
		manager.AddInputMap(map.get());
		manager.ActivateInputMap(map.get(), true);
		return map;
	}

	void ConfigurePIAInputs(PortManagerPortableFixture& f) {
		// CRA2/CRB2 select the PIA port data registers; stick directions drive them low.
		f.mPIA.WriteByte(2, 4);
		f.mPIA.WriteByte(3, 4);
	}

	uint8 ReadPort0(PortManagerPortableFixture& f) {
		return f.mPIA.ReadByte(0) & 15;
	}

	bool TestButtonsAndUnits(ATPortableTestContext& context) {
		auto f = std::make_unique<PortManagerPortableFixture>();
		ConfigurePIAInputs(*f);
		InputManagerScope scope(*f);
		auto& input = scope.mManager;

		auto generic = MakeMap(input, kATInputControllerType_Joystick, 0, {
			{ kATInputCode_KeyA, 0, kATInputTrigger_Up },
			{ kATInputCode_KeyB, 0, kATInputTrigger_Button0 },
		});
		auto unitBound = MakeMap(input, kATInputControllerType_Joystick, 0, {
			{ kATInputCode_JoyButton0 + 2, 0, kATInputTrigger_Down },
		}, 7);

		AT_PORTABLE_TEST_ASSERT(context, ReadPort0(*f) == 15);
		AT_PORTABLE_TEST_ASSERT(context, input.IsInputMapped(0, kATInputCode_KeyA));
		AT_PORTABLE_TEST_ASSERT(context, input.IsInputMapped(7, kATInputCode_JoyButton0 + 2));
		AT_PORTABLE_TEST_ASSERT(context, !input.IsInputMapped(6, kATInputCode_JoyButton0 + 2));

		// Two physical units share the generic KeyA mapping; the joystick releases
		// only after the last unit releases the same logical input.
		input.OnButtonDown(0, kATInputCode_KeyA);
		AT_PORTABLE_TEST_ASSERT(context, ReadPort0(*f) == 14);
		input.OnButtonDown(1, kATInputCode_KeyA);
		input.OnButtonUp(0, kATInputCode_KeyA);
		AT_PORTABLE_TEST_ASSERT(context, ReadPort0(*f) == 14);
		input.OnButtonUp(1, kATInputCode_KeyA);
		AT_PORTABLE_TEST_ASSERT(context, ReadPort0(*f) == 15);

		// The same merge rule applies to GTIA's active-low trigger input.
		input.OnButtonDown(0, kATInputCode_KeyB);
		input.OnButtonDown(1, kATInputCode_KeyB);
		AT_PORTABLE_TEST_ASSERT(context, f->Trigger(0) == 0);
		input.OnButtonUp(0, kATInputCode_KeyB);
		AT_PORTABLE_TEST_ASSERT(context, f->Trigger(0) == 0);
		input.OnButtonUp(1, kATInputCode_KeyB);
		AT_PORTABLE_TEST_ASSERT(context, f->Trigger(0) == 1);

		// The second map is bound to unit 7; an identically named input from
		// unit 6 is ignored, while unit 7 drives the real PIA down pin.
		input.OnButtonDown(6, kATInputCode_JoyButton0 + 2);
		AT_PORTABLE_TEST_ASSERT(context, ReadPort0(*f) == 15);
		input.OnButtonDown(7, kATInputCode_JoyButton0 + 2);
		AT_PORTABLE_TEST_ASSERT(context, ReadPort0(*f) == 13);
		input.OnButtonUp(7, kATInputCode_JoyButton0 + 2);
		AT_PORTABLE_TEST_ASSERT(context, ReadPort0(*f) == 15);
		input.OnButtonUp(6, kATInputCode_JoyButton0 + 2);

		input.Shutdown(); // Controllers are detached while the real fixture is alive.
		return true;
	}

	bool TestModesAndRestricted(ATPortableTestContext& context) {
		auto f = std::make_unique<PortManagerPortableFixture>();
		ConfigurePIAInputs(*f);
		f->mPorts.SetPorts34Enabled(true);
		ConsoleEventSink console;
		InputManagerScope scope(*f);
		auto& input = scope.mManager;
		input.SetConsoleCallback(&console);

		auto toggle = MakeMap(input, kATInputControllerType_Joystick, 0, {
			{ kATInputCode_KeyT, 0, kATInputTrigger_Button0 | kATInputTriggerMode_Toggle },
		});
		auto inverted = MakeMap(input, kATInputControllerType_Joystick, 1, {
			{ kATInputCode_KeyI, 0, kATInputTrigger_Button0 | kATInputTriggerMode_Inverted },
		});
		auto autofire = MakeMap(input, kATInputControllerType_Joystick, 2, {
			{ kATInputCode_KeyF, 0, kATInputTrigger_Button0 | kATInputTriggerMode_AutoFire },
		});
		auto ordinary = MakeMap(input, kATInputControllerType_Joystick, 3, {
			{ kATInputCode_KeyO, 0, kATInputTrigger_Button0 },
		});
		auto ui = MakeMap(input, kATInputControllerType_Console, 0, {
			{ kATInputCode_KeyU, 0, kATInputTrigger_UILeft },
		});

		AT_PORTABLE_TEST_ASSERT(context, f->Trigger(0) == 1 && f->Trigger(1) == 0);
		input.OnButtonDown(0, kATInputCode_KeyT);
		input.OnButtonUp(0, kATInputCode_KeyT);
		AT_PORTABLE_TEST_ASSERT(context, f->Trigger(0) == 0); // toggle latches after key-up
		input.OnButtonDown(0, kATInputCode_KeyT);
		input.OnButtonUp(0, kATInputCode_KeyT);
		AT_PORTABLE_TEST_ASSERT(context, f->Trigger(0) == 1);

		// Inverted mappings are pre-activated; pressing the bound key releases,
		// and releasing the key restores the idle asserted trigger.
		input.OnButtonDown(0, kATInputCode_KeyI);
		AT_PORTABLE_TEST_ASSERT(context, f->Trigger(1) == 1);
		input.OnButtonUp(0, kATInputCode_KeyI);
		AT_PORTABLE_TEST_ASSERT(context, f->Trigger(1) == 0);

		// Auto-fire starts asserted and changes state every third Poll call.
		input.OnButtonDown(0, kATInputCode_KeyF);
		for (int i = 0; i < 5; ++i)
			input.Poll(0.0f);
		AT_PORTABLE_TEST_ASSERT(context, f->Trigger(2) == 0);
		input.Poll(0.0f);
		AT_PORTABLE_TEST_ASSERT(context, f->Trigger(2) == 1);
		for (int i = 0; i < 3; ++i)
			input.Poll(0.0f);
		AT_PORTABLE_TEST_ASSERT(context, f->Trigger(2) == 0);
		input.OnButtonUp(0, kATInputCode_KeyF);
		AT_PORTABLE_TEST_ASSERT(context, f->Trigger(2) == 1);

		input.OnButtonDown(0, kATInputCode_KeyO);
		AT_PORTABLE_TEST_ASSERT(context, f->Trigger(3) == 0);
		input.SetRestrictedMode(true);
		// Restricted mode promises that non-UI controls are forced off, including
		// controls that were already held when the mode changed.
		AT_PORTABLE_TEST_ASSERT(context, f->Trigger(3) == 1);
		input.OnButtonDown(0, kATInputCode_KeyU);
		AT_PORTABLE_TEST_ASSERT(context, console.mEvents.size() == 1
			&& console.mEvents[0].mId == kATInputTrigger_UILeft && console.mEvents[0].mbState);
		input.OnButtonUp(0, kATInputCode_KeyU);
		AT_PORTABLE_TEST_ASSERT(context, console.mEvents.size() == 2 && !console.mEvents[1].mbState);
		input.OnButtonUp(0, kATInputCode_KeyO);
		input.SetRestrictedMode(false);
		input.OnButtonDown(0, kATInputCode_KeyO);
		AT_PORTABLE_TEST_ASSERT(context, f->Trigger(3) == 0);
		input.OnButtonUp(0, kATInputCode_KeyO);
		AT_PORTABLE_TEST_ASSERT(context, f->Trigger(3) == 1);

		input.Shutdown();
		return true;
	}

	bool TestFlags(ATPortableTestContext& context) {
		auto f = std::make_unique<PortManagerPortableFixture>();
		ConfigurePIAInputs(*f);
		InputManagerScope scope(*f);
		auto& input = scope.mManager;

		vdrefptr<ATInputMap> map(new ATInputMap);
		const uint32 stateController = map->AddController(kATInputControllerType_InputState, 0);
		const uint32 stickController = map->AddController(kATInputControllerType_Joystick, 0);
		map->AddMapping(kATInputCode_KeyS, stateController, kATInputTrigger_Flag0);
		map->AddMapping(kATInputCode_KeyK | kATInputCode_FlagCheck0, stickController, kATInputTrigger_Up);
		map->AddMapping(kATInputCode_KeyJ | kATInputCode_FlagCheck0 | kATInputCode_FlagValue0,
			stickController, kATInputTrigger_Button0);
		input.AddInputMap(map.get());
		input.ActivateInputMap(map.get(), true);

		// With map flag 0 initially false, its false-conditioned direction works
		// while its true-conditioned fire button remains disconnected.
		input.OnButtonDown(0, kATInputCode_KeyK);
		input.OnButtonDown(0, kATInputCode_KeyJ);
		AT_PORTABLE_TEST_ASSERT(context, ReadPort0(*f) == 14 && f->Trigger(0) == 1);

		// The state-controller's Flag0 target flips the map's first condition.
		// A currently held false-conditioned key is released; held keys do not
		// spuriously activate mappings when their condition later becomes true.
		input.OnButtonDown(0, kATInputCode_KeyS);
		AT_PORTABLE_TEST_ASSERT(context, ReadPort0(*f) == 15 && f->Trigger(0) == 1);
		input.OnButtonUp(0, kATInputCode_KeyK);
		input.OnButtonUp(0, kATInputCode_KeyJ);
		input.OnButtonDown(0, kATInputCode_KeyJ);
		AT_PORTABLE_TEST_ASSERT(context, f->Trigger(0) == 0);
		input.OnButtonUp(0, kATInputCode_KeyS);
		AT_PORTABLE_TEST_ASSERT(context, f->Trigger(0) == 1);
		input.OnButtonUp(0, kATInputCode_KeyJ);

		input.Shutdown();
		return true;
	}
}

bool ATTestAltirraInputManagerButtons(ATPortableTestContext& context) {
	return TestButtonsAndUnits(context);
}

bool ATTestAltirraInputManagerModes(ATPortableTestContext& context) {
	return TestModesAndRestricted(context);
}

bool ATTestAltirraInputManagerFlags(ATPortableTestContext& context) {
	return TestFlags(context);
}
