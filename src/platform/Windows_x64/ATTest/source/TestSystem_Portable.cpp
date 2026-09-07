// Adapter for running the shared portable tests in the Windows test harness.

#include <stdafx.h>
#include <at/attest/portabletest.h>
#include <test.h>

DEFINE_TEST(System_Portable) {
	size_t testCount = 0;
	const ATPortableTestCase *tests = ATGetPortableTests(testCount);

	for(size_t i = 0; i < testCount; ++i) {
		ATPortableTestContext context;
		const bool succeeded = tests[i].mpTestFn(context);

		TEST_ASSERTF(succeeded,
			"Portable test %s failed at %s:%d: %s",
			tests[i].mpName,
			context.mpFile ? context.mpFile : "<unknown>",
			context.mLine,
			context.mpExpression ? context.mpExpression : "<no expression>");
	}

	return 0;
}
