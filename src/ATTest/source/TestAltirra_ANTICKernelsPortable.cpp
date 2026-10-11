// Portable reference and bounds tests for the ANTIC kernels.
#include <algorithm>
#include <array>
#include <cstring>
#include <vector>

#include <at/attest/portabletest.h>
#include <antickernels.h>

namespace {
	constexpr uint8 kGuard = 0xa7;
	constexpr std::array<uint32, 12> kCounts { 0, 1, 2, 7, 8, 9, 15, 16, 17, 32, 114, 256 };

	uint8 DecodeByte(uint8 name, uint8 ch, uint8 inv, uint8 blink) {
		const uint8 hi = (name & 0x80) ? 0xff : 0;
		ch &= (uint8)(~hi | blink);
		ch ^= (uint8)(hi & inv);
		return ch;
	}

	bool TestDecode(ATPortableTestContext& context, uint32 offset, uint32 count,
		uint8 inv, uint8 blink) {
		std::vector<uint8> names(offset + count + 16, kGuard);
		std::vector<uint8> chars(offset + count + 16, kGuard);
		std::vector<uint8> output(offset + count * 2 + 16, kGuard);
		for (uint32 i = 0; i < count; ++i) {
			names[offset + i] = (uint8)(i * 131 + offset * 17);
			chars[offset + i] = (uint8)(i * 73 + offset * 29);
		}

		const uint32 x = offset;
		const uint32 limit = count ? x + count * 2 - 1 : x;
		const uint32 result = ATAnticDecodeMode2(output.data() + offset, names.data() + offset,
			chars.data() + offset, x, limit, inv, blink);
		const uint32 decoded = count ? (limit - x + 1) / 2 : 0;
		AT_PORTABLE_TEST_ASSERT(context, result == x + decoded * 2);
		for (uint32 i = 0; i < output.size(); ++i) {
			if (i >= offset && i < offset + decoded * 2) {
				const uint32 j = (i - offset) >> 1;
				const uint8 value = DecodeByte(names[offset + j], chars[offset + j], inv, blink);
				AT_PORTABLE_TEST_ASSERT(context, output[i] == ((i - offset) & 1 ? value & 15 : value >> 4));
			} else {
				AT_PORTABLE_TEST_ASSERT(context, output[i] == kGuard);
			}
		}
		for (uint32 i = 0; i < names.size(); ++i) {
			if (i < offset || i >= offset + count) {
				AT_PORTABLE_TEST_ASSERT(context, names[i] == kGuard);
				AT_PORTABLE_TEST_ASSERT(context, chars[i] == kGuard);
			}
		}
		return true;
	}

	bool TestDecodeExact(ATPortableTestContext& context) {
		const uint8 name = 0x80;
		const uint8 ch = 0x5a;
		uint8 output[2];
		AT_PORTABLE_TEST_ASSERT(context,
			ATAnticDecodeMode2(output, &name, &ch, 0xfffffffeu, 0xffffffffu, 0, 0xff) == 0);
		AT_PORTABLE_TEST_ASSERT(context,
			ATAnticDecodeMode2(output, &name, &ch, 0xfffffffdu, 0xffffffffu, 0, 0xff) == 0xffffffffu);

		for (uint32 count : kCounts) {
			if (!count) {
				AT_PORTABLE_TEST_ASSERT(context, ATAnticDecodeMode2(nullptr, nullptr, nullptr, 0, 0, 0, 0) == 0);
				AT_PORTABLE_TEST_ASSERT(context, ATAnticDecodeMode2(nullptr, nullptr, nullptr, 1, 1, 0, 0) == 1);
				AT_PORTABLE_TEST_ASSERT(context, ATAnticDecodeMode2(nullptr, nullptr, nullptr, 2, 1, 0, 0) == 2);
				continue;
			}
			std::vector<uint8> names(count), chars(count), output(count * 2);
			for (uint32 i = 0; i < count; ++i) {
				names[i] = (uint8)(i * 37);
				chars[i] = (uint8)(i * 113);
			}
			ATAnticDecodeMode2(output.data(), names.data(), chars.data(), 0, count * 2 - 1, 0xff, 0xff);
		}
		return true;
	}

	bool TestDecodeIndependentAlignments(ATPortableTestContext& context) {
		for (uint32 nameOffset = 0; nameOffset < 16; ++nameOffset) {
			for (uint32 charOffset = 0; charOffset < 16; ++charOffset) {
				for (uint32 dstOffset = 0; dstOffset < 16; ++dstOffset) {
					std::vector<uint8> names(nameOffset + 256, kGuard);
					std::vector<uint8> chars(charOffset + 256, kGuard);
					std::vector<uint8> output(dstOffset + 512 + 16, kGuard);
					for (uint32 i = 0; i < 256; ++i) {
						names[nameOffset + i] = (uint8)i;
						chars[charOffset + i] = (uint8)(i * 73 + 19);
					}
					for (uint8 inv : { (uint8)0, (uint8)0xff }) {
						for (uint8 blink : { (uint8)0, (uint8)0xff }) {
							std::fill(output.begin(), output.end(), kGuard);
							ATAnticDecodeMode2(output.data() + dstOffset, names.data() + nameOffset,
								chars.data() + charOffset, 0, 511, inv, blink);
							for (uint32 i = 0; i < 256; ++i) {
								const uint8 value = DecodeByte(names[nameOffset + i], chars[charOffset + i], inv, blink);
								AT_PORTABLE_TEST_ASSERT(context, output[dstOffset + i * 2] == (value >> 4));
								AT_PORTABLE_TEST_ASSERT(context, output[dstOffset + i * 2 + 1] == (value & 15));
							}
							for (uint32 i = 0; i < dstOffset; ++i)
								AT_PORTABLE_TEST_ASSERT(context, output[i] == kGuard);
							for (uint32 i = dstOffset + 512; i < output.size(); ++i)
								AT_PORTABLE_TEST_ASSERT(context, output[i] == kGuard);
						}
					}
				}
			}
		}
		return true;
	}

	bool TestDMA(ATPortableTestContext& context) {
		for (uint32 start = 0; start <= 116; ++start) {
			for (uint32 end = 0; end <= 120; ++end) {
				for (uint32 pattern : { 0u, 1u, 0x55u, 0xaau, 0xffu }) {
					alignas(128) uint8 actual[128];
					uint8 expected[128];
					for (uint32 i = 0; i < 128; ++i)
						actual[i] = expected[i] = (uint8)(i * 19 + 3);
					const uint32 cappedEnd = end > 115 ? 115 : end;
					if (cappedEnd > start) {
						for (uint32 i = start; i < cappedEnd; ++i) {
							if (pattern & (1u << (i & 7)))
								expected[i] |= 0x42;
						}
					}
					ATAnticSetDMACycles(actual, start, end, (uint8)pattern, 0x42);
					AT_PORTABLE_TEST_ASSERT(context, !memcmp(actual, expected, sizeof actual));
				}
			}
		}
		alignas(128) uint8 zero[128];
		memset(zero, 0x5a, sizeof zero);
		ATAnticSetDMACycles(zero, 8, 8, 0xff, 0xff);
		for (uint8 v : zero)
			AT_PORTABLE_TEST_ASSERT(context, v == 0x5a);
		return true;
	}

	void RefreshReference(uint8 *p) {
		int r = 24;
		for (int x = 25; x < 61; x += 4) {
			if (r >= x)
				continue;
			r = x;
			while (r < 107) {
				if (!(p[r] & 1)) {
					++p[r++];
					break;
				}
				++r;
			}
		}
	}

	bool TestRefreshAndClear(ATPortableTestContext& context) {
		for (uint32 seed = 0; seed < 256; ++seed) {
			alignas(128) uint8 actual[128];
			uint8 expected[128];
			for (uint32 i = 0; i < 128; ++i) {
				const uint8 value = seed == 0 ? 0xff : seed == 1 ? 0 : (uint8)(seed + i * 47);
				actual[i] = expected[i] = value;
			}
			RefreshReference(expected);
			ATAnticSetRefreshCycles(actual);
			AT_PORTABLE_TEST_ASSERT(context, !memcmp(actual, expected, sizeof actual));
			ATAnticClearDMACycles(actual);
			for (uint8 v : actual)
				AT_PORTABLE_TEST_ASSERT(context, v == 0);
		}

		// All refresh-eligible cycles are occupied, while padding after cycle 106
		// is free. Refresh must not escape the hardware-visible scan range.
		alignas(128) uint8 dense[128] = {};
		for (uint32 i = 25; i < 107; ++i)
			dense[i] = 1;
		uint8 expected[128];
		memcpy(expected, dense, sizeof dense);
		RefreshReference(expected);
		ATAnticSetRefreshCycles(dense);
		AT_PORTABLE_TEST_ASSERT(context, !memcmp(dense, expected, sizeof dense));
		return true;
	}
}

bool ATTestAltirraANTICKernels(ATPortableTestContext& context) {
	for (uint32 offset = 0; offset < 16; ++offset) {
		for (uint32 count : kCounts) {
			for (uint8 inv : { (uint8)0, (uint8)0xff }) {
				for (uint8 blink : { (uint8)0, (uint8)0xff }) {
					if (!TestDecode(context, offset, count, inv, blink))
						return false;
				}
			}
		}
		for (uint8 inv : { (uint8)1, (uint8)0x55, (uint8)0x80 }) {
			for (uint8 blink : { (uint8)1, (uint8)0x55, (uint8)0x80 }) {
				if (!TestDecode(context, offset, 17, inv, blink))
					return false;
			}
		}
	}
	if (!TestDecodeExact(context) || !TestDecodeIndependentAlignments(context)
		|| !TestDMA(context) || !TestRefreshAndClear(context))
		return false;
	return true;
}
