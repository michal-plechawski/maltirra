//	Altirra - Atari 800/800XL/5200 emulator
//	Copyright (C) 2009-2021 Avery Lee
//
//	This program is free software; you can redistribute it and/or modify
//	it under the terms of the GNU General Public License as published by
//	the Free Software Foundation; either version 2 of the License, or
//	(at your option) any later version.

#include <array>
#include <cstring>

#include <CoreFoundation/CoreFoundation.h>
#include <Security/Security.h>

#include <at/atcore/checksum.h>

namespace {
	// ASN.1 DER RSAPublicKey containing the same exponent and modulus as the
	// BCRYPT_RSAPUBLIC_BLOB used by updatefeed_win32.cpp.
	constexpr uint8 kPublicKey[] {
		0x30,0x82,0x01,0x0A,0x02,0x82,0x01,0x01,0x00,
		0xC7,0xD7,0x36,0xE5,0x7F,0xE4,0x1A,0x3E,0x3C,0x88,0x0D,0x7C,0xD0,0x40,0xE0,0x82,
		0xD7,0xEA,0x84,0x90,0xDE,0x64,0x09,0xB8,0x58,0xF9,0x0E,0xFF,0x11,0x0F,0x61,0x26,
		0x71,0x4C,0x36,0x9A,0xA3,0x7F,0x04,0x95,0xB1,0x75,0x56,0x72,0x24,0x3D,0x25,0xD1,
		0x83,0x3D,0x0A,0x85,0xFB,0x8C,0xD1,0x14,0x09,0x8A,0x4E,0x2B,0x53,0x7C,0x85,0x80,
		0xE9,0x59,0x06,0xF7,0x11,0x19,0x21,0xCB,0x36,0xFF,0x19,0x3C,0xF4,0xC6,0x70,0x65,
		0x3A,0xA8,0x38,0xF8,0xDA,0x1B,0xFD,0xD5,0xEF,0x9B,0xC7,0x60,0x28,0xAA,0x0B,0xDE,
		0x32,0x13,0xE1,0x8C,0xB5,0x78,0x18,0x8D,0x78,0x7D,0x18,0x5B,0xF8,0x94,0xCB,0xBD,
		0x79,0x56,0x03,0xE0,0x6F,0xCF,0xE7,0xF0,0x87,0xC5,0xCC,0x91,0xA2,0xD0,0xBF,0xE1,
		0x7D,0xDA,0xA5,0x53,0x1D,0xD9,0xC7,0xF5,0xFB,0x65,0x0C,0x15,0x18,0x22,0x04,0x9F,
		0xC1,0xEC,0x46,0x16,0xD1,0x82,0xF3,0x41,0x73,0x6B,0x69,0x55,0xDA,0x1C,0xBA,0xDD,
		0x2F,0xD0,0x88,0x1A,0x4E,0x03,0x91,0x71,0x65,0x87,0x6F,0x30,0x96,0xC7,0xCA,0x24,
		0x7B,0x7B,0x57,0xBD,0x75,0xC5,0x51,0xE3,0x96,0x2B,0xB3,0x44,0xF1,0x09,0x3E,0x6F,
		0xA6,0xE1,0x3F,0x84,0x04,0xE0,0x40,0x97,0x9E,0xD9,0x20,0x3D,0xA6,0xF1,0x6E,0x5D,
		0x27,0x37,0x82,0xAB,0xC3,0xA4,0x8A,0x3B,0xC7,0xB7,0xB2,0xE0,0xB6,0xBC,0x71,0x44,
		0x40,0x4B,0x9D,0x6B,0x98,0x6A,0x9E,0x2F,0xF6,0x82,0x59,0x93,0x2C,0x2E,0x27,0x67,
		0xCC,0x1F,0x9A,0xC8,0x0A,0x9F,0xC2,0x0E,0x7D,0x1D,0x6A,0xFB,0xB3,0xC6,0xB1,0x29,
		0x02,0x03,0x01,0x00,0x01
	};

	bool VerifyPSSSHA256Salt8(const uint8 *encodedMessage, const ATChecksumSHA256& messageDigest) {
		constexpr size_t kEncodedMessageSize = 256;
		constexpr size_t kDigestSize = sizeof(ATChecksumSHA256::mDigest);
		constexpr size_t kSaltSize = 8;
		constexpr size_t kDataBlockSize = kEncodedMessageSize - kDigestSize - 1;
		constexpr size_t kPaddingSize = kDataBlockSize - kSaltSize - 1;

		if ((encodedMessage[0] & 0x80) || encodedMessage[kEncodedMessageSize - 1] != 0xBC)
			return false;

		const uint8 *hash = encodedMessage + kDataBlockSize;
		std::array<uint8, kDataBlockSize> dataBlock {};
		std::array<uint8, kDigestSize + 4> maskInput {};
		memcpy(maskInput.data(), hash, kDigestSize);

		for(uint32 counter = 0, offset = 0; offset < kDataBlockSize; ++counter) {
			maskInput[kDigestSize + 0] = (uint8)(counter >> 24);
			maskInput[kDigestSize + 1] = (uint8)(counter >> 16);
			maskInput[kDigestSize + 2] = (uint8)(counter >> 8);
			maskInput[kDigestSize + 3] = (uint8)counter;

			const ATChecksumSHA256 maskDigest = ATComputeChecksumSHA256(maskInput.data(), maskInput.size());
			for(size_t i = 0; i < kDigestSize && offset < kDataBlockSize; ++i, ++offset)
				dataBlock[offset] = encodedMessage[offset] ^ maskDigest.mDigest[i];
		}

		dataBlock[0] &= 0x7F;
		for(size_t i = 0; i < kPaddingSize; ++i) {
			if (dataBlock[i])
				return false;
		}

		if (dataBlock[kPaddingSize] != 1)
			return false;

		std::array<uint8, 8 + kDigestSize + kSaltSize> hashInput {};
		memcpy(hashInput.data() + 8, messageDigest.mDigest, kDigestSize);
		memcpy(hashInput.data() + 8 + kDigestSize, dataBlock.data() + kPaddingSize + 1, kSaltSize);
		const ATChecksumSHA256 expectedHash = ATComputeChecksumSHA256(hashInput.data(), hashInput.size());

		return !memcmp(expectedHash.mDigest, hash, kDigestSize);
	}
}

bool ATUpdateVerifyFeedSignature(const void *signature, const void *data, size_t len) {
	const ATChecksumSHA256 digest = ATComputeChecksumSHA256(data, len);

	CFDataRef keyData = CFDataCreate(kCFAllocatorDefault, kPublicKey, sizeof kPublicKey);
	if (!keyData)
		return false;

	const void *attributeKeys[] { kSecAttrKeyType, kSecAttrKeyClass };
	const void *attributeValues[] { kSecAttrKeyTypeRSA, kSecAttrKeyClassPublic };
	CFDictionaryRef attributes = CFDictionaryCreate(
		kCFAllocatorDefault,
		attributeKeys,
		attributeValues,
		2,
		&kCFTypeDictionaryKeyCallBacks,
		&kCFTypeDictionaryValueCallBacks);

	CFErrorRef error = nullptr;
	SecKeyRef key = attributes ? SecKeyCreateWithData(keyData, attributes, &error) : nullptr;
	if (error) {
		CFRelease(error);
		error = nullptr;
	}

	CFRelease(keyData);
	if (attributes)
		CFRelease(attributes);

	if (!key)
		return false;

	CFDataRef signatureData = CFDataCreate(kCFAllocatorDefault, static_cast<const UInt8 *>(signature), 256);
	bool valid = false;

	if (signatureData) {
		CFDataRef encodedMessage = SecKeyCreateEncryptedData(
			key,
			kSecKeyAlgorithmRSAEncryptionRaw,
			signatureData,
			&error);

		if (encodedMessage) {
			if (CFDataGetLength(encodedMessage) == 256)
				valid = VerifyPSSSHA256Salt8(CFDataGetBytePtr(encodedMessage), digest);

			CFRelease(encodedMessage);
		}
	}

	if (error)
		CFRelease(error);
	if (signatureData)
		CFRelease(signatureData);
	CFRelease(key);

	return valid;
}
