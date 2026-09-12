// Altirra portable input map tests

#include <cwchar>
#include <vector>

#include <at/attest/portabletest.h>
#include <inputmap.h>
#include <vd2/system/registry.h>
#include <vd2/system/registrymemory.h>

namespace {
	class ATInputMapRegistryScope {
	public:
		explicit ATInputMapRegistryScope(IVDRegistryProvider *provider)
			: mpPrevious(VDGetRegistryProvider()) {
			VDSetRegistryProvider(provider);
		}

		~ATInputMapRegistryScope() {
			VDSetRegistryProvider(mpPrevious);
		}

	private:
		IVDRegistryProvider *mpPrevious;
	};
}

bool ATTestAltirraInputMap(ATPortableTestContext& context) {
	VDRegistryProviderMemory provider;
	ATInputMapRegistryScope providerScope(&provider);
	VDRegistryKey key("PortableInputMap", false, true);
	AT_PORTABLE_TEST_ASSERT(context, key.isReady());

	ATInputMap map;
	AT_PORTABLE_TEST_ASSERT(context, map.GetSpecificInputUnit() == -1);
	AT_PORTABLE_TEST_ASSERT(context, !map.IsQuickMap());
	map.SetName(L"A\u017C\U0001F600");
	map.SetQuickMap(true);
	map.SetSpecificInputUnit(7);
	const uint32 joystickId = map.AddController(kATInputControllerType_Joystick, 2);
	map.AddControllers({
		{ kATInputControllerType_Paddle, 5 },
		{ kATInputControllerType_Console, 5 }
	});
	AT_PORTABLE_TEST_ASSERT(context, joystickId == 0);
	AT_PORTABLE_TEST_ASSERT(context, map.GetControllerCount() == 3);
	AT_PORTABLE_TEST_ASSERT(context, map.HasControllerType(kATInputControllerType_Paddle));
	AT_PORTABLE_TEST_ASSERT(context, !map.HasControllerType(kATInputControllerType_Keyboard));
	AT_PORTABLE_TEST_ASSERT(context, map.UsesPhysicalPort(2));
	AT_PORTABLE_TEST_ASSERT(context, !map.UsesPhysicalPort(5));
	map.AddMapping(kATInputCode_KeyA, joystickId, 0x1234);
	map.AddMappings({ { kATInputCode_MouseLMB, 1, 0x5678 } });
	AT_PORTABLE_TEST_ASSERT(context, map.GetMappingCount() == 2);
	map.Save(key, "Current");

	const int byteLength = key.getBinaryLength("Current");
	AT_PORTABLE_TEST_ASSERT(context, byteLength == 19 * 4);
	std::vector<uint32> words(byteLength / 4);
	AT_PORTABLE_TEST_ASSERT(context,
		key.getBinary("Current", (char *)words.data(), byteLength));
	AT_PORTABLE_TEST_ASSERT(context, words[0] == 2);
	AT_PORTABLE_TEST_ASSERT(context, words[1] == 4); // UTF-16 units, including a surrogate pair
	AT_PORTABLE_TEST_ASSERT(context, words[2] == 3);
	AT_PORTABLE_TEST_ASSERT(context, words[3] == 2);
	AT_PORTABLE_TEST_ASSERT(context, words[4] == 7);
	AT_PORTABLE_TEST_ASSERT(context, words[5] == 0x017C0041);
	AT_PORTABLE_TEST_ASSERT(context, words[6] == 0xDE00D83D);

	ATInputMap loaded;
	AT_PORTABLE_TEST_ASSERT(context, loaded.Load(key, "Current"));
	AT_PORTABLE_TEST_ASSERT(context, !wcscmp(loaded.GetName(), L"A\u017C\U0001F600"));
	AT_PORTABLE_TEST_ASSERT(context, loaded.GetSpecificInputUnit() == 7);
	AT_PORTABLE_TEST_ASSERT(context, loaded.GetControllerCount() == 3);
	AT_PORTABLE_TEST_ASSERT(context, loaded.GetController(0).mType == kATInputControllerType_Joystick);
	AT_PORTABLE_TEST_ASSERT(context, loaded.GetController(1).mIndex == 5);
	AT_PORTABLE_TEST_ASSERT(context, loaded.GetMappingCount() == 2);
	AT_PORTABLE_TEST_ASSERT(context, loaded.GetMapping(0).mInputCode == kATInputCode_KeyA);
	AT_PORTABLE_TEST_ASSERT(context, loaded.GetMapping(0).mCode == 0x1234);
	AT_PORTABLE_TEST_ASSERT(context, loaded.GetMapping(1).mControllerId == 1);

	const uint32 shortV2[] {2, 0, 0, 0};
	AT_PORTABLE_TEST_ASSERT(context,
		key.setBinary("ShortV2", (const char *)shortV2, sizeof shortV2));
	AT_PORTABLE_TEST_ASSERT(context, !loaded.Load(key, "ShortV2"));
	AT_PORTABLE_TEST_ASSERT(context, loaded.GetSpecificInputUnit() == 7);
	AT_PORTABLE_TEST_ASSERT(context, loaded.GetControllerCount() == 3);

	const uint32 truncated[] {2, 0, 0xFFFFFF, 0xFFFFFF, 7};
	AT_PORTABLE_TEST_ASSERT(context,
		key.setBinary("Truncated", (const char *)truncated, sizeof truncated));
	AT_PORTABLE_TEST_ASSERT(context, !loaded.Load(key, "Truncated"));

	const uint32 badSurrogate[] {2, 1, 0, 0, 7, 0xD83D};
	AT_PORTABLE_TEST_ASSERT(context,
		key.setBinary("BadSurrogate", (const char *)badSurrogate, sizeof badSurrogate));
	AT_PORTABLE_TEST_ASSERT(context, !loaded.Load(key, "BadSurrogate"));
	AT_PORTABLE_TEST_ASSERT(context, !wcscmp(loaded.GetName(), L"A\u017C\U0001F600"));

	AT_PORTABLE_TEST_ASSERT(context,
		key.setBinary("Unaligned", (const char *)truncated, sizeof truncated - 1));
	AT_PORTABLE_TEST_ASSERT(context, !loaded.Load(key, "Unaligned"));

	const uint32 legacy[] {1, 1, 0, 0, 'L'};
	AT_PORTABLE_TEST_ASSERT(context,
		key.setBinary("Legacy", (const char *)legacy, sizeof legacy));
	AT_PORTABLE_TEST_ASSERT(context, loaded.Load(key, "Legacy"));
	AT_PORTABLE_TEST_ASSERT(context, !wcscmp(loaded.GetName(), L"L"));
	AT_PORTABLE_TEST_ASSERT(context, loaded.GetSpecificInputUnit() == -1);
	AT_PORTABLE_TEST_ASSERT(context, loaded.GetControllerCount() == 0);
	AT_PORTABLE_TEST_ASSERT(context, loaded.GetMappingCount() == 0);

	map.Clear();
	AT_PORTABLE_TEST_ASSERT(context, map.GetControllerCount() == 0);
	AT_PORTABLE_TEST_ASSERT(context, map.GetMappingCount() == 0);
	AT_PORTABLE_TEST_ASSERT(context, map.GetSpecificInputUnit() == -1);
	return true;
}
