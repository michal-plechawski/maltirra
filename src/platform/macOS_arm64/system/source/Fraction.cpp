//	VirtualDub - Video processing and capture application
//	System library component
//	Copyright (C) 1998-2006 Avery Lee, All Rights Reserved.
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
#include <math.h>

#include <vd2/system/Fraction.h>
#include <vd2/system/vdtypes.h>
#include <vd2/system/math.h>

namespace {
	uint64 GetMagnitude(sint64 value) {
		return value < 0 ? ~(uint64)value + 1 : (uint64)value;
	}

	sint64 ApplySign(uint64 value, bool negative) {
		return std::bit_cast<sint64>(negative ? ~value + 1 : value);
	}
}

VDFraction::VDFraction(double d) {
	int xp;
	double mant = frexp(d, &xp);

	if (xp >= 33) {
		hi = 0xFFFFFFFF;
		lo = 1;
	} else if (xp < -31) {
		hi = 0;
		lo = 1;
	} else if (xp >= 0) {
		*this = reduce((uint64)(0.5 + ldexp(mant, 62)), 1ll << (62 - xp));
	} else {
		VDFraction t(1.0 / d);
		lo = t.hi;
		hi = t.lo;
	}
}

VDFraction VDFraction::reduce(uint64 numerator, uint64 denominator) {
	if (!denominator)
		return VDFraction(0, 0);

	if (!numerator)
		return VDFraction(0, 1);

	if (!(denominator >> 32) && numerator > (denominator << 32) - denominator)
		return VDFraction(UINT32_MAX, 1);

	uint64 n0 = 0;
	uint64 d0 = 1;
	uint32 n1 = 1;
	uint32 d1 = 0;
	uint64 previousRemainder = 0;
	uint32 bestNumerator;
	uint32 bestDenominator;

	for(;;) {
		const uint64 term = numerator / denominator;
		const uint64 remainder = numerator % denominator;
		const uint64 n2 = n0 + n1 * term;
		const uint64 d2 = d0 + d1 * term;
		const uint32 numeratorOverflow = (uint32)(n2 >> 32);
		const uint32 denominatorOverflow = (uint32)(d2 >> 32);

		if (numeratorOverflow | denominatorOverflow) {
			uint64 limitedTerm = term;

			if (numeratorOverflow)
				limitedTerm = (UINT32_MAX - n0) / n1;

			if (denominatorOverflow) {
				const uint64 denominatorTerm = (UINT32_MAX - d0) / d1;
				if (limitedTerm > denominatorTerm)
					limitedTerm = denominatorTerm;
			}

			if (limitedTerm * 2 < term
				|| (limitedTerm * 2 == term && d0 * previousRemainder <= remainder * d1))
				return VDFraction(bestNumerator, bestDenominator);

			return VDFraction(
				(uint32)(n0 + n1 * limitedTerm),
				(uint32)(d0 + d1 * limitedTerm));
		}

		bestNumerator = (uint32)n2;
		bestDenominator = (uint32)d2;

		if (!remainder)
			return VDFraction(bestNumerator, bestDenominator);

		n0 = n1;
		n1 = (uint32)n2;
		d0 = d1;
		d1 = (uint32)d2;
		previousRemainder = remainder;
		numerator = denominator;
		denominator = remainder;
	}
}

bool VDFraction::operator==(VDFraction value) const {
	return (uint64)hi * value.lo == (uint64)lo * value.hi;
}

bool VDFraction::operator!=(VDFraction value) const {
	return (uint64)hi * value.lo != (uint64)lo * value.hi;
}

bool VDFraction::operator<(VDFraction value) const {
	return (uint64)hi * value.lo < (uint64)lo * value.hi;
}

bool VDFraction::operator<=(VDFraction value) const {
	return (uint64)hi * value.lo <= (uint64)lo * value.hi;
}

bool VDFraction::operator>(VDFraction value) const {
	return (uint64)hi * value.lo > (uint64)lo * value.hi;
}

bool VDFraction::operator>=(VDFraction value) const {
	return (uint64)hi * value.lo >= (uint64)lo * value.hi;
}

VDFraction VDFraction::operator*(VDFraction value) const {
	return reduce((uint64)hi * value.hi, (uint64)lo * value.lo);
}

VDFraction VDFraction::operator/(VDFraction value) const {
	return reduce((uint64)hi * value.lo, (uint64)lo * value.hi);
}

VDFraction VDFraction::operator*(uint32 value) const {
	return reduce((uint64)hi * value, lo);
}

VDFraction VDFraction::operator/(uint32 value) const {
	return reduce(hi, (uint64)lo * value);
}

VDFraction& VDFraction::operator*=(VDFraction value) {
	return *this = reduce((uint64)hi * value.hi, (uint64)lo * value.lo);
}

VDFraction& VDFraction::operator/=(VDFraction value) {
	return *this = reduce((uint64)hi * value.lo, (uint64)lo * value.hi);
}

VDFraction& VDFraction::operator*=(uint32 value) {
	return *this = reduce((uint64)hi * value, lo);
}

VDFraction& VDFraction::operator/=(uint32 value) {
	return *this = reduce(hi, (uint64)lo * value);
}

sint64 VDFraction::scale64t(sint64 value) const {
	uint32 remainder;
	const uint64 result = (uint64)VDFractionScale64(GetMagnitude(value), hi, lo, remainder);
	return ApplySign(result, value < 0);
}

sint64 VDFraction::scale64u(sint64 value) const {
	uint32 remainder;
	uint64 result = (uint64)VDFractionScale64(GetMagnitude(value), hi, lo, remainder);

	if (value >= 0 && remainder)
		++result;

	return ApplySign(result, value < 0);
}

sint64 VDFraction::scale64r(sint64 value) const {
	uint32 remainder;
	uint64 result = (uint64)VDFractionScale64(GetMagnitude(value), hi, lo, remainder);

	if (remainder >= (lo >> 1) + (lo & 1))
		++result;

	return ApplySign(result, value < 0);
}

sint64 VDFraction::scale64it(sint64 value) const {
	uint32 remainder;
	const uint64 result = (uint64)VDFractionScale64(GetMagnitude(value), lo, hi, remainder);
	return ApplySign(result, value < 0);
}

sint64 VDFraction::scale64ir(sint64 value) const {
	uint32 remainder;
	uint64 result = (uint64)VDFractionScale64(GetMagnitude(value), lo, hi, remainder);

	if (remainder >= (hi >> 1) + (hi & 1))
		++result;

	return ApplySign(result, value < 0);
}

sint64 VDFraction::scale64iu(sint64 value) const {
	uint32 remainder;
	uint64 result = (uint64)VDFractionScale64(GetMagnitude(value), lo, hi, remainder);

	if (value >= 0 && remainder)
		++result;

	return ApplySign(result, value < 0);
}

uint32 VDFraction::roundup32ul() const {
	return (uint32)(((uint64)hi + lo - 1) / lo);
}

bool VDFraction::Parse(const char *s) {
	char c;

	while((c = *s) && (c == ' ' || c == '\t'))
		++s;

	uint64 numerator = 0;
	uint64 denominator = 1;

	while((c = *s)) {
		const uint32 digit = (uint32)c - '0';

		if (digit >= 10)
			break;

		numerator = numerator * 10 + digit;
		if (numerator >> 32)
			return false;

		++s;
	}

	if (c == '.') {
		++s;

		while((c = *s)) {
			const uint32 digit = (uint32)c - '0';

			if (digit >= 10)
				break;

			if (numerator >= UINT64_C(100000000000000000)
				|| denominator >= UINT64_C(100000000000000000)) {
				if (digit >= 5)
					++numerator;
				while((c = *s) && (unsigned)(c - '0') < 10)
					++s;
				break;
			}

			numerator = numerator * 10 + digit;
			denominator *= 10;
			++s;
		}
	}

	while(c == ' ' || c == '\t')
		c = *++s;

	if (c)
		return false;

	if (!(denominator >> 32) && ((uint64)(uint32)denominator << 32) <= numerator)
		return false;

	*this = reduce(numerator, denominator);
	return true;
}
