// Generic compiler intrinsic selection.

#ifndef f_VD2_SYSTEM_INTRIN_H
#define f_VD2_SYSTEM_INTRIN_H

#if defined(_MSC_VER)
	#include <intrin.h>
#elif defined(__ARM_NEON) || defined(__ARM_NEON__)
	#include <arm_neon.h>
#elif defined(__SSE2__)
	#include <emmintrin.h>
#endif

#endif
