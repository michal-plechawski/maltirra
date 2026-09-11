// Altirra portable checksum and SHA-256 tests

#include <algorithm>
#include <array>
#include <iterator>
#include <vector>

#include <at/atcore/checksum.h>
#include <at/attest/portabletest.h>
#include <vd2/system/cpuaccel.h>

namespace {
	int ATHexDigit(char c) {
		if (c >= '0' && c <= '9')
			return c - '0';
		if (c >= 'a' && c <= 'f')
			return c - 'a' + 10;
		if (c >= 'A' && c <= 'F')
			return c - 'A' + 10;

		return -1;
	}

	bool ATDigestMatches(const ATChecksumSHA256& digest, const char *hex) {
		for(size_t i = 0; i < std::size(digest.mDigest); ++i) {
			const int high = ATHexDigit(hex[i * 2]);
			const int low = ATHexDigit(hex[i * 2 + 1]);

			if (high < 0 || low < 0
				|| digest.mDigest[i] != static_cast<uint8>((high << 4) | low))
				return false;
		}

		return !hex[std::size(digest.mDigest) * 2];
	}

	bool ATCheckSHA256Vectors() {
		static constexpr char kLongMessage[] =
			"abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";

		if (!ATDigestMatches(
			ATComputeChecksumSHA256("", 0),
			"e3b0c44298fc1c149afbf4c8996fb924"
			"27ae41e4649b934ca495991b7852b855"))
			return false;

		if (!ATDigestMatches(
			ATComputeChecksumSHA256("abc", 3),
			"ba7816bf8f01cfea414140de5dae2223"
			"b00361a396177a9cb410ff61f20015ad"))
			return false;

		if (!ATDigestMatches(
			ATComputeChecksumSHA256(kLongMessage, sizeof kLongMessage - 1),
			"248d6a61d20638b8e5c026930c3e6039"
			"a33ce45964ff2167f6ecedd419db06c1"))
			return false;

		std::vector<uint8> millionAs(1000000, static_cast<uint8>('a'));
		if (!ATDigestMatches(
			ATComputeChecksumSHA256(millionAs.data(), millionAs.size()),
			"cdc76e5c9914fb9281a1c7e284d73e67"
			"f1809a48a497200e046d39ccc7112cd0"))
			return false;

		std::array<uint8, 257> payload {};
		for(size_t i = 0; i < payload.size(); ++i)
			payload[i] = static_cast<uint8>(i * i + 31 * i + 7);

		const ATChecksumSHA256 expected = ATComputeChecksumSHA256(
			payload.data(), payload.size());
		if (!ATDigestMatches(
			expected,
			"77f28c443f47d7958db0367bc2952a4f"
			"8c4e51a9938b50767644bd66c41a29bf"))
			return false;

		for(size_t chunkSize = 1; chunkSize <= 73; ++chunkSize) {
			ATChecksumEngineSHA256 engine;
			engine.Process(nullptr, 0);

			for(size_t offset = 0; offset < payload.size();) {
				const size_t length = std::min(chunkSize, payload.size() - offset);
				engine.Process(payload.data() + offset, length);
				offset += length;
			}

			if (engine.Finalize() != expected)
				return false;
		}

		ATChecksumEngineSHA256 reusableEngine;
		reusableEngine.Process("abc", 3);
		if (!ATDigestMatches(
			reusableEngine.Finalize(),
			"ba7816bf8f01cfea414140de5dae2223"
			"b00361a396177a9cb410ff61f20015ad"))
			return false;

		reusableEngine.Reset();
		reusableEngine.Process(payload.data(), payload.size());
		if (reusableEngine.Finalize() != expected)
			return false;

		ATChecksumSHA256 different = expected;
		different.mDigest[31] ^= 1;
		return different != expected && !(different == expected);
	}

	struct ATCPUExtensionStateGuard {
		long mFlags;

		~ATCPUExtensionStateGuard() {
			CPUEnableExtensions(mFlags);
		}
	};
}

bool ATTestCoreChecksum(ATPortableTestContext& context) {
	static constexpr uint8 kOffsetBytes[] = {
		0xEF, 0xCD, 0xAB, 0x89, 0x67, 0x45, 0x23, 0x01
	};
	static constexpr uint8 kZeros[257] {};

	AT_PORTABLE_TEST_ASSERT(context,
		ATComputeBlockChecksum(kATBaseChecksum, "hello", 5)
			== UINT64_C(0x7B495389BDBDD4C7));
	AT_PORTABLE_TEST_ASSERT(context,
		ATComputeZeroBlockChecksum(kATBaseChecksum, std::size(kZeros))
			== UINT64_C(0x86062D4C200833DF));
	AT_PORTABLE_TEST_ASSERT(context,
		ATComputeBlockChecksum(kATBaseChecksum, kZeros, std::size(kZeros))
			== ATComputeZeroBlockChecksum(kATBaseChecksum, std::size(kZeros)));
	AT_PORTABLE_TEST_ASSERT(context,
		ATComputeOffsetChecksum(UINT64_C(0x0123456789ABCDEF))
			== UINT64_C(0x4C66A756F98346A5));
	AT_PORTABLE_TEST_ASSERT(context,
		ATComputeOffsetChecksum(UINT64_C(0x0123456789ABCDEF))
			== ATComputeBlockChecksum(
				kATBaseChecksum, kOffsetBytes, std::size(kOffsetBytes)));

	const ATCPUExtensionStateGuard extensionGuard {
		CPUGetEnabledExtensions()
	};
	const long detectedExtensions = CPUCheckForExtensions();

	CPUEnableExtensions(0);
	AT_PORTABLE_TEST_ASSERT(context, ATCheckSHA256Vectors());

	CPUEnableExtensions(detectedExtensions);
	AT_PORTABLE_TEST_ASSERT(context, ATCheckSHA256Vectors());

	return true;
}
