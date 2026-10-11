// Portable reference and bounds tests for PIA counted input state.
#include <cstring>

#include <at/attest/portabletest.h>
#include <piacountedinputs.h>

namespace {
	constexpr uint8 kGuard = 0xa7;

	struct GuardedCounts {
		uint8 mBefore[16];
		alignas(16) uint8 mCounts[32];
		uint8 mAfter[16];
	};

	void Init(GuardedCounts& state, uint8 value) {
		memset(state.mBefore, kGuard, sizeof state.mBefore);
		memset(state.mCounts, value, sizeof state.mCounts);
		memset(state.mAfter, kGuard, sizeof state.mAfter);
	}

	bool CheckCounts(ATPortableTestContext& context, const GuardedCounts& state,
		const uint8 expected[32]) {
		for (uint32 i = 0; i < 32; ++i)
			AT_PORTABLE_TEST_ASSERT(context, state.mCounts[i] == expected[i]);
		for (uint8 v : state.mBefore)
			AT_PORTABLE_TEST_ASSERT(context, v == kGuard);
		for (uint8 v : state.mAfter)
			AT_PORTABLE_TEST_ASSERT(context, v == kGuard);
		uint32 expectedMask = 0;
		for (uint32 i = 0; i < 32; ++i) {
			if (!expected[i])
				expectedMask |= uint32(1) << i;
		}
		AT_PORTABLE_TEST_ASSERT(context, ATPIAReadZeroMask(state.mCounts) == expectedMask);
		return true;
	}

	bool TestSingleBits(ATPortableTestContext& context) {
		for (uint32 bit = 0; bit < 32; ++bit) {
			GuardedCounts state;
			Init(state, 0);
			uint8 expected[32] {};
			const uint32 mask = uint32(1) << bit;
			ATPIAAddZeroBits(state.mCounts, mask);
			for (uint32 i = 0; i < 32; ++i) {
				if (i != bit)
					expected[i] = 1;
			}
			if (!CheckCounts(context, state, expected))
				return false;
			ATPIARemoveZeroBits(state.mCounts, mask);
			memset(expected, 0, sizeof expected);
			if (!CheckCounts(context, state, expected))
				return false;
		}
		return true;
	}

	bool TestBoundaries(ATPortableTestContext& context) {
		GuardedCounts state;
		uint8 expected[32];
		Init(state, 0);
		memset(expected, 0, sizeof expected);
		AT_PORTABLE_TEST_ASSERT(context, ATPIAReadZeroMask(state.mCounts) == 0xffffffffu);
		ATPIAAddZeroBits(state.mCounts, 0);
		memset(expected, 1, sizeof expected);
		if (!CheckCounts(context, state, expected))
			return false;
		ATPIARemoveZeroBits(state.mCounts, 0);
		memset(expected, 0, sizeof expected);
		if (!CheckCounts(context, state, expected))
			return false;

		Init(state, 0xff);
		memset(expected, 0xff, sizeof expected);
		AT_PORTABLE_TEST_ASSERT(context, ATPIAReadZeroMask(state.mCounts) == 0);
		ATPIAAddZeroBits(state.mCounts, 0xffffffffu);
		ATPIARemoveZeroBits(state.mCounts, 0xffffffffu);
		if (!CheckCounts(context, state, expected))
			return false;

		Init(state, 0);
		memset(expected, 0, sizeof expected);
		for (uint32 i = 0; i < 255; ++i) {
			ATPIAAddZeroBits(state.mCounts, 0);
			for (uint8& v : expected)
				++v;
		}
		if (!CheckCounts(context, state, expected))
			return false;
		ATPIAAddZeroBits(state.mCounts, 0);
		for (uint8& v : expected)
			++v;
		if (!CheckCounts(context, state, expected))
			return false;
		ATPIARemoveZeroBits(state.mCounts, 0);
		for (uint8& v : expected)
			--v;
		return CheckCounts(context, state, expected);
	}

	bool TestRandomOperations(ATPortableTestContext& context) {
		GuardedCounts state;
		Init(state, 0);
		uint8 expected[32] {};
		uint32 rng = 0x6d2b79f5;
		for (uint32 i = 0; i < 10000; ++i) {
			rng = rng * 1664525u + 1013904223u;
			const uint32 mask = rng;
			if (rng & 0x80000000u) {
				ATPIAAddZeroBits(state.mCounts, mask);
				for (uint32 bit = 0; bit < 32; ++bit) {
					if (!(mask & (uint32(1) << bit)))
						++expected[bit];
				}
			} else {
				ATPIARemoveZeroBits(state.mCounts, mask);
				for (uint32 bit = 0; bit < 32; ++bit) {
					if (!(mask & (uint32(1) << bit)))
						--expected[bit];
				}
			}
			if ((i & 127) == 127 && !CheckCounts(context, state, expected))
				return false;
		}
		return CheckCounts(context, state, expected);
	}

	bool TestExactAllocation(ATPortableTestContext& context) {
		alignas(16) uint8 counts[32] {};
		ATPIAAddZeroBits(counts, 0);
		ATPIARemoveZeroBits(counts, 0);
		AT_PORTABLE_TEST_ASSERT(context, ATPIAReadZeroMask(counts) == 0xffffffffu);
		return true;
	}
}

bool ATTestAltirraPIACountedInputs(ATPortableTestContext& context) {
	return TestSingleBits(context) && TestBoundaries(context)
		&& TestRandomOperations(context) && TestExactAllocation(context);
}
