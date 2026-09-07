// Altirra portable thread initialization hook tests

#include <string.h>
#include <vd2/system/tls.h>
#include <at/attest/portabletest.h>

namespace {
	int gHookCalls;
	bool gLastAttach;
	const char *gLastThreadName;

	void TestThreadHook(bool attach, const char *threadName) {
		++gHookCalls;
		gLastAttach = attach;
		gLastThreadName = threadName;
	}
}

bool ATTestSystemTLS(ATPortableTestContext& context) {
	gHookCalls = 0;
	gLastAttach = false;
	gLastThreadName = nullptr;

	VDSetThreadInitHook(TestThreadHook);
	VDInitThreadData("portable-test");

	AT_PORTABLE_TEST_ASSERT(context, gHookCalls == 1);
	AT_PORTABLE_TEST_ASSERT(context, gLastAttach);
	AT_PORTABLE_TEST_ASSERT(context, gLastThreadName != nullptr);
	AT_PORTABLE_TEST_ASSERT(context, !strcmp(gLastThreadName, "portable-test"));

	VDDeinitThreadData();
	AT_PORTABLE_TEST_ASSERT(context, gHookCalls == 2);
	AT_PORTABLE_TEST_ASSERT(context, !gLastAttach);
	AT_PORTABLE_TEST_ASSERT(context, gLastThreadName == nullptr);

	VDSetThreadInitHook(nullptr);
	VDInitThreadData("ignored");
	VDDeinitThreadData();
	AT_PORTABLE_TEST_ASSERT(context, gHookCalls == 2);

	return true;
}
