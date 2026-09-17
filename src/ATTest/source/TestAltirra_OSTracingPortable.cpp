// Altirra portable OS tracing lifecycle tests

#include <at/attest/portabletest.h>
#include <ostracing.h>

bool ATTestAltirraOSTracing(ATPortableTestContext& context) {
	ATShutdownOSTracing();
	AT_PORTABLE_TEST_ASSERT(context, !ATIsOSTracingEnabled());

	// Region calls must be safe before initialization and when unmatched.
	ATOSTraceSimulateBegin();
	ATOSTraceSimulateEnd();
	ATOSTraceSimulateEnd();

	ATInitOSTracing();
	ATInitOSTracing();
	AT_PORTABLE_TEST_ASSERT(context, ATIsOSTracingEnabled());
	ATOSTraceSimulateBegin();
	ATOSTraceSimulateBegin();
	ATOSTraceSimulateEnd();

	ATShutdownOSTracing();
	ATShutdownOSTracing();
	AT_PORTABLE_TEST_ASSERT(context, !ATIsOSTracingEnabled());

	return true;
}
