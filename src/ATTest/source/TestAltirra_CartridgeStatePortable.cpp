// Flash commands, cartridge RAM, image replacement and saved-state round trips.
#include "cartridgeportablefixture.h"
#include "diskinterfaceportablefixture.h"
#include <vd2/system/error.h>

bool ATTestAltirraCartridgeFlash(ATPortableTestContext& context) {
	auto p = std::make_unique<CartridgePortableFixture>(); auto& f = *p;
	struct Chip { const char *name; uint8 manufacturer, device; };
	for (const auto chip : {Chip{"", 0x01, 0x20}, Chip{"M29F010B", 0x20, 0x20}, Chip{"SST39SF010", 0xBF, 0xB5}}) {
		ATCartridgeFlashOptions options; options.mMaxflash1MbFlashChip = chip.name; f.mCart.SetFlashOptions(options);
		f.Load(kATCartridgeMode_MaxFlash_128K, false);
		AT_PORTABLE_TEST_ASSERT(context, !f.mCart.IsBASICDisableAllowed() && !f.mCart.IsDirty());
		f.FlashUnlock(0x90); f.Write(0xD500, 0);
		AT_PORTABLE_TEST_ASSERT(context, f.Read(0xA000) == chip.manufacturer && f.Read(0xA001) == chip.device);
		f.FlashWrite(0, 0xF0);
		f.FlashUnlock(0xA0); f.FlashWrite(0x1234, 0x5A); f.Tick(100);
		AT_PORTABLE_TEST_ASSERT(context, f.mCart.DebugReadLinear(0x1234) == 0x5A);
		AT_PORTABLE_TEST_ASSERT(context, f.mCart.IsDirty() && f.mCart.GetImage()->IsDirty() && f.mIndicators.mWrites > 0);
		f.FlashUnlock(0xA0); f.FlashWrite(0x1234, 0xF0); f.Tick(100);
		AT_PORTABLE_TEST_ASSERT(context, f.mCart.DebugReadLinear(0x1234) == 0x50); // programming can only clear bits
	}
	return true;
}

bool ATTestAltirraCartridgeRAM(ATPortableTestContext& context) {
	auto p = std::make_unique<CartridgePortableFixture>(); auto& f = *p;
	f.Load(kATCartridgeMode_TelelinkII);
	AT_PORTABLE_TEST_ASSERT(context, f.Read(0x8000) == 0xFF);
	f.Write(0x8005, 0x27);
	AT_PORTABLE_TEST_ASSERT(context, f.Read(0x8105) == 0xF7 && f.Read(0x9F05) == 0xF7);
	f.Write(0xD500, 0);
	AT_PORTABLE_TEST_ASSERT(context, f.mCart.DebugReadLinear(8192 + 5) == 0xF7 && f.mCart.IsDirty());
	f.Write(0x8005, 1); f.Read(0xD501);
	AT_PORTABLE_TEST_ASSERT(context, f.Read(0x8005) == 0xF7);
	f.Load(kATCartridgeMode_Pronto);
	f.Write(0x8F12, 0x42);
	AT_PORTABLE_TEST_ASSERT(context, f.Read(0x8F12) == 0xF2);
	f.Write(0xD502, 0);
	AT_PORTABLE_TEST_ASSERT(context, f.mCart.DebugReadLinear(0xF12) == 0xF2 && f.mCart.IsDirty());
	f.Write(0x8F12, 0x0C); f.Read(0xD501);
	AT_PORTABLE_TEST_ASSERT(context, f.Read(0x8F12) == 0xF2);
	return true;
}

bool ATTestAltirraCartridgeTheCart(ATPortableTestContext& context) {
	auto p = std::make_unique<CartridgePortableFixture>(); auto& f = *p;
	f.Load(kATCartridgeMode_TheCart_32M);
	AT_PORTABLE_TEST_ASSERT(context, f.Read(0xD5A2) == 1 && f.Read(0xD5A5) == 0 && f.Read(0xD5A6) == 1);
	f.Write(0xD5A0, 7);
	AT_PORTABLE_TEST_ASSERT(context, f.Read(0xA000) == 14);
	f.Write(0xD5A6, 0x20); // flexi mode enables independent windows
	f.Write(0xD5A3, 9);
	AT_PORTABLE_TEST_ASSERT(context, f.Read(0x8000) == 18);
	f.Write(0xD5A7, 3); // primary bank uses writable RAM
	f.Write(0xA123, 0x5A); f.Write(0xD5A0, 8); f.Write(0xA123, 0xA5); f.Write(0xD5A0, 7);
	AT_PORTABLE_TEST_ASSERT(context, f.Read(0xA123) == 0x5A);
	f.Write(0xD5A7, 2); f.Write(0xA123, 0x11);
	AT_PORTABLE_TEST_ASSERT(context, f.Read(0xA123) == 0x5A);
	f.Write(0xD5A7, 0);
	AT_PORTABLE_TEST_ASSERT(context, f.Read(0xA000) == 14);
	f.Write(0xD5A2, 0);
	AT_PORTABLE_TEST_ASSERT(context, f.Read(0xA000) == 0xCC && !f.mPort.IsLeftMapped());
	f.mCart.ActivateButton(kATDeviceButton_CartridgeResetBank, true);
	AT_PORTABLE_TEST_ASSERT(context, f.Read(0xA000) == 0 && f.Read(0xD5A0) == 0 && f.mPort.IsLeftMapped());
	return true;
}

bool ATTestAltirraCartridgeSnapshot(ATPortableTestContext& context) {
	auto p = std::make_unique<CartridgePortableFixture>(); auto& f = *p;
	f.Load(kATCartridgeMode_XEGS_64K); f.Write(0xD500, 5);
	vdrefptr<IATObjectState> state; f.mCart.SaveState(~state);
	auto decoded = CartridgeStateDecode(CartridgeStateJSON(*state));
	auto q = std::make_unique<CartridgePortableFixture>(); auto& restored = *q;
	restored.Load(kATCartridgeMode_XEGS_64K); restored.mCart.LoadState(*decoded);
	AT_PORTABLE_TEST_ASSERT(context, restored.mCart.GetCartBank() == 5 && restored.Read(0x8000) == 10);
	// Cartridge RAM is stored in the real ZIP-backed snapshot serializer.
	f.Load(kATCartridgeMode_TelelinkII); f.Write(0x8021, 9); f.mCart.SaveState(~state);
	decoded = CartridgeStateRoundTrip(*state);
	restored.Load(kATCartridgeMode_TelelinkII); restored.mCart.LoadState(*decoded);
	AT_PORTABLE_TEST_ASSERT(context, restored.Read(0x8121) == 0xF9);
	return true;
}

bool ATTestAltirraCartridgeImages(ATPortableTestContext& context) {
	auto p = std::make_unique<CartridgePortableFixture>(); auto& f = *p; DiskInterfaceSandbox files;
	f.Load(kATCartridgeMode_TelelinkII); f.Write(0x8003, 6); f.Write(0xD500, 0);
	const auto path = files.File(L"saved.car"); f.mCart.Save(path.c_str(), true);
	AT_PORTABLE_TEST_ASSERT(context, !f.mCart.IsDirty());
	f.Load(kATCartridgeMode_8K);
	AT_PORTABLE_TEST_ASSERT(context, f.mCart.Load(path.c_str(), nullptr));
	AT_PORTABLE_TEST_ASSERT(context, f.mCart.GetMode() == kATCartridgeMode_TelelinkII);
	AT_PORTABLE_TEST_ASSERT(context, f.mCart.GetImageFileCRC().has_value() && f.mCart.GetImageFileSHA256().has_value());
	f.Read(0xD501);
	AT_PORTABLE_TEST_ASSERT(context, f.Read(0x8003) == 0xF6);
	// A rejected raw image must leave the previous image and mappings intact.
	std::array<uint8, 32768> raw {}; VDMemoryStream input(raw.data(), raw.size());
	ATCartLoadContext load; load.mbReturnOnUnknownMapper = true;
	IATCartridgeImage *const old = f.mCart.GetImage();
	AT_PORTABLE_TEST_ASSERT(context, !f.mCart.Load(L"unknown.rom", input, &load));
	AT_PORTABLE_TEST_ASSERT(context, load.mLoadStatus == kATCartLoadStatus_UnknownMapper && f.mCart.GetImage() == old && f.Read(0x8003) == 0xF6);
	// The only remaining image reference can be owned by the emulator itself.
	f.mCart.Load(f.mCart.GetImage()); f.Read(0xD501);
	AT_PORTABLE_TEST_ASSERT(context, f.Read(0x8003) == 0xF6);
	f.mCart.Load(nullptr);
	AT_PORTABLE_TEST_ASSERT(context, !f.mCart.GetImage() && f.mCart.GetMode() == kATCartridgeMode_None && !f.mCart.IsDirty());
	return true;
}

bool ATTestAltirraCartridgeLifecycle(ATPortableTestContext& context) {
	auto p = std::make_unique<CartridgePortableFixture>(); auto& f = *p;
	f.Load(kATCartridgeMode_COS32K);
	AT_PORTABLE_TEST_ASSERT(context, f.Read(0x8000) == 4);
	f.Write(0xD500, 0);
	AT_PORTABLE_TEST_ASSERT(context, f.Read(0x8000) == 0);
	f.Tick(22);
	AT_PORTABLE_TEST_ASSERT(context, f.Read(0x8000) == 0);
	f.Tick(1);
	AT_PORTABLE_TEST_ASSERT(context, f.Read(0x8000) == 4);
	f.mCart.ActivateButton(kATDeviceButton_CartridgeSwitch, false);
	AT_PORTABLE_TEST_ASSERT(context, f.Read(0x8000) == 0xCC && !f.mPort.IsLeftMapped());
	f.mCart.ActivateButton(kATDeviceButton_CartridgeSwitch, true);
	f.Write(0xD500, 0); f.Load(kATCartridgeMode_8K); f.Tick(100);
	AT_PORTABLE_TEST_ASSERT(context, f.Read(0x8000) == 0xCC && f.Read(0xA000) == 0);
	f.Load(kATCartridgeMode_COS32K); f.Write(0xD500, 0);
	f.mCart.Shutdown(); f.mCart.Shutdown(); f.Tick(100);
	AT_PORTABLE_TEST_ASSERT(context, f.Read(0x8000) == 0xCC && f.Read(0xA000) == 0xCC && !f.mPort.IsLeftMapped());
	f.mCart.InitCartridge(&f.mPort); f.mCart.Init(f.mMemory, f.mScheduler, &f.mIndicators);
	f.Load(kATCartridgeMode_16K);
	AT_PORTABLE_TEST_ASSERT(context, f.Read(0x8000) == 0 && f.Read(0xA000) == 2);
	// A pending flash erase also belongs to the detached image, even with
	// no UI indicator service attached to this machine.
	f.mCart.Shutdown(); f.mCart.InitCartridge(&f.mPort); f.mCart.Init(f.mMemory, f.mScheduler, nullptr);
	f.Load(kATCartridgeMode_MaxFlash_128K, false);
	f.FlashUnlock(0xA0); f.FlashWrite(0x1234, 0x5A);
	AT_PORTABLE_TEST_ASSERT(context, f.mCart.DebugReadLinear(0x1234) == 0x5A);
	f.FlashUnlock(0x80); f.FlashWrite(0x5555, 0xAA); f.FlashWrite(0x2AAA, 0x55); f.FlashWrite(0x1234, 0x30);
	f.Load(kATCartridgeMode_8K); f.Tick(10000);
	AT_PORTABLE_TEST_ASSERT(context, f.Read(0xA000) == 0 && !f.mCart.IsDirty());
	return true;
}

bool ATTestAltirraCartridgeSmallROM(ATPortableTestContext& context) {
	auto p = std::make_unique<CartridgePortableFixture>(); auto& f = *p; DiskInterfaceSandbox files;
	for (auto mode : {kATCartridgeMode_2K, kATCartridgeMode_4K, kATCartridgeMode_RightSlot_4K}) {
		f.Load(mode, false); const uint32 size = ATGetImageSizeForCartridgeType(mode);
		auto *rom = static_cast<uint8 *>(f.mCart.GetImage()->GetBuffer()); rom[8191] = 0x5A;
		const uint32 base = mode == kATCartridgeMode_RightSlot_4K ? 0x8000 : 0xA000;
		for (uint32 i = 0; i < 8191; ++i) AT_PORTABLE_TEST_ASSERT(context, f.Read(base + i) == 0xFF);
		AT_PORTABLE_TEST_ASSERT(context, f.Read(base + 8191) == 0x5A);
		const auto path = files.File(L"small.car"); f.mCart.Save(path.c_str(), true);
		AT_PORTABLE_TEST_ASSERT(context, f.mCart.Load(path.c_str(), nullptr));
		AT_PORTABLE_TEST_ASSERT(context, f.mCart.GetImage()->GetImageSize() == size && f.Read(base + 8191) == 0x5A);
	}
	return true;
}

bool ATTestAltirraCartridgeModes(ATPortableTestContext& context) {
	auto p = std::make_unique<CartridgePortableFixture>(); auto& f = *p;
	// Every supported mapper is constructed, mapped, reset, snapshotted and
	// removed. This also bounds all four CPU/ANTIC/debug memory windows.
	for (uint32 i = 1; i < kATCartridgeModeCount; ++i) {
		const auto mode = static_cast<ATCartridgeMode>(i); f.Load(mode, false);
		AT_PORTABLE_TEST_ASSERT(context, f.mCart.GetMode() == mode && f.mCart.GetImage());
		for (uint32 a = 0x4000; a < 0xC000; a += 256) {
			f.Read(a); f.Debug(a); f.mMemory.AnticReadByte(a); f.mMemory.DebugAnticReadByte(static_cast<uint16>(a));
		}
		f.mCart.ColdReset();
		vdrefptr<IATObjectState> state; f.mCart.SaveState(~state); f.mCart.LoadState(*state);
		f.mCart.Unload();
		AT_PORTABLE_TEST_ASSERT(context, !f.mCart.GetImage() && !f.mPort.IsLeftMapped() && f.Read(0x8000) == 0xCC);
	}
	return true;
}
