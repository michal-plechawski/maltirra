// Altirra cassette audio filter implementation internals

#ifndef f_AT_ATIO_INTERNAL_CASSETTEAUDIOFILTERS_H
#define f_AT_ATIO_INTERNAL_CASSETTEAUDIOFILTERS_H

#include <vd2/system/vdtypes.h>

void ATCassetteAudioMinMax16x2_Reference(
	const sint16 *src, uint32 n,
	sint32& minvL, sint32& maxvL, sint32& minvR, sint32& maxvR);
void ATCassetteAudioMinMax16x2_Accelerated(
	const sint16 *src, uint32 n,
	sint32& minvL, sint32& maxvL, sint32& minvR, sint32& maxvR);

uint64 ATCassetteAudioResample16x2_Reference(
	sint16 *dst, const sint16 *src, uint32 count, uint64 accum, sint64 inc);
uint64 ATCassetteAudioResample16x2_Accelerated(
	sint16 *dst, const sint16 *src, uint32 count, uint64 accum, sint64 inc);

#if VD_CPU_X86 || VD_CPU_X64
void ATCassetteAudioMinMax16x2_SSE2(
	const sint16 *src, uint32 n,
	sint32& minvL, sint32& maxvL, sint32& minvR, sint32& maxvR);
uint64 ATCassetteAudioResample16x2_SSE2(
	sint16 *dst, const sint16 *src, uint32 count, uint64 accum, sint64 inc,
	const sint16 (*kernel)[8]);
#endif

#endif
