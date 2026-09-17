// Altirra portable UI file filter tests

#include <string_view>

#include <at/attest/portabletest.h>
#include <uifilefilters.h>

namespace {
	bool ValidateFilter(
		const wchar_t *filter,
		size_t expectedCount,
		std::wstring_view requiredPattern)
	{
		const wchar_t *cursor = filter;
		ATUIFileFilterEntry entry;
		size_t count = 0;
		bool foundRequiredPattern = false;

		while (ATUIGetNextFileFilter(cursor, entry)) {
			if (entry.mDescription.empty() || entry.mPatterns.empty())
				return false;

			foundRequiredPattern |= entry.mPatterns.find(requiredPattern) != std::wstring_view::npos;

			size_t start = 0;
			while (start < entry.mPatterns.size()) {
				const size_t end = entry.mPatterns.find(L';', start);
				const std::wstring_view pattern = entry.mPatterns.substr(
					start,
					end == std::wstring_view::npos ? end : end - start);

				if (!pattern.starts_with(L"*."))
					return false;

				if (end == std::wstring_view::npos)
					break;

				start = end + 1;
			}

			if (++count > 32)
				return false;
		}

		return !*cursor && count == expectedCount && foundRequiredPattern;
	}
}

bool ATTestAltirraUIFileFilters(ATPortableTestContext& context) {
	AT_PORTABLE_TEST_ASSERT(context, ValidateFilter(g_ATUIFileFilter_Disk, 6, L"*.atr"));
	AT_PORTABLE_TEST_ASSERT(context, ValidateFilter(g_ATUIFileFilter_DiskWithArchives, 8, L"*.zip"));
	AT_PORTABLE_TEST_ASSERT(context, ValidateFilter(g_ATUIFileFilter_Cheats, 4, L"*.atcheats"));
	AT_PORTABLE_TEST_ASSERT(context, ValidateFilter(g_ATUIFileFilter_LoadState, 3, L"*.atstate2"));
	AT_PORTABLE_TEST_ASSERT(context, ValidateFilter(g_ATUIFileFilter_SaveState, 1, L"*.atstate2"));
	AT_PORTABLE_TEST_ASSERT(context, ValidateFilter(g_ATUIFileFilter_LoadCartridge, 4, L"*.car"));
	AT_PORTABLE_TEST_ASSERT(context, ValidateFilter(g_ATUIFileFilter_LoadTape, 4, L"*.cas"));
	AT_PORTABLE_TEST_ASSERT(context, ValidateFilter(g_ATUIFileFilter_LoadTapeAudio, 3, L"*.flac"));
	AT_PORTABLE_TEST_ASSERT(context, ValidateFilter(g_ATUIFileFilter_SaveTape, 1, L"*.cas"));
	AT_PORTABLE_TEST_ASSERT(context, ValidateFilter(g_ATUIFileFilter_SaveTapeAudio, 1, L"*.wav"));
	AT_PORTABLE_TEST_ASSERT(context, ValidateFilter(g_ATUIFileFilter_SaveTapeAnalysis, 1, L"*.wav"));
	AT_PORTABLE_TEST_ASSERT(context, ValidateFilter(g_ATUIFileFilter_LoadSAP, 1, L"*.sap"));
	AT_PORTABLE_TEST_ASSERT(context, ValidateFilter(g_ATUIFileFilter_SaveXEX, 1, L"*.xex"));
	AT_PORTABLE_TEST_ASSERT(context, ValidateFilter(g_ATUIFileFilter_LoadCompatEngine, 1, L"*.atcpengine"));
	AT_PORTABLE_TEST_ASSERT(context, ValidateFilter(g_ATUIFileFilter_SaveCompatEngine, 2, L"*.atcpengine"));
	AT_PORTABLE_TEST_ASSERT(context, ValidateFilter(g_ATUIFileFilter_LoadCompatImageFile, 8, L"*.a52"));

	const wchar_t *nullCursor = nullptr;
	ATUIFileFilterEntry entry { L"stale", L"stale" };
	AT_PORTABLE_TEST_ASSERT(context, !ATUIGetNextFileFilter(nullCursor, entry));
	AT_PORTABLE_TEST_ASSERT(context, entry.mDescription.empty());
	AT_PORTABLE_TEST_ASSERT(context, entry.mPatterns.empty());

	return true;
}
