// MMU tests against real memory layers, including CPU-driven PIA banking.
#include <algorithm>
#include <array>
#include <iterator>
#include <memory>
#include <vector>
#include <at/attest/portabletest.h>
#include "mmuportablefixture.h"

bool ATTestAltirraMMUROM(ATPortableTestContext& context) {
	for (uint32 hw = 0; hw < kATHardwareModeCount; ++hw) {
		auto f = std::make_unique<MMUPortableFixture>(static_cast<ATHardwareMode>(hw), kATMemoryMode_64K);
		for (uint32 pb = 0; pb < 256; ++pb) {
			f->mMMU.SetBankRegister(static_cast<uint8>(pb));
			const bool kernel = !kATHardwareModeTraits[hw].mbRunsXLOS || (pb & 1);
			const bool basic = kATHardwareModeTraits[hw].mbInternalBASIC && !(pb & 2);
			const bool self = f->mpSelfTest && ((pb & 0x81) == 1);
			const bool game = hw == kATHardwareMode_XEGS && !basic && !(pb & 0x40);
			const bool hidden = (pb & 0xB1) == 0x30;
			ATMemoryMapState state {}; f->mMMU.GetMemoryMapState(state);
			AT_PORTABLE_TEST_ASSERT(context, state.mbKernelEnabled == kernel && state.mbBASICEnabled == basic);
			AT_PORTABLE_TEST_ASSERT(context, state.mbSelfTestEnabled == self && state.mbGameEnabled == game);
			AT_PORTABLE_TEST_ASSERT(context, !state.mbExtendedCPU && !state.mbExtendedANTIC);
			AT_PORTABLE_TEST_ASSERT(context, f->mMMU.IsKernelROMEnabled() == kernel && f->mMMU.IsSelfTestROMEnabled() == self);
			AT_PORTABLE_TEST_ASSERT(context, f->mMMU.IsBASICOrGameROMEnabled() == (basic || game));
			AT_PORTABLE_TEST_ASSERT(context, f->mMemory.ReadByte(0xC000) == (kernel ? 0xC1 : f->mRAM[0xC000]));
			AT_PORTABLE_TEST_ASSERT(context, f->mMemory.ReadByte(0xD800) == (kernel ? 0xE2 : f->mRAM[0xD800]));
			const uint8 cartridge = basic ? 0xBA : game ? 0xEA : f->mRAM[0xA000];
			AT_PORTABLE_TEST_ASSERT(context, f->mMemory.ReadByte(0xA000) == cartridge && f->mMemory.AnticReadByte(0xA000) == cartridge);
			AT_PORTABLE_TEST_ASSERT(context, f->mMemory.ReadByte(0x5000) == (self ? 0x5E : hidden ? f->mRAM[0xD000] : f->mRAM[0x5000]));
			f->mMemory.WriteByte(0xC000, 0x55);
			AT_PORTABLE_TEST_ASSERT(context, f->mLowerKernel[0] == 0xC1);
			if (!kernel) AT_PORTABLE_TEST_ASSERT(context, f->mRAM[0xC000] == 0x55);
			f->mRAM[0xC000] = 0x23;
		}
	}
	return true;
}

bool ATTestAltirraMMUBanking(ATPortableTestContext& context) {
	// Specify physical line wiring, independently of the production encoded table.
	// The backing offsets retain the original memory layout shared with VBXE.
	struct Wiring { ATMemoryMode mMode; std::array<uint8, 6> mLines; uint8 mCount; uint32 mBase; bool mbInverted, mbSeparateAntic, mbBlockBasic, mbBlockSelf; };
	const Wiring cases[] = {
		{ kATMemoryMode_128K, {2,3}, 2, 0x50000, true, true, false, false },
		{ kATMemoryMode_256K, {2,3,5,6}, 4, 0, false, false, false, false },
		{ kATMemoryMode_320K, {2,3,5,6}, 4, 0x50000, true, false, false, false },
		{ kATMemoryMode_576K, {2,3,5,6,1}, 5, 0x10000, true, false, true, false },
		{ kATMemoryMode_1088K, {2,3,5,6,1,7}, 6, 0x10000, true, false, true, true },
		{ kATMemoryMode_320K_Compy, {2,3,6,7}, 4, 0x50000, true, true, false, true },
		{ kATMemoryMode_576K_Compy, {2,3,6,7,1}, 5, 0x10000, true, true, true, true },
	};
	for (const auto& w : cases) {
		auto f = std::make_unique<MMUPortableFixture>(kATHardwareMode_130XE, w.mMode);
		for (uint32 pb = 0; pb < 256; ++pb) {
			uint32 bank = 0;
			for (uint32 i = 0; i < w.mCount; ++i)
				if (((pb >> w.mLines[i]) & 1) != w.mbInverted) bank |= 1U << i;
			const bool cpu = !(pb & 0x10), antic = !(pb & (w.mbSeparateAntic ? 0x20 : 0x10));
			const bool any = cpu || antic;
			const uint32 physical = w.mBase + bank * 0x4000;
			f->mMMU.SetBankRegister(static_cast<uint8>(pb));
			ATMemoryMapState state {}; f->mMMU.GetMemoryMapState(state);
			AT_PORTABLE_TEST_ASSERT(context, state.mbExtendedCPU == cpu && state.mbExtendedANTIC == antic);
			AT_PORTABLE_TEST_ASSERT(context, state.mExtendedBank == (any ? physical / 0x4000 : 0));
			AT_PORTABLE_TEST_ASSERT(context, state.mbBASICEnabled == (!(pb & 2) && !(w.mbBlockBasic && any)));
			AT_PORTABLE_TEST_ASSERT(context, state.mbSelfTestEnabled == (((pb & 0x81) == 1) && !(w.mbBlockSelf && any)));
			AT_PORTABLE_TEST_ASSERT(context, f->mMMU.ExtBankToMemoryOffset(pb) == (any ? physical : 0x4000));
			AT_PORTABLE_TEST_ASSERT(context, f->mMMU.GetCPUBankBase() == (cpu ? physical : 0));
			AT_PORTABLE_TEST_ASSERT(context, f->mMMU.GetAnticBankBase() == (antic ? physical : 0));
			const uint32 cpuOffset = cpu ? physical : 0x4000, anticOffset = antic ? physical : 0x4000;
			AT_PORTABLE_TEST_ASSERT(context, f->mMemory.ReadByte(0x4000) == f->mRAM[cpuOffset]);
			AT_PORTABLE_TEST_ASSERT(context, f->mMemory.AnticReadByte(0x4000) == f->mRAM[anticOffset]);
			const uint8 saved = f->mRAM[cpuOffset];
			f->mMemory.WriteByte(0x4000, static_cast<uint8>(0xA5 ^ pb));
			AT_PORTABLE_TEST_ASSERT(context, f->mRAM[cpuOffset] == static_cast<uint8>(0xA5 ^ pb));
			AT_PORTABLE_TEST_ASSERT(context, f->mMemory.AnticReadByte(0x4000) == f->mRAM[anticOffset]);
			f->mRAM[cpuOffset] = saved;
		}
	}
	return true;
}

bool ATTestAltirraMMUConnections(ATPortableTestContext& context) {
	auto f = std::make_unique<MMUPortableFixture>();
	std::vector<uint8> notified;
	bool coherent = true;
	vdfunction<void()> hook = [&] {
		notified.push_back(f->mMMU.GetBankRegister());
		coherent &= f->mMemory.ReadByte(0xC000) == (f->mMMU.IsKernelROMEnabled() ? 0xC1 : f->mRAM[0xC000]);
	};
	struct Detach { ATMMUEmulator& m; const vdfunction<void()> *f; ~Detach() { m.RemoveROMMappingHook(f); } } detach { f->mMMU, &hook };
	f->mMMU.AddROMMappingHook(&hook);
	AT_PORTABLE_TEST_ASSERT(context, notified.size() == 1 && notified[0] == 0xFF);
	f->mMMU.SetBankRegister(0xFE);
	f->mMMU.SetBankRegister(0xFE);
	AT_PORTABLE_TEST_ASSERT(context, notified.size() == 2 && coherent);
	f->mMMU.SetBankRegister(0xEF);
	f->mMMU.SetModeOverrides(kATMemoryMode_576K, false);
	AT_PORTABLE_TEST_ASSERT(context, f->mMMU.GetCPUBankBase() == 0x10000 && f->mMMU.GetAnticBankBase() == 0x10000);
	f->mMMU.ClearModeOverrides();
	AT_PORTABLE_TEST_ASSERT(context, f->mMMU.GetCPUBankBase() == 0x50000 && f->mMMU.GetAnticBankBase() == 0);
	f->mMMU.SetBankRegister(0xFF);
	// forceBASIC supplies BASIC on models without it; PORTB still controls visibility.
	auto optionalBasic = std::make_unique<MMUPortableFixture>(kATHardwareMode_1200XL, kATMemoryMode_64K);
	optionalBasic->mMMU.SetBankRegister(0xFD);
	AT_PORTABLE_TEST_ASSERT(context, !optionalBasic->mMMU.IsBASICROMEnabled());
	optionalBasic->mMMU.SetModeOverrides(-1, true);
	AT_PORTABLE_TEST_ASSERT(context, optionalBasic->mMMU.IsBASICROMEnabled() && optionalBasic->mMemory.ReadByte(0xA000) == 0xBA);
	optionalBasic->mMMU.SetBankRegister(0xFF);
	AT_PORTABLE_TEST_ASSERT(context, !optionalBasic->mMMU.IsBASICROMEnabled());
	optionalBasic->mMMU.SetBankRegister(0xFD);
	optionalBasic->mMMU.ClearModeOverrides();
	AT_PORTABLE_TEST_ASSERT(context, !optionalBasic->mMMU.IsBASICROMEnabled() && coherent);

	// Real 6502 code configures DDRB and PORTB on the real PIA, switches two
	// extended banks, then returns to base RAM. ANTIC remains on its base bank.
	const uint8 code[] = { 0xA9,0x00,0x8D,0x03,0xD3, 0xA9,0xFF,0x8D,0x01,0xD3,
		0xA9,0x04,0x8D,0x03,0xD3, 0xA9,0xEF,0x8D,0x01,0xD3, 0xA9,0x5A,0x8D,0x00,0x40,
		0xA9,0xEB,0x8D,0x01,0xD3, 0xA9,0xA5,0x8D,0x00,0x40, 0xA9,0xEF,0x8D,0x01,0xD3,
		0xAD,0x00,0x40,0x8D,0x00,0x30, 0xA9,0xFF,0x8D,0x01,0xD3, 0xAD,0x00,0x40,0x8D,0x01,0x30,0xEA };
	std::copy(std::begin(code), std::end(code), f->mRAM.begin() + 0x2000);
	AT_PORTABLE_TEST_ASSERT(context, f->RunCycles(30));
	AT_PORTABLE_TEST_ASSERT(context, f->mMemory.ReadByte(0x4000) == 0x5A && f->mMemory.AnticReadByte(0x4000) == 0x21);
	AT_PORTABLE_TEST_ASSERT(context, f->RunCycles(12));
	AT_PORTABLE_TEST_ASSERT(context, f->mMemory.ReadByte(0x4000) == 0xA5 && f->mMemory.AnticReadByte(0x4000) == 0x21);
	AT_PORTABLE_TEST_ASSERT(context, f->RunCycles(28));
	AT_PORTABLE_TEST_ASSERT(context, f->mMMU.GetBankRegister() == 0xFF && f->mPIA.GetPortBOutput() == 0xFF);
	AT_PORTABLE_TEST_ASSERT(context, f->mRAM[0x50000] == 0x5A && f->mRAM[0x54000] == 0xA5);
	AT_PORTABLE_TEST_ASSERT(context, f->mRAM[0x3000] == 0x5A && f->mRAM[0x3001] == 0x21);

	// The real 65C816 uses the MMU's linear bank-one mapping for long accesses.
	f->mMMU.SetHighMemory(2, f->mRAM.data() + 0x100000);
	f->mCPU.SetCPUMode(kATCPUMode_65C816, 1); f->mCPU.SetPC(0x2100);
	const uint8 code816[] = { 0xA9,0xC7,0x8F,0x23,0x01,0x01,0xAF,0x23,0x01,0x01,0x8D,0x02,0x30,0xEA };
	std::copy(std::begin(code816), std::end(code816), f->mRAM.begin() + 0x2100);
	AT_PORTABLE_TEST_ASSERT(context, f->RunCycles(16));
	AT_PORTABLE_TEST_ASSERT(context, f->mRAM[0x100123] == 0xC7 && f->mRAM[0x3002] == 0xC7);
	uint32 selfCalls = 0;
	bool removeSelf = false;
	vdfunction<void()> self;
	self = [&] { ++selfCalls; if (removeSelf) f->mMMU.RemoveROMMappingHook(&self); };
	Detach selfDetach { f->mMMU, &self };
	f->mMMU.AddROMMappingHook(&self);
	removeSelf = true;
	f->mMMU.SetBankRegister(0xFE);
	f->mMMU.SetBankRegister(0xFF);
	AT_PORTABLE_TEST_ASSERT(context, selfCalls == 2 && coherent);
	return true;
}

bool ATTestAltirraMMULifecycle(ATPortableTestContext& context) {
	auto f = std::make_unique<MMUPortableFixture>(kATHardwareMode_130XE, kATMemoryMode_64K);
	// Expansion settings can arrive while the external memory layers are detached.
	// No pointer into missing RAM may be formed until mapping is initialized again.
	f->mMMU.ShutdownMapping();
	f->mMMU.SetModeOverrides(kATMemoryMode_128K, false);
	f->mMMU.SetBankRegister(0xEF);
	f->Map(kATHardwareMode_130XE, kATMemoryMode_64K);
	AT_PORTABLE_TEST_ASSERT(context, f->mMMU.GetCPUBankBase() == 0x50000);
	AT_PORTABLE_TEST_ASSERT(context, f->mMemory.ReadByte(0x4000) == f->mRAM[0x50000]);
	f->mMMU.ClearModeOverrides();
	AT_PORTABLE_TEST_ASSERT(context, f->mMMU.GetCPUBankBase() == 0 && f->mMemory.ReadByte(0x4000) == f->mRAM[0x4000]);
	f->mMMU.SetAxlonMemory(2, true, f->mAxlonRAM.data());
	f->mMMU.SetAxlonBank(2);
	f->mMMU.SetHighMemory(1, f->mRAM.data() + 0x100000);
	f->mMMU.Shutdown();
	f->mMMU.Shutdown();
	AT_PORTABLE_TEST_ASSERT(context, f->mMemory.ExtReadByte(0x0100, 1) == 0xFF);
	const uint8 bank = f->mMMU.GetAxlonBank();
	f->mMemory.WriteByte(0xCFF0, 3);
	AT_PORTABLE_TEST_ASSERT(context, f->mMMU.GetAxlonBank() == bank);
	f->mMMU.Init(&f->mMemory);
	f->Map(kATHardwareMode_130XE, kATMemoryMode_128K);
	f->mMMU.SetBankRegister(0xEF);
	AT_PORTABLE_TEST_ASSERT(context, f->mMMU.GetCPUBankBase() == 0x50000 && f->mMMU.GetAnticBankBase() == 0);
	AT_PORTABLE_TEST_ASSERT(context, f->mMemory.ReadByte(0x4000) == f->mRAM[0x50000]);
	return true;
}
