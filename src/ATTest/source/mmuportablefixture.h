// Production CPU, PIA, MMU, scheduler, and memory manager in one test machine.
#ifndef f_ATTEST_MMUPORTABLEFIXTURE_H
#define f_ATTEST_MMUPORTABLEFIXTURE_H
#include <algorithm>
#include <array>
#include <vector>
#include <at/atcore/scheduler.h>
#include <at/atcore/address.h>
#include <cpu.h>
#include <cpuhookmanager.h>
#include <constants.h>
#include <irqcontroller.h>
#include <memorymanager.h>
#include <mmu.h>
#include <pia.h>

class MMUPortableFixture final : public ATCPUEmulatorCallbacks {
public:
	struct NoPBI { bool IsROMOverlayActive() const { return false; } };
	explicit MMUPortableFixture(ATHardwareMode hardware = kATHardwareMode_130XE, ATMemoryMode memory = kATMemoryMode_128K)
		: mRAM(0x200000), mAxlonRAM(0x3FC000) {
		for (size_t bank = 0; bank < mRAM.size() / 0x4000; ++bank)
			std::fill_n(mRAM.data() + bank * 0x4000, 0x4000, static_cast<uint8>(0x20 + bank));
		for (size_t bank = 0; bank < mAxlonRAM.size() / 0x4000; ++bank)
			std::fill_n(mAxlonRAM.data() + bank * 0x4000, 0x4000, static_cast<uint8>(bank + 1));
		mSelfTest.fill(0x5E); mLowerKernel.fill(0xC1); mUpperKernel.fill(0xE2);
		mBasic.fill(0xBA); mGame.fill(0xEA);
		mScheduler.SetRate(VDFraction(17897725, 10));
		mMemory.Init();
		mMemory.SetHighMemoryEnabled(true);
		auto *base = Layer(kATMemoryPri_BaseRAM, mRAM.data(), 0, 0x100, false);
		mMemory.SetLayerAddressSpace(base, kATAddressSpace_RAM);
		mMemory.EnableLayer(base, true);
		mpExt = Layer(kATMemoryPri_ExtRAM, mRAM.data(), 0x40, 0x40, false);
		mpSelfTest = kATHardwareModeTraits[hardware].mbRunsXLOS ? Layer(kATMemoryPri_ROM, mSelfTest.data(), 0x50, 8, true) : nullptr;
		mpLowerKernel = Layer(kATMemoryPri_ROM, mLowerKernel.data(), 0xC0, 0x10, true);
		mpUpperKernel = Layer(kATMemoryPri_ROM, mUpperKernel.data(), 0xD8, 0x28, true);
		mpBasic = Layer(kATMemoryPri_ROM, mBasic.data(), 0xA0, 0x20, true);
		mpGame = hardware == kATHardwareMode_XEGS ? Layer(kATMemoryPri_ROM, mGame.data(), 0xA0, 0x20, true) : nullptr;
		mpHidden = Layer(kATMemoryPri_ExtRAM, mRAM.data() + 0xD000, 0x50, 8, false);
		mMMU.Init(&mMemory);
		Map(hardware, memory);
		mHooks.Init(&mCPU, &mMMU, &mNoPBI);
		mCPU.Init(&mMemory, &mHooks, this);
		mCPU.SetCPUMode(kATCPUMode_65C816, 1); mCPU.SetCPUMode(kATCPUMode_6502, 1);
		mCPU.SetA(0); mCPU.SetX(0); mCPU.SetY(0); mCPU.SetS(0xFD); mCPU.SetP(0x24); mCPU.SetPC(0x2000);
		mIRQ.Init(&mCPU);
		mPIA.Init(&mScheduler);
		mPIA.SetIRQHandler([this](uint32 mask, bool asserted) {
			if (asserted) mIRQ.Assert(mask, true); else mIRQ.Negate(mask, false);
		});
		mPIA.ColdReset();
		mPIASlot = mPIA.AllocOutput([](void *p, uint32 value) {
			static_cast<MMUPortableFixture *>(p)->mMMU.SetBankRegister(static_cast<uint8>(value >> 8));
		}, this, 0xFF00);
		ATMemoryHandlerTable handlers {};
		handlers.mpThis = &mPIA;
		handlers.mpDebugReadHandler = [](void *p, uint32 a) -> sint32 { return static_cast<ATPIAEmulator *>(p)->DebugReadByte(a & 3); };
		handlers.mpReadHandler = [](void *p, uint32 a) -> sint32 { return static_cast<ATPIAEmulator *>(p)->ReadByte(a & 3); };
		handlers.mpWriteHandler = [](void *p, uint32 a, uint8 v) { static_cast<ATPIAEmulator *>(p)->WriteByte(a & 3, v); return true; };
		auto *pia = mMemory.CreateLayer(kATMemoryPri_Hardware, handlers, 0xD3, 1);
		mLayers.push_back(pia); mMemory.EnableLayer(pia, true);
	}
	~MMUPortableFixture() {
		mPIA.FreeOutput(mPIASlot);
		mPIA.SetIRQHandler({});
		mHooks.Shutdown();
		mMMU.Shutdown();
		for (auto *p : mLayers) mMemory.DeleteLayer(p);
	}
	void Map(ATHardwareMode hardware, ATMemoryMode memory) {
		mMMU.InitMapping(hardware, memory, mRAM.data(), mpExt, mpSelfTest,
			mpLowerKernel, mpUpperKernel, mpBasic, mpGame, mpHidden);
	}
	ATMemoryLayer *Layer(int priority, const uint8 *data, uint32 page, uint32 pages, bool ro) {
		auto *p = mMemory.CreateLayer(priority, data, page, pages, ro);
		mLayers.push_back(p); return p;
	}
	bool RunCycles(uint32 n) {
		while (n--) {
			mCPU.GetUnusedCycle();
			if (mCPU.Advance()) return false;
			ATSCHEDULER_ADVANCE(&mScheduler);
		}
		return true;
	}
	uint32 CPUGetCycle() override { return mScheduler.GetTick(); }
	uint32 CPUGetUnhaltedCycle() override { return mScheduler.GetTick(); }
	uint32 CPUGetUnhaltedAndRDYCycle() override { return mScheduler.GetTick(); }
	void CPUGetHistoryTimes(ATCPUHistoryEntry *he) const override { he->mCycle = he->mUnhaltedCycle = mScheduler.GetTick(); }

	std::vector<uint8> mRAM, mAxlonRAM;
	alignas(2) std::array<uint8, 0x800> mSelfTest;
	alignas(2) std::array<uint8, 0x1000> mLowerKernel;
	alignas(2) std::array<uint8, 0x2800> mUpperKernel;
	alignas(2) std::array<uint8, 0x2000> mBasic, mGame;
	ATScheduler mScheduler;
	ATMemoryManager mMemory;
	ATMMUEmulator mMMU;
	ATCPUEmulator mCPU;
	ATCPUHookManager mHooks;
	ATIRQController mIRQ;
	ATPIAEmulator mPIA;
	NoPBI mNoPBI;
	ATMemoryLayer *mpExt, *mpSelfTest, *mpLowerKernel, *mpUpperKernel, *mpBasic, *mpGame, *mpHidden;
private:
	std::vector<ATMemoryLayer *> mLayers;
	int mPIASlot = -1;
};
#endif
