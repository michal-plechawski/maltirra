// Regression for the actual main CPU's instruction-flag bitmap and bindings.

#include <memory>
#include <vd2/system/vdtypes.h>
#include <vd2/system/vdstl_block.h>
#include <at/attest/portabletest.h>
#include <cpuflags.h>
#include <bkptmanager.h>
#include <cpu.h>

bool ATTestAltirraMainCPUBreakpoints(ATPortableTestContext& context) {
	auto cpu = std::make_unique<ATCPUEmulator>();
	cpu->SetBreakpoint(0x1234);
	cpu->SetBreakpoint(0x1234);
	AT_PORTABLE_TEST_ASSERT(context, cpu->GetBreakpointCount() == 1);
	cpu->SetBreakpoint(0x5678);
	AT_PORTABLE_TEST_ASSERT(context, cpu->GetBreakpointCount() == 2);
	cpu->ClearBreakpoint(0x5678);
	AT_PORTABLE_TEST_ASSERT(context, cpu->IsBreakpointSet(0x1234));
	cpu->ClearBreakpoint(0x1234);
	AT_PORTABLE_TEST_ASSERT(context, !cpu->GetBreakpointCount());

	cpu->SetAllBreakpoints();
	AT_PORTABLE_TEST_ASSERT(context, cpu->GetBreakpointCount() == 65536);
	for (uint32 pc = 0; pc < 0x10000; ++pc)
		AT_PORTABLE_TEST_ASSERT(context, cpu->IsBreakpointSet(static_cast<uint16>(pc)));
	cpu->ClearBreakpoint(0x1234);
	AT_PORTABLE_TEST_ASSERT(context, !cpu->IsBreakpointSet(0x1234));
	AT_PORTABLE_TEST_ASSERT(context, cpu->GetBreakpointCount() == 65535);
	cpu->ClearAllBreakpoints();
	AT_PORTABLE_TEST_ASSERT(context, !cpu->GetBreakpointCount());
	for (uint32 pc = 0; pc < 0x10000; ++pc)
		AT_PORTABLE_TEST_ASSERT(context, !cpu->IsBreakpointSet(static_cast<uint16>(pc)));

	ATBreakpointManager manager;
	ATBreakpointManagerConnections connections;
	connections.mpSetBreakpointManager = [&](ATBreakpointManager *p) { cpu->SetBreakpointManager(p); };
	connections.mpSetBreakpoint = [&](uint16 pc) { cpu->SetBreakpoint(pc); };
	connections.mpClearBreakpoint = [&](uint16 pc) { cpu->ClearBreakpoint(pc); };
	connections.mpSetAllBreakpoints = [&] { cpu->SetAllBreakpoints(); };
	connections.mpClearAllBreakpoints = [&] { cpu->ClearAllBreakpoints(); };
	connections.mpGetXPC = [&] { return cpu->GetXPC(); };
	manager.Init(std::move(connections), nullptr);
	const uint32 pcA = manager.SetAtPC(0, 0x1111);
	const uint32 pcB = manager.SetAtPC(0, 0x2222);
	const uint32 insn = manager.SetInsnBP(0);
	AT_PORTABLE_TEST_ASSERT(context, cpu->IsBreakpointSet(0x3333));
	AT_PORTABLE_TEST_ASSERT(context, manager.Clear(insn));
	AT_PORTABLE_TEST_ASSERT(context, cpu->IsBreakpointSet(0x1111) && cpu->IsBreakpointSet(0x2222));
	AT_PORTABLE_TEST_ASSERT(context, !cpu->IsBreakpointSet(0x3333));
	AT_PORTABLE_TEST_ASSERT(context, cpu->GetNextBreakpoint(-1) == 0x1111);
	AT_PORTABLE_TEST_ASSERT(context, cpu->GetNextBreakpoint(0x1111) == 0x2222);
	AT_PORTABLE_TEST_ASSERT(context, cpu->GetNextBreakpoint(0x2222) == -1);
	AT_PORTABLE_TEST_ASSERT(context, manager.Clear(pcA) && manager.Clear(pcB));
	AT_PORTABLE_TEST_ASSERT(context, !cpu->GetBreakpointCount());
	manager.Shutdown();
	return true;
}
