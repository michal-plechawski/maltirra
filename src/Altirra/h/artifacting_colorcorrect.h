// Internal fixed-point color-correction kernels used by the artifacting engine.
#ifndef f_AT_ARTIFACTING_COLORCORRECT_H
#define f_AT_ARTIFACTING_COLORCORRECT_H
#include <vd2/system/vdtypes.h>

void ATArtifactColorCorrect_Scalar(uint8 *dst, uint32 n, const sint16 linear[256], const uint8 gamma[1024], const sint16 matrix[3][3]);
#if defined(VD_CPU_X86) || defined(VD_CPU_X64)
void ATArtifactColorCorrect_SSE2(uint8 *dst, uint32 n, const sint16 linear[256], const uint8 gamma[1024], const sint16 matrix[3][3]);
#endif
#if defined(VD_CPU_ARM64)
void ATArtifactColorCorrect_NEON(uint8 *dst, uint32 n, const sint16 linear[256], const uint8 gamma[1024], const sint16 matrix[3][3]);
#endif
#endif
