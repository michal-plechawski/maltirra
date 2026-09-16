// Altirra - portable scalar PAL artifacting kernels

#ifndef f_AT_ARTIFACTING_PAL_H
#define f_AT_ARTIFACTING_PAL_H

#include <vd2/system/vdtypes.h>

void ATArtifactPALLuma(uint32 *dst, const uint8 *src, uint32 n, const uint32 *kernels);
void ATArtifactPALChroma(uint32 *dst, const uint8 *src, uint32 n, const uint32 *kernels);
void ATArtifactPALFinal(uint32 *dst, const uint32 *ybuf, const uint32 *ubuf, const uint32 *vbuf, uint32 *ulbuf, uint32 *vlbuf, uint32 n);
void ATArtifactPALFinalMono(uint32 *dst, const uint32 *ybuf, uint32 n, const uint32 *monoTable);
void ATArtifactPAL32(void *dst, void *delayLine, uint32 n, bool compressExtendedRange);

#endif
