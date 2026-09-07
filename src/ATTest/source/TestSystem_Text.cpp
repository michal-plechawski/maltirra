// Altirra portable text conversion and formatting tests

#include <climits>
#include <cstdarg>
#include <cstring>

#include <at/attest/portabletest.h>
#include <vd2/system/text.h>
#include <vd2/system/VDString.h>

namespace {
	VDStringW VDTestFormatVa(const wchar_t *format, int argumentCount, ...) {
		va_list arguments;
		va_start(arguments, argumentCount);
		VDStringW result = VDvswprintf(format, argumentCount, arguments);
		va_end(arguments);
		return result;
	}
}

bool ATTestSystemText(ATPortableTestContext& context) {
	char narrowBuffer[32] {};
	wchar_t wideBuffer[32] {};

	AT_PORTABLE_TEST_ASSERT(context, VDTextWToALength(L"Altirra") == 7);
	AT_PORTABLE_TEST_ASSERT(context, VDTextWToALength(L"Altirra!", 7) == 7);
	AT_PORTABLE_TEST_ASSERT(context,
		VDTextWToA(narrowBuffer, 32, L"Altirra") == 7);
	AT_PORTABLE_TEST_ASSERT(context, !strcmp(narrowBuffer, "Altirra"));
	AT_PORTABLE_TEST_ASSERT(context, VDTextAToWLength("Altirra") == 7);
	AT_PORTABLE_TEST_ASSERT(context, VDTextAToWLength("Altirra!", 7) == 7);
	AT_PORTABLE_TEST_ASSERT(context,
		VDTextAToW(wideBuffer, 32, "Altirra") == 7);
	AT_PORTABLE_TEST_ASSERT(context, !wcscmp(wideBuffer, L"Altirra"));

	narrowBuffer[0] = 'x';
	AT_PORTABLE_TEST_ASSERT(context,
		VDTextWToA(narrowBuffer, 4, L"Altirra") == 0);
	AT_PORTABLE_TEST_ASSERT(context, narrowBuffer[0] == 0);
	wideBuffer[0] = L'x';
	AT_PORTABLE_TEST_ASSERT(context,
		VDTextAToW(wideBuffer, 4, "Altirra") == 0);
	AT_PORTABLE_TEST_ASSERT(context, wideBuffer[0] == 0);

	const VDStringA narrowString("plain ASCII");
	const VDStringW wideString = VDTextAToW(narrowString);
	AT_PORTABLE_TEST_ASSERT(context, wideString == L"plain ASCII");
	AT_PORTABLE_TEST_ASSERT(context, VDTextWToA(wideString) == "plain ASCII");
	AT_PORTABLE_TEST_ASSERT(context, VDTextAToW(nullptr).empty());
	AT_PORTABLE_TEST_ASSERT(context, VDTextWToA(nullptr).empty());

	const char utf8Text[] = {
		'A', (char)0xC3, (char)0xA9,
		(char)0xF0, (char)0x9F, (char)0x98, (char)0x80
	};
	const VDStringW decodedUTF8 = VDTextU8ToW(utf8Text, sizeof utf8Text);
	VDStringW expectedWide;
	expectedWide.push_back(L'A');
	expectedWide.push_back((wchar_t)0xE9);
#if WCHAR_MAX <= 0xFFFF
	expectedWide.push_back((wchar_t)0xD83D);
	expectedWide.push_back((wchar_t)0xDE00);
#else
	expectedWide.push_back((wchar_t)0x1F600);
#endif
	AT_PORTABLE_TEST_ASSERT(context, decodedUTF8 == expectedWide);
	const VDStringA encodedUTF8 = VDTextWToU8(decodedUTF8);
	AT_PORTABLE_TEST_ASSERT(context, encodedUTF8.size() == sizeof utf8Text);
	AT_PORTABLE_TEST_ASSERT(context,
		!memcmp(encodedUTF8.data(), utf8Text, sizeof utf8Text));
#if !defined(_WIN32)
	AT_PORTABLE_TEST_ASSERT(context, VDTextWToA(decodedUTF8) == encodedUTF8);
	AT_PORTABLE_TEST_ASSERT(context, VDTextAToW(encodedUTF8) == decodedUTF8);
#endif

	wchar_t emojiSource[2] {};
	size_t emojiSourceLength;
#if WCHAR_MAX <= 0xFFFF
	emojiSource[0] = (wchar_t)0xD83D;
	emojiSource[1] = (wchar_t)0xDE00;
	emojiSourceLength = 2;
#else
	emojiSource[0] = (wchar_t)0x1F600;
	emojiSourceLength = 1;
#endif
	uint8 encodedCodePoint[4] {};
	size_t sourceElementsUsed = 99;
	AT_PORTABLE_TEST_ASSERT(context,
		VDCodePointToU8(encodedCodePoint, 4, emojiSource,
			emojiSourceLength, sourceElementsUsed) == 4);
	AT_PORTABLE_TEST_ASSERT(context, sourceElementsUsed == emojiSourceLength);
	AT_PORTABLE_TEST_ASSERT(context,
		encodedCodePoint[0] == 0xF0 && encodedCodePoint[1] == 0x9F
		&& encodedCodePoint[2] == 0x98 && encodedCodePoint[3] == 0x80);
	sourceElementsUsed = 99;
	AT_PORTABLE_TEST_ASSERT(context,
		VDCodePointToU8(encodedCodePoint, 3, emojiSource,
			emojiSourceLength, sourceElementsUsed) == 0);
	AT_PORTABLE_TEST_ASSERT(context, sourceElementsUsed == 0);

	AT_PORTABLE_TEST_ASSERT(context,
		VDTextContainsSubstringMatchByLocale(
			VDStringSpanW(L"Alpha Beta"), VDStringSpanW(L"beta")));
	AT_PORTABLE_TEST_ASSERT(context,
		!VDTextContainsSubstringMatchByLocale(
			VDStringSpanW(L"Alpha Beta"), VDStringSpanW(L"gamma")));

	int signedValue = -42;
	unsigned unsignedValue = 42;
	const wchar_t *wideWord = L"word";
	const char *narrowWord = "ascii";
	double floatingValue = 3.5;
	const void *formatArguments[] = {
		&signedValue, &unsignedValue, &wideWord, &narrowWord, &floatingValue
	};
	const VDStringW formatted = VDaswprintf(
		L"%+d %#x %ls %hs %.2f", 5, formatArguments);
	AT_PORTABLE_TEST_ASSERT(context,
		formatted == L"-42 0x2a word ascii 3.50");

	int seven = 7;
	const wchar_t *indexedWord = L"item";
	const void *indexedArguments[] = { &seven, &indexedWord };
	AT_PORTABLE_TEST_ASSERT(context,
		VDaswprintf(L"%[1]s:%[0]03d", 2, indexedArguments) == L"item:007");

	int outputCount = -1;
	const void *countArguments[] = { &indexedWord, &outputCount };
	AT_PORTABLE_TEST_ASSERT(context,
		VDaswprintf(L"%s%n!", 2, countArguments) == L"item!");
	AT_PORTABLE_TEST_ASSERT(context, outputCount == 4);

	int width = 6;
	const void *widthArguments[] = { &width, &unsignedValue };
	AT_PORTABLE_TEST_ASSERT(context,
		VDaswprintf(L"%*u", 2, widthArguments) == L"    42");

	int64 byteCount = VD64(10) << 20;
	const void *sizeArguments[] = { &byteCount };
	AT_PORTABLE_TEST_ASSERT(context,
		VDaswprintf(L"%llzs", 1, sizeArguments) == L"10 MB");
	AT_PORTABLE_TEST_ASSERT(context, VDaswprintf(L"%%", 0, nullptr) == L"%");

	int nine = 9;
	const wchar_t *valueWord = L"value";
	AT_PORTABLE_TEST_ASSERT(context,
		VDswprintf(L"%s=%d", 2, &valueWord, &nine) == L"value=9");
	AT_PORTABLE_TEST_ASSERT(context,
		VDTestFormatVa(L"%s=%d", 2, &valueWord, &nine) == L"value=9");
	AT_PORTABLE_TEST_ASSERT(context,
		VDswprintf(L"%[16]d", 17,
			&nine, &nine, &nine, &nine, &nine, &nine, &nine, &nine, &nine,
			&nine, &nine, &nine, &nine, &nine, &nine, &nine, &nine) == L"9");

	return true;
}
