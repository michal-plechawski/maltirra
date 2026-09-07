// Altirra portable system memory tests

#include <array>
#include <cstring>

#include <at/attest/portabletest.h>
#include <vd2/system/memory.h>

bool ATTestSystemMemory(ATPortableTestContext& context) {
	for(unsigned alignment : { 8U, 16U, 64U, 4096U }) {
		void *p = VDAlignedMalloc(257, alignment);
		AT_PORTABLE_TEST_ASSERT(context, p != nullptr);
		AT_PORTABLE_TEST_ASSERT(context, !((uintptr)p & (alignment - 1)));
		memset(p, 0xA5, 257);
		VDAlignedFree(p);
	}

	VDAlignedFree(nullptr);

	void *virtualAllocation = VDAlignedVirtualAlloc(257);
	AT_PORTABLE_TEST_ASSERT(context, virtualAllocation != nullptr);
	AT_PORTABLE_TEST_ASSERT(context, !((uintptr)virtualAllocation & 4095));
	memset(virtualAllocation, 0x5A, 257);
	VDAlignedVirtualFree(virtualAllocation);

	for(size_t count = 0; count <= 65; ++count) {
		std::array<uint8, 70> left {};
		std::array<uint8, 70> right {};

		for(size_t i = 0; i < left.size(); ++i) {
			left[i] = (uint8)(i + 1);
			right[i] = (uint8)(200 - i);
		}

		const auto originalLeft = left;
		const auto originalRight = right;
		VDSwapMemory(left.data() + 1, right.data() + 2, count);

		AT_PORTABLE_TEST_ASSERT(context,
			!memcmp(left.data() + 1, originalRight.data() + 2, count));
		AT_PORTABLE_TEST_ASSERT(context,
			!memcmp(right.data() + 2, originalLeft.data() + 1, count));
		AT_PORTABLE_TEST_ASSERT(context, left[0] == originalLeft[0]);
		AT_PORTABLE_TEST_ASSERT(context, right[0] == originalRight[0]);
	}

	{
		std::array<uint8, 70> values {};
		for(size_t i = 0; i < values.size(); ++i)
			values[i] = (uint8)i;

		VDInvertMemory(values.data() + 1, 67);
		AT_PORTABLE_TEST_ASSERT(context, values[0] == 0);
		AT_PORTABLE_TEST_ASSERT(context, values[68] == 68);

		for(size_t i = 1; i < 68; ++i)
			AT_PORTABLE_TEST_ASSERT(context, values[i] == (uint8)~i);
	}

	AT_PORTABLE_TEST_ASSERT(context, VDIsValidReadRegion(nullptr, 0));
	AT_PORTABLE_TEST_ASSERT(context, VDIsValidWriteRegion(nullptr, 0));
	AT_PORTABLE_TEST_ASSERT(context, !VDIsValidReadRegion(nullptr, 1));
	AT_PORTABLE_TEST_ASSERT(context, !VDIsValidWriteRegion(nullptr, 1));

	std::array<uint8, 32> writable {};
	AT_PORTABLE_TEST_ASSERT(context, VDIsValidReadRegion(writable.data(), writable.size()));
	AT_PORTABLE_TEST_ASSERT(context, VDIsValidWriteRegion(writable.data(), writable.size()));

	static const char readOnlyData[] = "read only";
	AT_PORTABLE_TEST_ASSERT(context, VDIsValidReadRegion(readOnlyData, sizeof readOnlyData));
	AT_PORTABLE_TEST_ASSERT(context,
		!VDIsValidWriteRegion(const_cast<char *>(readOnlyData), sizeof readOnlyData));

	{
		const uint8 equalA[] = { 1, 2, 3, 0, 4, 5, 6, 0 };
		const uint8 equalB[] = { 1, 2, 3, 9, 4, 5, 6, 9 };
		uint8 different[] = { 1, 2, 3, 9, 4, 0, 6, 9 };

		AT_PORTABLE_TEST_ASSERT(context,
			!VDCompareRect(const_cast<uint8 *>(equalA), 4, equalB, 4, 3, 2));
		AT_PORTABLE_TEST_ASSERT(context,
			VDCompareRect(const_cast<uint8 *>(equalA), 4, different, 4, 3, 2));
		AT_PORTABLE_TEST_ASSERT(context,
			!VDCompareRect(const_cast<uint8 *>(equalA), 4, different, 4, 0, 2));
	}

	for(size_t count = 0; count <= 80; ++count) {
		std::array<uint8, 80> values {};
		values.fill(0xA5);
		AT_PORTABLE_TEST_ASSERT(context, VDMemCheck8(values.data(), 0xA5, count) == nullptr);

		if (count) {
			values[count - 1] = 0x5A;
			AT_PORTABLE_TEST_ASSERT(context,
				VDMemCheck8(values.data(), 0xA5, count) == values.data() + count - 1);
		}
	}

	{
		std::array<uint8, 12> values {};
		VDMemset8(values.data() + 1, 0xA5, 10);
		AT_PORTABLE_TEST_ASSERT(context, values[0] == 0 && values[11] == 0);
		for(size_t i = 1; i < 11; ++i)
			AT_PORTABLE_TEST_ASSERT(context, values[i] == 0xA5);
	}

	{
		alignas(16) uint16 values16[4] {};
		alignas(16) uint32 values32[4] {};
		alignas(16) uint64 values64[4] {};
		VDMemset16(values16, UINT16_C(0xA1B2), 4);
		VDMemset32(values32, UINT32_C(0xA1B2C3D4), 4);
		VDMemset64(values64, UINT64_C(0x0123456789ABCDEF), 4);

		for(size_t i = 0; i < 4; ++i) {
			AT_PORTABLE_TEST_ASSERT(context, values16[i] == UINT16_C(0xA1B2));
			AT_PORTABLE_TEST_ASSERT(context, values32[i] == UINT32_C(0xA1B2C3D4));
			AT_PORTABLE_TEST_ASSERT(context, values64[i] == UINT64_C(0x0123456789ABCDEF));
		}
	}

	{
		const uint8 expected[] = { 0xC3, 0xB2, 0xA1, 0xC3, 0xB2, 0xA1 };
		uint8 values[sizeof expected] {};
		VDMemset24(values, UINT32_C(0xA1B2C3), 2);
		AT_PORTABLE_TEST_ASSERT(context, !memcmp(values, expected, sizeof values));
	}

	{
		const std::array<uint8, 16> pattern = {
			0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
		};
		std::array<uint8, 48> values {};
		VDMemset128(values.data(), pattern.data(), 3);

		for(size_t offset = 0; offset < values.size(); offset += pattern.size())
			AT_PORTABLE_TEST_ASSERT(context,
				!memcmp(values.data() + offset, pattern.data(), pattern.size()));

		VDMemset128(nullptr, nullptr, 0);
	}

	{
		int marker = 0;
		void *expected = &marker;
		void *values[5] {};
		VDMemsetPointer(values, expected, 5);
		for(void *value : values)
			AT_PORTABLE_TEST_ASSERT(context, value == expected);
	}

	{
		std::array<uint8, 24> values {};
		values.fill(0xCC);
		VDMemset8Rect(values.data() + 1, 8, 0x42, 5, 3);

		for(size_t row = 0; row < 3; ++row) {
			AT_PORTABLE_TEST_ASSERT(context, values[row * 8] == 0xCC);
			for(size_t column = 1; column < 6; ++column)
				AT_PORTABLE_TEST_ASSERT(context, values[row * 8 + column] == 0x42);
			AT_PORTABLE_TEST_ASSERT(context, values[row * 8 + 6] == 0xCC);
		}
	}

	{
		alignas(16) uint16 values16[12] {};
		alignas(16) uint32 values32[12] {};
		VDMemset16Rect(values16, 8, UINT16_C(0x1234), 3, 3);
		VDMemset32Rect(values32, 16, UINT32_C(0x89ABCDEF), 3, 3);

		for(size_t row = 0; row < 3; ++row) {
			for(size_t column = 0; column < 3; ++column) {
				AT_PORTABLE_TEST_ASSERT(context, values16[row * 4 + column] == UINT16_C(0x1234));
				AT_PORTABLE_TEST_ASSERT(context, values32[row * 4 + column] == UINT32_C(0x89ABCDEF));
			}
			AT_PORTABLE_TEST_ASSERT(context, values16[row * 4 + 3] == 0);
			AT_PORTABLE_TEST_ASSERT(context, values32[row * 4 + 3] == 0);
		}
	}

	{
		std::array<uint8, 24> values {};
		values.fill(0xCC);
		VDMemset24Rect(values.data(), 8, UINT32_C(0x332211), 2, 3);

		for(size_t row = 0; row < 3; ++row) {
			const uint8 expected[] = { 0x11, 0x22, 0x33, 0x11, 0x22, 0x33 };
			AT_PORTABLE_TEST_ASSERT(context,
				!memcmp(values.data() + row * 8, expected, sizeof expected));
			AT_PORTABLE_TEST_ASSERT(context, values[row * 8 + 6] == 0xCC);
		}
	}

	{
		const uint8 source[] = {
			1, 2, 3, 0, 0,
			4, 5, 6, 0, 0,
			7, 8, 9, 0, 0,
		};
		std::array<uint8, 18> destination {};
		destination.fill(0xCC);
		VDMemcpyRect(destination.data(), 6, source, 5, 3, 3);

		for(size_t row = 0; row < 3; ++row) {
			AT_PORTABLE_TEST_ASSERT(context,
				!memcmp(destination.data() + row * 6, source + row * 5, 3));
			AT_PORTABLE_TEST_ASSERT(context, destination[row * 6 + 3] == 0xCC);
		}
	}

	{
		const uint8 source[] = { 1, 2, 3, 4, 5, 6 };
		uint8 destination[sizeof source] {};
		VDFastMemcpyAutodetect();
		VDFastMemcpyPartial(destination, source, sizeof source);
		VDFastMemcpyFinish();
		AT_PORTABLE_TEST_ASSERT(context, !memcmp(destination, source, sizeof source));

		memset(destination, 0, sizeof destination);
		VDMemcpyRect(destination, 3, source, 3, 3, 2);
		AT_PORTABLE_TEST_ASSERT(context, !memcmp(destination, source, sizeof source));
	}

	{
		const uint8 source[] = { 9, 8, 7, 6 };
		uint8 destination[sizeof source] {};
		AT_PORTABLE_TEST_ASSERT(context,
			VDMemcpyGuarded(destination, source, sizeof source));
		AT_PORTABLE_TEST_ASSERT(context, !memcmp(destination, source, sizeof source));
		AT_PORTABLE_TEST_ASSERT(context, !VDMemcpyGuarded(nullptr, source, 1));
		AT_PORTABLE_TEST_ASSERT(context, !VDMemcpyGuarded(destination, nullptr, 1));
		AT_PORTABLE_TEST_ASSERT(context, VDMemcpyGuarded(nullptr, nullptr, 0));
	}

	return true;
}
