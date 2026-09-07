// Altirra portable test interface

#ifndef f_AT_ATTEST_PORTABLETEST_H
#define f_AT_ATTEST_PORTABLETEST_H

#include <stddef.h>

struct ATPortableTestContext {
	const char *mpFile = nullptr;
	const char *mpExpression = nullptr;
	int mLine = 0;
};

using ATPortableTestFn = bool (*)(ATPortableTestContext& context);

struct ATPortableTestCase {
	const char *mpName;
	ATPortableTestFn mpTestFn;
};

const ATPortableTestCase *ATGetPortableTests(size_t& count);

#define AT_PORTABLE_TEST_ASSERT(context, expression) \
	do { \
		if (!(expression)) { \
			(context).mpFile = __FILE__; \
			(context).mpExpression = #expression; \
			(context).mLine = __LINE__; \
			return false; \
		} \
	} while(false)

#endif
