// Altirra portable callback adapter tests

#include <at/attest/portabletest.h>
#include <callback.h>

namespace {
	struct CallbackTarget {
		int mValue = 10;

		int Call0() {
			return ++mValue;
		}

		int Call1(int value) {
			mValue += value;
			return mValue;
		}

		int Call2(int x, int y) {
			mValue += x * y;
			return mValue;
		}

		void Set(int value) {
			mValue = value;
		}
	};

	int FreeCall0(void *data) {
		return ++*static_cast<int *>(data);
	}

	int FreeCall1(void *data, int value) {
		return *static_cast<int *>(data) += value;
	}

	int FreeCall2(void *data, int x, int y) {
		return *static_cast<int *>(data) += x * y;
	}
}

bool ATTestAltirraCallback(ATPortableTestContext& context) {
	ATCallbackHandler0<int> emptyHandler {};
	AT_PORTABLE_TEST_ASSERT(context, !emptyHandler);

	CallbackTarget target;
	auto member0 = ATBINDCALLBACK(&target, &CallbackTarget::Call0);
	auto member1 = ATBINDCALLBACK(&target, &CallbackTarget::Call1);
	auto member2 = ATBINDCALLBACK(&target, &CallbackTarget::Call2);
	auto memberVoid = ATBINDCALLBACK(&target, &CallbackTarget::Set);

	AT_PORTABLE_TEST_ASSERT(context, member0);
	AT_PORTABLE_TEST_ASSERT(context, member0() == 11);
	AT_PORTABLE_TEST_ASSERT(context, member1(4) == 15);
	AT_PORTABLE_TEST_ASSERT(context, member2(3, 5) == 30);
	memberVoid(7);
	AT_PORTABLE_TEST_ASSERT(context, target.mValue == 7);

	int freeValue = 20;
	auto free0 = ATMakeCallbackHandlerFn(FreeCall0, &freeValue);
	auto free1 = ATMakeCallbackHandlerFn(FreeCall1, &freeValue);
	auto free2 = ATMakeCallbackHandlerFn(FreeCall2, &freeValue);

	AT_PORTABLE_TEST_ASSERT(context, free0() == 21);
	AT_PORTABLE_TEST_ASSERT(context, free1(4) == 25);
	AT_PORTABLE_TEST_ASSERT(context, free2(3, 5) == 40);
	AT_PORTABLE_TEST_ASSERT(context, freeValue == 40);

	return true;
}
