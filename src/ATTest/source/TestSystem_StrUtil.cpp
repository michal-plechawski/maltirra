// Altirra portable C string utility tests

#include <string.h>
#include <wchar.h>
#include <vd2/system/strutil.h>
#include <at/attest/portabletest.h>

bool ATTestSystemStrUtil(ATPortableTestContext& context) {
	char narrowCopy[7] = { 'L', '?', '?', '?', '?', '?', 'R' };
	AT_PORTABLE_TEST_ASSERT(context, vdstrlcpy(narrowCopy + 1, "abcdef", 5) == 6);
	AT_PORTABLE_TEST_ASSERT(context, !strcmp(narrowCopy + 1, "abcd"));
	AT_PORTABLE_TEST_ASSERT(context, narrowCopy[0] == 'L' && narrowCopy[6] == 'R');

	char oneCharacterBuffer[3] = { 'L', '?', 'R' };
	AT_PORTABLE_TEST_ASSERT(context, vdstrlcpy(oneCharacterBuffer + 1, "abc", 1) == 3);
	AT_PORTABLE_TEST_ASSERT(context, oneCharacterBuffer[1] == 0);
	AT_PORTABLE_TEST_ASSERT(context, oneCharacterBuffer[0] == 'L' && oneCharacterBuffer[2] == 'R');
	AT_PORTABLE_TEST_ASSERT(context, vdstrlcpy(nullptr, "abc", 0) == 3);

	wchar_t wideCopy[7] = { L'L', L'?', L'?', L'?', L'?', L'?', L'R' };
	AT_PORTABLE_TEST_ASSERT(context, vdwcslcpy(wideCopy + 1, L"abcdef", 5) == 6);
	AT_PORTABLE_TEST_ASSERT(context, !wcscmp(wideCopy + 1, L"abcd"));
	AT_PORTABLE_TEST_ASSERT(context, wideCopy[0] == L'L' && wideCopy[6] == L'R');
	AT_PORTABLE_TEST_ASSERT(context, vdwcslcpy(nullptr, L"abc", 0) == 3);

	char narrowZ[7] = { 'L', '?', '?', '?', '?', '?', 'R' };
	AT_PORTABLE_TEST_ASSERT(context, strncpyz(narrowZ + 1, "abcdef", 5) == narrowZ + 1);
	AT_PORTABLE_TEST_ASSERT(context, !strcmp(narrowZ + 1, "abcd"));
	AT_PORTABLE_TEST_ASSERT(context, narrowZ[0] == 'L' && narrowZ[6] == 'R');
	AT_PORTABLE_TEST_ASSERT(context, strncpyz(narrowZ + 1, "ignored", 0) == narrowZ + 1);
	AT_PORTABLE_TEST_ASSERT(context, narrowZ[0] == 'L' && narrowZ[6] == 'R');

	wchar_t wideZ[7] = { L'L', L'?', L'?', L'?', L'?', L'?', L'R' };
	AT_PORTABLE_TEST_ASSERT(context, wcsncpyz(wideZ + 1, L"abcdef", 5) == wideZ + 1);
	AT_PORTABLE_TEST_ASSERT(context, !wcscmp(wideZ + 1, L"abcd"));
	AT_PORTABLE_TEST_ASSERT(context, wideZ[0] == L'L' && wideZ[6] == L'R');
	AT_PORTABLE_TEST_ASSERT(context, wcsncpyz(wideZ + 1, L"ignored", 0) == wideZ + 1);

	char concatenated[7] = { 'L', 'a', 'b', 0, '?', '?', 'R' };
	AT_PORTABLE_TEST_ASSERT(context, vdstrlcat(concatenated + 1, "cdefg", 5) == 7);
	AT_PORTABLE_TEST_ASSERT(context, !strcmp(concatenated + 1, "abcd"));
	AT_PORTABLE_TEST_ASSERT(context, concatenated[0] == 'L' && concatenated[6] == 'R');

	char unterminated[5] = { 'L', 'a', 'b', 'R', '!' };
	AT_PORTABLE_TEST_ASSERT(context, vdstrlcat(unterminated + 1, "x", 2) == 3);
	AT_PORTABLE_TEST_ASSERT(context, unterminated[0] == 'L' && unterminated[1] == 'a');
	AT_PORTABLE_TEST_ASSERT(context, unterminated[2] == 'b' && unterminated[3] == 'R');
	AT_PORTABLE_TEST_ASSERT(context, vdstrlcat(nullptr, "abc", 0) == 3);

	const char spaced[] = " \t\r\nvalue";
	AT_PORTABLE_TEST_ASSERT(context, !strcmp(strskipspace(spaced), "value"));
	AT_PORTABLE_TEST_ASSERT(context, *strskipspace("value") == 'v');
	AT_PORTABLE_TEST_ASSERT(context, *strskipspace("") == 0);
	char mutableSpaced[] = "  mutable";
	AT_PORTABLE_TEST_ASSERT(context, strskipspace(mutableSpaced) == mutableSpaced + 2);

	AT_PORTABLE_TEST_ASSERT(context, vdstricmp("Altirra", "aLTIRRA") == 0);
	AT_PORTABLE_TEST_ASSERT(context, vdstricmp("abc", "abd") < 0);
	AT_PORTABLE_TEST_ASSERT(context, vdstricmp("Prefix-A", "prefix-B", 7) == 0);
	AT_PORTABLE_TEST_ASSERT(context, vdwcsicmp(L"Altirra", L"aLTIRRA") == 0);
	AT_PORTABLE_TEST_ASSERT(context, vdwcsnicmp(L"Prefix-A", L"prefix-B", 7) == 0);

	return true;
}
