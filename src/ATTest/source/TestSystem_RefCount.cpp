// Altirra portable reference-counting tests

#include <utility>
#include <vd2/system/refcount.h>
#include <at/attest/portabletest.h>

namespace {
	class TestRefObject final : public IVDRefCount {
	public:
		explicit TestRefObject(int& destructionCount)
			: mDestructionCount(destructionCount) {
		}

		int AddRef() override {
			return ++mReferences;
		}

		int Release() override {
			const int references = --mReferences;

			if (!references)
				delete this;

			return references;
		}

		int GetReferences() const {
			return mReferences;
		}

	private:
		~TestRefObject() {
			++mDestructionCount;
		}

		int& mDestructionCount;
		int mReferences = 0;
	};
}

bool ATTestSystemRefCount(ATPortableTestContext& context) {
	int destructionCount = 0;
	TestRefObject *object = new TestRefObject(destructionCount);

	{
		vdrefptr<TestRefObject> first(object);
		AT_PORTABLE_TEST_ASSERT(context, object->GetReferences() == 1);

		vdrefptr<TestRefObject> second(first);
		AT_PORTABLE_TEST_ASSERT(context, object->GetReferences() == 2);

		vdrefptr<TestRefObject> third(std::move(second));
		AT_PORTABLE_TEST_ASSERT(context, !second);
		AT_PORTABLE_TEST_ASSERT(context, object->GetReferences() == 2);

		third.clear();
		AT_PORTABLE_TEST_ASSERT(context, object->GetReferences() == 1);

		TestRefObject *raw = object;
		raw->AddRef();
		AT_PORTABLE_TEST_ASSERT(context, object->GetReferences() == 2);
		vdsaferelease <<= raw;
		AT_PORTABLE_TEST_ASSERT(context, raw == nullptr);
		AT_PORTABLE_TEST_ASSERT(context, object->GetReferences() == 1);
	}

	AT_PORTABLE_TEST_ASSERT(context, destructionCount == 1);
	return true;
}
