// Altirra portable OS helper tests

#include <cstring>
#include <at/attest/portabletest.h>
#include <vd2/system/file.h>
#include <vd2/system/filesys.h>
#include <vd2/system/process.h>
#include <vd2/system/time.h>
#include <vd2/system/vdstring.h>
#include <oshelper.h>

namespace {
	class ATOSHelperTestFile {
	public:
		ATOSHelperTestFile() {
			mPath.sprintf(
				L"altirra-oshelper-test-%u-%llu.tmp",
				static_cast<unsigned>(VDGetCurrentProcessId()),
				static_cast<unsigned long long>(VDGetCurrentTick64()));

			VDFile file(mPath.c_str(), nsVDFile::kWrite | nsVDFile::kCreateAlways);
			file.write("test", 4);
		}

		~ATOSHelperTestFile() {
			try {
				ATFileSetReadOnlyAttribute(mPath.c_str(), false);
			} catch(...) {
			}

			VDRemoveFile(mPath.c_str());
		}

		VDStringW mPath;
	};
}

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

	{
		ATOSHelperTestFile file;
		AT_PORTABLE_TEST_ASSERT(context,
			!(VDFileGetAttributes(file.mPath.c_str()) & kVDFileAttr_ReadOnly));

		ATFileSetReadOnlyAttribute(file.mPath.c_str(), true);
		AT_PORTABLE_TEST_ASSERT(context,
			VDFileGetAttributes(file.mPath.c_str()) & kVDFileAttr_ReadOnly);

		ATFileSetReadOnlyAttribute(file.mPath.c_str(), false);
		AT_PORTABLE_TEST_ASSERT(context,
			!(VDFileGetAttributes(file.mPath.c_str()) & kVDFileAttr_ReadOnly));
	}

	{
		uint8 firstGuid[16] {};
		uint8 secondGuid[16] {};
		ATGenerateGuid(firstGuid);
		ATGenerateGuid(secondGuid);

		const uint8 zeroGuid[16] {};
		AT_PORTABLE_TEST_ASSERT(context, std::memcmp(firstGuid, zeroGuid, sizeof firstGuid));
		AT_PORTABLE_TEST_ASSERT(context, std::memcmp(secondGuid, zeroGuid, sizeof secondGuid));
		AT_PORTABLE_TEST_ASSERT(context, std::memcmp(firstGuid, secondGuid, sizeof firstGuid));
	}

	// This is environment-dependent, but both paths through the native
	// implementation must be safe to query without elevated privileges.
	(void)ATIsUserAdministrator();

	return true;
}
