// Compare boot acceleration against the same routines executed by a real 6502.
#include <array>
#include <memory>
#include <vd2/system/vdtypes.h>
#include <at/attest/portabletest.h>
#include <at/atcore/ksyms.h>
#include <hlefastboothook.h>
#include <cpuheatmap.h>
#include "mmuportablefixture.h"

namespace {
	constexpr uint8 kCheck[] = {0xA9,0xFF,0x91,4,0xD1,4,0xF0,2,0x46,1,0xA9,0,0x91,4,0xD1,4,0xF0,2,0x46,1,0xC8,0xD0,0xE9};
	constexpr uint8 kClear[] = {0x91,0x66,0xC8,0xD0,0xFB,0xE6,0x67,0xCA,0xD0,0xF6};
	constexpr uint8 kSum[] = {0xA0,0,0x18,0xB1,0x9E,0x65,0x8B,0x85,0x8B,0x90,2,0xE6,0x8C,0xE6,0x9E,0xD0,2,0xE6,0x9F,0xA5,0x9E,0xC5,0xA0,0xD0,0xE9,0xA5,0x9F,0xC5,0xA1,0xD0,0xE3};
	struct BootMachine {
		explicit BootMachine(bool accelerate) : mHook(nullptr, ATDestroyHLEFastBootHook) {
			mHost.mLowerKernel.fill(0xEA); mHost.mUpperKernel.fill(0xEA);
			std::copy(std::begin(kCheck), std::end(kCheck), mHost.mLowerKernel.begin() + 0x2E4);
			std::copy(std::begin(kClear), std::end(kClear), mHost.mUpperKernel.begin() + 0xC00);
			std::copy(std::begin(kSum), std::end(kSum), mHost.mUpperKernel.begin() + 0x27B7);
			mHost.mHooks.EnableOSHooks(true); mHost.mMMU.SetBankRegister(0xFF);
			if (accelerate) mHook.reset(ATCreateHLEFastBootHook(&mHost.mCPU));
			mHost.mHooks.CallInitHooks(mHost.mLowerKernel.data(), mHost.mUpperKernel.data());
		}
		void Word(uint16 a, uint16 v) { mHost.mMemory.WriteByte(a, static_cast<uint8>(v)); mHost.mMemory.WriteByte(a + 1, static_cast<uint8>(v >> 8)); }
		bool Run(uint16 pc, uint16 end, uint8 a, uint8 x, uint8 y, uint8 p = 0x24) {
			auto& cpu = mHost.mCPU; bool stopped = false;
			auto *stop = mHost.mHooks.AddHook(kATCPUHookMode_Always, end, 100, [&](uint16) { stopped = true; return uint8(0xEA); });
			cpu.SetA(a); cpu.SetX(x); cpu.SetY(y); cpu.SetP(p); cpu.SetPC(pc);
			for (uint32 i = 0; !stopped && i < 3000000; ++i) {
				if (!mHost.RunCycles(1)) { mHost.mHooks.RemoveHook(stop); return false; }
			}
			mHost.mHooks.RemoveHook(stop); return stopped;
		}
		bool SameCPU(const BootMachine& b) const {
			const auto& x = mHost.mCPU; const auto& y = b.mHost.mCPU;
			return x.GetA() == y.GetA() && x.GetX() == y.GetX() && x.GetY() == y.GetY() && x.GetP() == y.GetP() && x.GetS() == y.GetS();
		}
		MMUPortableFixture mHost;
		std::unique_ptr<ATHLEFastBootHook, void(*)(ATHLEFastBootHook *)> mHook;
	};
}

bool ATTestAltirraFastBootClear(ATPortableTestContext& context) {
	for (uint16 address : {0x3000, 0x30F1, 0xFFFF})
		for (uint8 y : {uint8(0), uint8(17), uint8(253)}) {
			auto a = std::make_unique<BootMachine>(true); auto b = std::make_unique<BootMachine>(false);
			a->Word(ATKernelSymbols::TOADR, address); b->Word(ATKernelSymbols::TOADR, address);
			AT_PORTABLE_TEST_ASSERT(context, a->Run(0xE400, 0xE40A, 0x5A, 2, y));
			AT_PORTABLE_TEST_ASSERT(context, b->Run(0xE400, 0xE40A, 0x5A, 2, y));
			AT_PORTABLE_TEST_ASSERT(context, std::equal(a->mHost.mRAM.begin(), a->mHost.mRAM.begin() + 65536, b->mHost.mRAM.begin()));
			AT_PORTABLE_TEST_ASSERT(context, a->SameCPU(*b));
		}
	return true;
}

bool ATTestAltirraFastBootMemCheck(ATPortableTestContext& context) {
	for (uint16 address : {0x3000, 0x30FD, 0xC300, 0xFFFF})
		for (uint8 y : {uint8(0), uint8(1), uint8(254)}) {
			auto a = std::make_unique<BootMachine>(true); auto b = std::make_unique<BootMachine>(false);
			a->Word(4, address); b->Word(4, address);
			AT_PORTABLE_TEST_ASSERT(context, a->Run(0xC2E4, 0xC2FB, 0x71, 0x23, y));
			AT_PORTABLE_TEST_ASSERT(context, b->Run(0xC2E4, 0xC2FB, 0x71, 0x23, y));
			AT_PORTABLE_TEST_ASSERT(context, std::equal(a->mHost.mRAM.begin(), a->mHost.mRAM.begin() + 65536, b->mHost.mRAM.begin()));
			AT_PORTABLE_TEST_ASSERT(context, a->SameCPU(*b));
		}
	return true;
}

bool ATTestAltirraFastBootChecksum(ATPortableTestContext& context) {
	for (const auto range : {std::pair<uint16,uint16>{0x3000,0x3203}, {0xFFFC,3}, {0x3100,0x3101}}) {
		for (uint8 flags : {uint8(0x24), uint8(0x2C)}) {
			auto a = std::make_unique<BootMachine>(true); auto b = std::make_unique<BootMachine>(false);
			for (auto *m : {a.get(), b.get()}) { m->Word(0x9E, range.first); m->Word(0xA0, range.second); m->Word(0x8B, 0xFF71); }
			AT_PORTABLE_TEST_ASSERT(context, a->Run(0xFFB7, 0xFFD6, 0x91, 0x3A, 0x27, flags));
			AT_PORTABLE_TEST_ASSERT(context, b->Run(0xFFB7, 0xFFD6, 0x91, 0x3A, 0x27, flags));
			AT_PORTABLE_TEST_ASSERT(context, std::equal(a->mHost.mRAM.begin(), a->mHost.mRAM.begin() + 65536, b->mHost.mRAM.begin()));
			AT_PORTABLE_TEST_ASSERT(context, a->SameCPU(*b));
		}
	}
	return true;
}

bool ATTestAltirraFastBootLifecycle(ATPortableTestContext& context) {
	auto a = std::make_unique<BootMachine>(true); auto& h = a->mHost;
	// The accelerated memory test marks written bytes as initialized.
	auto heat = std::make_unique<ATCPUHeatMap>(); heat->ResetMemoryRange(0x3000, 256);
	h.mCPU.SetHeatMap(heat.get()); a->Word(4, 0x3000); h.mCPU.SetY(17);
	const auto opcode = h.mHooks.OnHookHit(0xC2E4); h.mCPU.SetHeatMap(nullptr);
	AT_PORTABLE_TEST_ASSERT(context, opcode == 0xEA && heat->GetMemoryValidity(0x3010) == 0 && heat->GetMemoryValidity(0x3011) == 0xFF && heat->GetMemoryValidity(0x30FF) == 0xFF);
	h.mHooks.CallInitHooks(nullptr, nullptr);
	AT_PORTABLE_TEST_ASSERT(context, h.mHooks.OnHookHit(0xE400) == 0 && h.mHooks.OnHookHit(0xC2E4) == 0 && h.mHooks.OnHookHit(0xFFB7) == 0);
	// Every byte in a recognized routine is significant.
	for (size_t i = 0; i < std::size(kClear); ++i) {
		h.mUpperKernel[0xC00 + i] ^= 1;
		h.mHooks.CallInitHooks(nullptr, h.mUpperKernel.data());
		AT_PORTABLE_TEST_ASSERT(context, h.mHooks.OnHookHit(0xE400) == 0);
		h.mUpperKernel[0xC00 + i] ^= 1;
	}
	h.mHooks.CallInitHooks(h.mLowerKernel.data(), h.mUpperKernel.data());
	h.mHooks.EnableOSHooks(false); AT_PORTABLE_TEST_ASSERT(context, h.mHooks.OnHookHit(0xE400) == 0);
	h.mHooks.EnableOSHooks(true); h.mMMU.SetBankRegister(0xFE); AT_PORTABLE_TEST_ASSERT(context, h.mHooks.OnHookHit(0xE400) == 0);
	h.mMMU.SetBankRegister(0xFF);
	h.mUpperKernel.fill(0xEA);
	std::copy(std::begin(kClear), std::end(kClear), h.mUpperKernel.end() - std::size(kClear));
	h.mHooks.CallInitHooks(nullptr, h.mUpperKernel.data());
	a->Word(ATKernelSymbols::TOADR, 0x3000); h.mCPU.SetA(0x5A); h.mCPU.SetX(1); h.mCPU.SetY(255);
	AT_PORTABLE_TEST_ASSERT(context, h.mHooks.OnHookHit(0xFFF6) == 0xEA && h.mMemory.ReadByte(0x30FF) == 0x5A);
	a->mHook.reset();
	AT_PORTABLE_TEST_ASSERT(context, h.mHooks.OnHookHit(0xE400) == 0);
	h.mHooks.CallInitHooks(h.mLowerKernel.data(), h.mUpperKernel.data());
	AT_PORTABLE_TEST_ASSERT(context, h.mHooks.OnHookHit(0xE400) == 0);
	return true;
}
