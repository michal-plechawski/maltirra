// VirtualDub system library: macOS ARM64 binary primitives

#include <vd2/system/binary.h>

static_assert(VDSwizzleU16(0x1234) == 0x3412);
static_assert(VDSwizzleS16(0x1234) == 0x3412);
static_assert(VDSwizzleU32(0x12345678) == 0x78563412);
static_assert(VDSwizzleS32(0x12345678) == 0x78563412);
static_assert(VDSwizzleU64(0x123456789ABCDEF0) == 0xF0DEBC9A78563412);
static_assert(VDSwizzleS64(0x123456789ABCDEF0) == 0xF0DEBC9A78563412);
