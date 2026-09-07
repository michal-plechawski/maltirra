// Altirra portable bit math tests

#include <vd2/system/bitmath.h>
#include <at/attest/portabletest.h>

namespace {
	int ReferenceCountBits(uint32 value) {
		int count = 0;

		while(value) {
			count += value & 1;
			value >>= 1;
		}

		return count;
	}
}

bool ATTestSystemBitMath(ATPortableTestContext& context) {
	for(uint32 value = 0; value <= UINT16_MAX; ++value) {
		AT_PORTABLE_TEST_ASSERT(context, VDCountBits(value) == ReferenceCountBits(value));
		AT_PORTABLE_TEST_ASSERT(context, VDCountBits8((uint8)value) == ReferenceCountBits((uint8)value));
	}

	AT_PORTABLE_TEST_ASSERT(context, VDCountBits(UINT32_C(0xFFFFFFFF)) == 32);
	AT_PORTABLE_TEST_ASSERT(context, VDCountBits(UINT32_C(0xAAAAAAAA)) == 16);
	AT_PORTABLE_TEST_ASSERT(context, VDCountBits(UINT32_C(0x80000001)) == 2);

	AT_PORTABLE_TEST_ASSERT(context, VDFindLowestSetBit(0) == 32);
	AT_PORTABLE_TEST_ASSERT(context, VDFindHighestSetBit(0) == -1);

	for(int bit = 0; bit < 32; ++bit) {
		const uint32 value = UINT32_C(1) << bit;
		AT_PORTABLE_TEST_ASSERT(context, VDFindLowestSetBit(value) == bit);
		AT_PORTABLE_TEST_ASSERT(context, VDFindHighestSetBit(value) == bit);
		AT_PORTABLE_TEST_ASSERT(context, VDFindLowestSetBitFast(value) == bit);
		AT_PORTABLE_TEST_ASSERT(context, VDFindHighestSetBitFast(value) == bit);
	}

	for(int bit = 0; bit < 64; ++bit) {
		const uint64 value = UINT64_C(1) << bit;
		AT_PORTABLE_TEST_ASSERT(context, VDFindLowestSetBitFast64(value) == bit);
	}

	static constexpr struct {
		uint32 mInput;
		uint32 mExpected;
	} kCeilCases[] = {
		{ 1, 1 },
		{ 2, 2 },
		{ 3, 4 },
		{ 4, 4 },
		{ 5, 8 },
		{ 255, 256 },
		{ 256, 256 },
		{ 257, 512 },
		{ UINT32_C(0x40000001), UINT32_C(0x80000000) },
		{ UINT32_C(0x80000000), UINT32_C(0x80000000) },
	};

	for(const auto& testCase : kCeilCases)
		AT_PORTABLE_TEST_ASSERT(context, VDCeilToPow2(testCase.mInput) == testCase.mExpected);

	return true;
}
