// Altirra portable vdfunction tests

#include <functional>
#include <utility>
#include <vd2/system/function.h>
#include <at/attest/portabletest.h>

namespace {
	struct LifetimeState {
		int mLive = 0;
		int mCopies = 0;
		int mMoves = 0;
		int mDestructions = 0;
		bool mValid = true;
	};

	class SmallCallable {
	public:
		SmallCallable(LifetimeState& state, int value)
			: mpState(&state), mValue(value) {
			++mpState->mLive;
		}

		SmallCallable(const SmallCallable& src)
			: mpState(src.mpState), mValue(src.mValue) {
			++mpState->mLive;
			++mpState->mCopies;
		}

		SmallCallable(SmallCallable&& src) vdnoexcept
			: mpState(src.mpState), mValue(src.mValue) {
			++mpState->mLive;
			++mpState->mMoves;
			src.mValue = -1;
		}

		~SmallCallable() {
			--mpState->mLive;
			++mpState->mDestructions;
			if (mpState->mLive < 0)
				mpState->mValid = false;
		}

		int operator()() const {
			return mValue;
		}

	private:
		LifetimeState *mpState;
		int mValue;
	};

	class LargeCallable {
	public:
		LargeCallable(LifetimeState& state, int value)
			: mpState(&state), mValue(value) {
			++mpState->mLive;
		}

		LargeCallable(const LargeCallable& src)
			: mpState(src.mpState), mValue(src.mValue) {
			++mpState->mLive;
			++mpState->mCopies;
		}

		LargeCallable(LargeCallable&& src) vdnoexcept
			: mpState(src.mpState), mValue(src.mValue) {
			++mpState->mLive;
			++mpState->mMoves;
			src.mValue = -1;
		}

		~LargeCallable() {
			--mpState->mLive;
			++mpState->mDestructions;
			if (mpState->mLive < 0)
				mpState->mValid = false;
		}

		int operator()() const {
			return mValue;
		}

	private:
		LifetimeState *mpState;
		int mValue;
		char mPadding[64] {};
	};

	struct ReferenceCallable {
		int mValue = 0;

		int operator()(int delta) {
			mValue += delta;
			return mValue;
		}
	};

	static_assert(vdfunc_mode<SmallCallable>::value == 1);
	static_assert(vdfunc_mode<LargeCallable>::value == 2);
}

bool ATTestSystemVDFunction(ATPortableTestContext& context) {
	vdfunction<int()> empty;
	AT_PORTABLE_TEST_ASSERT(context, !empty);
	AT_PORTABLE_TEST_ASSERT(context, empty == nullptr);
	AT_PORTABLE_TEST_ASSERT(context, nullptr == empty);

	empty = [] { return 17; };
	AT_PORTABLE_TEST_ASSERT(context, !!empty);
	AT_PORTABLE_TEST_ASSERT(context, empty != nullptr);
	AT_PORTABLE_TEST_ASSERT(context, nullptr != empty);
	AT_PORTABLE_TEST_ASSERT(context, empty() == 17);
	empty = nullptr;
	AT_PORTABLE_TEST_ASSERT(context, !empty);

	vdfunction<int(int, int)> add = [](int x, int y) { return x + y; };
	AT_PORTABLE_TEST_ASSERT(context, add(19, 23) == 42);

	vdfunction<int()> counter = [count = 0]() mutable { return ++count; };
	AT_PORTABLE_TEST_ASSERT(context, counter() == 1);
	vdfunction<int()> counterCopy = counter;
	AT_PORTABLE_TEST_ASSERT(context, counter() == 2);
	AT_PORTABLE_TEST_ASSERT(context, counterCopy() == 2);
	vdfunction<int()> counterMoved = std::move(counterCopy);
	AT_PORTABLE_TEST_ASSERT(context, !counterCopy);
	AT_PORTABLE_TEST_ASSERT(context, counterMoved() == 3);

	LifetimeState smallState;
	LifetimeState largeState;
	{
		vdfunction<int()> small = SmallCallable(smallState, 10);
		vdfunction<int()> large = LargeCallable(largeState, 20);
		AT_PORTABLE_TEST_ASSERT(context, smallState.mLive == 1);
		AT_PORTABLE_TEST_ASSERT(context, largeState.mLive == 1);
		AT_PORTABLE_TEST_ASSERT(context, small() == 10);
		AT_PORTABLE_TEST_ASSERT(context, large() == 20);

		vdfunction<int()> smallCopy = small;
		vdfunction<int()> largeCopy = large;
		AT_PORTABLE_TEST_ASSERT(context, smallState.mCopies > 0);
		AT_PORTABLE_TEST_ASSERT(context, largeState.mCopies > 0);
		AT_PORTABLE_TEST_ASSERT(context, smallState.mLive == 2);
		AT_PORTABLE_TEST_ASSERT(context, largeState.mLive == 2);

		vdfunction<int()> smallMoved = std::move(small);
		vdfunction<int()> largeMoved = std::move(large);
		AT_PORTABLE_TEST_ASSERT(context, !small && !large);
		AT_PORTABLE_TEST_ASSERT(context, smallMoved() == 10);
		AT_PORTABLE_TEST_ASSERT(context, largeMoved() == 20);
		AT_PORTABLE_TEST_ASSERT(context, smallState.mLive == 2);
		AT_PORTABLE_TEST_ASSERT(context, largeState.mLive == 2);

		smallMoved.swap(largeMoved);
		AT_PORTABLE_TEST_ASSERT(context, smallMoved() == 20);
		AT_PORTABLE_TEST_ASSERT(context, largeMoved() == 10);

		smallCopy = largeMoved;
		AT_PORTABLE_TEST_ASSERT(context, smallCopy() == 10);
		AT_PORTABLE_TEST_ASSERT(context, smallState.mLive == 2);
		largeCopy = nullptr;
		AT_PORTABLE_TEST_ASSERT(context, largeState.mLive == 1);
	}
	AT_PORTABLE_TEST_ASSERT(context, smallState.mValid && smallState.mLive == 0);
	AT_PORTABLE_TEST_ASSERT(context, largeState.mValid && largeState.mLive == 0);
	AT_PORTABLE_TEST_ASSERT(context, smallState.mDestructions > 0);
	AT_PORTABLE_TEST_ASSERT(context, largeState.mDestructions > 0);

	ReferenceCallable referenced;
	vdfunction<int(int)> referenceFunction(std::ref(referenced));
	vdfunction<int(int)> referenceCopy = referenceFunction;
	AT_PORTABLE_TEST_ASSERT(context, referenceFunction(4) == 4);
	AT_PORTABLE_TEST_ASSERT(context, referenceCopy(5) == 9);
	AT_PORTABLE_TEST_ASSERT(context, referenced.mValue == 9);

	vdfunction<int()> bound = [] { return 99; };
	vdfunction<int()> unbound;
	bound.swap(unbound);
	AT_PORTABLE_TEST_ASSERT(context, !bound);
	AT_PORTABLE_TEST_ASSERT(context, unbound() == 99);

	return true;
}
