// Diagnostic output from the actual main CPU and its attached memory bus.
#include <vd2/system/VDString.h>
#include <at/atcpu/history.h>
#include <cpu.h>
#include <cpumemory.h>
#include <consoletext.h>
#include <disasm.h>

void ATCPUEmulator::DumpDisassembly() {
	ATCPUHistoryEntry h {};
	h.mPC = GetInsnPC(); h.mP = GetP(); h.mX = GetX(); h.mY = GetY(); h.mS = GetS();
	h.mbEmulation = GetEmulationFlag();
	if (GetCPUMode() == kATCPUMode_65C816) {
		h.mExt.mXH = GetXH(); h.mExt.mYH = GetYH(); h.mExt.mSH = GetSH();
		h.mB = GetB(); h.mK = GetK(); h.mD = GetD();
	}
	for(uint32 i = 0; i < 4; ++i)
		h.mOpcode[i] = mpMemory->DebugExtReadByte(static_cast<uint16>(h.mPC + i), h.mK);
	ATDisasmServices services;
	services.mpDebugReadByte = [this](uint32 address) {
		return mpMemory->DebugExtReadByte(static_cast<uint16>(address), static_cast<uint8>(address >> 16));
	};
	services.mpIsPathStart = [this](uint16 address) { return IsPathfindingEnabled() && IsPathStart(address); };
	VDStringA line;
	ATDisassembleInsn(line, services, nullptr, GetDisasmMode(), h, true, false, true, true, true);
	line += '\n';
	ATConsoleWrite(line.c_str());
}
