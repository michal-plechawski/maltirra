// Altirra portable 128-bit integer tests

#include <limits>
#include <vd2/system/int128.h>
#include <at/attest/portabletest.h>

namespace {
	vduint128 SlowMultiply(vduint128 x, vduint128 y) {
		vduint128 result(0U);

		for(int i = 0; i < 128; ++i) {
			if (y.getLo() & 1)
				result += x;

			x <<= 1;
			y >>= 1;
		}

		return result;
	}

	uint64 NextRandom(uint64& state) {
		state ^= state << 13;
		state ^= state >> 7;
		state ^= state << 17;
		return state;
	}
}

bool ATTestSystemInt128(ATPortableTestContext& context) {
	AT_PORTABLE_TEST_ASSERT(context,
		vduint128(UINT64_C(0x0000000080000000), UINT64_C(0x8000000080000000))
		+ vduint128(UINT64_C(0x0000000080000000), UINT64_C(0x8000000080000000))
		== vduint128(UINT64_C(0x0000000100000001), UINT64_C(0x0000000100000000)));
	AT_PORTABLE_TEST_ASSERT(context,
		vduint128(0U) - vduint128(1U)
		== vduint128(UINT64_MAX, UINT64_MAX));

	for(int bit = 0; bit < 128; ++bit) {
		const vduint128 value = vduint128(1U) << bit;
		if (bit < 64) {
			AT_PORTABLE_TEST_ASSERT(context, value.getLo() == (UINT64_C(1) << bit));
			AT_PORTABLE_TEST_ASSERT(context, value.getHi() == 0);
		} else {
			AT_PORTABLE_TEST_ASSERT(context, value.getLo() == 0);
			AT_PORTABLE_TEST_ASSERT(context, value.getHi() == (UINT64_C(1) << (bit - 64)));
		}
		AT_PORTABLE_TEST_ASSERT(context, (value >> bit) == vduint128(1U));
	}

	AT_PORTABLE_TEST_ASSERT(context, (vduint128(1U) << 128) == vduint128(0U));
	AT_PORTABLE_TEST_ASSERT(context, (vduint128(UINT64_MAX, UINT64_MAX) >> 128) == vduint128(0U));
	AT_PORTABLE_TEST_ASSERT(context, (vdint128(-1) >> 128) == vdint128(-1));
	AT_PORTABLE_TEST_ASSERT(context, (vdint128(-1) << 128) == vdint128(0));
	AT_PORTABLE_TEST_ASSERT(context,
		(vdint128((sint64)UINT64_C(0x8000000000000000), 0) >> 64)
		== vdint128(-1, UINT64_C(0x8000000000000000)));

	AT_PORTABLE_TEST_ASSERT(context, vduint128(0U) < vduint128(1U));
	AT_PORTABLE_TEST_ASSERT(context, vduint128(1U) < vduint128(1, 0));
	AT_PORTABLE_TEST_ASSERT(context, vdint128(-2) < vdint128(-1));
	AT_PORTABLE_TEST_ASSERT(context, vdint128(-1) < vdint128(0));
	AT_PORTABLE_TEST_ASSERT(context, vdint128(1) > vdint128(0));

	AT_PORTABLE_TEST_ASSERT(context, VDUMul64x64To128(3, 7) == vduint128(21U));
	AT_PORTABLE_TEST_ASSERT(context,
		VDUMul64x64To128(UINT64_C(0x123456789ABCDEF0), UINT64_C(0xBAADF00DDEADBEEF))
		== vduint128(UINT64_C(0x0D4665441D7CFEBC), UINT64_C(0xD182EA976BFA4210)));
	AT_PORTABLE_TEST_ASSERT(context,
		VDUMul64x64To128(UINT64_MAX, UINT64_MAX)
		== vduint128(UINT64_C(0xFFFFFFFFFFFFFFFE), UINT64_C(1)));

	uint64 randomState = UINT64_C(0xE7037ED1A0B428DB);
	for(int i = 0; i < 2048; ++i) {
		const vduint128 x(NextRandom(randomState), NextRandom(randomState));
		const vduint128 y(NextRandom(randomState), NextRandom(randomState));
		AT_PORTABLE_TEST_ASSERT(context, x * y == SlowMultiply(x, y));
	}

	AT_PORTABLE_TEST_ASSERT(context,
		vduint128(UINT64_C(1), UINT64_C(0)) / vduint128(3U)
		== vduint128(UINT64_C(0), UINT64_C(0x5555555555555555)));
	AT_PORTABLE_TEST_ASSERT(context,
		vduint128(UINT64_C(0x8000000000000000), UINT64_C(0)) / vduint128(2U)
		== vduint128(UINT64_C(0x4000000000000000), UINT64_C(0)));
	AT_PORTABLE_TEST_ASSERT(context,
		vduint128(5U) / vduint128(UINT64_C(1), UINT64_C(0)) == vduint128(0U));
	AT_PORTABLE_TEST_ASSERT(context,
		vduint128(UINT64_MAX, UINT64_MAX) / UINT32_C(0xFFFFFFFF)
		== vduint128(UINT64_C(0x0000000100000001), UINT64_C(0x0000000100000001)));

	uint64 remainder = 0;
	AT_PORTABLE_TEST_ASSERT(context,
		VDUDiv128x64To64(vduint128(27U), 7, remainder) == 3 && remainder == 6);
	AT_PORTABLE_TEST_ASSERT(context,
		VDUDiv128x64To64(
			vduint128(UINT64_C(0x123456789ABCDEF0), UINT64_C(0xBAADF00DDEADBEEF)),
			UINT64_C(0xFEDCBA9876543210), remainder) == UINT64_C(0x1249249249249238)
		&& remainder == UINT64_C(0xA72CB7FA5D75AB6F));

	vdint128 square;
	square.setSquare(-3);
	AT_PORTABLE_TEST_ASSERT(context, square == vdint128(9));
	square.setSquare(std::numeric_limits<sint64>::min());
	AT_PORTABLE_TEST_ASSERT(context,
		square == vdint128((sint64)UINT64_C(0x4000000000000000), UINT64_C(0)));
	AT_PORTABLE_TEST_ASSERT(context, vdint128(-100) / 3 == vdint128(-33));
	AT_PORTABLE_TEST_ASSERT(context, vdint128(-100) / -3 == vdint128(33));
	AT_PORTABLE_TEST_ASSERT(context,
		vdint128(-1, UINT64_C(0)) / 2 == vdint128(-1, UINT64_C(0x8000000000000000)));
	AT_PORTABLE_TEST_ASSERT(context,
		vdint128(-3) * vdint128(7) == vdint128(-21));
	AT_PORTABLE_TEST_ASSERT(context, (double)vdint128(-42) == -42.0);
	AT_PORTABLE_TEST_ASSERT(context,
		(double)vdint128(1, 0) == 18446744073709551616.0);

	return true;
}
