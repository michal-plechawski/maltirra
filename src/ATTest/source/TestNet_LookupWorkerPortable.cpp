// Altirra portable asynchronous network lookup worker tests

#include <at/attest/portabletest.h>
#include <at/atnetworksockets/internal/lookupworker.h>
#include <vd2/system/thread.h>

namespace {
#ifdef _WIN32
	class ATNativeSocketSystemScope {
	public:
		ATNativeSocketSystemScope() {
			mbInitialized = ATSocketInit();
		}

		~ATNativeSocketSystemScope() {
			if (mbInitialized)
				ATSocketShutdown();
		}

		bool mbInitialized = false;
	};
#endif
}

bool ATTestNetLookupWorker(ATPortableTestContext& context) {
#ifdef _WIN32
	ATNativeSocketSystemScope socketSystem;
	AT_PORTABLE_TEST_ASSERT(context, socketSystem.mbInitialized);
#endif

	ATNetLookupWorker worker;
	AT_PORTABLE_TEST_ASSERT(context, worker.Init());

	const vdrefptr<IATNetLookupResult> result = worker.Lookup(L"localhost", L"6502");
	AT_PORTABLE_TEST_ASSERT(context, result != nullptr);

	for(int attempt = 0; attempt < 200 && !result->Completed(); ++attempt)
		VDThreadSleep(5);

	AT_PORTABLE_TEST_ASSERT(context, result->Completed());
	AT_PORTABLE_TEST_ASSERT(context, result->Succeeded());
	AT_PORTABLE_TEST_ASSERT(context, result->Address().mPort == 6502);
	AT_PORTABLE_TEST_ASSERT(context,
		result->Address().mType == ATSocketAddressType::IPv4
		|| result->Address().mType == ATSocketAddressType::IPv6);

	return true;
}
