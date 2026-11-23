#include <stdafx.h>
#include <utility>
#include "hleciohook.h"
#include "simulator.h"

IATHLECIOHook *ATCreateHLECIOHook(ATCPUEmulator *cpu, ATSimulator *sim, ATMemoryManager *memory) {
	ATHLECIOConnections c;
	c.mpGetCassette = [sim] { return &sim->GetCassette(); };
	c.mpGetVirtualScreenHandler = [sim] { return sim->GetVirtualScreenHandler(); };
	c.mpIsCassetteSIOPatchEnabled = [sim] { return sim->IsCassetteSIOPatchEnabled(); };
	return ATCreateHLECIOHook(cpu, std::move(c), memory);
}
