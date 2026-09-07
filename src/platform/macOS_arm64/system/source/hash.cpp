//	VirtualDub - Video processing and capture application
//	System library component - hashing module
//	Copyright (C) 1998-2014 Avery Lee, All Rights Reserved.
//
//	Beginning with 1.6.0, the VirtualDub system library is licensed
//	differently than the remainder of VirtualDub.  This particular file is
//	thus licensed as follows (the "zlib" license):
//
//	This software is provided 'as-is', without any express or implied
//	warranty.  In no event will the authors be held liable for any
//	damages arising from the use of this software.
//
//	Permission is granted to anyone to use this software for any purpose,
//	including commercial applications, and to alter it and redistribute it
//	freely, subject to the following restrictions:
//
//	1.	The origin of this software must not be misrepresented; you must
//		not claim that you wrote the original software. If you use this
//		software in a product, an acknowledgment in the product
//		documentation would be appreciated but is not required.
//	2.	Altered source versions must be plainly marked as such, and must
//		not be misrepresented as being the original software.
//	3.	This notice may not be removed or altered from any source
//		distribution.

#include <string.h>
#include <wchar.h>
#include <vd2/system/hash.h>
#include <vd2/system/binary.h>
#include <vd2/system/int128.h>

uint32 VDHashString32(const char *s) {
	return VDHashString32(s, (uint32)strlen(s));
}

uint32 VDHashString32(const char *s, uint32 len) {
	uint32 hash = UINT32_C(2166136261);

	for(uint32 i = 0; i < len; ++i) {
		hash *= UINT32_C(16777619);
		hash ^= (unsigned char)s[i];
	}

	return hash;
}

uint32 VDHashString32(const wchar_t *s) {
	return VDHashString32(s, (uint32)wcslen(s));
}

uint32 VDHashString32(const wchar_t *s, uint32 len) {
	uint32 hash = UINT32_C(2166136261);

	for(uint32 i = 0; i < len; ++i) {
		hash *= UINT32_C(16777619);
		hash ^= (unsigned)s[i];
	}

	return hash;
}

uint32 VDHashString32I(const char *s) {
	return VDHashString32I(s, (uint32)strlen(s));
}

uint32 VDHashString32I(const char *s, uint32 len) {
	uint32 hash = UINT32_C(2166136261);

	for(uint32 i = 0; i < len; ++i) {
		uint32 c = (unsigned char)*s++;
		if (c >= 'A' && c <= 'Z')
			c += 'a' - 'A';

		hash *= UINT32_C(16777619);
		hash ^= c;
	}

	return hash;
}

uint32 VDHashString32I(const wchar_t *s) {
	return VDHashString32I(s, (uint32)wcslen(s));
}

uint32 VDHashString32I(const wchar_t *s, uint32 len) {
	uint32 hash = UINT32_C(2166136261);

	for(uint32 i = 0; i < len; ++i) {
		uint32 c = (uint32)*s++;
		if (c >= L'A' && c <= L'Z')
			c += L'a' - L'A';

		hash *= UINT32_C(16777619);
		hash ^= c;
	}

	return hash;
}

#define ROTL32(value, bits) ((value << bits) | (value >> (32 - bits)))
#define FMIX32(h) \
	h ^= h >> 16; \
	h *= UINT32_C(0x85EBCA6B); \
	h ^= h >> 13; \
	h *= UINT32_C(0xC2B2AE35); \
	h ^= h >> 16

vduint128 VDHash128(const void *data0, size_t len) {
	const uint8 *data = (const uint8 *)data0;
	const int blockCount = (int)(len >> 4);
	uint32 h1 = 0;
	uint32 h2 = 0;
	uint32 h3 = 0;
	uint32 h4 = 0;
	const uint32 c1 = UINT32_C(0x239B961B);
	const uint32 c2 = UINT32_C(0xAB0E9789);
	const uint32 c3 = UINT32_C(0x38B34AE5);
	const uint32 c4 = UINT32_C(0xA1E38B93);
	const uint8 *tail = blockCount ? data + blockCount * 16 : data;

	for(ptrdiff_t offset = -(ptrdiff_t)(blockCount << 4); offset; offset += 16) {
		uint32 k1 = VDReadUnalignedU32(tail + offset);
		uint32 k2 = VDReadUnalignedU32(tail + offset + 4);
		uint32 k3 = VDReadUnalignedU32(tail + offset + 8);
		uint32 k4 = VDReadUnalignedU32(tail + offset + 12);

		k1 *= c1; k1 = ROTL32(k1, 15); k1 *= c2; h1 ^= k1;
		h1 = ROTL32(h1, 19); h1 += h2; h1 = h1 * 5 + UINT32_C(0x561CCD1B);
		k2 *= c2; k2 = ROTL32(k2, 16); k2 *= c3; h2 ^= k2;
		h2 = ROTL32(h2, 17); h2 += h3; h2 = h2 * 5 + UINT32_C(0x0BCAA747);
		k3 *= c3; k3 = ROTL32(k3, 17); k3 *= c4; h3 ^= k3;
		h3 = ROTL32(h3, 15); h3 += h4; h3 = h3 * 5 + UINT32_C(0x96CD1C35);
		k4 *= c4; k4 = ROTL32(k4, 18); k4 *= c1; h4 ^= k4;
		h4 = ROTL32(h4, 13); h4 += h1; h4 = h4 * 5 + UINT32_C(0x32AC3B17);
	}

	uint32 k1 = 0;
	uint32 k2 = 0;
	uint32 k3 = 0;
	uint32 k4 = 0;

	switch(len & 15) {
	case 15: k4 ^= (uint32)tail[14] << 16;
	case 14: k4 ^= (uint32)tail[13] << 8;
	case 13: k4 ^= tail[12];
		k4 *= c4; k4 = ROTL32(k4, 18); k4 *= c1; h4 ^= k4;
	case 12: k3 ^= (uint32)tail[11] << 24;
	case 11: k3 ^= (uint32)tail[10] << 16;
	case 10: k3 ^= (uint32)tail[9] << 8;
	case 9: k3 ^= tail[8];
		k3 *= c3; k3 = ROTL32(k3, 17); k3 *= c4; h3 ^= k3;
	case 8: k2 ^= (uint32)tail[7] << 24;
	case 7: k2 ^= (uint32)tail[6] << 16;
	case 6: k2 ^= (uint32)tail[5] << 8;
	case 5: k2 ^= tail[4];
		k2 *= c2; k2 = ROTL32(k2, 16); k2 *= c3; h2 ^= k2;
	case 4: k1 ^= (uint32)tail[3] << 24;
	case 3: k1 ^= (uint32)tail[2] << 16;
	case 2: k1 ^= (uint32)tail[1] << 8;
	case 1: k1 ^= tail[0];
		k1 *= c1; k1 = ROTL32(k1, 15); k1 *= c2; h1 ^= k1;
	}

	h1 ^= (uint32)len;
	h2 ^= (uint32)len;
	h3 ^= (uint32)len;
	h4 ^= (uint32)len;
	h1 += h2; h1 += h3; h1 += h4;
	h2 += h1; h3 += h1; h4 += h1;
	FMIX32(h1);
	FMIX32(h2);
	FMIX32(h3);
	FMIX32(h4);
	h1 += h2; h1 += h3; h1 += h4;
	h2 += h1; h3 += h1; h4 += h1;

	vduint128 result;
	result.d[0] = h1;
	result.d[1] = h2;
	result.d[2] = h3;
	result.d[3] = h4;
	return result;
}

#undef FMIX32
#undef ROTL32
