// Portable integration tests for the production main CPU core.

#include <algorithm>
#include <memory>
#include <vector>

#include <at/attest/portabletest.h>
#include <at/atcore/snapshot.h>
#include <at/atcpu/history.h>
#include <at/atdebugger/target.h>
#include "maincpuportablefixture.h"

namespace {
	struct MainCPUSnapshotCase {
		ATCPUMode mMode;
		uint32 mPC;
		uint32 mSubCycles;
		bool mbNative;
		uint8 mP;
	};

	struct BreakpointHitSink {
		void OnHit(ATBreakpointManager *, ATBreakpointEvent *event) {
			++mHits;
			event->mbBreak = true;
		}
		uint32 mHits = 0;
		VDDelegate mDelegate;
	};

	bool SameBusAfterSnapshot(const std::vector<MainCPUPortableFixture::BusAccess>& a,
		const std::vector<MainCPUPortableFixture::BusAccess>& b, uint32 cycleA, uint32 cycleB)
	{
		if (a.size() != b.size())
			return false;

		for(size_t i = 0; i < a.size(); ++i) {
			if ((a[i].mCycle - cycleA) != (b[i].mCycle - cycleB)
				|| a[i].mAddress != b[i].mAddress
				|| a[i].mValue != b[i].mValue
				|| a[i].mbWrite != b[i].mbWrite)
				return false;
		}

		return true;
	}

	bool TestHistory(ATPortableTestContext& context) {
		auto fixture = std::make_unique<MainCPUPortableFixture>();
		fixture->Prepare(kATCPUMode_6502, 0xFFFF, 1, false, 0x20);
		fixture->Load(0xFFFF, { 0xEA, 0xEA });
		fixture->mCPU.SetHistoryEnabled(true);
		const uint32 historyStart = fixture->mCPU.GetHistoryCounter();

		AT_PORTABLE_TEST_ASSERT(context, fixture->Step() > 0);
		AT_PORTABLE_TEST_ASSERT(context, fixture->mCPU.GetHistoryCounter() > historyStart);
		const ATCPUHistoryEntry& entry = fixture->mCPU.GetHistory(0);
		AT_PORTABLE_TEST_ASSERT(context, entry.mPC == 0xFFFF);
		AT_PORTABLE_TEST_ASSERT(context, entry.mOpcode[0] == 0xEA);
		AT_PORTABLE_TEST_ASSERT(context, entry.mCycle <= fixture->CPUGetCycle());
		AT_PORTABLE_TEST_ASSERT(context, entry.mGlobalPCBase == 0);
		return true;
	}

	bool TestSnapshotCase(ATPortableTestContext& context, const MainCPUSnapshotCase& test, uint32 cutCycles) {
		auto source = std::make_unique<MainCPUPortableFixture>();
		auto restored = std::make_unique<MainCPUPortableFixture>();
		source->Prepare(test.mMode, test.mPC, test.mSubCycles, test.mbNative, test.mP);
		restored->Prepare(test.mMode, test.mPC, test.mSubCycles, test.mbNative, test.mP);

		const uint32 bank = test.mMode == kATCPUMode_65C816 ? (test.mPC & 0xFF0000) : 0;
		const std::vector<uint8> program = test.mbNative
			? std::vector<uint8>{ 0xA9, 0x34, 0x12, 0x8D, 0x00, 0x30, 0xEA }
			: std::vector<uint8>{ 0xA9, 0x34, 0x8D, 0x00, 0x30, 0xEA };
		// Program fetch wraps inside PBK, including the operands after $FFFF.
		for (size_t i = 0; i < program.size(); ++i) {
			const uint32 address = bank | static_cast<uint16>(test.mPC + i);
			source->mRAM[address] = restored->mRAM[address] = program[i];
		}
		source->Load(0x3000, { 0x00, 0x00 });
		restored->Load(0x3000, { 0x00, 0x00 });
		source->EnableBusTrace();
		restored->EnableBusTrace();

		AT_PORTABLE_TEST_ASSERT(context, source->RunCycles(cutCycles));
		vdrefptr<IATObjectState> state;
		source->mCPU.SaveState(~state);
		AT_PORTABLE_TEST_ASSERT(context, !!state);

		const auto sourceRAM = source->mRAM;
		const uint32 sourceTraceStart = source->mBus.size();
		const uint32 sourceTraceCycle = source->CPUGetCycle();
		AT_PORTABLE_TEST_ASSERT(context, source->RunCycles(40));

		// CPU snapshots do not own RAM. Restore its state from the same cut point.
		restored->mRAM = sourceRAM;
		restored->mCPU.LoadState(state.get());
		const uint32 restoredTraceStart = restored->mBus.size();
		const uint32 restoredTraceCycle = restored->CPUGetCycle();
		AT_PORTABLE_TEST_ASSERT(context, restored->RunCycles(40));

		const auto sourceTrace = std::vector<MainCPUPortableFixture::BusAccess>(source->mBus.begin() + sourceTraceStart, source->mBus.end());
		const auto restoredTrace = std::vector<MainCPUPortableFixture::BusAccess>(restored->mBus.begin() + restoredTraceStart, restored->mBus.end());
		AT_PORTABLE_TEST_ASSERT(context, SameBusAfterSnapshot(sourceTrace, restoredTrace, sourceTraceCycle, restoredTraceCycle));
		AT_PORTABLE_TEST_ASSERT(context, source->mRAM == restored->mRAM);
		AT_PORTABLE_TEST_ASSERT(context, source->mCPU.GetPC() == restored->mCPU.GetPC());
		AT_PORTABLE_TEST_ASSERT(context, source->mCPU.GetXPC() == restored->mCPU.GetXPC());
		AT_PORTABLE_TEST_ASSERT(context, source->mCPU.GetA() == restored->mCPU.GetA());
		AT_PORTABLE_TEST_ASSERT(context, source->mCPU.GetX() == restored->mCPU.GetX());
		AT_PORTABLE_TEST_ASSERT(context, source->mCPU.GetY() == restored->mCPU.GetY());
		AT_PORTABLE_TEST_ASSERT(context, source->mCPU.GetP() == restored->mCPU.GetP());
		AT_PORTABLE_TEST_ASSERT(context, source->mCPU.GetS() == restored->mCPU.GetS());
		AT_PORTABLE_TEST_ASSERT(context, source->mCPU.GetAH() == restored->mCPU.GetAH());
		AT_PORTABLE_TEST_ASSERT(context, source->mCPU.GetXH() == restored->mCPU.GetXH());
		AT_PORTABLE_TEST_ASSERT(context, source->mCPU.GetYH() == restored->mCPU.GetYH());
		AT_PORTABLE_TEST_ASSERT(context, source->mCPU.GetSH() == restored->mCPU.GetSH());
		AT_PORTABLE_TEST_ASSERT(context, source->mCPU.GetD() == restored->mCPU.GetD());
		AT_PORTABLE_TEST_ASSERT(context, source->mCPU.GetB() == restored->mCPU.GetB());
		AT_PORTABLE_TEST_ASSERT(context, source->mCPU.GetK() == restored->mCPU.GetK());
		return true;
	}

	bool StopAtHandler(MainCPUPortableFixture& f, uint16 handler) {
		ATCPUStepCondition c = ATCPUStepCondition::CreateSingleStep();
		c.mIgnorePCStart = 0;
		c.mIgnorePCLength = handler;
		f.mCPU.AddStepCondition(c);
		uint32 remaining = 32;
		while (remaining-- && !f.Tick()) {}
		f.mCPU.RemoveStepCondition(c);
		return f.mLastEvent == kATSimEvent_CPUSingleStep && f.mCPU.GetPC() == handler;
	}

	bool TestMainCPUDebug(ATPortableTestContext& context) {
		auto fixture = std::make_unique<MainCPUPortableFixture>();
		fixture->Prepare(kATCPUMode_6502, 0x2100, 1, false, 0x20);
		fixture->Load(0x2100, { 0xEA, 0xEA, 0xEA });
		uint32 hookHits = 0;
		ATCPUHookNode *hook = fixture->mHooks.AddHook(kATCPUHookMode_Always, 0x2100, 0,
			[&](uint16 pc) { if (pc == 0x2100) ++hookHits; return uint8(0); });
		AT_PORTABLE_TEST_ASSERT(context, fixture->Step() > 0);
		AT_PORTABLE_TEST_ASSERT(context, hookHits == 1);
		fixture->mHooks.RemoveHook(hook);

		fixture->Prepare(kATCPUMode_6502, 0x2200, 1, false, 0x20);
		fixture->Load(0x2200, { 0xEA, 0xEA });
		BreakpointHitSink hitSink;
		fixture->mBreakpoints.OnBreakpointHit() += hitSink.mDelegate.Bind(&hitSink, &BreakpointHitSink::OnHit);
		const uint32 breakpoint = fixture->mBreakpoints.SetAtPC(0, 0x2201);
		AT_PORTABLE_TEST_ASSERT(context, breakpoint != 0);
		for(uint32 i = 0; i < 32 && fixture->mLastEvent == kATSimEvent_None; ++i)
			fixture->Tick();
		AT_PORTABLE_TEST_ASSERT(context, hitSink.mHits == 1);
		AT_PORTABLE_TEST_ASSERT(context, fixture->mLastEvent == kATSimEvent_CPUPCBreakpoint);
		AT_PORTABLE_TEST_ASSERT(context, fixture->mBreakpoints.Clear(breakpoint));
		fixture->mBreakpoints.OnBreakpointHit() -= hitSink.mDelegate;

		// OS hooks are disabled for native 65C816 execution and for nonzero banks.
		fixture->Prepare(kATCPUMode_65C816, 0x003000, 1, true, 0x20);
		fixture->Load(0x003000, { 0xEA, 0xEA });
		hookHits = 0;
		hook = fixture->mHooks.AddHook(kATCPUHookMode_Always, 0x3000, 0,
			[&](uint16) { ++hookHits; return uint8(0); });
		AT_PORTABLE_TEST_ASSERT(context, fixture->Step() > 0);
		AT_PORTABLE_TEST_ASSERT(context, hookHits == 0);
		fixture->mHooks.RemoveHook(hook);

		fixture->Prepare(kATCPUMode_65C816, 0x013000, 1, false, 0x20);
		fixture->Load(0x013000, { 0xEA, 0xEA });
		hookHits = 0;
		hook = fixture->mHooks.AddHook(kATCPUHookMode_Always, 0x3000, 0,
			[&](uint16) { ++hookHits; return uint8(0); });
		AT_PORTABLE_TEST_ASSERT(context, fixture->Step() > 0);
		AT_PORTABLE_TEST_ASSERT(context, hookHits == 0);
		fixture->mHooks.RemoveHook(hook);

		// Interrupt entry saves the return state on the real emulated stack and RTI returns.
		fixture->Prepare(kATCPUMode_6502, 0x2300, 1, false, 0x20);
		fixture->Load(0x2300, { 0xEA, 0xEA });
		fixture->Load(0x2400, { 0x40 });
		fixture->mRAM[0xFFFA] = 0x00; fixture->mRAM[0xFFFB] = 0x24;
		fixture->mCPU.AssertNMI();
		AT_PORTABLE_TEST_ASSERT(context, StopAtHandler(*fixture, 0x2400));
		AT_PORTABLE_TEST_ASSERT(context, fixture->mCPU.GetPC() == 0x2400);
		AT_PORTABLE_TEST_ASSERT(context, fixture->mCPU.GetS() == 0xFA);
		AT_PORTABLE_TEST_ASSERT(context, fixture->mRAM[0x01FD] == 0x23);
		AT_PORTABLE_TEST_ASSERT(context, fixture->mRAM[0x01FC] == 0x01);
		AT_PORTABLE_TEST_ASSERT(context, fixture->Step() > 0);
		AT_PORTABLE_TEST_ASSERT(context, fixture->mCPU.GetPC() == 0x2301);
		AT_PORTABLE_TEST_ASSERT(context, fixture->mCPU.GetS() == 0xFD);

		auto irqFixture = std::make_unique<MainCPUPortableFixture>();
		irqFixture->Prepare(kATCPUMode_6502, 0x2500, 1, false, 0x20);
		irqFixture->Load(0x2500, { 0xEA, 0xEA });
		irqFixture->Load(0x2600, { 0x40 });
		irqFixture->mRAM[0xFFFE] = 0x00; irqFixture->mRAM[0xFFFF] = 0x26;
		irqFixture->mCPU.AssertIRQ(0);
		AT_PORTABLE_TEST_ASSERT(context, StopAtHandler(*irqFixture, 0x2600));
		AT_PORTABLE_TEST_ASSERT(context, irqFixture->mCPU.GetPC() == 0x2600);
		AT_PORTABLE_TEST_ASSERT(context, irqFixture->mCPU.GetS() == 0xFA);
		irqFixture->mCPU.NegateIRQ();
		AT_PORTABLE_TEST_ASSERT(context, irqFixture->Step() > 0);
		AT_PORTABLE_TEST_ASSERT(context, irqFixture->mCPU.GetPC() == 0x2502);
		AT_PORTABLE_TEST_ASSERT(context, irqFixture->mCPU.GetS() == 0xFD);
		return true;
	}
}

bool ATTestAltirraMainCPUHistory(ATPortableTestContext& context) {
	return TestHistory(context);
}

bool ATTestAltirraMainCPUSnapshot(ATPortableTestContext& context) {
	static constexpr MainCPUSnapshotCase kCases[] = {
		{ kATCPUMode_6502, 0x2000, 1, false, 0x24 },
		{ kATCPUMode_65C816, 0x122000, 1, true, 0x00 },
		{ kATCPUMode_65C816, 0x12FFFC, 4, true, 0x00 },
	};
	for(const auto& test : kCases)
		for(uint32 cut = 1; cut <= 8; ++cut)
			if (!TestSnapshotCase(context, test, cut))
				return false;
	return true;
}

bool ATTestAltirraMainCPUDebug(ATPortableTestContext& context) {
	return TestMainCPUDebug(context);
}
