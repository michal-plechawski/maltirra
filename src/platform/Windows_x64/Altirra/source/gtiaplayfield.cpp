#include <gtiaplayfield.h>
#include <gtiatables.h>
#include <vd2/system/cpuaccel.h>

using namespace ATGTIA;

#ifdef VD_CPU_X86
extern "C" void VDCDECL atasm_update_playfield_160_sse2(void *dst, const uint8 *src, uint32 n);
#endif
#if defined(VD_CPU_X86) || defined(VD_CPU_X64)
#include "gtia_sse2_intrin.inl"
#elif defined(VD_CPU_ARM64)
#include "gtia_neon.inl"
#endif

void ATGTIAUnpackPlayfield160(uint8 *dst, const uint8 *src, uint32 n) {
	if (!n) return;
	// The legacy masked kernels touch the surrounding aligned vectors. Only
	// submit complete, aligned vectors; the scalar tail has no padding contract.
	if (!((reinterpret_cast<uintptr>(src) | reinterpret_cast<uintptr>(dst)) & 15)) {
		const uint32 bulk = n & ~uint32(15);
#if defined(VD_CPU_X64)
		atasm_update_playfield_160_sse2(dst, src, bulk);
#elif defined(VD_CPU_X86)
		if (!SSE2_enabled) goto scalar160;
		atasm_update_playfield_160_sse2(dst, src, bulk);
#elif defined(VD_CPU_ARM64)
		atasm_update_playfield_160_neon(dst, src, bulk);
#else
		goto scalar160;
#endif
		if (bulk) { dst += bulk * 2; src += bulk; n -= bulk; }
	}
#if !defined(VD_CPU_X64) && !defined(VD_CPU_ARM64)
scalar160:
#endif
	while (n--) {
		const uint8 value = *src++;
		*dst++ = value >> 4;
		*dst++ = value & 15;
	}
}

void ATGTIAUnpackPlayfield320(uint8 *priority, uint8 *hires, const uint8 *src, uint32 x, uint32 n) {
	if (!n) return;
	priority += x; hires += x;
#if defined(VD_CPU_X86) || defined(VD_CPU_X64)
	if (!((reinterpret_cast<uintptr>(src) | reinterpret_cast<uintptr>(priority) | reinterpret_cast<uintptr>(hires)) & 15)) {
		const uint32 bulk = n & ~uint32(15);
#ifdef VD_CPU_X86
		if (!SSE2_enabled) goto scalar320;
#endif
		atasm_update_playfield_320_sse2(priority, hires, src, 0, bulk);
		if (bulk) { priority += bulk * 2; hires += bulk * 2; src += bulk; n -= bulk; }
	}
#ifdef VD_CPU_X86
scalar320:
#endif
#endif
	while (n--) {
		const uint8 value = *src++;
		*priority++ = PF2; *priority++ = PF2;
		*hires++ = (value >> 2) & 3;
		*hires++ = value & 3;
	}
}
