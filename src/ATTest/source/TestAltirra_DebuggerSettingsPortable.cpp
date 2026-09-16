// Altirra portable debugger settings tests

#include <cstring>

#include <at/attest/portabletest.h>
#include <debuggersettings.h>
#include <vd2/system/registry.h>
#include <vd2/system/registrymemory.h>

namespace {
	class ATDebuggerSettingsRegistryScope {
	public:
		explicit ATDebuggerSettingsRegistryScope(IVDRegistryProvider *provider)
			: mpPrevious(VDGetRegistryProvider()) {
			VDSetRegistryProvider(provider);
		}

		~ATDebuggerSettingsRegistryScope() {
			VDSetRegistryProvider(mpPrevious);
		}

	private:
		IVDRegistryProvider *mpPrevious;
	};
}

bool ATTestAltirraDebuggerSettings(ATPortableTestContext& context) {
	VDRegistryProviderMemory provider;
	ATDebuggerSettingsRegistryScope providerScope(&provider);
	VDRegistryAppKey key("Settings", true);
	AT_PORTABLE_TEST_ASSERT(context, key.isReady());
	AT_PORTABLE_TEST_ASSERT(context, key.setBool("Portable bool", false));
	AT_PORTABLE_TEST_ASSERT(context, key.setString("Portable mode", L"m16x8"));

	ATDebuggerSetting<bool> boolSetting("Portable bool", true);
	AT_PORTABLE_TEST_ASSERT(context, !static_cast<bool>(boolSetting));

	// The first read is cached. An unrelated registry change must not silently
	// replace the live debugger setting or attached views.
	AT_PORTABLE_TEST_ASSERT(context, key.setBool("Portable bool", true));
	AT_PORTABLE_TEST_ASSERT(context, !static_cast<bool>(boolSetting));

	int firstChanges = 0;
	int secondChanges = 0;
	ATDebuggerSettingView<bool> firstView;
	ATDebuggerSettingView<bool> secondView;
	firstView.Attach(boolSetting, [&] { ++firstChanges; });
	secondView.Attach(boolSetting, [&] { ++secondChanges; });
	AT_PORTABLE_TEST_ASSERT(context, !static_cast<bool>(firstView));
	AT_PORTABLE_TEST_ASSERT(context, !static_cast<bool>(secondView));

	boolSetting = true;
	AT_PORTABLE_TEST_ASSERT(context, static_cast<bool>(firstView));
	AT_PORTABLE_TEST_ASSERT(context, static_cast<bool>(secondView));
	AT_PORTABLE_TEST_ASSERT(context, firstChanges == 1 && secondChanges == 1);
	AT_PORTABLE_TEST_ASSERT(context, key.getBool("Portable bool", false));

	// Reassigning the same value neither saves nor emits redundant callbacks.
	boolSetting = true;
	AT_PORTABLE_TEST_ASSERT(context, firstChanges == 1 && secondChanges == 1);

	secondView = false;
	AT_PORTABLE_TEST_ASSERT(context, !static_cast<bool>(firstView));
	AT_PORTABLE_TEST_ASSERT(context, !static_cast<bool>(secondView));
	AT_PORTABLE_TEST_ASSERT(context, firstChanges == 2 && secondChanges == 2);
	AT_PORTABLE_TEST_ASSERT(context, !key.getBool("Portable bool", true));

	ATDebuggerSetting<ATDebugger816MXPredictionMode> modeSetting(
		"Portable mode", ATDebugger816MXPredictionMode::Auto);
	AT_PORTABLE_TEST_ASSERT(
		context,
		static_cast<ATDebugger816MXPredictionMode>(modeSetting) ==
			ATDebugger816MXPredictionMode::M16X8);

	int modeChanges = 0;
	ATDebuggerSettingView<ATDebugger816MXPredictionMode> modeView;
	modeView.Attach(modeSetting, [&] { ++modeChanges; });
	modeSetting = ATDebugger816MXPredictionMode::Emulation;
	AT_PORTABLE_TEST_ASSERT(
		context,
		static_cast<ATDebugger816MXPredictionMode>(modeView) ==
			ATDebugger816MXPredictionMode::Emulation);
	AT_PORTABLE_TEST_ASSERT(context, modeChanges == 1);
	VDStringW persistedMode;
	AT_PORTABLE_TEST_ASSERT(context, key.getString("Portable mode", persistedMode));
	AT_PORTABLE_TEST_ASSERT(context, persistedMode == L"emulation");
	AT_PORTABLE_TEST_ASSERT(
		context,
		!strcmp(ATEnumToString(ATDebugger816MXPredictionMode::CurrentContext),
			"current_context"));

	// Assignment must win even before the lazy first read, including when the
	// assigned value happens to equal the constructor default.
	AT_PORTABLE_TEST_ASSERT(context, key.setBool("Assign before read", false));
	ATDebuggerSetting<bool> assignedBeforeRead("Assign before read", true);
	assignedBeforeRead = true;
	AT_PORTABLE_TEST_ASSERT(context, static_cast<bool>(assignedBeforeRead));
	AT_PORTABLE_TEST_ASSERT(context, key.getBool("Assign before read", false));

	return true;
}
