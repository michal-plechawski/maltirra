// Bounded counted-input bitset operations used by the PIA.
#ifndef f_AT_PIACOUNTEDINPUTS_H
#define f_AT_PIACOUNTEDINPUTS_H

#include <vd2/system/vdtypes.h>

// counts points to exactly 32 bytes aligned to 16 bytes.
void ATPIAAddZeroBits(uint8 *counts, uint32 mask);
void ATPIARemoveZeroBits(uint8 *counts, uint32 mask);
uint32 ATPIAReadZeroMask(const uint8 *counts);

#endif
