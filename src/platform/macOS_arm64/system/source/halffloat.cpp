// VirtualDub system library: macOS ARM64 half-precision conversion

#include <bit>
#include <vd2/system/halffloat.h>

uint16 VDConvertFloatToHalf(const void *f) {
	uint32 v = std::bit_cast<uint32>(*static_cast<const float *>(f));

	uint32 sign = (v >> 16) & 0x8000;
	sint32 exmant = v & 0x7fffffff;

	if (exmant > 0x7f800000) {
		exmant = (exmant & 0x00400000) + 0x47a00000;
	} else if (exmant > 0x47800000) {
		exmant = 0x47800000;
	} else if (exmant < 0x33800000) {
		exmant = 0x38000000;
	} else if (exmant < 0x38800000) {
		uint32 ex = exmant & 0x7f800000;
		uint32 mant = (exmant & 0x007fffff) | 0x800000;
		uint32 sticky = 0;

		while(ex < 0x38800000) {
			ex += 0x00800000;
			sticky |= mant;
			mant >>= 1;
		}

		sticky |= mant >> 13;
		mant += sticky & 1;
		mant += 0x0fff;
		exmant = ex + mant - 0x800000;
	} else {
		exmant |= (exmant & 0x00002000) >> 13;
		exmant += 0x00000fff;
	}

	exmant -= 0x38000000;
	exmant >>= 13;

	return (uint16)(sign + exmant);
}

void VDConvertHalfToFloat(uint16 h, void *dst) {
	uint32 sign = ((uint32)h << 16) & 0x80000000;
	uint32 exmant = (uint32)h & 0x7fff;
	uint32 v = 0;

	if (exmant >= 0x7c00) {
		v = (exmant << 13) + 0x70000000;
	} else if (exmant >= 0x0400) {
		v = (exmant << 13) + 0x38000000;
	} else if (exmant) {
		uint32 ex32 = 0x38000000;
		uint32 mant32 = (exmant & 0x3ff) << 13;

		while(!(mant32 & 0x800000)) {
			mant32 <<= 1;
			ex32 -= 0x800000;
		}

		v = ex32 + mant32;
	}

	*static_cast<float *>(dst) = std::bit_cast<float>(v + sign);
}
