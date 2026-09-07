// Altirra portable hash table base tests

#include <limits>

#include <at/attest/portabletest.h>
#include <vd2/system/vdstl_hashtable.h>

namespace {
	class TestHashTableBase final : public vdhashtable_base {
	public:
		static size_type ComputeBucketCount(size_type count) {
			return compute_bucket_count(count);
		}

		void SetBuckets(vdhashtable_base_node **buckets, size_type count, size_type elements) {
			mpBucketStart = buckets;
			mpBucketEnd = buckets + count;
			mBucketCount = count;
			mElementCount = elements;
		}
	};
}

bool ATTestSystemVDSTLHashTable(ATPortableTestContext& context) {
	AT_PORTABLE_TEST_ASSERT(context, VDComputePrimeBucketCount(0) == 11);
	AT_PORTABLE_TEST_ASSERT(context, VDComputePrimeBucketCount(11) == 11);
	AT_PORTABLE_TEST_ASSERT(context, VDComputePrimeBucketCount(12) == 17);
	AT_PORTABLE_TEST_ASSERT(context, VDComputePrimeBucketCount(17) == 17);
	AT_PORTABLE_TEST_ASSERT(context, VDComputePrimeBucketCount(18) == 37);
	AT_PORTABLE_TEST_ASSERT(context, VDComputePrimeBucketCount(2049) == 2049);
	AT_PORTABLE_TEST_ASSERT(context, VDComputePrimeBucketCount(2050) == 4099);
	AT_PORTABLE_TEST_ASSERT(context,
		VDComputePrimeBucketCount(1073741827) == 1073741827);
	AT_PORTABLE_TEST_ASSERT(context,
		TestHashTableBase::ComputeBucketCount(65538) == 131101);

	TestHashTableBase table;
	AT_PORTABLE_TEST_ASSERT(context, table.empty());
	AT_PORTABLE_TEST_ASSERT(context, table.size() == 0);
	AT_PORTABLE_TEST_ASSERT(context, table.bucket_count() == 0);
	AT_PORTABLE_TEST_ASSERT(context,
		table.max_bucket_count() == (std::numeric_limits<size_t>::max() >> 1));

	vdhashtable_base_node third { nullptr };
	vdhashtable_base_node second { &third };
	vdhashtable_base_node first { &second };
	vdhashtable_base_node fourth { nullptr };
	vdhashtable_base_node *buckets[] = { &first, nullptr, &fourth };
	table.SetBuckets(buckets, 3, 4);

	AT_PORTABLE_TEST_ASSERT(context, !table.empty());
	AT_PORTABLE_TEST_ASSERT(context, table.size() == 4);
	AT_PORTABLE_TEST_ASSERT(context, table.bucket_count() == 3);
	AT_PORTABLE_TEST_ASSERT(context, table.bucket_size(0) == 3);
	AT_PORTABLE_TEST_ASSERT(context, table.bucket_size(1) == 0);
	AT_PORTABLE_TEST_ASSERT(context, table.bucket_size(2) == 1);

	return true;
}
