// Altirra portable startup logger lifecycle tests

#include <at/attest/portabletest.h>
#include <startuplogger.h>

bool ATTestAltirraStartupLogger(ATPortableTestContext& context) {
	ATStartupLogShutdown();
	AT_PORTABLE_TEST_ASSERT(context, !ATStartupLogIsInited());
	ATStartupLog("ignored before initialization");

	ATStartupLogInit(L"time");
	ATStartupLogInit(nullptr);
	AT_PORTABLE_TEST_ASSERT(context, ATStartupLogIsInited());
	ATStartupLog("portable startup logger test");

	const VDStringA spanMessage("portable span overload test");
	ATStartupLog(VDStringSpanA(spanMessage));

	ATStartupLogShutdown();
	ATStartupLogShutdown();
	AT_PORTABLE_TEST_ASSERT(context, !ATStartupLogIsInited());

	ATStartupLogInit(nullptr);
	AT_PORTABLE_TEST_ASSERT(context, ATStartupLogIsInited());
	ATStartupLogShutdown();

	return true;
}
