// Portable tests for the shared Altirra breakpoint manager.

#include <at/attest/portabletest.h>
#include <at/atcpu/co6502.h>
#include <at/atcpu/execstate.h>
#include <at/atcore/scheduler.h>
#include <at/atdebugger/breakpointsimpl.h>
#include <at/atdebugger/target.h>
#include <vd2/system/event.h>
#include <array>
#include <memory>
#include <vector>
#include "bkptmanager.h"

namespace {
	struct BreakpointHarness {
		BreakpointHarness() {
			ATBreakpointManagerConnections c;
			c.mpSetBreakpointManager = [this](ATBreakpointManager *p) { mpManager = p; };
			c.mpSetBreakpoint = [this](uint16 pc) { mPCMap[pc] = true; };
			c.mpClearBreakpoint = [this](uint16 pc) { mPCMap[pc] = false; };
			c.mpSetAllBreakpoints = [this]() { mAll = true; };
			c.mpClearAllBreakpoints = [this]() { mAll = false; for (bool& v : mPCMap) v = false; };
			c.mpGetXPC = [this]() { return mXPC; };
			c.mpPostInterruptingEvent = [this](ATSimulatorEvent e) { mPosted.push_back(e); };
			mpManager = nullptr;
			mManager.Init(std::move(c), nullptr);
		}

		~BreakpointHarness() { mManager.Shutdown(); }

		ATBreakpointManager mManager;
		ATBreakpointManager *mpManager = nullptr;
		bool mPCMap[65536] {};
		bool mAll = false;
		uint32 mXPC = 0;
		std::vector<ATSimulatorEvent> mPosted;
	};

	struct HitLog {
		void OnHit(ATBreakpointManager *, ATBreakpointEvent *const& ev) {
			mIndices.push_back(ev->mIndex);
			if (mpAction)
				mpAction(*ev);
			if (ev->mIndex == mClearOnIndex && mpManager)
				mpManager->Clear(ev->mIndex);
			if (ev->mIndex == mBreakOnIndex) {
				ev->mbBreak = true;
				ev->mbSilentBreak = mSilent;
			}
		}
		ATBreakpointManager *mpManager = nullptr;
		uint32 mBreakOnIndex = 0;
		uint32 mClearOnIndex = 0;
		bool mSilent = false;
		std::vector<uint32> mIndices;
		VDDelegate mDelegate;
		vdfunction<void(ATBreakpointEvent&)> mpAction;
	};

	struct RealDebugTarget final : IATDebugTarget {
		RealDebugTarget()
			: mCPU(false, false) {
			for (uint32 i = 0; i < 256; ++i) {
				mCPU.GetReadMap()[i] = reinterpret_cast<uintptr>(mRAM);
				mCPU.GetWriteMap()[i] = reinterpret_cast<uintptr>(mRAM);
			}
			mBreakpoints.BindBPHandler(mCPU);
		}

		void *AsInterface(uint32 id) override {
			if (id == IATDebugTargetBreakpoints::kTypeID)
				return &mBreakpoints;
			return id == 0 ? static_cast<IATDebugTarget *>(this) : nullptr;
		}
		const char *GetName() override { return "portable 6502 target"; }
		ATDebugDisasmMode GetDisasmMode() override { return kATDebugDisasmMode_6502; }
		float GetDisplayCPUClock() const override { return 1.0f; }
		void GetExecState(ATCPUExecState& state) override { mCPU.GetExecState(state); }
		void SetExecState(const ATCPUExecState& state) override { mCPU.SetExecState(state); }
		sint32 GetTimeSkew() override { return 0; }
		uint8 ReadByte(uint32 address) override { return mRAM[address & 0xffff]; }
		void ReadMemory(uint32 address, void *dst, uint32 n) override {
			for (uint32 i = 0; i < n; ++i) static_cast<uint8 *>(dst)[i] = ReadByte(address + i);
		}
		uint8 DebugReadByte(uint32 address) override { return ReadByte(address); }
		void DebugReadMemory(uint32 address, void *dst, uint32 n) override { ReadMemory(address, dst, n); }
		void WriteByte(uint32 address, uint8 value) override { mRAM[address & 0xffff] = value; }
		void WriteMemory(uint32 address, const void *src, uint32 n) override {
			for (uint32 i = 0; i < n; ++i) WriteByte(address + i, static_cast<const uint8 *>(src)[i]);
		}

		ATCoProc6502 mCPU;
		ATDebugTargetBreakpointsImpl mBreakpoints;
		uint8 mRAM[65536] {};
	};
}

bool ATTestAltirraBreakpointPC(ATPortableTestContext& context) {
	auto harness = std::make_unique<BreakpointHarness>();
	auto& h = *harness;
	HitLog log;
	log.mpManager = &h.mManager;
	h.mManager.OnBreakpointHit() += log.mDelegate(&log, &HitLog::OnHit);

	const uint32 a = h.mManager.SetAtPC(0, 0x1234);
	const uint32 duplicate = h.mManager.SetAtPC(0, 0x1234);
	const uint32 other = h.mManager.SetAtPC(0, 0x2234);
	AT_PORTABLE_TEST_ASSERT(context, a == 1 && duplicate == 2 && other == 3);
	AT_PORTABLE_TEST_ASSERT(context, h.mPCMap[0x1234] && h.mPCMap[0x2234]);
	ATBreakpointInfo info {};
	AT_PORTABLE_TEST_ASSERT(context, h.mManager.GetInfo(a, info) && info.mTargetIndex == 0 && info.mAddress == 0x1234 && info.mbBreakOnPC && !info.mbBreakOnInsn);
	ATBreakpointIndices ids;
	h.mManager.GetAll(ids);
	AT_PORTABLE_TEST_ASSERT(context, ids.size() == 3 && ids[0] == a && ids[2] == other);
	h.mManager.GetAtPC(0, 0x1234, ids);
	AT_PORTABLE_TEST_ASSERT(context, ids.size() == 2 && ids[0] == a && ids[1] == duplicate);
	h.mManager.GetAtPC(0, 0x11234, ids);
	AT_PORTABLE_TEST_ASSERT(context, ids.empty());

	log.mBreakOnIndex = duplicate;
	AT_PORTABLE_TEST_ASSERT(context, h.mManager.TestPCBreakpoint(0, 0x1234) == kATSimEvent_CPUPCBreakpoint);
	AT_PORTABLE_TEST_ASSERT(context, log.mIndices.size() == 2 && log.mIndices[0] == a && log.mIndices[1] == duplicate);
	log.mIndices.clear();
	log.mBreakOnIndex = 0;
	h.mXPC = 0x1234566;
	const uint32 global = h.mManager.SetAtPC(0, 0x1234567);
	AT_PORTABLE_TEST_ASSERT(context, global > 0 && h.mManager.TestPCBreakpoint(0, 0x4567) == kATSimEvent_None);
	// A global breakpoint is checked alongside regular entries only at the same low-16 PC.
	h.mManager.GetAtPC(0, 0x1234567, ids);
	AT_PORTABLE_TEST_ASSERT(context, ids.size() == 1 && ids[0] == global);
	h.mXPC = 0x1234567;
	log.mBreakOnIndex = global;
	AT_PORTABLE_TEST_ASSERT(context, h.mManager.TestPCBreakpoint(0, 0x4567) == kATSimEvent_CPUPCBreakpoint);
	log.mBreakOnIndex = 0;

	const uint32 selfClear1 = h.mManager.SetAtPC(0, 0x3456);
	const uint32 selfClear2 = h.mManager.SetAtPC(0, 0x3456);
	log.mIndices.clear();
	log.mClearOnIndex = selfClear1;
	AT_PORTABLE_TEST_ASSERT(context, h.mManager.TestPCBreakpoint(0, 0x3456) == kATSimEvent_None);
	AT_PORTABLE_TEST_ASSERT(context, log.mIndices.size() == 2 && log.mIndices[0] == selfClear1 && log.mIndices[1] == selfClear2);
	log.mIndices.clear();
	AT_PORTABLE_TEST_ASSERT(context, h.mManager.TestPCBreakpoint(0, 0x3456) == kATSimEvent_None);
	AT_PORTABLE_TEST_ASSERT(context, log.mIndices.size() == 1 && log.mIndices[0] == selfClear2);
	log.mClearOnIndex = 0;

	AT_PORTABLE_TEST_ASSERT(context, h.mManager.Clear(duplicate));
	AT_PORTABLE_TEST_ASSERT(context, h.mPCMap[0x1234]);
	AT_PORTABLE_TEST_ASSERT(context, !h.mManager.Clear(duplicate));
	const uint32 reused = h.mManager.SetAtPC(0, 0x3333);
	AT_PORTABLE_TEST_ASSERT(context, reused == duplicate);
	AT_PORTABLE_TEST_ASSERT(context, h.mManager.GetInfo(reused, info) && info.mAddress == 0x3333);

	// Removing a pending entry and reusing its ID must not deliver the new
	// breakpoint as if it were the original pending event in this check.
	const uint32 trigger = h.mManager.SetAtPC(0, 0x5678);
	const uint32 victim = h.mManager.SetAtPC(0, 0x5678);
	uint32 replacement = 0;
	log.mIndices.clear();
	log.mpAction = [&](ATBreakpointEvent& ev) {
		if (ev.mIndex == trigger && !replacement) {
			h.mManager.Clear(victim);
			replacement = h.mManager.SetAtPC(0, 0x5678);
		}
	};
	h.mManager.TestPCBreakpoint(0, 0x5678);
	AT_PORTABLE_TEST_ASSERT(context, replacement == victim);
	AT_PORTABLE_TEST_ASSERT(context, log.mIndices == std::vector<uint32>({ trigger }));
	log.mpAction = {};
	log.mIndices.clear();
	h.mManager.TestPCBreakpoint(0, 0x5678);
	AT_PORTABLE_TEST_ASSERT(context, log.mIndices == std::vector<uint32>({ trigger, replacement }));

	// Shutdown during notification releases the bindings and aborts dispatch.
	log.mIndices.clear();
	log.mpAction = [&](ATBreakpointEvent&) { h.mManager.Shutdown(); };
	AT_PORTABLE_TEST_ASSERT(context, h.mManager.TestPCBreakpoint(0, 0x5678) == kATSimEvent_None);
	AT_PORTABLE_TEST_ASSERT(context, log.mIndices == std::vector<uint32>({ trigger }));
	AT_PORTABLE_TEST_ASSERT(context, !h.mpManager && !h.mPCMap[0x5678]);
	AT_PORTABLE_TEST_ASSERT(context, h.mManager.TestPCBreakpoint(0, 0x5678) == kATSimEvent_None);
	return true;
}

bool ATTestAltirraBreakpointInstructions(ATPortableTestContext& context) {
	auto harness = std::make_unique<BreakpointHarness>();
	auto& h = *harness;
	HitLog log;
	log.mpManager = &h.mManager;
	h.mManager.OnBreakpointHit() += log.mDelegate(&log, &HitLog::OnHit);
	const uint32 pcA = h.mManager.SetAtPC(0, 0x1111);
	const uint32 pcB = h.mManager.SetAtPC(0, 0x2222);
	const uint32 insnA = h.mManager.SetInsnBP(0);
	const uint32 insnB = h.mManager.SetInsnBP(0);
	AT_PORTABLE_TEST_ASSERT(context, h.mAll);
	log.mBreakOnIndex = insnA;
	AT_PORTABLE_TEST_ASSERT(context, h.mManager.TestPCBreakpoint(0, 0x4567) == kATSimEvent_CPUPCBreakpoint);
	AT_PORTABLE_TEST_ASSERT(context, log.mIndices.size() == 2 && log.mIndices[0] == insnA && log.mIndices[1] == insnB);
	log.mIndices.clear();
	log.mSilent = true;
	AT_PORTABLE_TEST_ASSERT(context, h.mManager.TestPCBreakpoint(0, 0x4567) == kATSimEvent_AnonymousInterrupt);
	log.mSilent = false;

	log.mClearOnIndex = insnA;
	log.mIndices.clear();
	AT_PORTABLE_TEST_ASSERT(context, h.mManager.TestPCBreakpoint(0, 0x4567) == kATSimEvent_CPUPCBreakpoint);
	AT_PORTABLE_TEST_ASSERT(context, log.mIndices.size() == 2 && log.mIndices[0] == insnA && log.mIndices[1] == insnB);
	log.mClearOnIndex = 0;
	AT_PORTABLE_TEST_ASSERT(context, h.mManager.Clear(insnB));
	AT_PORTABLE_TEST_ASSERT(context, !h.mAll && h.mPCMap[0x1111] && h.mPCMap[0x2222]);
	AT_PORTABLE_TEST_ASSERT(context, h.mManager.TestPCBreakpoint(0, 0x1111) == kATSimEvent_None);
	AT_PORTABLE_TEST_ASSERT(context, h.mManager.TestPCBreakpoint(0, 0x2222) == kATSimEvent_None);
	ATBreakpointInfo info {};
	AT_PORTABLE_TEST_ASSERT(context, h.mManager.GetInfo(pcA, info));
	AT_PORTABLE_TEST_ASSERT(context, h.mManager.Clear(pcB));
	return true;
}

bool ATTestAltirraBreakpointTargets(ATPortableTestContext& context) {
	auto target = std::make_unique<RealDebugTarget>();
	auto replacement = std::make_unique<RealDebugTarget>();
	auto thirdTarget = std::make_unique<RealDebugTarget>();
	auto harness = std::make_unique<BreakpointHarness>();
	auto& h = *harness;
	HitLog log;
	log.mpManager = &h.mManager;
	h.mManager.OnBreakpointHit() += log.mDelegate(&log, &HitLog::OnHit);
	target->mRAM[0x0200] = 0xA9; // LDA #$42 executes before the breakpoint.
	target->mRAM[0x0201] = 0x42;
	target->mRAM[0x0202] = 0xEA;
	ATCPUExecState state {};
	state.m6502.mPC = 0x0200;
	state.m6502.mS = 0xFF;
	state.m6502.mP = 0x20;
	target->SetExecState(state);
	h.mManager.AttachTarget(1, target.get());
	const uint32 bp = h.mManager.SetAtPC(1, 0x0202);
	ATBreakpointInfo info {};
	AT_PORTABLE_TEST_ASSERT(context, h.mManager.GetInfo(bp, info) && info.mTargetIndex == 1 && info.mAddress == 0x0202);
	ATBreakpointIndices ids;
	h.mManager.GetAtPC(1, 0x0202, ids);
	AT_PORTABLE_TEST_ASSERT(context, ids.size() == 1 && ids[0] == bp);
	const uint32 wideAlias = h.mManager.SetAtPC(1, 0x100202);
	h.mManager.GetAtPC(1, 0x100202, ids);
	AT_PORTABLE_TEST_ASSERT(context, ids.size() == 1 && ids[0] == wideAlias);
	h.mManager.GetAtPC(1, 0x0202, ids);
	AT_PORTABLE_TEST_ASSERT(context, ids.size() == 1 && ids[0] == bp);
	log.mBreakOnIndex = bp;
	ATScheduler scheduler;
	scheduler.SetStopTime(scheduler.GetTick() + 32);
	AT_PORTABLE_TEST_ASSERT(context, !target->mCPU.Run(scheduler));
	AT_PORTABLE_TEST_ASSERT(context, target->mCPU.GetA() == 0x42 && target->mCPU.GetPC() == 0x0202);
	AT_PORTABLE_TEST_ASSERT(context, log.mIndices.size() == 1 && log.mIndices[0] == bp);
	AT_PORTABLE_TEST_ASSERT(context, !h.mPosted.empty() && h.mPosted.back() == kATSimEvent_CPUPCBreakpoint);
	AT_PORTABLE_TEST_ASSERT(context, h.mManager.Clear(wideAlias));

	h.mManager.DetachTarget(1);
	AT_PORTABLE_TEST_ASSERT(context, h.mManager.GetInfo(bp, info));
	AT_PORTABLE_TEST_ASSERT(context, h.mManager.Clear(bp));

	replacement->mRAM[0x0300] = 0xEA;
	state.m6502.mPC = 0x0300;
	replacement->SetExecState(state);
	h.mManager.AttachTarget(1, replacement.get());
	const uint32 preserved = h.mManager.SetAtPC(1, 0x0300);
	h.mManager.DetachTarget(1);
	thirdTarget->mRAM[0x0300] = 0xEA;
	thirdTarget->SetExecState(state);
	h.mManager.AttachTarget(1, thirdTarget.get());
	log.mBreakOnIndex = preserved;
	AT_PORTABLE_TEST_ASSERT(context, thirdTarget->mBreakpoints.CheckBP(0x0300));
	AT_PORTABLE_TEST_ASSERT(context, h.mManager.Clear(preserved));
	log.mBreakOnIndex = 0;
	AT_PORTABLE_TEST_ASSERT(context, !thirdTarget->mBreakpoints.CheckBP(0x0300));

	h.mManager.Shutdown();
	h.mManager.Shutdown();
	ATBreakpointManagerConnections c;
	c.mpSetBreakpointManager = [&h](ATBreakpointManager *p) { h.mpManager = p; };
	c.mpSetBreakpoint = [&h](uint16 pc) { h.mPCMap[pc] = true; };
	c.mpClearBreakpoint = [&h](uint16 pc) { h.mPCMap[pc] = false; };
	c.mpSetAllBreakpoints = [&h]() { h.mAll = true; };
	c.mpClearAllBreakpoints = [&h]() { h.mAll = false; };
	c.mpGetXPC = [&h]() { return h.mXPC; };
	c.mpPostInterruptingEvent = [&h](ATSimulatorEvent e) { h.mPosted.push_back(e); };
	h.mManager.Init(std::move(c), nullptr);
	AT_PORTABLE_TEST_ASSERT(context, h.mpManager == &h.mManager);
	h.mManager.Shutdown();
	return true;
}
