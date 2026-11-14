// Host formatting for keyboard codes in the shared input manager.
#ifndef f_AT_INPUTKEYNAME_H
#define f_AT_INPUTKEYNAME_H
#include <vd2/system/vdtypes.h>
class VDStringW;
void ATInputGetKeyName(uint32 code, VDStringW& name);
#endif
