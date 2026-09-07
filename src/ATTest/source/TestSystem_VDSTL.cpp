// Altirra portable system container tests

#include <cstring>

#include <at/attest/portabletest.h>
#include <vd2/system/Error.h>
#include <vd2/system/vdstl.h>

namespace {
	class TestAllocatorBase final : public vdallocator_base {
	public:
		static void ThrowOutOfMemory() {
			TestAllocatorBase allocator;
			allocator.throw_oom();
		}
	};
}

bool ATTestSystemVDSTL(ATPortableTestContext& context) {
	bool caughtAllocationError = false;

	try {
		TestAllocatorBase::ThrowOutOfMemory();
	} catch(const VDAllocationFailedException& error) {
		caughtAllocationError = true;
		AT_PORTABLE_TEST_ASSERT(context, !strcmp(error.c_str(), "Out of memory"));
	}

	AT_PORTABLE_TEST_ASSERT(context, caughtAllocationError);

	vdallocator<int> allocator;
	int *allocation = allocator.allocate(4);
	AT_PORTABLE_TEST_ASSERT(context, allocation != nullptr);

	for(int i = 0; i < 4; ++i)
		allocation[i] = i * i;

	AT_PORTABLE_TEST_ASSERT(context,
		allocation[0] == 0 && allocation[1] == 1
		&& allocation[2] == 4 && allocation[3] == 9);
	allocator.deallocate(allocation, 4);

	vdfastvector<int> values;
	AT_PORTABLE_TEST_ASSERT(context, values.empty());
	AT_PORTABLE_TEST_ASSERT(context, values.size() == 0);

	values.push_back(1);
	values.push_back(2);
	values.push_back(3);
	AT_PORTABLE_TEST_ASSERT(context, values.size() == 3);
	AT_PORTABLE_TEST_ASSERT(context,
		values[0] == 1 && values[1] == 2 && values[2] == 3);

	values.insert(values.begin() + 1, values.back());
	AT_PORTABLE_TEST_ASSERT(context, values.size() == 4);
	AT_PORTABLE_TEST_ASSERT(context,
		values[0] == 1 && values[1] == 3 && values[2] == 2 && values[3] == 3);

	values.erase(values.begin() + 2);
	AT_PORTABLE_TEST_ASSERT(context, values.size() == 3);
	AT_PORTABLE_TEST_ASSERT(context,
		values[0] == 1 && values[1] == 3 && values[2] == 3);

	values.resize(8, 7);
	AT_PORTABLE_TEST_ASSERT(context, values.size() == 8);
	for(size_t i = 3; i < values.size(); ++i)
		AT_PORTABLE_TEST_ASSERT(context, values[i] == 7);

	const int replacement[] = { 10, 20, 30, 40 };
	values.assign(replacement, replacement + 4);
	AT_PORTABLE_TEST_ASSERT(context, values.size() == 4);
	AT_PORTABLE_TEST_ASSERT(context,
		values.front() == 10 && values.back() == 40);

	vdfastvector<int> other;
	other.push_back(99);
	values.swap(other);
	AT_PORTABLE_TEST_ASSERT(context, values.size() == 1 && values[0] == 99);
	AT_PORTABLE_TEST_ASSERT(context,
		other.size() == 4 && other[0] == 10 && other[3] == 40);

	int spanValues[] = { 5, 6, 7 };
	vdspan<int> span(spanValues);
	AT_PORTABLE_TEST_ASSERT(context, span.size() == 3);
	AT_PORTABLE_TEST_ASSERT(context, span.front() == 5 && span.back() == 7);

	return true;
}
