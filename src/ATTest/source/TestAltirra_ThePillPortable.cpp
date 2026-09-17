// Altirra portable The Pill cartridge tests

#include <array>
#include <memory>

#include <at/atcore/device.h>
#include <at/attest/portabletest.h>
#include <cartridgeport.h>
#include <memorymanager.h>
#include <thepill.h>

namespace {
	class ThePillDeviceManager final : public IATDeviceManager {
	public:
		explicit ThePillDeviceManager(ATMemoryManager& memory)
			: mMemory(memory)
		{
		}

		void *GetService(uint32 iid) override {
			return iid == ATMemoryManager::kTypeID ? &mMemory : nullptr;
		}

		void NotifyDeviceStatusChanged(IATDevice&) override {}

		ATMemoryManager& mMemory;
	};
}

bool ATTestAltirraThePill(ATPortableTestContext& context) {
	auto memory = std::make_unique<ATMemoryManager>();
	memory->Init();

	alignas(2) std::array<uint8, 0x4000> backingMemory {};
	ATMemoryLayer *backingLayer = memory->CreateLayer(
		kATMemoryPri_BaseRAM, backingMemory.data(), 0x80, 0x40, false);
	memory->EnableLayer(backingLayer, kATMemoryAccessMode_RW, true);

	ATCartridgePort cartridgePort;
	ThePillDeviceManager deviceManager(*memory);
	ATDeviceThePill device;
	device.InitCartridge(&cartridgePort);
	device.SetManager(&deviceManager);
	device.Init();

	AT_PORTABLE_TEST_ASSERT(context, device.IsSaveStateAgnostic());
	AT_PORTABLE_TEST_ASSERT(context,
		device.GetSupportedButtons() == (UINT32_C(1) << kATDeviceButton_CartridgeSwitch));
	AT_PORTABLE_TEST_ASSERT(context, !device.IsButtonDepressed(kATDeviceButton_CartridgeSwitch));
	AT_PORTABLE_TEST_ASSERT(context, !device.IsButtonDepressed(kATDeviceButton_CartridgeResetBank));

	memory->WriteByte(0x8010, 0x11);
	AT_PORTABLE_TEST_ASSERT(context, backingMemory[0x10] == 0x11);
	device.ActivateButton(kATDeviceButton_CartridgeSwitch, true);
	AT_PORTABLE_TEST_ASSERT(context, device.IsButtonDepressed(kATDeviceButton_CartridgeSwitch));
	AT_PORTABLE_TEST_ASSERT(context, device.IsLeftCartActive());
	memory->WriteByte(0x8010, 0x22);
	memory->WriteByte(0xA010, 0x33);
	AT_PORTABLE_TEST_ASSERT(context, backingMemory[0x10] == 0x11);
	AT_PORTABLE_TEST_ASSERT(context, backingMemory[0x2010] == 0x00);

	device.SetCartEnables(false, true, true);
	memory->WriteByte(0x8010, 0x44);
	memory->WriteByte(0xA010, 0x55);
	AT_PORTABLE_TEST_ASSERT(context, backingMemory[0x10] == 0x44);
	AT_PORTABLE_TEST_ASSERT(context, backingMemory[0x2010] == 0x00);

	device.SetCartEnables(true, false, true);
	memory->WriteByte(0x8010, 0x66);
	memory->WriteByte(0xA010, 0x77);
	AT_PORTABLE_TEST_ASSERT(context, backingMemory[0x10] == 0x44);
	AT_PORTABLE_TEST_ASSERT(context, backingMemory[0x2010] == 0x77);

	device.ActivateButton(kATDeviceButton_CartridgeSwitch, false);
	memory->WriteByte(0x8010, 0x88);
	AT_PORTABLE_TEST_ASSERT(context, backingMemory[0x10] == 0x88);

	device.Shutdown();
	device.SetManager(nullptr);
	memory->DeleteLayer(backingLayer);
	return true;
}
