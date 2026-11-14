// Production main CPU, memory manager and schedulers; no instruction executor here.
#ifndef f_ATTEST_MAINCPUPORTABLEFIXTURE_H
#define f_ATTEST_MAINCPUPORTABLEFIXTURE_H

#include <initializer_list>
#include <vector>
#include <at/atcore/scheduler.h>
#include <at/atcpu/history.h>
#include <vd2/system/VDString.h>
#include <cpu.h>
#include <cpuhookmanager.h>
#include <bkptmanager.h>
#include <memorymanager.h>

class MainCPUPortableFixture final : public ATCPUEmulatorCallbacks, public IATCPUTimestampDecoderProvider {
public:
	struct BusAccess { uint32 mCycle, mAddress; uint8 mValue; bool mbWrite; };
	struct RAMMapping {
		bool IsKernelROMEnabled() const { return false; }
		bool IsROMOverlayActive() const { return false; }
	};
	MainCPUPortableFixture() : mRAM(0x1000000, 0xEA) {
		mFastScheduler.SetRate(VDFraction(17897725, 10));
		mSlowScheduler.SetRate(VDFraction(17897725, 1140));
		mStartCycle = mFastScheduler.GetTick();
		mMemory.Init();
		mMemory.SetHighMemoryEnabled(true);
		mpRAMLayer = mMemory.CreateLayer(kATMemoryPri_BaseRAM, mRAM.data(), 0, 0x10000, false);
		mMemory.SetLayerFastBus(mpRAMLayer, true);
		mMemory.EnableLayer(mpRAMLayer, true);
		mMemory.SetFastBusEnabled(true);
		mHooks.Init(&mCPU, &mRAMMapping, &mRAMMapping);
		mCPU.Init(&mMemory, &mHooks, this);
		// Initialize bank registers through the real mode-change API.
		mCPU.SetCPUMode(kATCPUMode_65C816, 1);
		mCPU.SetCPUMode(kATCPUMode_6502, 1);
		ATBreakpointManagerConnections c;
		c.mpSetBreakpointManager = [this](ATBreakpointManager *p) { mCPU.SetBreakpointManager(p); };
		c.mpSetBreakpoint = [this](uint16 pc) { mCPU.SetBreakpoint(pc); };
		c.mpClearBreakpoint = [this](uint16 pc) { mCPU.ClearBreakpoint(pc); };
		c.mpSetAllBreakpoints = [this] { mCPU.SetAllBreakpoints(); };
		c.mpClearAllBreakpoints = [this] { mCPU.ClearAllBreakpoints(); };
		c.mpGetXPC = [this] { return mCPU.GetXPC(); };
		c.mpPostInterruptingEvent = [this](ATSimulatorEvent e) { mPostedEvents.push_back(e); };
		mBreakpoints.Init(std::move(c), &mMemory);
		Prepare();
	}
	~MainCPUPortableFixture() {
		mCPU.SetProfiler(nullptr);
		mCPU.SetVerifier(nullptr);
		mCPU.SetHeatMap(nullptr);
		mBreakpoints.Shutdown();
		mHooks.Shutdown();
		if (mpTraceLayer) mMemory.DeleteLayer(mpTraceLayer);
		mMemory.DeleteLayer(mpRAMLayer);
	}
	void Prepare(ATCPUMode mode = kATCPUMode_6502, uint32 pc = 0x2000,
		uint32 subCycles = 1, bool native = false, uint8 p = 0x24) {
		mCPU.SetCPUMode(mode, subCycles);
		mCPU.SetEmulationFlag(!native);
		mCPU.SetP(p);
		mCPU.SetA(0); mCPU.SetX(0); mCPU.SetY(0); mCPU.SetS(0xFD);
		mCPU.SetAH(0); mCPU.SetXH(0); mCPU.SetYH(0); mCPU.SetSH(1);
		mCPU.SetD(0); mCPU.SetK(static_cast<uint8>(pc >> 16)); mCPU.SetB(0);
		mCPU.SetPC(static_cast<uint16>(pc));
		mCPU.GetUnusedCycle();
		mLastEvent = kATSimEvent_None;
		mBus.clear();
	}
	void Load(uint32 address, std::initializer_list<uint8> bytes) {
		for(uint8 b : bytes) mRAM[address++ & 0xFFFFFF] = b;
	}
	void EnableBusTrace() {
		if (mpTraceLayer) return;
		ATMemoryHandlerTable h {};
		h.mbPassReads = h.mbPassWrites = h.mbPassAnticReads = true;
		h.mpThis = this;
		h.mpReadHandler = [](void *p, uint32 a) -> sint32 {
			auto& f = *static_cast<MainCPUPortableFixture *>(p);
			f.mBus.push_back({ f.CPUGetCycle(), a, f.mMemory.RedirectDebugReadByte(a, &f), false });
			return -1;
		};
		h.mpWriteHandler = [](void *p, uint32 a, uint8 v) {
			auto& f = *static_cast<MainCPUPortableFixture *>(p);
			f.mBus.push_back({ f.CPUGetCycle(), a, v, true });
			return false;
		};
		mpTraceLayer = mMemory.CreateLayer(kATMemoryPri_AccessBP - 1, h, 0, 0x10000);
		mMemory.SetLayerTag(mpTraceLayer, this);
		mMemory.SetLayerFastBus(mpTraceLayer, true);
		mMemory.EnableLayer(mpTraceLayer, kATMemoryAccessMode_RW, true);
	}
	int Tick() {
		mCPU.GetUnusedCycle();
		mLastEvent = mCPU.Advance();
		if (!mLastEvent) {
			ATSCHEDULER_ADVANCE(&mFastScheduler);
			if (++mSlowDivider == 114) { mSlowDivider = 0; ATSCHEDULER_ADVANCE(&mSlowScheduler); }
		}
		return mLastEvent;
	}
	bool RunCycles(uint32 count) {
		while(count--) if (Tick()) return false;
		return true;
	}
	// Step by the production debugger boundary. Bounded even for a self-loop.
	int Step(uint32 maxTicks = 256) {
		ATCPUStepCondition c = ATCPUStepCondition::CreateSingleStep();
		c.mIgnorePCStart = mCPU.GetPC(); c.mIgnorePCLength = 1;
		mCPU.AddStepCondition(c);
		const uint32 start = CPUGetCycle();
		while(maxTicks-- && !Tick()) {}
		mCPU.RemoveStepCondition(c);
		return mLastEvent == kATSimEvent_CPUSingleStep ? static_cast<int>(CPUGetCycle() - start) : -1;
	}
	uint32 CPUGetCycle() override { return mFastScheduler.GetTick(); }
	uint32 CPUGetUnhaltedCycle() override { return mFastScheduler.GetTick(); }
	uint32 CPUGetUnhaltedAndRDYCycle() override { return mFastScheduler.GetTick(); }
	void CPUGetHistoryTimes(ATCPUHistoryEntry *he) const override {
		he->mCycle = mFastScheduler.GetTick(); he->mUnhaltedCycle = mFastScheduler.GetTick();
	}
	ATCPUTimestampDecoder GetTimestampDecoder() const override { return { mStartCycle, 0, 262 * 114 }; }

	std::vector<uint8> mRAM;
	ATScheduler mFastScheduler, mSlowScheduler;
	ATMemoryManager mMemory;
	ATCPUHookManager mHooks;
	ATCPUEmulator mCPU;
	ATBreakpointManager mBreakpoints;
	RAMMapping mRAMMapping;
	std::vector<BusAccess> mBus;
	std::vector<ATSimulatorEvent> mPostedEvents;
	int mLastEvent = kATSimEvent_None;
	uint32 mStartCycle = 0;
private:
	ATMemoryLayer *mpRAMLayer = nullptr, *mpTraceLayer = nullptr;
	uint32 mSlowDivider = 0;
};
#endif
