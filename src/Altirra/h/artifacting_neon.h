// Altirra - ARM64 NEON artifacting kernels

#ifndef f_AT_ARTIFACTING_NEON_H
#define f_AT_ARTIFACTING_NEON_H

#include <vd2/system/vdtypes.h>

#if defined(VD_CPU_ARM64)
void ATArtifactBlend_NEON(uint32 *dst, const uint32 *src, uint32 n);
void ATArtifactBlendExchange_NEON(uint32 *dst, uint32 *blendDst, uint32 n);
void ATArtifactBlendLinear_NEON(uint32 *dst, const uint32 *src, uint32 n, bool extendedRange);
void ATArtifactBlendExchangeLinear_NEON(uint32 *dst, uint32 *blendDst, uint32 n, bool extendedRange);
void ATArtifactBlendCopyMonoPersistence_NEON(uint32 *dst, uint32 *blendDst, const uint32 *palette, float factor, float factor2, float limit, uint32 n);
void ATArtifactBlendMonoPersistence_NEON(uint32 *dst, const uint32 *src, const uint32 *palette, float factor, float factor2, float limit, uint32 n);
void ATArtifactBlendExchangeMonoPersistence_NEON(uint32 *dst, uint32 *blendDst, const uint32 *palette, float factor, float factor2, float limit, uint32 n);
void ATArtifactBlendScanlines_NEON(uint32 *dst, const uint32 *src1, const uint32 *src2, uint32 n, float intensity);
void ATArtifactNTSCAccum_NEON(void *dst, const void *table, const void *src, uint32 count);
void ATArtifactNTSCAccumTwin_NEON(void *dst, const void *table, const void *src, uint32 count);
void ATArtifactNTSCFinal_NEON(void *dst, const void *srcR, const void *srcG, const void *srcB, uint32 count);
void ATArtifactPALLuma_NEON(uint32 *dst, const uint8 *src, uint32 n, const uint32 *kernels);
void ATArtifactPALLumaTwin_NEON(uint32 *dst, const uint8 *src, uint32 n, const uint32 *kernels);
void ATArtifactPALChroma_NEON(uint32 *dst, const uint8 *src, uint32 n, const uint32 *kernels);
void ATArtifactPALChromaTwin_NEON(uint32 *dst, const uint8 *src, uint32 n, const uint32 *kernels);
void ATArtifactPALFinalMono_NEON(uint32 *dst, const uint32 *ybuf, uint32 n, const uint32 palette[256]);
void ATArtifactPALFinal_NEON(uint32 *dst, const uint32 *ybuf, const uint32 *ubuf, const uint32 *vbuf, uint32 *ulbuf, uint32 *vlbuf, uint32 n);
void ATArtifactPAL32_NEON(void *dst, void *delayLine, uint32 n, bool compressExtendedRange);
#endif

#endif
