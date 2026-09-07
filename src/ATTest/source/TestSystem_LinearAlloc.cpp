// Altirra portable linear allocator tests

#include <array>
#include <cstring>

#include <at/attest/portabletest.h>
#include <vd2/system/Error.h>
#include <vd2/system/linearalloc.h>

namespace {
	struct TestValue {
		explicit TestValue(int value)
			: mValue(value)
		{
		}

		int mValue;
	};

	struct TestDefaultValue {
		TestDefaultValue() noexcept
			: mValue(11)
		{
		}

		int mValue;
	};
}

bool ATTestSystemLinearAlloc(ATPortableTestContext& context) {
	VDLinearAllocator allocator(512);
	AT_PORTABLE_TEST_ASSERT(context, allocator.GetTotalAllocatedSize() == 0);

	void *first = allocator.Allocate(1);
	AT_PORTABLE_TEST_ASSERT(context, first != nullptr);
	AT_PORTABLE_TEST_ASSERT(context, allocator.Contains(first));
	AT_PORTABLE_TEST_ASSERT(context, allocator.GetTotalAllocatedSize() == 512);

	void *cursor = allocator.Allocate(0);
	if (!((uintptr)cursor & 63)) {
		allocator.Allocate(sizeof(void *));
		cursor = allocator.Allocate(0);
	}
	AT_PORTABLE_TEST_ASSERT(context, ((uintptr)cursor & 63) != 0);

	void *aligned = allocator.Allocate(16, 64);
	AT_PORTABLE_TEST_ASSERT(context, ((uintptr)aligned & 63) == 0);
	memset(aligned, 0xA5, 16);

	void *afterAligned = allocator.Allocate(16);
	AT_PORTABLE_TEST_ASSERT(context,
		(uintptr)afterAligned >= (uintptr)aligned + 16);
	memset(afterAligned, 0x5A, 16);

	const auto *alignedBytes = static_cast<const unsigned char *>(aligned);
	for(size_t i = 0; i < 16; ++i)
		AT_PORTABLE_TEST_ASSERT(context, alignedBytes[i] == 0xA5);

	TestValue *value = allocator.Allocate<TestValue>(37);
	AT_PORTABLE_TEST_ASSERT(context, value->mValue == 37);
	AT_PORTABLE_TEST_ASSERT(context, allocator.Contains(value));

	TestDefaultValue *array = allocator.AllocateArray<TestDefaultValue>(3);
	for(size_t i = 0; i < 3; ++i)
		AT_PORTABLE_TEST_ASSERT(context, array[i].mValue == 11);

	void *large = allocator.Allocate(480);
	AT_PORTABLE_TEST_ASSERT(context, allocator.Contains(large));
	AT_PORTABLE_TEST_ASSERT(context, allocator.GetTotalAllocatedSize() == 992);

	allocator.Reset();
	AT_PORTABLE_TEST_ASSERT(context, allocator.GetTotalAllocatedSize() == 480);
	AT_PORTABLE_TEST_ASSERT(context, !allocator.Contains(first));
	AT_PORTABLE_TEST_ASSERT(context, allocator.Contains(large));
	void *reused = allocator.Allocate(64);
	AT_PORTABLE_TEST_ASSERT(context, reused == large);
	AT_PORTABLE_TEST_ASSERT(context, allocator.GetTotalAllocatedSize() == 480);

	allocator.Clear();
	AT_PORTABLE_TEST_ASSERT(context, allocator.GetTotalAllocatedSize() == 0);
	AT_PORTABLE_TEST_ASSERT(context, !allocator.Contains(reused));

	VDLinearAllocator left(128);
	VDLinearAllocator right(256);
	void *leftValue = left.Allocate(8);
	void *rightValue = right.Allocate(8);
	left.Swap(right);
	AT_PORTABLE_TEST_ASSERT(context, left.Contains(rightValue));
	AT_PORTABLE_TEST_ASSERT(context, !left.Contains(leftValue));
	AT_PORTABLE_TEST_ASSERT(context, left.GetTotalAllocatedSize() == 256);
	AT_PORTABLE_TEST_ASSERT(context, right.Contains(leftValue));
	AT_PORTABLE_TEST_ASSERT(context, !right.Contains(rightValue));
	AT_PORTABLE_TEST_ASSERT(context, right.GetTotalAllocatedSize() == 128);

	alignas(16) std::array<unsigned char, 16> fixedStorage {};
	VDFixedLinearAllocator fixed(fixedStorage.data(), fixedStorage.size());
	AT_PORTABLE_TEST_ASSERT(context, fixed.Allocate(8) == fixedStorage.data());
	AT_PORTABLE_TEST_ASSERT(context,
		fixed.Allocate(8) == fixedStorage.data() + 8);

	bool caughtAllocationError = false;
	try {
		fixed.Allocate(1);
	} catch(const VDAllocationFailedException&) {
		caughtAllocationError = true;
	}
	AT_PORTABLE_TEST_ASSERT(context, caughtAllocationError);

	return true;
}
