// Observable ROM mapping, banking and device protocols on the production bus.
#include "cartridgeportablefixture.h"
#include "maincpuportablefixture.h"
#include <constants.h>
#include <oshelper.h>
#include "../../Altirra/res/resource.h"

bool ATTestAltirraCartridgeMapping(ATPortableTestContext& context) {
	auto p = std::make_unique<CartridgePortableFixture>(); auto& f = *p;
	struct Mapping { ATCartridgeMode mode; uint32 base, size; };
	for (const auto c : {Mapping{kATCartridgeMode_8K, 0xA000, 0x2000},
		Mapping{kATCartridgeMode_16K, 0x8000, 0x4000}, Mapping{kATCartridgeMode_RightSlot_8K, 0x8000, 0x2000},
		Mapping{kATCartridgeMode_5200_32K, 0x4000, 0x8000}}) {
		f.Load(c.mode);
		AT_PORTABLE_TEST_ASSERT(context, f.Read(c.base - 1) == 0xCC && f.Read(c.base + c.size) == 0xCC);
		for (uint32 i = 0; i < c.size; ++i) {
			AT_PORTABLE_TEST_ASSERT(context, f.Read(c.base + i) == static_cast<uint8>(i >> 12));
			AT_PORTABLE_TEST_ASSERT(context, f.Debug(c.base + i) == f.Read(c.base + i));
			AT_PORTABLE_TEST_ASSERT(context, f.mMemory.AnticReadByte(c.base + i) == static_cast<uint8>(i >> 12));
		}
		f.Write(c.base, 0x7E);
		AT_PORTABLE_TEST_ASSERT(context, f.Read(c.base) == 0);
		AT_PORTABLE_TEST_ASSERT(context, f.mCart.DebugReadLinear(c.size) == 0);
		f.mCart.Unload();
		AT_PORTABLE_TEST_ASSERT(context, f.Read(c.base) == 0xCC && !f.mPort.IsLeftMapped());
	}
	f.Load(kATCartridgeMode_16K);
	AT_PORTABLE_TEST_ASSERT(context, f.mPort.IsLeftMapped());
	f.mPort.EnableCarts(false, true, true);
	AT_PORTABLE_TEST_ASSERT(context, f.Read(0xA000) == 0xCC && f.Read(0x8000) == 0);
	f.mPort.EnableCarts(true, false, true);
	AT_PORTABLE_TEST_ASSERT(context, f.Read(0xA000) == 2 && f.Read(0x8000) == 0xCC);
	f.mPort.EnableCarts(true, true, true);
	for (uint32 bank = 0; bank < 4; ++bank)
		AT_PORTABLE_TEST_ASSERT(context, f.mCart.DebugReadBanked(0x8000 + bank * 4096) == bank);
	AT_PORTABLE_TEST_ASSERT(context, ATIsCartridgeModeHWCompatible(kATCartridgeMode_5200_32K, kATHardwareMode_5200));
	AT_PORTABLE_TEST_ASSERT(context, !ATIsCartridgeModeHWCompatible(kATCartridgeMode_8K, kATHardwareMode_5200));
	f.mCart.Load5200Default();
	std::array<uint8, 4096> firmware {};
	AT_PORTABLE_TEST_ASSERT(context, ATLoadKernelResource(IDR_NOCARTRIDGE, firmware.data(), 0, firmware.size(), false));
	for (uint32 i = 0; i < firmware.size(); ++i)
		AT_PORTABLE_TEST_ASSERT(context, f.Read(0xB000 + i) == firmware[i]);
	return true;
}

bool ATTestAltirraCartridgeOSS(ATPortableTestContext& context) {
	auto p = std::make_unique<CartridgePortableFixture>(); auto& f = *p;
	std::array<uint8, 16384> rom;
	constexpr std::array<uint8, 4> values {0xF0, 0xCC, 0xAA, 0x55};
	for (size_t i = 0; i < values.size(); ++i) std::fill_n(rom.data() + i * 4096, 4096, values[i]);
	for (auto mode : {kATCartridgeMode_OSS_034M, kATCartridgeMode_OSS_043M}) {
		VDMemoryStream stream(rom.data(), rom.size()); ATCartLoadContext load; load.mCartMapper = mode;
		AT_PORTABLE_TEST_ASSERT(context, f.mCart.Load(L"oss.rom", stream, &load));
		const bool swapped = mode == kATCartridgeMode_OSS_043M;
		const uint8 second = values[swapped ? 2 : 1], third = values[swapped ? 1 : 2];
		const uint8 expected[] = {values[0], static_cast<uint8>(values[0] & second), 0xFF, second,
			third, static_cast<uint8>(values[1] & values[2]), 0xFF, second};
		for (uint32 i = 0; i < 16; ++i) {
			f.Write(0xD500 + i, 0);
			AT_PORTABLE_TEST_ASSERT(context, f.Read(0xA000) == (i < 8 ? expected[i] : 0xCC));
			AT_PORTABLE_TEST_ASSERT(context, f.Read(0xBFFF) == (i < 8 ? values[3] : 0xCC));
		}
		// Newly created OSS images must have space for the decoded AND banks.
		f.Load(mode, false);
		for (uint32 i = 0; i < 8; ++i) { f.Write(0xD500 + i, 0); AT_PORTABLE_TEST_ASSERT(context, f.Read(0xA000) == 0xFF); }
	}
	return true;
}

bool ATTestAltirraCartridgeDataBanking(ATPortableTestContext& context) {
	auto p = std::make_unique<CartridgePortableFixture>(); auto& f = *p;
	for (const auto mode : {kATCartridgeMode_XEGS_32K, kATCartridgeMode_XEGS_64K, kATCartridgeMode_XEGS_128K,
		kATCartridgeMode_XEGS_256K, kATCartridgeMode_XEGS_512K, kATCartridgeMode_XEGS_1M,
		kATCartridgeMode_Switchable_XEGS_32K, kATCartridgeMode_Switchable_XEGS_1M}) {
		f.Load(mode); const uint32 banks = f.mCart.GetImage()->GetImageSize() / 8192;
		for (uint32 bank = 0; bank < banks; ++bank) {
			f.Write(0xD53F, static_cast<uint8>(bank));
			AT_PORTABLE_TEST_ASSERT(context, f.Read(0x8000) == bank * 2 && f.Read(0x9FFF) == bank * 2 + 1);
			AT_PORTABLE_TEST_ASSERT(context, f.Read(0xA000) == (banks - 1) * 2);
		}
		if (mode == kATCartridgeMode_Switchable_XEGS_32K || mode == kATCartridgeMode_Switchable_XEGS_1M) {
			f.Write(0xD500, 0x80);
			AT_PORTABLE_TEST_ASSERT(context, f.Read(0x8000) == 0xCC && f.Read(0xA000) == 0xCC && !f.mPort.IsLeftMapped());
		}
		f.mCart.ColdReset();
		AT_PORTABLE_TEST_ASSERT(context, f.mCart.GetCartBank() == 0 && f.Read(0x8000) == 0);
	}
	for (const auto mode : {kATCartridgeMode_MegaCart_16K, kATCartridgeMode_MegaCart_64K, kATCartridgeMode_MegaCart_2M}) {
		f.Load(mode); const uint32 banks = f.mCart.GetImage()->GetImageSize() / 16384;
		for (uint32 bank = 0; bank < banks; ++bank) {
			f.Write(0xD500, static_cast<uint8>(bank));
			AT_PORTABLE_TEST_ASSERT(context, f.Read(0x8000) == static_cast<uint8>(bank * 4));
			AT_PORTABLE_TEST_ASSERT(context, f.Read(0xBFFF) == static_cast<uint8>(bank * 4 + 3));
		}
	}
	return true;
}

bool ATTestAltirraCartridgeAddressBanking(ATPortableTestContext& context) {
	auto p = std::make_unique<CartridgePortableFixture>(); auto& f = *p;
	for (const auto mode : {kATCartridgeMode_Williams_16K, kATCartridgeMode_Williams_32K, kATCartridgeMode_Williams_64K,
		kATCartridgeMode_Turbosoft_64K, kATCartridgeMode_Turbosoft_128K}) {
		f.Load(mode); const uint32 banks = f.mCart.GetImage()->GetImageSize() / 8192;
		for (uint32 bank = 0; bank < banks; ++bank) {
			f.Write(0xD500 + bank, 0xFF);
			AT_PORTABLE_TEST_ASSERT(context, f.Read(0xA000) == bank * 2);
			f.Debug(0xD500); // debugger reads must not clock the bank latch
			AT_PORTABLE_TEST_ASSERT(context, f.mCart.GetCartBank() == bank);
			f.Read(0xD500);
			AT_PORTABLE_TEST_ASSERT(context, f.Read(0xA000) == 0);
		}
	}
	f.Load(kATCartridgeMode_JAtariCart_512K);
	f.Write(0xD53F, 0);
	AT_PORTABLE_TEST_ASSERT(context, f.Read(0xA000) == 126);
	f.Write(0xD580, 0);
	AT_PORTABLE_TEST_ASSERT(context, f.Read(0xA000) == 0xCC && !f.mPort.IsLeftMapped());
	f.Load(kATCartridgeMode_MDDOS);
	AT_PORTABLE_TEST_ASSERT(context, f.Read(0xA000) == 13 && f.Read(0xB000) == 12);
	return true;
}

bool ATTestAltirraCartridgeBountyBob(ATPortableTestContext& context) {
	auto p = std::make_unique<CartridgePortableFixture>(); auto& f = *p;
	for (auto mode : {kATCartridgeMode_BountyBob800, kATCartridgeMode_BountyBob5200}) {
		f.Load(mode); const uint32 base = mode == kATCartridgeMode_BountyBob800 ? 0x8000 : 0x4000;
		for (uint32 bank = 0; bank < 4; ++bank) {
			const uint8 old = f.Read(base);
			f.Debug(base + 0xFF6 + bank);
			AT_PORTABLE_TEST_ASSERT(context, f.Read(base) == old);
			AT_PORTABLE_TEST_ASSERT(context, f.Read(base + 0xFF6 + bank) == old);
			AT_PORTABLE_TEST_ASSERT(context, f.Read(base) == bank);
			f.Write(base + 0x1FF6 + bank, 0);
			AT_PORTABLE_TEST_ASSERT(context, f.Read(base + 0x1000) == bank + 4);
		}
		AT_PORTABLE_TEST_ASSERT(context, f.Read(0xA000) == 8 && f.Read(0xB000) == 9);
	}
	return true;
}

bool ATTestAltirraCartridgeSIC(ATPortableTestContext& context) {
	auto p = std::make_unique<CartridgePortableFixture>(); auto& f = *p;
	for (auto mode : {kATCartridgeMode_SIC_128K, kATCartridgeMode_SIC_256K, kATCartridgeMode_SIC_512K, kATCartridgeMode_SICPlus}) {
		f.Load(mode);
		AT_PORTABLE_TEST_ASSERT(context, f.Read(0x8000) == 0xCC && f.Read(0xA000) == 2);
		const uint32 banks = f.mCart.GetImage()->GetImageSize() / 16384;
		for (uint32 bank = 0; bank < banks; ++bank) {
			const uint8 reg = static_cast<uint8>((bank & 31) | 0x20 | ((bank & 32) ? 0x80 : 0));
			f.Write(0xD500, reg);
			AT_PORTABLE_TEST_ASSERT(context, f.Read(0xD500) == reg);
			AT_PORTABLE_TEST_ASSERT(context, f.Read(0x8000) == bank * 4 && f.Read(0xBFFF) == bank * 4 + 3);
		}
		f.Write(0xD500, 0x40);
		AT_PORTABLE_TEST_ASSERT(context, f.Read(0xA000) == 0xCC && !f.mPort.IsLeftMapped());
	}
	return true;
}

bool ATTestAltirraCartridgeSC3D(ATPortableTestContext& context) {
	auto p = std::make_unique<CartridgePortableFixture>(); auto& f = *p; f.Load(kATCartridgeMode_SuperCharger3D);
	f.Write(0xD500, 13); f.Write(0xD502, 21); f.Write(0xD503, 2);
	AT_PORTABLE_TEST_ASSERT(context, f.Read(0xD501) == 1 && f.Read(0xD502) == 17 && f.Read(0xD503) == 0);
	f.Write(0xD500, 10); f.Write(0xD501, 1); f.Write(0xD502, 17); f.Write(0xD503, 1);
	AT_PORTABLE_TEST_ASSERT(context, f.Read(0xD501) == 3 && f.Read(0xD502) == 27 && f.Read(0xD503) == 0);
	f.Write(0xD500, 0); f.Write(0xD503, 1); // division by zero must report failure
	AT_PORTABLE_TEST_ASSERT(context, f.Read(0xD503) == 1);
	return true;
}

bool ATTestAltirraCartridgeCPU(ATPortableTestContext& context) {
	auto machine = std::make_unique<MainCPUPortableFixture>(); auto& h = *machine;
	ATCartridgePort port; CartridgeIndicatorProbe indicators;
	ATCartridgeEmulator cart(kATMemoryPri_Cartridge1, kATCartridgePriority_Default, false);
	cart.InitCartridge(&port); cart.Init(h.mMemory, h.mFastScheduler, &indicators);
	cart.LoadNewCartridge(kATCartridgeMode_XEGS_32K);
	auto *rom = static_cast<uint8 *>(cart.GetImage()->GetBuffer()); std::fill_n(rom, 32768, 0xEA);
	// Execute code from the fixed bank, switch the lower bank, read it, and
	// store the result in RAM using the production 6502 interpreter.
	const uint8 code[] = {0xA9, 2, 0x8D, 0, 0xD5, 0xAD, 0, 0x80, 0x8D, 0, 0x30, 0x4C, 0x0B, 0xA0};
	std::copy(std::begin(code), std::end(code), rom + 0x6000); rom[0x4000] = 0x5A;
	h.Prepare(kATCPUMode_6502, 0xA000);
	AT_PORTABLE_TEST_ASSERT(context, h.RunCycles(80));
	AT_PORTABLE_TEST_ASSERT(context, h.mRAM[0x3000] == 0x5A && cart.GetCartBank() == 2);
	cart.Shutdown();
	return true;
}
