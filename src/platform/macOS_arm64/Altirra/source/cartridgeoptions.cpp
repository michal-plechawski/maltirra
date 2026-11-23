// Native defaults match the stock AMD chips. Per-cartridge options are
// supplied through ATCartridgeEmulator::SetFlashOptions().
#include <cartridge.h>

ATCartridgeFlashOptions ATGetDefaultCartridgeFlashOptions() {
	return {};
}
