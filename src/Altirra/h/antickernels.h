// Bounded ANTIC DMA and playfield decode kernels.
#ifndef f_AT_ANTICKERNELS_H
#define f_AT_ANTICKERNELS_H

#include <vd2/system/vdtypes.h>

// Decode up to limit-x clocks into two output bytes per character.
// Reads exactly ceil((limit-x)/2) bytes from each input and writes exactly
// twice that many bytes. Masks use normal bytewise AND semantics (core values
// are 0 or 0xFF). x >= limit is a no-op and permits null pointers.
uint32 ATAnticDecodeMode2(uint8 *dst, const uint8 *src, const uint8 *chdata,
	uint32 x, uint32 limit, uint8 invMask, uint8 blinkMask);

// dmaPattern is the ANTIC 128-byte, 128-byte-aligned DMA pattern buffer.
// Only cycles in [start, min(end, 115)) are considered; an empty range is a no-op.
void ATAnticSetDMACycles(void *dmaPattern, uint32 start, uint32 end,
	uint8 cyclePattern, uint8 dmaMask);

// These operate on the complete, aligned 128-byte ANTIC DMA pattern buffer.
void ATAnticSetRefreshCycles(uint8 *dmaPattern);
void ATAnticClearDMACycles(uint8 *dmaPattern);

#endif
