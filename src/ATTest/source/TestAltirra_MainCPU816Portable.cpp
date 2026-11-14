// Portable instruction-level tests for the production 65C816 CPU core.

#include <memory>

#include <at/attest/portabletest.h>
#include "maincpuportablefixture.h"

namespace {
	using FixturePtr = std::unique_ptr<MainCPUPortableFixture>;

	struct RunResult {
		uint16 mPC;
		uint16 mA;
		uint16 mX;
		uint16 mY;
		uint16 mS;
		uint8 mP;
		uint8 mB;
		uint8 mK;
		uint16 mD;
		uint8 mRAM0;
		uint8 mRAM1;
		bool mbAllStepsCompleted;
		std::vector<MainCPUPortableFixture::BusAccess> mBus;
	};

	bool SameState(const RunResult& a, const RunResult& b) {
		return a.mPC==b.mPC && a.mA==b.mA && a.mX==b.mX && a.mY==b.mY
			&& a.mS==b.mS && a.mP==b.mP && a.mB==b.mB && a.mK==b.mK && a.mD==b.mD
			&& a.mRAM0==b.mRAM0 && a.mRAM1==b.mRAM1 && a.mbAllStepsCompleted && b.mbAllStepsCompleted;
	}

	bool SameBus(const RunResult& a, const RunResult& b) {
		if (a.mBus.size() != b.mBus.size())
			return false;
		for (size_t i=0; i<a.mBus.size(); ++i) {
			const auto& x=a.mBus[i]; const auto& y=b.mBus[i];
			if (x.mAddress!=y.mAddress || x.mValue!=y.mValue || x.mbWrite!=y.mbWrite)
				return false;
		}
		return true;
	}

	RunResult RunFastProgram(uint32 subCycles, bool injectDMA, bool forceSlow) {
		FixturePtr f = std::make_unique<MainCPUPortableFixture>();
		f->Prepare(kATCPUMode_65C816, 0x002000, subCycles, true, AT6502::kFlagI | AT6502::kFlagM | AT6502::kFlagX);
		f->Load(0x002000, { 0xC2,0x30,             // REP #$30
			0xA9,0x34,0x12,                         // LDA #$1234
			0x8F,0x00,0x50,0x00,                    // STA $005000
			0xAF,0x00,0x50,0x00,                    // LDA $005000
			0xEA });
		f->EnableBusTrace();
		if (injectDMA) {
			f->mCPU.Advance65816HiSpeed(true);
			ATSCHEDULER_ADVANCE(&f->mFastScheduler);
		}
		if (forceSlow)
			f->mCPU.ForceNextCycleSlow();
		bool allStepsCompleted=true;
		const uint16 expectedPCs[]={ 0x2002, 0x2005, 0x2009, 0x200D };
		for (int i=0; i<4; ++i) {
			const int baseCycles=f->Step();
			allStepsCompleted &= baseCycles >= 0 && f->mCPU.GetPC()==expectedPCs[i];
		}
		return {
			f->mCPU.GetPC(), (uint16)(f->mCPU.GetA() | ((uint16)f->mCPU.GetAH()<<8)),
			(uint16)(f->mCPU.GetX() | ((uint16)f->mCPU.GetXH()<<8)),
			(uint16)(f->mCPU.GetY() | ((uint16)f->mCPU.GetYH()<<8)),
			f->mCPU.GetS16(), f->mCPU.GetP(), f->mCPU.GetB(), f->mCPU.GetK(), f->mCPU.GetD(),
			f->mRAM[0x5000], f->mRAM[0x5001], allStepsCompleted, f->mBus
		};
	}

	struct SlowRAMContext {
		MainCPUPortableFixture *mpFixture;
		uint32 mReadCount=0;
	};

	struct MemoryLayerGuard {
		ATMemoryManager& mMemory;
		ATMemoryLayer *mpLayer;
		~MemoryLayerGuard() { if (mpLayer) mMemory.DeleteLayer(mpLayer); }
	};
}

bool ATTestAltirraMainCPU816(ATPortableTestContext& context) {
	FixturePtr f = std::make_unique<MainCPUPortableFixture>();
	// Emulation mode keeps accumulator and indexes 8-bit and forces M/X flags.
	f->Prepare(kATCPUMode_65C816, 0x002000, 1, false, 0);
	f->Load(0x002000, { 0xA9,0x7A, 0xA2,0x45 });
	AT_PORTABLE_TEST_ASSERT(context, f->Step() > 0);
	AT_PORTABLE_TEST_ASSERT(context, f->mCPU.GetA()==0x7A && f->mCPU.GetAH()==0);
	AT_PORTABLE_TEST_ASSERT(context, f->mCPU.GetP() & AT6502::kFlagM);
	AT_PORTABLE_TEST_ASSERT(context, f->mCPU.GetP() & AT6502::kFlagX);
	AT_PORTABLE_TEST_ASSERT(context, f->Step() > 0 && f->mCPU.GetX()==0x45 && f->mCPU.GetXH()==0);

	// Native M/X widths are changed by REP/SEP and affect following immediates.
	f->Prepare(kATCPUMode_65C816, 0x002100, 1, true, AT6502::kFlagI|AT6502::kFlagM|AT6502::kFlagX);
	f->Load(0x002100, { 0xC2,0x30, 0xA9,0x34,0x12, 0xA2,0x78,0x56, 0xE2,0x30, 0xA9,0x9A });
	AT_PORTABLE_TEST_ASSERT(context, f->Step() > 0 && !(f->mCPU.GetP() & (AT6502::kFlagM|AT6502::kFlagX)));
	AT_PORTABLE_TEST_ASSERT(context, f->Step() > 0 && f->mCPU.GetA()==0x34 && f->mCPU.GetAH()==0x12);
	AT_PORTABLE_TEST_ASSERT(context, f->Step() > 0 && f->mCPU.GetX()==0x78 && f->mCPU.GetXH()==0x56);
	AT_PORTABLE_TEST_ASSERT(context, f->Step() > 0 && (f->mCPU.GetP() & (AT6502::kFlagM|AT6502::kFlagX))==(AT6502::kFlagM|AT6502::kFlagX));
	AT_PORTABLE_TEST_ASSERT(context, f->Step() > 0 && f->mCPU.GetA()==0x9A);

	// XCE leaves emulation mode by exchanging E with carry.
	f->Prepare(kATCPUMode_65C816, 0x002200, 1, false, AT6502::kFlagM|AT6502::kFlagX);
	f->Load(0x002200, { 0xFB });
	AT_PORTABLE_TEST_ASSERT(context, f->Step() > 0 && !f->mCPU.GetEmulationFlag() && (f->mCPU.GetP()&AT6502::kFlagC));

	// SetP selects the current decode width and only truncates indexes in 8-bit mode.
	f->Prepare(kATCPUMode_65C816, 0x002300, 1, true, AT6502::kFlagM|AT6502::kFlagX);
	f->mCPU.SetP(0);
	f->mCPU.SetXH(0x12);
	f->Load(0x002300, { 0xA2,0x34,0x56 });
	AT_PORTABLE_TEST_ASSERT(context, f->Step() > 0 && f->mCPU.GetX()==0x34 && f->mCPU.GetXH()==0x56);
	f->mCPU.SetP(AT6502::kFlagC);
	AT_PORTABLE_TEST_ASSERT(context, f->mCPU.GetXH()==0x56);
	f->mCPU.SetP(AT6502::kFlagC|AT6502::kFlagX);
	AT_PORTABLE_TEST_ASSERT(context, f->mCPU.GetXH()==0);
	f->mCPU.SetEmulationFlag(true);
	AT_PORTABLE_TEST_ASSERT(context, (f->mCPU.GetP() & 0x30) == 0x30);
	f->Load(f->mCPU.GetPC(), { 0xA9, 0x7B });
	AT_PORTABLE_TEST_ASSERT(context, f->Step() == 2 && f->mCPU.GetA() == 0x7B);

	// W65C816 absolute indexed data references carry across the bank boundary.
	f->Prepare(kATCPUMode_65C816, 0x002400, 1, true, AT6502::kFlagI|AT6502::kFlagM|AT6502::kFlagX);
	f->mCPU.SetB(0x23); f->mCPU.SetX(1); f->mRAM[0x240000]=0xA5;
	f->Load(0x002400, { 0xBD,0xFF,0xFF }); // LDA $FFFF,X
	f->EnableBusTrace();
	AT_PORTABLE_TEST_ASSERT(context, f->Step()>0 && f->mCPU.GetA()==0xA5);
	bool sawDBKRead=false;
	for (const auto& access : f->mBus) if (!access.mbWrite && access.mAddress==0x240000) sawDBKRead=true;
	AT_PORTABLE_TEST_ASSERT(context, sawDBKRead);

	// Direct-page long-indirect pointers use bank zero; stack-relative data stays in bank zero.
	f->Prepare(kATCPUMode_65C816, 0x002480, 1, true, AT6502::kFlagI|AT6502::kFlagM|AT6502::kFlagX);
	f->mCPU.SetD(0x1200);
	f->Load(0x002480, { 0xA7,0x10 }); // LDA [$10]
	f->mRAM[0x1210]=0x56; f->mRAM[0x1211]=0x34; f->mRAM[0x1212]=0x12; f->mRAM[0x123456]=0xB8;
	f->EnableBusTrace();
	AT_PORTABLE_TEST_ASSERT(context, f->Step()>0 && f->mCPU.GetA()==0xB8);
	bool sawDP0=false, sawDP1=false, sawDP2=false, sawLongData=false;
	for (const auto& access : f->mBus) if (!access.mbWrite) {
		sawDP0 |= access.mAddress==0x1210; sawDP1 |= access.mAddress==0x1211;
		sawDP2 |= access.mAddress==0x1212; sawLongData |= access.mAddress==0x123456;
	}
	AT_PORTABLE_TEST_ASSERT(context, sawDP0 && sawDP1 && sawDP2 && sawLongData);
	// Changing D's low byte through the debugger must select the extra-cycle decoder.
	f->Prepare(kATCPUMode_65C816, 0x002480, 1, true, AT6502::kFlagI|AT6502::kFlagM|AT6502::kFlagX);
	f->mCPU.SetD(0x1201);
	f->Load(0x1211, { 0x56, 0x34, 0x12 });
	AT_PORTABLE_TEST_ASSERT(context, f->Step() == 7 && f->mCPU.GetA() == 0xB8);
	f->Prepare(kATCPUMode_65C816, 0x002490, 1, true, AT6502::kFlagI|AT6502::kFlagM|AT6502::kFlagX);
	f->mCPU.SetS(0xF0); f->mCPU.SetSH(0x02); f->mRAM[0x02F5]=0xD3;
	f->Load(0x002490, { 0xA3,0x05 }); // LDA $05,S
	AT_PORTABLE_TEST_ASSERT(context, f->Step()>0 && f->mCPU.GetA()==0xD3);

	f->Prepare(kATCPUMode_65C816, 0x002500, 1, true, AT6502::kFlagI|AT6502::kFlagM|AT6502::kFlagX);
	f->Load(0x002500, { 0xAF,0x56,0x34,0x12, 0x8F,0x57,0x34,0x12 });
	f->mRAM[0x123456]=0xC7;
	AT_PORTABLE_TEST_ASSERT(context, f->Step()>0 && f->mCPU.GetA()==0xC7);
	AT_PORTABLE_TEST_ASSERT(context, f->Step()>0 && f->mRAM[0x123457]==0xC7);

	// Native JSL/RTL push and restore the banked return address on the 16-bit stack.
	f->Prepare(kATCPUMode_65C816, 0x012600, 1, true, AT6502::kFlagI|AT6502::kFlagM|AT6502::kFlagX);
	f->Load(0x012600, { 0x22,0x00,0x30,0x02 });
	f->Load(0x023000, { 0x6B });
	f->EnableBusTrace();
	AT_PORTABLE_TEST_ASSERT(context, f->Step()>0 && f->mCPU.GetPC()==0x3000 && f->mCPU.GetK()==0x02);
	AT_PORTABLE_TEST_ASSERT(context, f->mRAM[0x01FD]==0x01 && f->mRAM[0x01FC]==0x26 && f->mRAM[0x01FB]==0x03);
	AT_PORTABLE_TEST_ASSERT(context, f->Step()>0 && f->mCPU.GetPC()==0x2604 && f->mCPU.GetK()==0x01);
	return true;
}

bool ATTestAltirraMainCPUAcceleration(ATPortableTestContext& context) {
	const RunResult normal = RunFastProgram(1, false, false);
	const RunResult fast = RunFastProgram(4, false, false);
	const RunResult dma = RunFastProgram(4, true, false);
	const RunResult forcedSlow = RunFastProgram(4, false, true);
	AT_PORTABLE_TEST_ASSERT(context, normal.mPC==fast.mPC && normal.mA==fast.mA && normal.mX==fast.mX && normal.mY==fast.mY);
	AT_PORTABLE_TEST_ASSERT(context, normal.mS==fast.mS && normal.mP==fast.mP && normal.mRAM0==fast.mRAM0 && normal.mRAM1==fast.mRAM1);
	AT_PORTABLE_TEST_ASSERT(context, SameBus(normal,fast));
	AT_PORTABLE_TEST_ASSERT(context, fast.mPC==dma.mPC && fast.mA==dma.mA && fast.mRAM0==dma.mRAM0 && fast.mRAM1==dma.mRAM1);
	AT_PORTABLE_TEST_ASSERT(context, SameBus(fast,dma));
	AT_PORTABLE_TEST_ASSERT(context, fast.mPC==forcedSlow.mPC && fast.mA==forcedSlow.mA && fast.mRAM0==forcedSlow.mRAM0 && fast.mRAM1==forcedSlow.mRAM1);
	AT_PORTABLE_TEST_ASSERT(context, SameBus(fast,forcedSlow));
	AT_PORTABLE_TEST_ASSERT(context, normal.mbAllStepsCompleted && normal.mPC==0x200D && normal.mA==0x1234);
	AT_PORTABLE_TEST_ASSERT(context, normal.mRAM0==0x34 && normal.mRAM1==0x12);
	AT_PORTABLE_TEST_ASSERT(context, normal.mX==0 && normal.mY==0 && normal.mS==0x01FD && normal.mP==AT6502::kFlagI);
	AT_PORTABLE_TEST_ASSERT(context, normal.mB==0 && normal.mK==0 && normal.mD==0);
	AT_PORTABLE_TEST_ASSERT(context, SameState(normal,fast) && SameState(fast,dma) && SameState(fast,forcedSlow));

	// A non-fast handler forces the production -256 retry path, then falls through to RAM.
	FixturePtr slow = std::make_unique<MainCPUPortableFixture>();
	slow->Prepare(kATCPUMode_65C816, 0x002800, 4, true, AT6502::kFlagI|AT6502::kFlagM|AT6502::kFlagX);
	slow->Load(0x002800, { 0xAD,0x00,0x40 });
	slow->mRAM[0x4000]=0x6D;
	SlowRAMContext slowRAM { slow.get() };
	ATMemoryHandlerTable h {};
	h.mpThis=&slowRAM;
	h.mpReadHandler=[](void *p,uint32 address)->sint32 {
		auto& ctx=*static_cast<SlowRAMContext *>(p);
		++ctx.mReadCount;
		return ctx.mpFixture->mRAM[address & 0xFFFFFF];
	};
	auto *layer=slow->mMemory.CreateLayer(kATMemoryPri_Hardware,h,0x40,1);
	MemoryLayerGuard layerGuard { slow->mMemory, layer };
	slow->mMemory.EnableLayer(layer,true);
	slow->EnableBusTrace();
	AT_PORTABLE_TEST_ASSERT(context, slow->Step()>=0 && slow->mCPU.GetA()==0x6D);
	AT_PORTABLE_TEST_ASSERT(context, slow->mCPU.GetPC()==0x2803);
	AT_PORTABLE_TEST_ASSERT(context, slowRAM.mReadCount>0);
	return true;
}
