// Altirra portable dynamic string tests

#include <array>
#include <cstdarg>
#include <cstring>
#include <cwchar>
#include <utility>

#include <at/attest/portabletest.h>
#include <vd2/system/VDString.h>

namespace {
	int AppendNarrowAndReadFirstArgument(VDStringA& target, const char *format, ...) {
		va_list arguments;
		va_start(arguments, format);
		target.append_vsprintf(format, arguments);
		const int firstArgument = va_arg(arguments, int);
		va_end(arguments);
		return firstArgument;
	}

	int AppendWideAndReadFirstArgument(VDStringW& target, const wchar_t *format, ...) {
		va_list arguments;
		va_start(arguments, format);
		target.append_vsprintf(format, arguments);
		const int firstArgument = va_arg(arguments, int);
		va_end(arguments);
		return firstArgument;
	}
}

bool ATTestSystemVDString(ATPortableTestContext& context) {
	VDStringA narrow;
	AT_PORTABLE_TEST_ASSERT(context, narrow.empty());
	AT_PORTABLE_TEST_ASSERT(context, !strcmp(narrow.c_str(), ""));

	narrow.push_back('a');
	narrow += 'b';
	narrow.append(3, 'c');
	AT_PORTABLE_TEST_ASSERT(context, narrow == "abccc");

	narrow.resize(8, 'x');
	AT_PORTABLE_TEST_ASSERT(context, narrow.size() == 8);
	AT_PORTABLE_TEST_ASSERT(context, narrow == "abcccxxx");
	narrow.resize(2);
	AT_PORTABLE_TEST_ASSERT(context, narrow == "ab");

	narrow.reserve(64);
	AT_PORTABLE_TEST_ASSERT(context, narrow.capacity() >= 64);
	AT_PORTABLE_TEST_ASSERT(context, narrow == "ab");

	VDStringA narrowCopy(narrow);
	VDStringA narrowMoved(std::move(narrowCopy));
	AT_PORTABLE_TEST_ASSERT(context, narrowMoved == "ab");
	AT_PORTABLE_TEST_ASSERT(context, narrowCopy.empty());

	VDStringA formattedNarrow;
	formattedNarrow.sprintf("value=%d/%s", 42, "ok");
	AT_PORTABLE_TEST_ASSERT(context, formattedNarrow == "value=42/ok");
	formattedNarrow.append_sprintf("/%X", 255);
	AT_PORTABLE_TEST_ASSERT(context, formattedNarrow == "value=42/ok/FF");

	VDStringA directNarrow("direct:");
	AT_PORTABLE_TEST_ASSERT(context,
		AppendNarrowAndReadFirstArgument(directNarrow, "%d", 73) == 73);
	AT_PORTABLE_TEST_ASSERT(context, directNarrow == "direct:73");

	std::array<char, 5001> longNarrow {};
	longNarrow.fill('n');
	longNarrow.back() = 0;
	formattedNarrow.sprintf("%s:%d", longNarrow.data(), 17);
	AT_PORTABLE_TEST_ASSERT(context, formattedNarrow.size() == 5003);
	AT_PORTABLE_TEST_ASSERT(context,
		!memcmp(formattedNarrow.data(), longNarrow.data(), 5000));
	AT_PORTABLE_TEST_ASSERT(context,
		!strcmp(formattedNarrow.c_str() + 5000, ":17"));

	VDStringW wide;
	AT_PORTABLE_TEST_ASSERT(context, wide.empty());
	AT_PORTABLE_TEST_ASSERT(context, !wcscmp(wide.c_str(), L""));

	wide.push_back(L'a');
	wide += L'b';
	wide.append(3, L'c');
	AT_PORTABLE_TEST_ASSERT(context, wide == L"abccc");
	wide.resize(8, L'x');
	AT_PORTABLE_TEST_ASSERT(context, wide == L"abcccxxx");
	wide.resize(2);
	wide.reserve(64);
	AT_PORTABLE_TEST_ASSERT(context, wide.capacity() >= 64);
	AT_PORTABLE_TEST_ASSERT(context, wide == L"ab");

	VDStringW formattedWide;
	formattedWide.sprintf(L"value=%d/%ls", 42, L"ok");
	AT_PORTABLE_TEST_ASSERT(context, formattedWide == L"value=42/ok");
	formattedWide.append_sprintf(L"/%X", 255);
	AT_PORTABLE_TEST_ASSERT(context, formattedWide == L"value=42/ok/FF");

	VDStringW directWide(L"direct:");
	AT_PORTABLE_TEST_ASSERT(context,
		AppendWideAndReadFirstArgument(directWide, L"%d", 91) == 91);
	AT_PORTABLE_TEST_ASSERT(context, directWide == L"direct:91");

	std::array<wchar_t, 3001> longWide {};
	longWide.fill(L'w');
	longWide.back() = 0;
	formattedWide.sprintf(L"%ls:%d", longWide.data(), 23);
	AT_PORTABLE_TEST_ASSERT(context, formattedWide.size() == 3003);
	AT_PORTABLE_TEST_ASSERT(context,
		!wmemcmp(formattedWide.data(), longWide.data(), 3000));
	AT_PORTABLE_TEST_ASSERT(context,
		!wcscmp(formattedWide.c_str() + 3000, L":23"));

	return true;
}
