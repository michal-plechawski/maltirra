// Portable VBXE device lifecycle and snapshot tests.

#include <array>
#include <cstring>
#include <memory>
#include <vector>

#include <at/atcore/deviceimpl.h>
#include <at/atcore/devicesnapshot.h>
#include <at/atcore/deviceu1mb.h>
#include <at/atcore/propertyset.h>
#include <at/atcore/scheduler.h>
#include <at/attest/portabletest.h>
#include <irqcontroller.h>
#include <memorymanager.h>
#include <vbxe.h>

extern const ATDeviceDefinition g_ATDeviceDefVBXE;

namespace {
	class SchedulingService final : public IATDeviceSchedulingService {
	public:
		explicit SchedulingService(ATScheduler& scheduler) : mScheduler(scheduler) {}
		ATScheduler *GetMachineScheduler() const override { return &mScheduler; }
		ATScheduler *GetSlowScheduler() const override { return &mScheduler; }
		ATScheduler& mScheduler;
	};

	class TestDeviceManager final : public IATDeviceManager {
	public:
		explicit TestDeviceManager(IATDeviceSchedulingService& schedulingService)
			: mSchedulingService(schedulingService) {}

		void *GetService(uint32 iid) override {
			return iid == IATDeviceSchedulingService::kTypeID ? &mSchedulingService : nullptr;
		}
		void NotifyDeviceStatusChanged(IATDevice&) override {}

		IATDeviceSchedulingService& mSchedulingService;
	};

	class IRQTarget {
	public:
		void AssertIRQ(int cycleOffset) { mAssertOffsets.push_back(cycleOffset); }
		void NegateIRQ() { ++mNegateCount; }

		std::vector<int> mAssertOffsets;
		uint32 mNegateCount = 0;
	};

	class DeviceLifecycle {
	public:
		explicit DeviceLifecycle(IATDevice& device) : mDevice(device) {}
		~DeviceLifecycle() {
			if (mbInitialized)
				mDevice.Shutdown();
			mDevice.SetManager(nullptr);
		}
		void MarkInitialized() { mbInitialized = true; }
		void Shutdown() {
			if (mbInitialized) {
				mDevice.Shutdown();
				mbInitialized = false;
			}
		}

	private:
		IATDevice& mDevice;
		bool mbInitialized = false;
	};

	bool CheckRange(ATPortableTestContext& context, IATDeviceMemMap& memMap, uint32 lo, uint32 hi) {
		uint32 actualLo = 0;
		uint32 actualHi = 0;
		AT_PORTABLE_TEST_ASSERT(context, memMap.GetMappedRange(0, actualLo, actualHi));
		AT_PORTABLE_TEST_ASSERT(context, actualLo == lo && actualHi == hi);
		AT_PORTABLE_TEST_ASSERT(context, !memMap.GetMappedRange(1, actualLo, actualHi));
		return true;
	}
}

bool ATTestAltirraVBXEDevice(ATPortableTestContext& context) {
	ATScheduler scheduler;
	scheduler.SetRate(VDFraction(1790000, 1));
	SchedulingService schedulingService(scheduler);
	TestDeviceManager deviceManager(schedulingService);
	auto baseRAM = std::make_unique<std::vector<uint8>>(65536);
	auto memoryStorage = std::make_unique<ATMemoryManager>();
	ATMemoryManager& memory = *memoryStorage;
	memory.Init();
	ATMemoryLayer *baseRAMLayer = memory.CreateLayer(kATMemoryPri_BaseRAM, baseRAM->data(), 0, 0x100, false);
	memory.EnableLayer(baseRAMLayer, true);
	std::vector<uint8> sharedMemory(524288);
	std::vector<uint8> secondSharedMemory(524288);
	IRQTarget irqTarget;
	ATIRQController irqController;
	irqController.Init(&irqTarget);

	ATPropertySet settings;
	settings.SetBool("shared_mem", false);
	settings.SetBool("alt_page", true);
	settings.SetUint32("version", 126);
	vdrefptr<IATDevice> device;
	g_ATDeviceDefVBXE.mpFactoryFn(settings, ~device);
	AT_PORTABLE_TEST_ASSERT(context, device != nullptr);
	AT_PORTABLE_TEST_ASSERT(context, device->SetSettings(settings));
	DeviceLifecycle lifecycle(*device);

	ATDeviceInfo info {};
	device->GetDeviceInfo(info);
	AT_PORTABLE_TEST_ASSERT(context, info.mpDef == &g_ATDeviceDefVBXE);
	device->SetManager(&deviceManager);
	IATDeviceScheduling *scheduling = vdpoly_cast<IATDeviceScheduling *>(device);
	IATDeviceMemMap *memMap = vdpoly_cast<IATDeviceMemMap *>(device);
	IATDeviceIRQSource *irqSource = vdpoly_cast<IATDeviceIRQSource *>(device);
	IATDeviceU1MBControllable *u1mb = vdpoly_cast<IATDeviceU1MBControllable *>(device);
	IATDeviceSnapshot *snapshot = vdpoly_cast<IATDeviceSnapshot *>(device);
	IATVBXEDevice *vbxeDevice = vdpoly_cast<IATVBXEDevice *>(device);
	ATVBXEEmulator *emulator = vdpoly_cast<ATVBXEEmulator *>(device);
	AT_PORTABLE_TEST_ASSERT(context, scheduling && memMap && irqSource && u1mb && snapshot && vbxeDevice && emulator);

	scheduling->InitScheduling(&scheduler, &scheduler);
	memMap->InitMemMap(&memory);
	irqSource->InitIRQSource(&irqController);
	device->Init();
	lifecycle.MarkInitialized();
	AT_PORTABLE_TEST_ASSERT(context, !vbxeDevice->GetSharedMemoryMode());
	AT_PORTABLE_TEST_ASSERT(context, vbxeDevice->GetAltPageEnabled());
	AT_PORTABLE_TEST_ASSERT(context, emulator->GetVersion() == 126);
	AT_PORTABLE_TEST_ASSERT(context, emulator->ReadControl(0x40) == 0x10);
	AT_PORTABLE_TEST_ASSERT(context, memory.CPUReadByte(0xD740) == 0x10);
	if (!CheckRange(context, *memMap, 0xD700, 0xD800))
		return false;

	ATPropertySet reportedSettings;
	device->GetSettings(reportedSettings);
	AT_PORTABLE_TEST_ASSERT(context, !reportedSettings.GetBool("shared_mem", true));
	AT_PORTABLE_TEST_ASSERT(context, reportedSettings.GetBool("alt_page", false));
	AT_PORTABLE_TEST_ASSERT(context, reportedSettings.GetUint32("version") == 126);
	VDStringW settingsBlurb;
	device->GetSettingsBlurb(settingsBlurb);
	AT_PORTABLE_TEST_ASSERT(context, settingsBlurb.find(L"FX1.26") != VDStringW::npos);

	// U1MB's explicit register-page override wins over the local alt-page toggle,
	// and releasing it restores the configured page.
	u1mb->SetU1MBControl(kATU1MBControl_VBXEBase, 0xD5);
	if (!CheckRange(context, *memMap, 0xD500, 0xD600))
		return false;
	AT_PORTABLE_TEST_ASSERT(context, memory.CPUReadByte(0xD540) == 0x10);
	vbxeDevice->SetAltPageEnabled(false);
	AT_PORTABLE_TEST_ASSERT(context, !vbxeDevice->GetAltPageEnabled());
	if (!CheckRange(context, *memMap, 0xD500, 0xD600))
		return false;
	u1mb->SetU1MBControl(kATU1MBControl_VBXEBase, -1);
	if (!CheckRange(context, *memMap, 0xD600, 0xD700))
		return false;
	AT_PORTABLE_TEST_ASSERT(context, memory.CPUReadByte(0xD640) == 0x10);

	// The production MEMAC window points into the device-owned 512 KiB RAM.
	emulator->WriteControl(0x5F, 0x80); // Bank A enabled, bank zero.
	emulator->WriteControl(0x5E, 0x48); // CPU read/write, 4 KiB window at $4000.
	uint8 *ownedMemory = emulator->GetMemoryBase();
	AT_PORTABLE_TEST_ASSERT(context, ownedMemory != nullptr);
	memory.CPUWriteByte(0x4456, 0x81);
	AT_PORTABLE_TEST_ASSERT(context, ownedMemory[0x456] == 0x81);
	AT_PORTABLE_TEST_ASSERT(context, memory.CPUReadByte(0x4456) == 0x81);

	// The wrapper snapshot must restore both architectural registers and owned RAM.
	ATSnapshotContext snapshotContext {};
	vdrefptr<IATObjectState> savedState = snapshot->SaveState(snapshotContext);
	AT_PORTABLE_TEST_ASSERT(context, savedState != nullptr);
	ownedMemory[0x456] = 0x22;
	emulator->WriteControl(0x5E, 0);
	snapshot->LoadState(savedState, snapshotContext);
	AT_PORTABLE_TEST_ASSERT(context, emulator->GetMemoryBase()[0x456] == 0x81);
	AT_PORTABLE_TEST_ASSERT(context, memory.CPUReadByte(0x4456) == 0x81);

	// Keep the MEMAC mapping live while switching its backing allocation. The
	// wrapper must rebind manager layers before releasing the owned buffer.
	sharedMemory[0x456] = 0x39;
	vbxeDevice->SetSharedMemory(sharedMemory.data());
	vbxeDevice->SetSharedMemoryMode(true);
	AT_PORTABLE_TEST_ASSERT(context, vbxeDevice->GetSharedMemoryMode());
	AT_PORTABLE_TEST_ASSERT(context, emulator->GetMemoryBase() == sharedMemory.data());
	AT_PORTABLE_TEST_ASSERT(context, memory.CPUReadByte(0x4456) == 0x39);
	memory.CPUWriteByte(0x4456, 0xA6);
	AT_PORTABLE_TEST_ASSERT(context, sharedMemory[0x456] == 0xA6);

	// Shared RAM remains externally owned and is intentionally absent from a
	// device snapshot; loading registers must not roll its bytes back.
	savedState = snapshot->SaveState(snapshotContext);
	sharedMemory[0x456] = 0x5C;
	snapshot->LoadState(savedState, snapshotContext);
	AT_PORTABLE_TEST_ASSERT(context, sharedMemory[0x456] == 0x5C);
	AT_PORTABLE_TEST_ASSERT(context, memory.CPUReadByte(0x4456) == 0x5C);

	secondSharedMemory[0x456] = 0xC4;
	vbxeDevice->SetSharedMemory(secondSharedMemory.data());
	AT_PORTABLE_TEST_ASSERT(context, emulator->GetMemoryBase() == secondSharedMemory.data());
	AT_PORTABLE_TEST_ASSERT(context, memory.CPUReadByte(0x4456) == 0xC4);
	memory.CPUWriteByte(0x4456, 0xE7);
	AT_PORTABLE_TEST_ASSERT(context, secondSharedMemory[0x456] == 0xE7);
	AT_PORTABLE_TEST_ASSERT(context, sharedMemory[0x456] == 0x5C);

	// A null shared-memory pointer must unmap MEMAC instead of leaving layers
	// aimed at invalid storage; the underlying base RAM bus then becomes visible.
	(*baseRAM)[0x4456] = 0x72;
	vbxeDevice->SetSharedMemory(nullptr);
	AT_PORTABLE_TEST_ASSERT(context, emulator->GetMemoryBase() == nullptr);
	AT_PORTABLE_TEST_ASSERT(context, memory.CPUReadByte(0x4456) == 0x72);
	memory.CPUWriteByte(0x4456, 0x73);
	AT_PORTABLE_TEST_ASSERT(context, (*baseRAM)[0x4456] == 0x73);

	vbxeDevice->SetSharedMemoryMode(false);
	AT_PORTABLE_TEST_ASSERT(context, !vbxeDevice->GetSharedMemoryMode());
	AT_PORTABLE_TEST_ASSERT(context, emulator->GetMemoryBase() != nullptr);
	AT_PORTABLE_TEST_ASSERT(context, emulator->GetMemoryBase() != sharedMemory.data());
	AT_PORTABLE_TEST_ASSERT(context, emulator->GetMemoryBase() != secondSharedMemory.data());
	memory.CPUWriteByte(0x4456, 0xD3);
	AT_PORTABLE_TEST_ASSERT(context, emulator->GetMemoryBase()[0x456] == 0xD3);

	lifecycle.Shutdown();
	return true;
}
