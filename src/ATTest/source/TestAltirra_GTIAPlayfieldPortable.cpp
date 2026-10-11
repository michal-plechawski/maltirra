// Portable bounds and reference tests for the GTIA playfield unpackers.

#include <array>
#include <vector>

#include <at/attest/portabletest.h>
#include <gtiaplayfield.h>
#include <gtiatables.h>

namespace {
	constexpr std::array<uint32, 10> kCounts { 0, 1, 2, 15, 16, 17, 31, 32, 33, 114 };
	constexpr uint8 kGuard = 0xa7;

	bool Test160(ATPortableTestContext& context, uint32 sourceOffset, uint32 destOffset,
		uint32 count, uint8 seed) {
		std::vector<uint8> source(sourceOffset + count + 16, kGuard);
		std::vector<uint8> dest(destOffset + count * 2 + 16, kGuard);
		for(uint32 i = 0; i < count; ++i)
			source[sourceOffset + i] = (uint8)(seed + i * 73);

		ATGTIAUnpackPlayfield160(dest.data() + destOffset, source.data() + sourceOffset, count);

		for(uint32 i = 0; i < dest.size(); ++i) {
			if (i >= destOffset && i < destOffset + count * 2) {
				const uint8 value = source[sourceOffset + (i - destOffset) / 2];
				const uint8 expected = (i - destOffset) & 1 ? value & 15 : value >> 4;
				AT_PORTABLE_TEST_ASSERT(context, dest[i] == expected);
			} else {
				AT_PORTABLE_TEST_ASSERT(context, dest[i] == kGuard);
			}
		}
		for(uint32 i = 0; i < source.size(); ++i) {
			if (i < sourceOffset || i >= sourceOffset + count)
				AT_PORTABLE_TEST_ASSERT(context, source[i] == kGuard);
		}
		return true;
	}

	bool Test320(ATPortableTestContext& context, uint32 sourceOffset, uint32 destOffset,
		uint32 count, uint8 seed) {
		std::vector<uint8> source(sourceOffset + count + 16, kGuard);
		std::vector<uint8> priority(destOffset + count * 2 + 16, kGuard);
		std::vector<uint8> hires(destOffset + count * 2 + 16, kGuard);
		for(uint32 i = 0; i < count; ++i)
			source[sourceOffset + i] = (uint8)(seed + i * 73);

		ATGTIAUnpackPlayfield320(priority.data(), hires.data(), source.data() + sourceOffset,
			destOffset, count);

		for(uint32 i = 0; i < priority.size(); ++i) {
			if (i >= destOffset && i < destOffset + count * 2) {
				AT_PORTABLE_TEST_ASSERT(context, priority[i] == ATGTIA::PF2);
				const uint8 value = source[sourceOffset + (i - destOffset) / 2];
				const uint8 expected = (i - destOffset) & 1 ? value & 3 : (value >> 2) & 3;
				AT_PORTABLE_TEST_ASSERT(context, hires[i] == expected);
			} else {
				AT_PORTABLE_TEST_ASSERT(context, priority[i] == kGuard);
				AT_PORTABLE_TEST_ASSERT(context, hires[i] == kGuard);
			}
		}
		for(uint32 i = 0; i < source.size(); ++i) {
			if (i < sourceOffset || i >= sourceOffset + count)
				AT_PORTABLE_TEST_ASSERT(context, source[i] == kGuard);
		}
		return true;
	}

	bool TestExactAllocations(ATPortableTestContext& context) {
		for(uint32 count : kCounts) {
			if (!count) {
				ATGTIAUnpackPlayfield160(nullptr, nullptr, 0);
				for(uint32 x = 0; x < 16; ++x)
					ATGTIAUnpackPlayfield320(nullptr, nullptr, nullptr, x, 0);
				continue;
			}

			std::vector<uint8> source(count);
			std::vector<uint8> output160(count * 2);
			std::vector<uint8> priority320(count * 2);
			std::vector<uint8> hires320(count * 2);
			for(uint32 i = 0; i < count; ++i)
			source[i] = (uint8)(i * 151 + 37);

			ATGTIAUnpackPlayfield160(output160.data(), source.data(), count);
			ATGTIAUnpackPlayfield320(priority320.data(), hires320.data(), source.data(), 0, count);
			for(uint32 i = 0; i < count; ++i) {
				AT_PORTABLE_TEST_ASSERT(context, output160[2 * i] == source[i] >> 4);
				AT_PORTABLE_TEST_ASSERT(context, output160[2 * i + 1] == (source[i] & 15));
				AT_PORTABLE_TEST_ASSERT(context, priority320[2 * i] == ATGTIA::PF2);
				AT_PORTABLE_TEST_ASSERT(context, priority320[2 * i + 1] == ATGTIA::PF2);
				AT_PORTABLE_TEST_ASSERT(context, hires320[2 * i] == ((source[i] >> 2) & 3));
				AT_PORTABLE_TEST_ASSERT(context, hires320[2 * i + 1] == (source[i] & 3));
			}
		}
		return true;
	}
}

bool ATTestAltirraGTIAPlayfield(ATPortableTestContext& context) {
	// Every input byte is tested at every source and destination alignment.
	for(uint32 sourceOffset = 0; sourceOffset < 16; ++sourceOffset) {
		for(uint32 destOffset = 0; destOffset < 16; ++destOffset) {
			if (!Test160(context, sourceOffset, destOffset, 256, 0))
				return false;
			if (!Test320(context, sourceOffset, destOffset, 256, 0))
				return false;
		}
	}

	for(uint32 sourceOffset = 0; sourceOffset < 16; ++sourceOffset) {
		for(uint32 destOffset = 0; destOffset < 16; ++destOffset) {
			for(uint32 count : kCounts) {
				if (!Test160(context, sourceOffset, destOffset, count, (uint8)(sourceOffset * 17 + destOffset * 29)))
					return false;
				if (!Test320(context, sourceOffset, destOffset, count, (uint8)(sourceOffset * 17 + destOffset * 29)))
					return false;
			}
		}
	}

	return TestExactAllocations(context);
}
