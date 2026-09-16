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
#endif

#endif
