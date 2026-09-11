// Altirra portable test runner for macOS ARM64

#include <stdio.h>
#include <at/attest/portabletest.h>
#include <vd2/system/cpuaccel.h>

int main() {
	CPUEnableExtensions(CPUCheckForExtensions());

	size_t testCount = 0;
	const ATPortableTestCase *tests = ATGetPortableTests(testCount);
	int failures = 0;

	for(size_t i = 0; i < testCount; ++i) {
		ATPortableTestContext context;
		printf("Running portable test: %s\n", tests[i].mpName);

		if (!tests[i].mpTestFn(context)) {
			fprintf(stderr, "FAILED: %s at %s:%d: %s\n",
				tests[i].mpName,
				context.mpFile ? context.mpFile : "<unknown>",
				context.mLine,
				context.mpExpression ? context.mpExpression : "<no expression>");
			++failures;
		}
	}

	printf("Portable tests complete. Tests: %zu, failures: %d\n", testCount, failures);
	VDCPUCleanupExtensions();
	return failures ? 1 : 0;
}
