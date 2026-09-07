// Altirra portable allocation helper tests

#include <utility>
#include <vd2/system/vdtypes.h>
#include <vd2/system/vdalloc.h>
#include <at/attest/portabletest.h>

namespace {
	struct TrackedObject {
		explicit TrackedObject(int& destructionCount)
			: mDestructionCount(destructionCount) {
		}

		~TrackedObject() {
			++mDestructionCount;
		}

		int& mDestructionCount;
	};
}

bool ATTestSystemVDAlloc(ATPortableTestContext& context) {
	int destructionCount = 0;

	{
		vdautoptr<TrackedObject> first(new TrackedObject(destructionCount));
		vdautoptr<TrackedObject> second(std::move(first));
		AT_PORTABLE_TEST_ASSERT(context, first.get() == nullptr);
		AT_PORTABLE_TEST_ASSERT(context, second.get() != nullptr);

		TrackedObject *released = second.release();
		AT_PORTABLE_TEST_ASSERT(context, second.get() == nullptr);
		AT_PORTABLE_TEST_ASSERT(context, released != nullptr);
		vdsafedelete <<= released;
		AT_PORTABLE_TEST_ASSERT(context, released == nullptr);
	}

	AT_PORTABLE_TEST_ASSERT(context, destructionCount == 1);

	TrackedObject *objects[] = {
		new TrackedObject(destructionCount),
		new TrackedObject(destructionCount),
	};
	vdsafedelete <<= objects;
	AT_PORTABLE_TEST_ASSERT(context, objects[0] == nullptr && objects[1] == nullptr);
	AT_PORTABLE_TEST_ASSERT(context, destructionCount == 3);

	int scopeExitCalls = 0;
	{
		vdscopeexit onExit([&scopeExitCalls] { ++scopeExitCalls; });
	}
	AT_PORTABLE_TEST_ASSERT(context, scopeExitCalls == 1);

	{
		vdscopeexit releasedExit([&scopeExitCalls] { ++scopeExitCalls; });
		releasedExit.release();
	}
	AT_PORTABLE_TEST_ASSERT(context, scopeExitCalls == 1);

	return true;
}
