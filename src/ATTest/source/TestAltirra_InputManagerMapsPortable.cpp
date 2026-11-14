// Portable input-manager map, unit, and persistence tests.

#include <array>
#include <cstring>
#include <cwchar>
#include <memory>
#include <at/attest/portabletest.h>
#include <vd2/system/registry.h>
#include <vd2/system/registrymemory.h>
#include "inputmanager.h"
#include "inputmap.h"
#include "portmanagerportablefixture.h"

namespace {
	class ATInputManagerRegistryScope {
	public:
		explicit ATInputManagerRegistryScope(IVDRegistryProvider *provider)
			: mpPrevious(VDGetRegistryProvider()) { VDSetRegistryProvider(provider); }
		~ATInputManagerRegistryScope() { VDSetRegistryProvider(mpPrevious); }
	private:
		IVDRegistryProvider *mpPrevious;
	};

	struct InputNameSource final : IATInputUnitNameSource {
		uint32 mCode = 0;
		const wchar_t *mpName = nullptr;
		bool GetInputCodeName(uint32 id, VDStringW& name) const override {
			if (id != mCode)
				return false;
			name = mpName;
			return true;
		}
	};

	void InitPortableInputManager(ATInputManager& manager, PortManagerPortableFixture& fixture) {
		manager.Init(&fixture.mVideo.mScheduler, &fixture.mVideo.mScheduler, fixture.mPOKEY, fixture.mPorts, &fixture.mLightPen);
	}

	vdrefptr<ATInputMap> MakeJoystickMap(const wchar_t *name, uint32 port, bool quick, uint32 inputCode, uint32 target) {
		vdrefptr<ATInputMap> map(new ATInputMap);
		map->SetName(name);
		map->SetQuickMap(quick);
		const uint32 controller = map->AddController(kATInputControllerType_Joystick, port);
		map->AddMapping(inputCode, controller, target);
		return map;
	}

	bool FindMap(ATInputManager& manager, const wchar_t *name, vdrefptr<ATInputMap>& result) {
		for(uint32 i = 0; i < manager.GetInputMapCount(); ++i) {
			vdrefptr<ATInputMap> map;
			if (!manager.GetInputMapByIndex(i, ~map))
				return false;
			if (!wcscmp(map->GetName(), name)) {
				result = map;
				return true;
			}
		}
		return false;
	}
}

bool ATTestAltirraInputManagerMaps(ATPortableTestContext& context) {
	auto fixture = std::make_unique<PortManagerPortableFixture>();
	fixture->mPIA.WriteByte(2, 4);
	ATInputManager manager;
	InitPortableInputManager(manager, *fixture);

	auto map = MakeJoystickMap(L"Unit-specific joystick", 0, false, kATInputCode_JoyButton0, kATInputTrigger_Up);
	map->SetSpecificInputUnit(0);
	manager.AddInputMap(map);
	AT_PORTABLE_TEST_ASSERT(context, manager.GetInputMapCount() == 1 && !manager.IsInputMapEnabled(map));
	AT_PORTABLE_TEST_ASSERT(context, !manager.IsInputMapped(0, kATInputCode_JoyButton0));
	manager.ActivateInputMap(map, true);
	AT_PORTABLE_TEST_ASSERT(context, manager.IsInputMapped(0, kATInputCode_JoyButton0));
	AT_PORTABLE_TEST_ASSERT(context, !manager.IsInputMapped(1, kATInputCode_JoyButton0));
	manager.OnButtonDown(1, kATInputCode_JoyButton0);
	AT_PORTABLE_TEST_ASSERT(context, (fixture->mPIA.ReadByte(0) & 1) != 0);
	manager.OnButtonUp(1, kATInputCode_JoyButton0);
	manager.OnButtonDown(0, kATInputCode_JoyButton0);
	AT_PORTABLE_TEST_ASSERT(context, (fixture->mPIA.ReadByte(0) & 1) == 0);
	manager.OnButtonUp(0, kATInputCode_JoyButton0);
	AT_PORTABLE_TEST_ASSERT(context, (fixture->mPIA.ReadByte(0) & 1) != 0);

	manager.ActivateInputMap(map, false);
	AT_PORTABLE_TEST_ASSERT(context, !manager.IsInputMapped(0, kATInputCode_JoyButton0));
	manager.RemoveInputMap(map);
	AT_PORTABLE_TEST_ASSERT(context, manager.GetInputMapCount() == 0 && !manager.IsInputMapEnabled(map));

	auto alpha = MakeJoystickMap(L"Alpha quick", 0, true, kATInputCode_KeyA, kATInputTrigger_Up);
	auto zulu = MakeJoystickMap(L"Zulu quick", 1, true, kATInputCode_KeyA, kATInputTrigger_Up);
	auto normal = MakeJoystickMap(L"Not quick", 0, false, kATInputCode_KeyA, kATInputTrigger_Up);
	manager.AddInputMap(alpha); manager.AddInputMap(zulu); manager.AddInputMap(normal);
	AT_PORTABLE_TEST_ASSERT(context, manager.CycleQuickMaps() == alpha);
	AT_PORTABLE_TEST_ASSERT(context, manager.IsInputMapEnabled(alpha) && !manager.IsInputMapEnabled(normal));
	manager.OnButtonDown(0, kATInputCode_KeyA);
	AT_PORTABLE_TEST_ASSERT(context, (fixture->mPIA.ReadByte(0) & 1) == 0);
	manager.OnButtonUp(0, kATInputCode_KeyA);
	AT_PORTABLE_TEST_ASSERT(context, manager.CycleQuickMaps() == zulu);
	manager.OnButtonDown(0, kATInputCode_KeyA);
	AT_PORTABLE_TEST_ASSERT(context, (fixture->mPIA.ReadByte(0) & 0x10) == 0);
	manager.OnButtonUp(0, kATInputCode_KeyA);
	AT_PORTABLE_TEST_ASSERT(context, manager.CycleQuickMaps() == nullptr);
	AT_PORTABLE_TEST_ASSERT(context, !manager.IsInputMapEnabled(alpha) && !manager.IsInputMapEnabled(zulu));

	manager.Set5200Mode(true);
	manager.ResetToDefaults();
	AT_PORTABLE_TEST_ASSERT(context, manager.GetInputMapCount() > 0);
	vdrefptr<ATInputMap> preset;
	AT_PORTABLE_TEST_ASSERT(context, manager.GetPresetInputMapByIndex(0, ~preset));
	AT_PORTABLE_TEST_ASSERT(context, !wcscmp(preset->GetName(), L"Arrow Keys -> Joystick (port 1)"));
	AT_PORTABLE_TEST_ASSERT(context, !preset->IsQuickMap() && preset->GetControllerCount() == 1);
	manager.Shutdown();
	return true;
}

bool ATTestAltirraInputManagerUnits(ATPortableTestContext& context) {
	auto fixture = std::make_unique<PortManagerPortableFixture>();
	ATInputManager manager;
	InitPortableInputManager(manager, *fixture);
	std::array<InputNameSource, 32> sources;
	std::array<ATInputUnitIdentifier, 32> ids {};
	AT_PORTABLE_TEST_ASSERT(context, manager.GetInputUnitCount() == 32);
	for(int i = 0; i < 32; ++i) {
		ids[i].SetZero();
		ids[i].buf[0] = (char)(i + 1);
		sources[i].mCode = i == 1 || i == 2 ? kATInputCode_KeyA : kATInputCode_KeyB;
		sources[i].mpName = i == 1 ? L"first source" : (i == 2 ? L"second source" : L"unused source");
		const int unit = manager.RegisterInputUnit(ids[i], L"Portable unit", &sources[i]);
		AT_PORTABLE_TEST_ASSERT(context, unit == i);
		AT_PORTABLE_TEST_ASSERT(context, !wcscmp(manager.GetInputUnitName(unit), L"Portable unit"));
		AT_PORTABLE_TEST_ASSERT(context, manager.GetInputUnitIndexById(ids[i]) == i);
	}
	AT_PORTABLE_TEST_ASSERT(context, !manager.GetInputUnitName(-1) && !manager.GetInputUnitName(32));
	ATInputUnitIdentifier extra {};
	extra.SetZero(); extra.buf[0] = 99;
	AT_PORTABLE_TEST_ASSERT(context, manager.RegisterInputUnit(extra, L"overflow", nullptr) == -1);
	// Unplugging releases only that unit's contribution, including the highest
	// unit bit. Reusing the slot must not inherit a stale pressed button.
	auto map = MakeJoystickMap(L"Unplug test", 0, false, kATInputCode_JoyButton0, kATInputTrigger_Button0);
	manager.AddInputMap(map); manager.ActivateInputMap(map, true);
	manager.OnButtonDown(6, kATInputCode_JoyButton0);
	manager.OnButtonDown(31, kATInputCode_JoyButton0);
	AT_PORTABLE_TEST_ASSERT(context, fixture->Trigger(0) == 0);
	manager.UnregisterInputUnit(31);
	AT_PORTABLE_TEST_ASSERT(context, fixture->Trigger(0) == 0);
	manager.UnregisterInputUnit(6);
	AT_PORTABLE_TEST_ASSERT(context, fixture->Trigger(0) == 1);
	AT_PORTABLE_TEST_ASSERT(context, manager.RegisterInputUnit(ids[6], L"Reconnected", &sources[6]) == 6);
	manager.OnButtonDown(6, kATInputCode_JoyButton0);
	AT_PORTABLE_TEST_ASSERT(context, fixture->Trigger(0) == 0);
	manager.OnButtonUp(6, kATInputCode_JoyButton0);
	AT_PORTABLE_TEST_ASSERT(context, fixture->Trigger(0) == 1);
	AT_PORTABLE_TEST_ASSERT(context, manager.RegisterInputUnit(ids[31], L"Reconnected 31", &sources[31]) == 31);
	VDStringW name;
	manager.GetNameForInputCode(kATInputCode_KeyA | kATInputCode_SpecificUnit | (31u << kATInputCode_UnitShift), name);
	AT_PORTABLE_TEST_ASSERT(context, name == L"first source");
	manager.UnregisterInputUnit(1);
	AT_PORTABLE_TEST_ASSERT(context, !manager.GetInputUnitName(1) && manager.GetInputUnitIndexById(ids[1]) == -1);
	manager.GetNameForInputCode(kATInputCode_KeyA, name);
	AT_PORTABLE_TEST_ASSERT(context, name == L"second source");
	InputNameSource replacement;
	replacement.mCode = kATInputCode_KeyA;
	replacement.mpName = L"replacement source";
	AT_PORTABLE_TEST_ASSERT(context, manager.RegisterInputUnit(extra, L"Reused slot", &replacement) == 1);
	manager.GetNameForInputCode(kATInputCode_KeyA, name);
	AT_PORTABLE_TEST_ASSERT(context, name == L"replacement source");
	manager.UnregisterInputUnit(0);
	manager.UnregisterInputUnit(1);
	for(int i = 2; i < 32; ++i)
		manager.UnregisterInputUnit(i);
	manager.Shutdown();
	return true;
}

bool ATTestAltirraInputManagerPersistence(ATPortableTestContext& context) {
	VDRegistryProviderMemory provider;
	ATInputManagerRegistryScope providerScope(&provider);
	VDRegistryKey key("PortableInputManager", false, true);
	AT_PORTABLE_TEST_ASSERT(context, key.isReady());
	auto fixture = std::make_unique<PortManagerPortableFixture>();
	fixture->mPIA.WriteByte(2, 4);

	{
		ATInputManager manager;
		InitPortableInputManager(manager, *fixture);
		auto zulu = MakeJoystickMap(L"Zulu", 1, true, kATInputCode_KeyZ, kATInputTrigger_Up);
		auto alpha = MakeJoystickMap(L"Alpha", 0, false, kATInputCode_KeyA, kATInputTrigger_Left);
		manager.AddInputMap(zulu); manager.AddInputMap(alpha);
		manager.ActivateInputMap(zulu, true);
		manager.SaveMaps(key);
		manager.SaveSelections(key);
		ATInputMap firstSaved;
		AT_PORTABLE_TEST_ASSERT(context, firstSaved.Load(key, "Input map 0"));
		AT_PORTABLE_TEST_ASSERT(context, !wcscmp(firstSaved.GetName(), L"Alpha"));
		manager.OnButtonDown(0, kATInputCode_KeyZ);
		AT_PORTABLE_TEST_ASSERT(context, (fixture->mPIA.ReadByte(0) & 0x10) == 0);
		manager.OnButtonUp(0, kATInputCode_KeyZ);
		manager.Shutdown();
	}

	{
		ATInputManager manager;
		InitPortableInputManager(manager, *fixture);
		AT_PORTABLE_TEST_ASSERT(context, manager.LoadMaps(key));
		AT_PORTABLE_TEST_ASSERT(context, manager.GetInputMapCount() == 2);
		manager.LoadSelections(key, kATInputControllerType_Joystick);
		vdrefptr<ATInputMap> alpha, zulu;
		AT_PORTABLE_TEST_ASSERT(context, FindMap(manager, L"Alpha", alpha) && FindMap(manager, L"Zulu", zulu));
		AT_PORTABLE_TEST_ASSERT(context, !manager.IsInputMapEnabled(alpha) && manager.IsInputMapEnabled(zulu));
		AT_PORTABLE_TEST_ASSERT(context, !alpha->IsQuickMap() && zulu->IsQuickMap());
		manager.OnButtonDown(0, kATInputCode_KeyZ);
		AT_PORTABLE_TEST_ASSERT(context, (fixture->mPIA.ReadByte(0) & 0x10) == 0);
		manager.OnButtonUp(0, kATInputCode_KeyZ);
		manager.ActivateInputMap(zulu, false);
		manager.ActivateInputMap(alpha, true);
		manager.OnButtonDown(0, kATInputCode_KeyA);
		AT_PORTABLE_TEST_ASSERT(context, (fixture->mPIA.ReadByte(0) & 0x04) == 0);
		manager.OnButtonUp(0, kATInputCode_KeyA);
		manager.Shutdown();
	}
	return true;
}
