// Altirra portable command line parser tests

#include <cwchar>

#include <at/attest/portabletest.h>
#include <vd2/system/cmdline.h>
#include <vd2/system/VDString.h>

bool ATTestSystemCommandLine(ATPortableTestContext& context) {
	VDCommandLine commandLine(
		L"\"/Applications/Altirra App\" /Help input.atr "
		L"/out:\"result file.atr\" /? /Mode Turbo");

	AT_PORTABLE_TEST_ASSERT(context, commandLine.GetCount() == 8);
	AT_PORTABLE_TEST_ASSERT(context,
		!wcscmp(commandLine[0], L"/Applications/Altirra App"));
	AT_PORTABLE_TEST_ASSERT(context, !wcscmp(commandLine[1], L"/Help"));
	AT_PORTABLE_TEST_ASSERT(context, !wcscmp(commandLine[2], L"input.atr"));
	AT_PORTABLE_TEST_ASSERT(context, !wcscmp(commandLine[3], L"/out"));
	AT_PORTABLE_TEST_ASSERT(context, !wcscmp(commandLine[4], L"result file.atr"));
	AT_PORTABLE_TEST_ASSERT(context, !wcscmp(commandLine[5], L"/?"));
	AT_PORTABLE_TEST_ASSERT(context, !wcscmp(commandLine[6], L"/Mode"));
	AT_PORTABLE_TEST_ASSERT(context, !wcscmp(commandLine[7], L"Turbo"));
	AT_PORTABLE_TEST_ASSERT(context, commandLine[-1] == nullptr);
	AT_PORTABLE_TEST_ASSERT(context, commandLine[8] == nullptr);
	AT_PORTABLE_TEST_ASSERT(context, commandLine(8).empty());
	AT_PORTABLE_TEST_ASSERT(context, commandLine(4) == L"result file.atr");

	AT_PORTABLE_TEST_ASSERT(context, commandLine.FindSwitch(L"help"));
	AT_PORTABLE_TEST_ASSERT(context, commandLine.FindSwitch(L"OUT"));
	AT_PORTABLE_TEST_ASSERT(context, commandLine.FindSwitch(L"?"));
	AT_PORTABLE_TEST_ASSERT(context, !commandLine.FindSwitch(L"Turbo"));
	AT_PORTABLE_TEST_ASSERT(context, !commandLine.FindSwitch(L"missing"));

	VDCommandLineIterator iterator;
	const wchar_t *token = nullptr;
	bool isSwitch = false;
	AT_PORTABLE_TEST_ASSERT(context,
		commandLine.GetNextArgument(iterator, token, isSwitch));
	AT_PORTABLE_TEST_ASSERT(context, isSwitch && !wcscmp(token, L"/Help"));
	AT_PORTABLE_TEST_ASSERT(context,
		commandLine.GetNextArgument(iterator, token, isSwitch));
	AT_PORTABLE_TEST_ASSERT(context, !isSwitch && !wcscmp(token, L"input.atr"));

	VDCommandLineIterator typedIterator;
	AT_PORTABLE_TEST_ASSERT(context,
		!commandLine.GetNextNonSwitchArgument(typedIterator, token));
	AT_PORTABLE_TEST_ASSERT(context,
		commandLine.GetNextSwitchArgument(typedIterator, token));
	AT_PORTABLE_TEST_ASSERT(context, !wcscmp(token, L"/Help"));
	AT_PORTABLE_TEST_ASSERT(context,
		commandLine.GetNextNonSwitchArgument(typedIterator, token));
	AT_PORTABLE_TEST_ASSERT(context, !wcscmp(token, L"input.atr"));
	AT_PORTABLE_TEST_ASSERT(context,
		commandLine.GetNextSwitchArgument(typedIterator, token));
	AT_PORTABLE_TEST_ASSERT(context, !wcscmp(token, L"/out"));

	VDCommandLine removable(
		L"app /Help input.atr /out:\"result file.atr\" /Mode Turbo");
	const wchar_t *switchValue = nullptr;
	AT_PORTABLE_TEST_ASSERT(context,
		removable.FindAndRemoveSwitch(L"OUT", switchValue));
	AT_PORTABLE_TEST_ASSERT(context, !wcscmp(switchValue, L"result file.atr"));
	AT_PORTABLE_TEST_ASSERT(context, removable.GetCount() == 5);
	AT_PORTABLE_TEST_ASSERT(context, !removable.FindSwitch(L"out"));
	AT_PORTABLE_TEST_ASSERT(context, removable.FindAndRemoveSwitch(L"help"));
	AT_PORTABLE_TEST_ASSERT(context, removable.GetCount() == 4);
	AT_PORTABLE_TEST_ASSERT(context, !wcscmp(removable[1], L"input.atr"));
	AT_PORTABLE_TEST_ASSERT(context, !removable.FindAndRemoveSwitch(L"missing"));

	VDCommandLine noValue(L"app /First /Second");
	switchValue = nullptr;
	AT_PORTABLE_TEST_ASSERT(context,
		noValue.FindAndRemoveSwitch(L"first", switchValue));
	AT_PORTABLE_TEST_ASSERT(context, switchValue && !*switchValue);
	AT_PORTABLE_TEST_ASSERT(context, noValue.FindSwitch(L"second"));

	VDCommandLine quoting(L"app \"two words\" \"a\\\"b\" plain\\path");
	AT_PORTABLE_TEST_ASSERT(context, quoting.GetCount() == 4);
	AT_PORTABLE_TEST_ASSERT(context, !wcscmp(quoting[1], L"two words"));
	AT_PORTABLE_TEST_ASSERT(context, !wcscmp(quoting[2], L"a\"b"));
	AT_PORTABLE_TEST_ASSERT(context, !wcscmp(quoting[3], L"plain\\path"));

	quoting.Init(L"newapp one\ttwo");
	AT_PORTABLE_TEST_ASSERT(context, quoting.GetCount() == 3);
	AT_PORTABLE_TEST_ASSERT(context, !wcscmp(quoting[0], L"newapp"));
	AT_PORTABLE_TEST_ASSERT(context, !wcscmp(quoting[1], L"one"));
	AT_PORTABLE_TEST_ASSERT(context, !wcscmp(quoting[2], L"two"));

	return true;
}
