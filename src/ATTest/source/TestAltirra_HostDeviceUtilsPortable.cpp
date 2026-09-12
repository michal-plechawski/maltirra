// Altirra portable host device filename tests

#include <cstring>

#include <at/attest/portabletest.h>
#include <hostdeviceutils.h>

bool ATTestAltirraHostDeviceUtils(ATPortableTestContext& context) {
	AT_PORTABLE_TEST_ASSERT(context, ATHostDeviceIsDevice(L"CON"));
	AT_PORTABLE_TEST_ASSERT(context, ATHostDeviceIsDevice(L"con.txt"));
	AT_PORTABLE_TEST_ASSERT(context, ATHostDeviceIsDevice(L"COM9.log"));
	AT_PORTABLE_TEST_ASSERT(context, ATHostDeviceIsDevice(L"LPT1"));
	AT_PORTABLE_TEST_ASSERT(context, !ATHostDeviceIsDevice(L"COM10"));
	AT_PORTABLE_TEST_ASSERT(context, !ATHostDeviceIsDevice(L"printer.txt"));

	AT_PORTABLE_TEST_ASSERT(context, ATHostDeviceIsPathWild(L"FILE*.TXT"));
	AT_PORTABLE_TEST_ASSERT(context, ATHostDeviceIsPathWild(L"FILE?.TXT"));
	AT_PORTABLE_TEST_ASSERT(context, !ATHostDeviceIsPathWild(L"FILE.TXT"));
	AT_PORTABLE_TEST_ASSERT(context, ATHostDeviceIsValidPathChar('A'));
	AT_PORTABLE_TEST_ASSERT(context, ATHostDeviceIsValidPathChar('9'));
	AT_PORTABLE_TEST_ASSERT(context, ATHostDeviceIsValidPathChar('_'));
	AT_PORTABLE_TEST_ASSERT(context, !ATHostDeviceIsValidPathChar('a'));
	AT_PORTABLE_TEST_ASSERT(context, !ATHostDeviceIsValidPathChar('-'));
	AT_PORTABLE_TEST_ASSERT(context, ATHostDeviceIsValidPathCharWide(L'Z'));
	AT_PORTABLE_TEST_ASSERT(context, !ATHostDeviceIsValidPathCharWide(L'z'));
	AT_PORTABLE_TEST_ASSERT(context, ATHostDeviceIsValidPathCharLFN(' '));
	AT_PORTABLE_TEST_ASSERT(context, ATHostDeviceIsValidPathCharWideLFN(L'{'));
	AT_PORTABLE_TEST_ASSERT(context, !ATHostDeviceIsValidPathCharLFN('|'));
	AT_PORTABLE_TEST_ASSERT(context, !ATHostDeviceIsValidPathCharWideLFN(L'\u017C'));

	VDStringA shortName;
	ATHostDeviceEncodeName(shortName, L"hello.txt", false, false);
	AT_PORTABLE_TEST_ASSERT(context, !strcmp(shortName.c_str(), "HELLO.TXT"));
	VDStringA hiddenName;
	ATHostDeviceEncodeName(hiddenName, L"!hello.txt", false, false);
	AT_PORTABLE_TEST_ASSERT(context, !strcmp(hiddenName.c_str(), "HELLO.TXT"));

	VDStringA truncated;
	ATHostDeviceEncodeName(truncated, L"longfilename.txt", false, false);
	AT_PORTABLE_TEST_ASSERT(context, !strcmp(truncated.c_str(), "LONGFILE.TXT"));
	VDStringA encoded;
	ATHostDeviceEncodeName(encoded, L"longfilename.txt", true, false);
	AT_PORTABLE_TEST_ASSERT(context, !strcmp(encoded.c_str(), "LONGF_KF.TXT"));

	VDStringA longName;
	ATHostDeviceEncodeName(longName, L"Read Me.txt", false, true);
	AT_PORTABLE_TEST_ASSERT(context, !strcmp(longName.c_str(), "Read Me.txt"));
	VDStringA filteredLongName;
	ATHostDeviceEncodeName(filteredLongName, L"a|b", false, true);
	AT_PORTABLE_TEST_ASSERT(context, !strcmp(filteredLongName.c_str(), "ab"));

	VDStringA unicodeName;
	ATHostDeviceEncodeName(unicodeName, L"Za\u017C\u00F3\u0142\u0107.txt", true, false);
	AT_PORTABLE_TEST_ASSERT(context, unicodeName.size() == 9);
	AT_PORTABLE_TEST_ASSERT(context, !strncmp(unicodeName.c_str(), "ZA_", 3));
	AT_PORTABLE_TEST_ASSERT(context, !strcmp(unicodeName.c_str() + 5, ".TXT"));

	VDStringA invalidQuestion;
	VDStringA invalidStar;
	ATHostDeviceEncodeName(invalidQuestion, L"name?.txt", true, false);
	ATHostDeviceEncodeName(invalidStar, L"name*.txt", true, false);
	AT_PORTABLE_TEST_ASSERT(context, invalidQuestion != invalidStar);
	return true;
}
