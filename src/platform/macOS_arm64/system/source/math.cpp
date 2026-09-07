//	VirtualDub - Video processing and capture application
//	System library component
//	Copyright (C) 1998-2004 Avery Lee, All Rights Reserved.
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

#include <bit>
#include <limits>
#include <vd2/system/math.h>
#include <vd2/system/int128.h>

int VDRoundToInt(float x) {
	return VDRoundToIntFast(x);
}

int VDRoundToInt(double x) {
	return VDRoundToIntFastFullRange(x);
}

sint32 VDRoundToInt32(float x) {
	return VDRoundToIntFast(x);
}

sint32 VDRoundToInt32(double x) {
	return VDRoundToIntFastFullRange(x);
}

sint64 VDRoundToInt64(float x) {
	return vcvtnd_s64_f64((double)x);
}

sint64 VDRoundToInt64(double x) {
	return vcvtnd_s64_f64(x);
}

sint64 VDFractionScale64(uint64 a, uint32 b, uint32 c, uint32& remainder) {
	using NativeUInt128 = unsigned __int128;

	const NativeUInt128 product = (NativeUInt128)a * b;
	if (!c || product / c > std::numeric_limits<uint64>::max())
		return -1;

	remainder = (uint32)(product % c);
	return std::bit_cast<sint64>((uint64)(product / c));
}

uint64 VDUMulDiv64x32(uint64 a, uint32 b, uint32 c) {
	uint32 remainder;
	return (uint64)VDFractionScale64(a, b, c, remainder);
}

sint64 VDMulDiv64(sint64 a, sint64 b, sint64 c) {
	const bool negative = ((a < 0) != (b < 0)) != (c < 0);
	const uint64 ua = a < 0 ? ~(uint64)a + 1 : (uint64)a;
	const uint64 ub = b < 0 ? ~(uint64)b + 1 : (uint64)b;
	const uint64 uc = c < 0 ? ~(uint64)c + 1 : (uint64)c;

	uint64 remainder;
	uint64 value = VDUDiv128x64To64(VDUMul64x64To128(ua, ub), uc, remainder);

	if (remainder >= uc - remainder)
		++value;

	return negative ? std::bit_cast<sint64>(~value + 1) : std::bit_cast<sint64>(value);
}

bool VDVerifyFiniteFloats(const float *values, uint32 count) {
	while(count--) {
		const uint32 value = std::bit_cast<uint32>(*values++);

		if ((value & UINT32_C(0x7FFFFFFF)) >= UINT32_C(0x7F800000))
			return false;
	}

	return true;
}

VDFastMathScope::VDFastMathScope() {
}

VDFastMathScope::~VDFastMathScope() {
}
