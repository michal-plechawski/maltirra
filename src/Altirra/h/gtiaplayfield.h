// Bounded ANTIC-to-GTIA playfield conversion. Architecture kernels are supplied
// by the platform, preserving the same byte-for-byte contract on Windows/macOS.
#ifndef f_AT_GTIAPLAYFIELD_H
#define f_AT_GTIAPLAYFIELD_H

#include <vd2/system/vdtypes.h>

// Read exactly n source bytes and write exactly 2*n destination bytes.
// Source/destination alignment is unrestricted; source must not overlap output.
// n==0 is a no-op, including with null buffers.
void ATGTIAUnpackPlayfield160(uint8 *dst, const uint8 *src, uint32 n);
// The priority and hires outputs start at byte offset x, each spanning 2*n.
void ATGTIAUnpackPlayfield320(uint8 *priority, uint8 *hires, const uint8 *src, uint32 x, uint32 n);

#endif
