// Altirra portable UI accessor tests

#include <at/attest/portabletest.h>
#include <uiaccessors.h>

bool ATTestAltirraUIAccessors(ATPortableTestContext& context) {
	struct DispatcherSentinel {} first, second;

	auto *firstDispatcher = reinterpret_cast<IATAsyncDispatcher *>(&first);
	auto *secondDispatcher = reinterpret_cast<IATAsyncDispatcher *>(&second);

	ATUISetDispatcher(nullptr);
	AT_PORTABLE_TEST_ASSERT(context, ATUIGetDispatcher() == nullptr);

	ATUISetDispatcher(firstDispatcher);
	AT_PORTABLE_TEST_ASSERT(context, ATUIGetDispatcher() == firstDispatcher);

	ATUISetDispatcher(secondDispatcher);
	AT_PORTABLE_TEST_ASSERT(context, ATUIGetDispatcher() == secondDispatcher);

	ATUISetDispatcher(nullptr);
	AT_PORTABLE_TEST_ASSERT(context, ATUIGetDispatcher() == nullptr);

	return true;
}
