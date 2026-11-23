#include <stdafx.h>
#include <utility>
#include "hleprogramloader.h"
#include "cpuheatmap.h"
#include "debugger.h"
#include "simulator.h"

void ATHLEProgramLoader::Init(ATCPUEmulator *cpu, ATSimulatorEventManager *events, ATSimulator *sim, IATDeviceSIOManager *sio) {
	ATHLEProgramLoaderConnections c;
	c.mpGetTick = [sim] { return sim->GetScheduler()->GetTick64(); };
	c.mpIsKernelROMLocation = [sim](uint16 a) { return sim->IsKernelROMLocation(a); };
	c.mpRandomizeRawMemory = [sim](uint16 a, uint32 n, uint32 seed) { return sim->RandomizeRawMemory(a, n, seed); };
	c.mpPresetMemoryRange = [sim](uint32 a, uint32 n) { if (auto *hm = sim->GetHeatMap()) hm->PresetMemoryRange(a, n); };
	c.mpIsSymbolLoadingEnabled = [] { auto *d = ATGetDebugger(); return d && d->IsSymbolLoadingEnabled(); };
	c.mpLoadSymbols = [](const wchar_t *path) -> uint32 { const uint32 target = 0; auto *d = ATGetDebugger(); return d ? d->LoadSymbols(path, false, &target) : 0; };
	c.mpProcessSymbolDirectives = [](uint32 id) { if (auto *d = ATGetDebugger()) d->ProcessSymbolDirectives(id); };
	c.mpQueueAutoLoadBatchFile = [](const wchar_t *path) { if (auto *d = ATGetDebugger()) d->QueueAutoLoadBatchFile(path); };
	c.mpUnloadSymbols = [](uint32 id) { if (auto *d = ATGetDebugger()) d->UnloadSymbols(id); };
	Init(cpu, events, std::move(c), sio);
}
