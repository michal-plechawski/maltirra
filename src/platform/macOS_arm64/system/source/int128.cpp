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

#include <math.h>

#include <vd2/system/int128.h>

namespace {
	using NativeUInt128 = unsigned __int128;
	static_assert(sizeof(NativeUInt128) == 16);

	NativeUInt128 ToNative(uint64 hi, uint64 lo) {
		return (NativeUInt128)hi << 64 | lo;
	}

	vduint128 ToVDUnsigned(NativeUInt128 value) {
		return vduint128((uint64)(value >> 64), (uint64)value);
	}

	vdint128 ToVDSignedBits(NativeUInt128 value) {
		return vdint128((sint64)(uint64)(value >> 64), (uint64)value);
	}
}

void vdasm_uint128_add(uint64 dst[2], const uint64 x[2], const uint64 y[2]) {
	const uint64 x0 = x[0];
	const uint64 x1 = x[1];
	const uint64 y0 = y[0];
	const uint64 y1 = y[1];

	dst[0] = x0 + y0;
	dst[1] = x1 + y1 + (dst[0] < x0);
}

void vdasm_uint128_sub(uint64 dst[2], const uint64 x[2], const uint64 y[2]) {
	const uint64 x0 = x[0];
	const uint64 x1 = x[1];
	const uint64 y0 = y[0];
	const uint64 y1 = y[1];

	dst[0] = x0 - y0;
	dst[1] = x1 - y1 - (dst[0] > x0);
}

void vdint128::setSquare(sint64 value) {
	uint64 magnitude = (uint64)value;
	if (value < 0)
		magnitude = ~magnitude + 1;

	const NativeUInt128 result = (NativeUInt128)magnitude * magnitude;
	q[0] = (uint64)result;
	q[1] = (sint64)(uint64)(result >> 64);
}

const vdint128 vdint128::operator<<(int count) const {
	if (count >= 128)
		return vdint128(0);

	return ToVDSignedBits(ToNative((uint64)q[1], (uint64)q[0]) << count);
}

const vdint128 vdint128::operator>>(int count) const {
	if (count >= 128)
		return vdint128(q[1] < 0 ? -1 : 0);

	if (count >= 64)
		return vdint128(q[1] >> 63, (uint64)(q[1] >> (count - 64)));

	if (!count)
		return *this;

	return vdint128(
		q[1] >> count,
		((uint64)q[0] >> count) | ((uint64)q[1] << (64 - count)));
}

const vduint128 vduint128::operator<<(int count) const {
	if (count >= 128)
		return vduint128(0U);

	return ToVDUnsigned(ToNative(q[1], q[0]) << count);
}

const vduint128 vduint128::operator>>(int count) const {
	if (count >= 128)
		return vduint128(0U);

	return ToVDUnsigned(ToNative(q[1], q[0]) >> count);
}

const vdint128 vdint128::operator*(const vdint128& value) const {
	const NativeUInt128 result = ToNative((uint64)q[1], (uint64)q[0])
		* ToNative((uint64)value.q[1], (uint64)value.q[0]);
	return ToVDSignedBits(result);
}

const vdint128 vdint128::operator/(int divisor) const {
	NativeUInt128 magnitude = ToNative((uint64)q[1], (uint64)q[0]);
	const bool negativeDividend = q[1] < 0;
	const bool negativeDivisor = divisor < 0;

	if (negativeDividend)
		magnitude = -magnitude;

	const uint64 divisorMagnitude = negativeDivisor
		? (uint64)(-(sint64)divisor)
		: (uint64)divisor;
	NativeUInt128 result = magnitude / divisorMagnitude;

	if (negativeDividend != negativeDivisor)
		result = -result;

	return ToVDSignedBits(result);
}

vdint128::operator double() const {
	NativeUInt128 magnitude = ToNative((uint64)q[1], (uint64)q[0]);
	const bool negative = q[1] < 0;

	if (negative)
		magnitude = -magnitude;

	const double result = (double)(uint64)magnitude
		+ ldexp((double)(uint64)(magnitude >> 64), 64);
	return negative ? -result : result;
}

const vduint128 vduint128::operator*(const vduint128& value) const {
	return ToVDUnsigned(ToNative(q[1], q[0]) * ToNative(value.q[1], value.q[0]));
}

const vduint128 vduint128::operator/(uint32 divisor) const {
	return ToVDUnsigned(ToNative(q[1], q[0]) / divisor);
}

const vduint128 vduint128::operator/(const vduint128& divisor) const {
	return ToVDUnsigned(ToNative(q[1], q[0]) / ToNative(divisor.q[1], divisor.q[0]));
}

vduint128 VDUMul64x64To128(uint64 x, uint64 y) {
	return ToVDUnsigned((NativeUInt128)x * y);
}

uint64 VDUDiv128x64To64(const vduint128& dividend, uint64 divisor, uint64& remainder) {
	const NativeUInt128 nativeDividend = ToNative(dividend.q[1], dividend.q[0]);
	remainder = (uint64)(nativeDividend % divisor);
	return (uint64)(nativeDividend / divisor);
}
