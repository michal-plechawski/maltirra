// Altirra portable OS helper tests

#include <cstring>
#include <at/attest/portabletest.h>
#include <vd2/system/vdstring.h>
#include <oshelper.h>

bool ATTestAltirraOSHelper(ATPortableTestContext& context) {
	AT_PORTABLE_TEST_ASSERT(context, !std::strcmp(ATEnumToString(ATProcessEfficiencyMode::Default), "default"));
	AT_PORTABLE_TEST_ASSERT(context, !std::strcmp(ATEnumToString(ATProcessEfficiencyMode::Performance), "performance"));
	AT_PORTABLE_TEST_ASSERT(context, !std::strcmp(ATEnumToString(ATProcessEfficiencyMode::Efficiency), "efficiency"));

	const auto performance = ATParseEnum<ATProcessEfficiencyMode>(VDStringSpanA("performance"));
	AT_PORTABLE_TEST_ASSERT(context, performance.mValid);
	AT_PORTABLE_TEST_ASSERT(context, performance.mValue == ATProcessEfficiencyMode::Performance);

	const auto invalid = ATParseEnum<ATProcessEfficiencyMode>(VDStringSpanA("invalid"));
	AT_PORTABLE_TEST_ASSERT(context, !invalid.mValid);
	AT_PORTABLE_TEST_ASSERT(context, invalid.mValue == ATProcessEfficiencyMode::Default);

	{
		const wchar_t *args[] { L"/portable", L"plain", L"path/to/file" };
		AT_PORTABLE_TEST_ASSERT(context, ATBuildEscapedCommandLine(args) == L"/portable plain path/to/file");
	}

	{
		const wchar_t *args[] { L"", L"two words", L"tab\tvalue", L"say\"hello" };
		AT_PORTABLE_TEST_ASSERT(context,
			ATBuildEscapedCommandLine(args) == L"\"\" \"two words\" \"tab\tvalue\" \"say\\\"hello\"");
	}

	{
		const wchar_t *args[] { L"C:\\plain\\path", L"C:\\space path\\", L"a\\\\\"b" };
		AT_PORTABLE_TEST_ASSERT(context,
			ATBuildEscapedCommandLine(args) == L"C:\\plain\\path \"C:\\space path\\\\\" \"a\\\\\\\\\\\"b\"");
	}

	{
		const wchar_t *args[] { nullptr };
		AT_PORTABLE_TEST_ASSERT(context, ATBuildEscapedCommandLine(args) == L"\"\"");
	}

	return true;
}
