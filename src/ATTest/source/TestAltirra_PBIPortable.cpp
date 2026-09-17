// Altirra portable Parallel Bus Interface manager tests

#include <array>
#include <memory>
#include <vector>

#include <at/atcore/devicepbi.h>
#include <at/attest/portabletest.h>
#include <irqcontroller.h>
#include <memorymanager.h>
#include <pbi.h>

namespace {
	class IRQTarget {
	public:
		void AssertIRQ(int cycleOffset) { mAssertOffsets.push_back(cycleOffset); }
		void NegateIRQ() { ++mNegateCount; }

		std::vector<int> mAssertOffsets;
		uint32 mNegateCount = 0;
	};

	class PBIDevice final : public IATPBIDevice {
	public:
		PBIDevice(uint8 id, bool hasIRQ)
			: mId(id), mbHasIRQ(hasIRQ)
		{
		}

		void GetPBIDeviceInfo(ATPBIDeviceInfo& info) const override {
			info.mDeviceId = mId;
			info.mbHasIrq = mbHasIRQ;
		}

		void SelectPBIDevice(bool enable) override {
			mbSelected = enable;
			mSelectionHistory.push_back(enable);
		}

		bool IsPBIOverlayActive() const override { return mbOverlayActive; }

		uint8 ReadPBIStatus(uint8 busData, bool debugOnly) override {
			if (debugOnly)
				++mDebugReadCount;
			else
				++mReadCount;

			if (!mbHasIRQ)
				return busData;

			return mbIRQActive ? busData & ~mId : busData | mId;
		}

		uint8 mId;
		bool mbHasIRQ;
		bool mbSelected = false;
		bool mbOverlayActive = false;
		bool mbIRQActive = false;
		uint32 mReadCount = 0;
		uint32 mDebugReadCount = 0;
		std::vector<bool> mSelectionHistory;
	};
}

bool ATTestAltirraPBI(ATPortableTestContext& context) {
	auto memory = std::make_unique<ATMemoryManager>();
	memory->Init();

	alignas(2) std::array<uint8, 256> backing {};
	backing[0xFF] = 0xCC;
	ATMemoryLayer *backingLayer = memory->CreateLayer(
		kATMemoryPri_BaseRAM, backing.data(), 0xD1, 1, false);
	memory->EnableLayer(backingLayer, kATMemoryAccessMode_RW, true);

	IRQTarget irqTarget;
	ATIRQController irqController;
	irqController.Init(&irqTarget);
	PBIDevice device1(0x01, false);
	PBIDevice device2(0x02, true);
	ATPBIManager pbi;
	pbi.Init(memory.get(), &irqController);

	AT_PORTABLE_TEST_ASSERT(context, pbi.GetSelectRegister() == 0);
	AT_PORTABLE_TEST_ASSERT(context, !pbi.IsROMOverlayActive());
	AT_PORTABLE_TEST_ASSERT(context, memory->ReadByte(0xD1FF) == 0xCC);
	memory->WriteByte(0xD1FF, 0x55);
	AT_PORTABLE_TEST_ASSERT(context, backing[0xFF] == 0x55);
	backing[0xFF] = 0xCC;

	pbi.AddDevice(&device1);
	memory->WriteByte(0xD1FF, 0x01);
	AT_PORTABLE_TEST_ASSERT(context, backing[0xFF] == 0xCC);
	AT_PORTABLE_TEST_ASSERT(context, pbi.GetSelectRegister() == 0x01);
	AT_PORTABLE_TEST_ASSERT(context, device1.mbSelected);
	AT_PORTABLE_TEST_ASSERT(context, memory->ReadByte(0xD1FF) == 0xCC);
	AT_PORTABLE_TEST_ASSERT(context, device1.mReadCount == 0);
	device1.mbOverlayActive = true;
	AT_PORTABLE_TEST_ASSERT(context, pbi.IsROMOverlayActive());

	pbi.AddDevice(&device2);
	AT_PORTABLE_TEST_ASSERT(context, memory->DebugReadByte(0xD1FF) == 0xFF);
	AT_PORTABLE_TEST_ASSERT(context, device1.mDebugReadCount == 1);
	AT_PORTABLE_TEST_ASSERT(context, device2.mDebugReadCount == 1);
	device2.mbIRQActive = true;
	AT_PORTABLE_TEST_ASSERT(context, memory->ReadByte(0xD1FF) == 0xFD);
	AT_PORTABLE_TEST_ASSERT(context, device1.mReadCount == 1);
	AT_PORTABLE_TEST_ASSERT(context, device2.mReadCount == 1);

	memory->WriteByte(0xD1FF, 0x03);
	AT_PORTABLE_TEST_ASSERT(context, !device1.mbSelected);
	AT_PORTABLE_TEST_ASSERT(context, device2.mbSelected);
	AT_PORTABLE_TEST_ASSERT(context, pbi.GetSelectRegister() == 0x03);
	pbi.DeselectSelf(&device1);
	AT_PORTABLE_TEST_ASSERT(context, device2.mbSelected);
	pbi.DeselectSelf(&device2);
	AT_PORTABLE_TEST_ASSERT(context, pbi.GetSelectRegister() == 0);
	AT_PORTABLE_TEST_ASSERT(context, !device2.mbSelected);

	pbi.AssertIRQ(0x02);
	AT_PORTABLE_TEST_ASSERT(context, irqTarget.mAssertOffsets == std::vector<int>({ -1 }));
	pbi.AssertIRQ(0x02);
	pbi.AssertIRQ(0x04);
	AT_PORTABLE_TEST_ASSERT(context, irqTarget.mAssertOffsets.size() == 1);
	pbi.NegateIRQ(0x02);
	AT_PORTABLE_TEST_ASSERT(context, irqTarget.mNegateCount == 0);
	pbi.NegateIRQ(0x04);
	AT_PORTABLE_TEST_ASSERT(context, irqTarget.mNegateCount == 1);

	pbi.Select(0x01);
	AT_PORTABLE_TEST_ASSERT(context, device1.mbSelected);
	pbi.WarmReset();
	AT_PORTABLE_TEST_ASSERT(context, !device1.mbSelected && pbi.GetSelectRegister() == 0);
	pbi.Select(0x02);
	pbi.ColdReset();
	AT_PORTABLE_TEST_ASSERT(context, !device2.mbSelected && pbi.GetSelectRegister() == 0);

	pbi.RemoveDevice(&device2);
	AT_PORTABLE_TEST_ASSERT(context, memory->ReadByte(0xD1FF) == 0xCC);
	pbi.RemoveDevice(&device1);
	memory->WriteByte(0xD1FF, 0x77);
	AT_PORTABLE_TEST_ASSERT(context, backing[0xFF] == 0x77);

	pbi.Shutdown();
	memory->DeleteLayer(backingLayer);
	return true;
}
