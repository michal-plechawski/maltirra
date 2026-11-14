// Main CPU diagnostic disassembly bound to the actual Windows simulator.
#include <stdafx.h>
#include "cpu.h"
#include "disasm.h"

void ATCPUEmulator::DumpDisassembly() {
	ATDisassembleInsn(mInsnPC, mK);
}
