// Altirra portable string hash adapter tests

#include <at/attest/portabletest.h>
#include <vd2/system/hash.h>
#include <vd2/system/VDString.h>
#include <vd2/system/vdstl_hash.h>

bool ATTestSystemVDSTLHash(ATPortableTestContext& context) {
	const VDStringA narrow("Altirra");
	const VDStringA narrowSame("Altirra");
	const VDStringA narrowCase("aLTIRRA");
	const VDStringA narrowDifferent("Atari");
	const VDStringSpanA narrowSpan(narrow.data(), narrow.end());

	const vdhash<VDStringA> narrowHash;
	const size_t expectedNarrowHash = VDHashString32("Altirra");
	AT_PORTABLE_TEST_ASSERT(context, narrowHash(narrow) == expectedNarrowHash);
	AT_PORTABLE_TEST_ASSERT(context, narrowHash(narrowSpan) == expectedNarrowHash);
	AT_PORTABLE_TEST_ASSERT(context, narrowHash("Altirra") == expectedNarrowHash);

	const VDStringW wide(L"Altirra");
	const VDStringW wideSame(L"Altirra");
	const VDStringW wideCase(L"aLTIRRA");
	const VDStringW wideDifferent(L"Atari");
	const VDStringSpanW wideSpan(wide.data(), wide.end());

	const vdhash<VDStringW> wideHash;
	const size_t expectedWideHash = VDHashString32(L"Altirra");
	AT_PORTABLE_TEST_ASSERT(context, wideHash(wide) == expectedWideHash);
	AT_PORTABLE_TEST_ASSERT(context, wideHash(wideSpan) == expectedWideHash);
	AT_PORTABLE_TEST_ASSERT(context, wideHash(L"Altirra") == expectedWideHash);
	AT_PORTABLE_TEST_ASSERT(context, expectedWideHash == expectedNarrowHash);

	const vdstringhashi insensitiveHash;
	AT_PORTABLE_TEST_ASSERT(context,
		insensitiveHash(narrow) == insensitiveHash(narrowCase));
	AT_PORTABLE_TEST_ASSERT(context,
		insensitiveHash("Altirra") == insensitiveHash("aLTIRRA"));
	AT_PORTABLE_TEST_ASSERT(context,
		insensitiveHash(wide) == insensitiveHash(wideCase));
	AT_PORTABLE_TEST_ASSERT(context,
		insensitiveHash(L"Altirra") == insensitiveHash(L"aLTIRRA"));

	const vdstringpred exact;
	AT_PORTABLE_TEST_ASSERT(context, exact("Altirra", "Altirra"));
	AT_PORTABLE_TEST_ASSERT(context, !exact("Altirra", "aLTIRRA"));
	AT_PORTABLE_TEST_ASSERT(context, exact(narrow, narrowSame));
	AT_PORTABLE_TEST_ASSERT(context, exact(narrow, narrowSpan));
	AT_PORTABLE_TEST_ASSERT(context, exact(narrow, "Altirra"));
	AT_PORTABLE_TEST_ASSERT(context, !exact(narrow, narrowDifferent));
	AT_PORTABLE_TEST_ASSERT(context, exact(L"Altirra", L"Altirra"));
	AT_PORTABLE_TEST_ASSERT(context, !exact(L"Altirra", L"aLTIRRA"));
	AT_PORTABLE_TEST_ASSERT(context, exact(wide, wideSame));
	AT_PORTABLE_TEST_ASSERT(context, exact(wide, wideSpan));
	AT_PORTABLE_TEST_ASSERT(context, exact(wide, L"Altirra"));
	AT_PORTABLE_TEST_ASSERT(context, !exact(wide, wideDifferent));

	const vdstringpredi insensitive;
	AT_PORTABLE_TEST_ASSERT(context, insensitive("Altirra", "aLTIRRA"));
	AT_PORTABLE_TEST_ASSERT(context, !insensitive("Altirra", "Atari"));
	AT_PORTABLE_TEST_ASSERT(context, insensitive(narrow, narrowCase));
	AT_PORTABLE_TEST_ASSERT(context, insensitive(narrow, narrowSpan));
	AT_PORTABLE_TEST_ASSERT(context, insensitive(narrow, "aLTIRRA"));
	AT_PORTABLE_TEST_ASSERT(context, !insensitive(narrow, narrowDifferent));
	AT_PORTABLE_TEST_ASSERT(context, insensitive(L"Altirra", L"aLTIRRA"));
	AT_PORTABLE_TEST_ASSERT(context, !insensitive(L"Altirra", L"Atari"));
	AT_PORTABLE_TEST_ASSERT(context, insensitive(wide, wideCase));
	AT_PORTABLE_TEST_ASSERT(context, insensitive(wide, wideSpan));
	AT_PORTABLE_TEST_ASSERT(context, insensitive(wide, L"aLTIRRA"));
	AT_PORTABLE_TEST_ASSERT(context, !insensitive(wide, wideDifferent));

	return true;
}
