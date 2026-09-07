// VirtualDub system library: macOS ARM64 constexpr primitives

#include <vd2/system/vdtypes.h>
#include <vd2/system/constexpr.h>

static_assert(VDCxSinPi(0.0f) == 0.0f);
static_assert(VDCxSinPi(0.5f) == 1.0f);
static_assert(VDCxCosPi(0.0f) == 1.0f);
static_assert(VDCxCosPi(1.0f) == -1.0f);
static_assert(VDCxSin(0.0f) == 0.0f);
static_assert(VDCxCos(0.0f) == 1.0f);
static_assert(VDCxSqrt(9.0f) == 3.0f);
static_assert(VDCxFloor(-3.5f) == -4.0f);
static_assert(VDCxExp(0.0f) == 1.0f);
