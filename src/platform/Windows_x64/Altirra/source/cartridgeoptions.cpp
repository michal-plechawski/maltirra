// Bridge the cartridge core to the existing Windows preference store.
#include <stdafx.h>
#include "cartridge.h"
#include "options.h"

ATCartridgeFlashOptions ATGetDefaultCartridgeFlashOptions() {
	return {g_ATOptions.mMaxflash1MbFlashChip, g_ATOptions.mMaxflash8MbFlashChip, g_ATOptions.mSICFlashChip};
}
