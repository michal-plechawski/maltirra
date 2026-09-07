// Altirra portable hash tests

#include <vd2/system/hash.h>
#include <vd2/system/int128.h>
#include <at/attest/portabletest.h>

namespace {
	uint32 RotateLeft(uint32 value, int bits) {
		return value << bits | value >> (32 - bits);
	}

	uint32 FinalMix(uint32 value) {
		value ^= value >> 16;
		value *= UINT32_C(0x85EBCA6B);
		value ^= value >> 13;
		value *= UINT32_C(0xC2B2AE35);
		value ^= value >> 16;
		return value;
	}

	uint32 ReadLittleEndian(const uint8 *data) {
		return (uint32)data[0]
			| (uint32)data[1] << 8
			| (uint32)data[2] << 16
			| (uint32)data[3] << 24;
	}

	vduint128 ReferenceHash128(const uint8 *data, size_t length) {
		static constexpr uint32 kConstants[] = {
			UINT32_C(0x239B961B), UINT32_C(0xAB0E9789),
			UINT32_C(0x38B34AE5), UINT32_C(0xA1E38B93),
		};
		uint32 hashes[4] {};
		const size_t blockCount = length / 16;

		for(size_t block = 0; block < blockCount; ++block) {
			uint32 keys[] = {
				ReadLittleEndian(data + block * 16),
				ReadLittleEndian(data + block * 16 + 4),
				ReadLittleEndian(data + block * 16 + 8),
				ReadLittleEndian(data + block * 16 + 12),
			};
			static constexpr int kRotations[] = { 15, 16, 17, 18 };
			static constexpr int kHashRotations[] = { 19, 17, 15, 13 };
			static constexpr uint32 kAdds[] = {
				UINT32_C(0x561CCD1B), UINT32_C(0x0BCAA747),
				UINT32_C(0x96CD1C35), UINT32_C(0x32AC3B17),
			};

			for(int i = 0; i < 4; ++i) {
				keys[i] *= kConstants[i];
				keys[i] = RotateLeft(keys[i], kRotations[i]);
				keys[i] *= kConstants[(i + 1) & 3];
				hashes[i] ^= keys[i];
				hashes[i] = RotateLeft(hashes[i], kHashRotations[i]);
				hashes[i] += hashes[(i + 1) & 3];
				hashes[i] = hashes[i] * 5 + kAdds[i];
			}
		}

		uint32 tailKeys[4] {};
		const uint8 *tail = data + blockCount * 16;
		const size_t tailLength = length & 15;
		for(size_t i = 0; i < tailLength; ++i)
			tailKeys[i >> 2] |= (uint32)tail[i] << ((i & 3) * 8);

		for(int i = 3; i >= 0; --i) {
			if (tailLength > (size_t)i * 4) {
				tailKeys[i] *= kConstants[i];
				tailKeys[i] = RotateLeft(tailKeys[i], 15 + i);
				tailKeys[i] *= kConstants[(i + 1) & 3];
				hashes[i] ^= tailKeys[i];
			}
		}

		for(uint32& hash : hashes)
			hash ^= (uint32)length;
		hashes[0] += hashes[1] + hashes[2] + hashes[3];
		hashes[1] += hashes[0];
		hashes[2] += hashes[0];
		hashes[3] += hashes[0];
		for(uint32& hash : hashes)
			hash = FinalMix(hash);
		hashes[0] += hashes[1] + hashes[2] + hashes[3];
		hashes[1] += hashes[0];
		hashes[2] += hashes[0];
		hashes[3] += hashes[0];

		vduint128 result;
		for(int i = 0; i < 4; ++i)
			result.d[i] = hashes[i];
		return result;
	}
}

bool ATTestSystemHash(ATPortableTestContext& context) {
	AT_PORTABLE_TEST_ASSERT(context, VDHashString32("") == UINT32_C(0x811C9DC5));
	AT_PORTABLE_TEST_ASSERT(context, VDHashString32("a") == UINT32_C(0x050C5D7E));
	AT_PORTABLE_TEST_ASSERT(context, VDHashString32("Altirra") == UINT32_C(0x2C1EDD10));
	AT_PORTABLE_TEST_ASSERT(context, VDHashString32(L"Altirra") == UINT32_C(0x2C1EDD10));

	const char embeddedZero[] = { 'a', 'b', 'c', 0, 'd', 'e', 'f' };
	AT_PORTABLE_TEST_ASSERT(context,
		VDHashString32(embeddedZero, 7) == UINT32_C(0xF0A096E6));
	AT_PORTABLE_TEST_ASSERT(context,
		VDHashString32(embeddedZero) == VDHashString32(embeddedZero, 3));

	AT_PORTABLE_TEST_ASSERT(context,
		VDHashString32I("AlTiRrA") == UINT32_C(0xE609D030));
	AT_PORTABLE_TEST_ASSERT(context,
		VDHashString32I(L"AlTiRrA") == UINT32_C(0xE609D030));
	AT_PORTABLE_TEST_ASSERT(context,
		VDHashString32I("PREFIX-a", 6) == VDHashString32I("prefix-B", 6));
	const char nonAscii[] = { (char)0xC0, 0 };
	AT_PORTABLE_TEST_ASSERT(context, VDHashString32I(nonAscii) == VDHashString32(nonAscii));
	AT_PORTABLE_TEST_ASSERT(context, VDHashString32I(L"\u00C0") == VDHashString32(L"\u00C0"));
	static_assert(VDHashString32IC("AlTiRrA") == UINT32_C(0xE609D030));

	const vduint128 emptyHash = VDHash128(nullptr, 0);
	AT_PORTABLE_TEST_ASSERT(context, emptyHash == vduint128(0U));
	const vduint128 fooHash = VDHash128("foo", 3);
	AT_PORTABLE_TEST_ASSERT(context,
		fooHash.d[0] == UINT32_C(0x577C1B25)
		&& fooHash.d[1] == UINT32_C(0x60B62565)
		&& fooHash.d[2] == UINT32_C(0x60B62565)
		&& fooHash.d[3] == UINT32_C(0x60B62565));
	uint8 sequential[31];
	for(size_t i = 0; i < 31; ++i)
		sequential[i] = (uint8)(i + 1);
	const vduint128 sequential15 = VDHash128(sequential, 15);
	AT_PORTABLE_TEST_ASSERT(context,
		sequential15.d[0] == UINT32_C(0x4B54782F)
		&& sequential15.d[1] == UINT32_C(0xD463959A)
		&& sequential15.d[2] == UINT32_C(0x779B8D59)
		&& sequential15.d[3] == UINT32_C(0xB78C1A7B));
	const vduint128 sequential16 = VDHash128(sequential, 16);
	AT_PORTABLE_TEST_ASSERT(context,
		sequential16.d[0] == UINT32_C(0x121B035E)
		&& sequential16.d[1] == UINT32_C(0x3FF275B7)
		&& sequential16.d[2] == UINT32_C(0x8B693961)
		&& sequential16.d[3] == UINT32_C(0x7769EAE2));
	const vduint128 sequential31 = VDHash128(sequential, 31);
	AT_PORTABLE_TEST_ASSERT(context,
		sequential31.d[0] == UINT32_C(0x5C3EC2E2)
		&& sequential31.d[1] == UINT32_C(0xE5005FC5)
		&& sequential31.d[2] == UINT32_C(0x067A8F34)
		&& sequential31.d[3] == UINT32_C(0x4FCBE20A));

	uint8 storage[82] {};
	uint8 *unalignedData = storage + 1;
	for(size_t i = 0; i < 80; ++i)
		unalignedData[i] = (uint8)(i * 37 + 0x8B);

	for(size_t length = 0; length <= 80; ++length) {
		AT_PORTABLE_TEST_ASSERT(context,
			VDHash128(unalignedData, length) == ReferenceHash128(unalignedData, length));
	}

	return true;
}
