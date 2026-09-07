// Altirra portable system exception tests

#include <array>
#include <cstring>
#include <cwchar>
#include <utility>

#include <at/attest/portabletest.h>
#include <vd2/system/Error.h>

bool ATTestSystemError(ATPortableTestContext& context) {
	VDException empty;
	AT_PORTABLE_TEST_ASSERT(context, empty.empty());
	AT_PORTABLE_TEST_ASSERT(context, !empty.visible());
	AT_PORTABLE_TEST_ASSERT(context, !*empty.c_str());
	AT_PORTABLE_TEST_ASSERT(context, !*empty.wc_str());
	empty.set_hidden();
	AT_PORTABLE_TEST_ASSERT(context, !empty.visible());

	VDException narrow("portable error");
	AT_PORTABLE_TEST_ASSERT(context, !narrow.empty());
	AT_PORTABLE_TEST_ASSERT(context, narrow.visible());
	AT_PORTABLE_TEST_ASSERT(context, !strcmp(narrow.c_str(), "portable error"));
	AT_PORTABLE_TEST_ASSERT(context, !wcscmp(narrow.wc_str(), L"portable error"));
	AT_PORTABLE_TEST_ASSERT(context, narrow.what() == narrow.c_str());

	VDException wide(L"wide \u03A9");
	AT_PORTABLE_TEST_ASSERT(context, !wcscmp(wide.wc_str(), L"wide \u03A9"));
	AT_PORTABLE_TEST_ASSERT(context, !strcmp(wide.c_str(), "wide ?"));

	VDException copy(narrow);
	narrow.clear();
	AT_PORTABLE_TEST_ASSERT(context, narrow.empty());
	AT_PORTABLE_TEST_ASSERT(context, !strcmp(copy.c_str(), "portable error"));

	VDException assigned;
	assigned = copy;
	AT_PORTABLE_TEST_ASSERT(context, !strcmp(assigned.c_str(), "portable error"));

	VDException moved(std::move(assigned));
	AT_PORTABLE_TEST_ASSERT(context, assigned.empty());
	AT_PORTABLE_TEST_ASSERT(context, !strcmp(moved.c_str(), "portable error"));

	VDException moveAssigned;
	moveAssigned = std::move(moved);
	AT_PORTABLE_TEST_ASSERT(context, moved.empty());
	AT_PORTABLE_TEST_ASSERT(context, !strcmp(moveAssigned.c_str(), "portable error"));

	VDException hidden("hidden");
	VDException hiddenCopy(hidden);
	hiddenCopy.set_hidden();
	AT_PORTABLE_TEST_ASSERT(context, !hidden.visible());
	AT_PORTABLE_TEST_ASSERT(context, !hiddenCopy.visible());

	VDException formatted("value=%d, text=%s", 17, "ok");
	AT_PORTABLE_TEST_ASSERT(context, !strcmp(formatted.c_str(), "value=17, text=ok"));
	AT_PORTABLE_TEST_ASSERT(context, !wcscmp(formatted.wc_str(), L"value=17, text=ok"));

	std::array<char, 600> longText {};
	longText.fill('x');
	longText.back() = 0;
	formatted.setf("long:%s", longText.data());
	AT_PORTABLE_TEST_ASSERT(context, strlen(formatted.c_str()) == 604);
	AT_PORTABLE_TEST_ASSERT(context, !memcmp(formatted.c_str(), "long:", 5));
	AT_PORTABLE_TEST_ASSERT(context, formatted.c_str()[603] == 'x');

	std::array<wchar_t, 600> longWideText {};
	longWideText.fill(L'y');
	longWideText.back() = 0;
	formatted.wsetf(L"wide:%ls", longWideText.data());
	AT_PORTABLE_TEST_ASSERT(context, wcslen(formatted.wc_str()) == 604);
	AT_PORTABLE_TEST_ASSERT(context, !wmemcmp(formatted.wc_str(), L"wide:", 5));
	AT_PORTABLE_TEST_ASSERT(context, formatted.wc_str()[603] == L'y');

	VDException invalidWideConversion("%ls", L"\uFFFE");
	AT_PORTABLE_TEST_ASSERT(context, !strcmp(invalidWideConversion.c_str(), "<%ls>"));
	AT_PORTABLE_TEST_ASSERT(context, !wcscmp(invalidWideConversion.wc_str(), L"<%ls>"));

	formatted.setf("");
	AT_PORTABLE_TEST_ASSERT(context, !formatted.empty());
	AT_PORTABLE_TEST_ASSERT(context, !formatted.visible());
	AT_PORTABLE_TEST_ASSERT(context, !*formatted.c_str());

	VDAllocationFailedException allocationError(12345);
	AT_PORTABLE_TEST_ASSERT(context, strstr(allocationError.c_str(), "12345") != nullptr);
	AT_PORTABLE_TEST_ASSERT(context, allocationError.visible());

	VDAllocationFailedException genericAllocationError;
	AT_PORTABLE_TEST_ASSERT(context,
		!strcmp(genericAllocationError.c_str(), "Out of memory"));

	VDUserCancelException cancelled;
	AT_PORTABLE_TEST_ASSERT(context, !cancelled.empty());
	AT_PORTABLE_TEST_ASSERT(context, !cancelled.visible());
	AT_PORTABLE_TEST_ASSERT(context,
		!strcmp(cancelled.c_str(), "Operation cancelled by user"));

	return true;
}
