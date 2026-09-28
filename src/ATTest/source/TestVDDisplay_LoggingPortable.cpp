// Portable VDDisplay logging tests.

#include <string>

#include <at/attest/portabletest.h>
#include <vd2/VDDisplay/logging.h>

namespace {
	struct LogHookReset {
		~LogHookReset() {
			VDDispSetLogHook({});
		}
	};
}

bool ATTestVDDisplayLogging(ATPortableTestContext& context) {
	LogHookReset reset;
	std::string captured;
	int callCount = 0;
	VDDispSetLogHook([&](const char *message) {
		captured = message;
		++callCount;
	});

	VDDispLog("plain message");
	AT_PORTABLE_TEST_ASSERT(context, captured == "plain message");
	AT_PORTABLE_TEST_ASSERT(context, callCount == 1);

	VDDispLogF("%s %d", "frame", 42);
	AT_PORTABLE_TEST_ASSERT(context, captured == "frame 42");
	AT_PORTABLE_TEST_ASSERT(context, callCount == 2);

	const std::string longMessage(100, 'x');
	VDDispLogF("[%s]", longMessage.c_str());
	AT_PORTABLE_TEST_ASSERT(context, captured == "[" + longMessage + "]");
	AT_PORTABLE_TEST_ASSERT(context, callCount == 3);

	const std::string oversizedMessage(40000, 'y');
	VDDispLogF("%s", oversizedMessage.c_str());
	AT_PORTABLE_TEST_ASSERT(context, captured.size() == 32768);
	AT_PORTABLE_TEST_ASSERT(context, captured == oversizedMessage.substr(0, 32768));
	AT_PORTABLE_TEST_ASSERT(context, callCount == 4);

	VDDispLogF("");
	AT_PORTABLE_TEST_ASSERT(context, callCount == 4);
	return true;
}
