// Altirra portable SCSI disk tests

#include <array>
#include <cstring>
#include <vector>

#include <vd2/system/binary.h>
#include <vd2/system/error.h>
#include <vd2/system/refcount.h>
#include <vd2/system/vdstring.h>
#include <at/atcore/blockdevice.h>
#include <at/atcore/scheduler.h>
#include <at/atemulation/scsi.h>
#include <at/attest/portabletest.h>
#include <scsidisk.h>

namespace {
	class MemoryBlockDevice final : public vdrefcounted<IATBlockDevice> {
	public:
		explicit MemoryBlockDevice(uint32 sectorCount)
			: mData(static_cast<size_t>(sectorCount) * 512)
		{
		}

		void *AsInterface(uint32 iid) override {
			return iid == IATBlockDevice::kTypeID ? static_cast<IATBlockDevice *>(this) : nullptr;
		}

		bool IsReadOnly() const override { return mbReadOnly; }
		uint32 GetSectorCount() const override { return static_cast<uint32>(mData.size() / 512); }
		ATBlockDeviceGeometry GetGeometry() const override { return { 0, 0, 0, true }; }
		uint32 GetSerialNumber() const override { return 1; }
		void Flush() override {}

		void ReadSectors(void *data, uint32 lba, uint32 n) override {
			if (lba > GetSectorCount() || n > GetSectorCount() - lba)
				throw MyError("Read outside test block device.");

			memcpy(data, mData.data() + static_cast<size_t>(lba) * 512, static_cast<size_t>(n) * 512);
		}

		void WriteSectors(const void *data, uint32 lba, uint32 n) override {
			if (mbReadOnly)
				throw MyError("Test block device is read-only.");
			if (lba > GetSectorCount() || n > GetSectorCount() - lba)
				throw MyError("Write outside test block device.");

			memcpy(mData.data() + static_cast<size_t>(lba) * 512, data, static_cast<size_t>(n) * 512);
		}

		std::vector<uint8> mData;
		bool mbReadOnly = false;
	};

	struct SCSIResult {
		bool mbProtocolValid = true;
		std::vector<uint8> mData;
		std::vector<uint8> mStatus;
	};

	class SCSIInitiator {
	public:
		explicit SCSIInitiator(ATSCSIBusEmulator& bus)
			: mBus(bus)
		{
		}

		SCSIResult Execute(const std::vector<uint8>& command, const std::vector<uint8>& output = {}) {
			SCSIResult result;
			size_t commandIndex = 0;
			size_t outputIndex = 0;

			mBus.SetControl(0, 0x01 | kATSCSICtrlState_SEL, kATSCSICtrlState_All | 0xFF);
			if (!(mBus.GetBusState() & kATSCSICtrlState_BSY)) {
				result.mbProtocolValid = false;
				return result;
			}

			mBus.SetControl(0, 0, kATSCSICtrlState_SEL);

			for(uint32 step = 0; step < 100000; ++step) {
				uint32 state = mBus.GetBusState();
				if (!(state & kATSCSICtrlState_BSY)) {
					if (commandIndex != command.size() || outputIndex != output.size())
						result.mbProtocolValid = false;
					return result;
				}

				const uint32 phase = state & (kATSCSICtrlState_IO | kATSCSICtrlState_CD | kATSCSICtrlState_MSG);
				if (phase & kATSCSICtrlState_IO) {
					mBus.SetControl(0, 0xFF, 0xFF);
					state = mBus.GetBusState();
				}

				if (!(state & kATSCSICtrlState_REQ)) {
					result.mbProtocolValid = false;
					return result;
				}

				uint8 value = 0xFF;
				if (phase == kATSCSICtrlState_CD) {
					if (commandIndex >= command.size()) {
						result.mbProtocolValid = false;
						return result;
					}
					value = command[commandIndex++];
				} else if (phase == 0) {
					if (outputIndex >= output.size()) {
						result.mbProtocolValid = false;
						return result;
					}
					value = output[outputIndex++];
				} else if (phase == kATSCSICtrlState_IO) {
					result.mData.push_back(static_cast<uint8>(state));
				} else if (phase == (kATSCSICtrlState_IO | kATSCSICtrlState_CD)) {
					result.mStatus.push_back(static_cast<uint8>(state));
				} else {
					result.mbProtocolValid = false;
					return result;
				}

				mBus.SetControl(0, value | kATSCSICtrlState_ACK, 0xFF | kATSCSICtrlState_ACK);
				mBus.SetControl(0, 0, kATSCSICtrlState_ACK);
			}

			result.mbProtocolValid = false;
			return result;
		}

	private:
		ATSCSIBusEmulator& mBus;
	};

	bool HasStatus(const SCSIResult& result, uint8 status) {
		return result.mbProtocolValid
			&& result.mStatus.size() == 2
			&& result.mStatus[0] == status
			&& result.mStatus[1] == 0;
	}
}

bool ATTestAltirraSCSIDisk(ATPortableTestContext& context) {
	ATScheduler scheduler;
	scheduler.SetRate(VDFraction(17897725, 10));

	ATSCSIBusEmulator bus;
	bus.Init(&scheduler);

	vdrefptr<MemoryBlockDevice> disk(new MemoryBlockDevice(4));
	for(size_t i = 0; i < disk->mData.size(); ++i)
		disk->mData[i] = static_cast<uint8>((i * 29 + i / 512 * 17) & 0xFF);

	vdrefptr<IATSCSIDiskDevice> device;
	ATCreateSCSIDiskDevice(disk, ~device);
	bus.AttachDevice(0, device);
	SCSIInitiator initiator(bus);

	SCSIResult result = initiator.Execute({ 0x12, 0, 0, 0, 2, 0 });
	AT_PORTABLE_TEST_ASSERT(context, HasStatus(result, 0x80));
	AT_PORTABLE_TEST_ASSERT(context, result.mData == std::vector<uint8>({ 0, 0 }));

	result = initiator.Execute({ 0x25, 0, 0, 0, 0, 0, 0, 0, 0, 0 });
	AT_PORTABLE_TEST_ASSERT(context, HasStatus(result, 0x80));
	AT_PORTABLE_TEST_ASSERT(context, result.mData.size() == 8);
	AT_PORTABLE_TEST_ASSERT(context, VDReadUnalignedBEU32(result.mData.data()) == 3);
	AT_PORTABLE_TEST_ASSERT(context, VDReadUnalignedBEU32(result.mData.data() + 4) == 512);

	result = initiator.Execute({ 0x08, 0, 0, 1, 1, 0 });
	AT_PORTABLE_TEST_ASSERT(context, HasStatus(result, 0x80));
	AT_PORTABLE_TEST_ASSERT(context, result.mData.size() == 512);
	AT_PORTABLE_TEST_ASSERT(context,
		!memcmp(result.mData.data(), disk->mData.data() + 512, 512));

	std::vector<uint8> writeData(512);
	for(size_t i = 0; i < writeData.size(); ++i)
		writeData[i] = static_cast<uint8>(0xA5 ^ i);
	result = initiator.Execute({ 0x0A, 0, 0, 2, 1, 0 }, writeData);
	AT_PORTABLE_TEST_ASSERT(context, HasStatus(result, 0x80));
	AT_PORTABLE_TEST_ASSERT(context,
		!memcmp(writeData.data(), disk->mData.data() + 2 * 512, 512));

	disk->mbReadOnly = true;
	result = initiator.Execute({ 0x0A, 0, 0, 0, 1, 0 });
	AT_PORTABLE_TEST_ASSERT(context, HasStatus(result, 0x82));
	AT_PORTABLE_TEST_ASSERT(context, result.mData.empty());
	result = initiator.Execute({ 0x03, 0, 0, 0, 4, 0 });
	AT_PORTABLE_TEST_ASSERT(context, HasStatus(result, 0x82));
	AT_PORTABLE_TEST_ASSERT(context, result.mData == std::vector<uint8>({ 0x17, 0, 0, 0 }));
	disk->mbReadOnly = false;

	device->SetBlockSize(256);
	result = initiator.Execute({ 0x25, 0, 0, 0, 0, 0, 0, 0, 0, 0 });
	AT_PORTABLE_TEST_ASSERT(context, HasStatus(result, 0x80));
	AT_PORTABLE_TEST_ASSERT(context, VDReadUnalignedBEU32(result.mData.data()) == 7);
	AT_PORTABLE_TEST_ASSERT(context, VDReadUnalignedBEU32(result.mData.data() + 4) == 256);

	std::array<uint8, 256> preservedHalf;
	memcpy(preservedHalf.data(), disk->mData.data() + 2 * 512, preservedHalf.size());
	std::vector<uint8> halfSector(256, 0x5A);
	result = initiator.Execute({ 0x0A, 0, 0, 5, 1, 0 }, halfSector);
	AT_PORTABLE_TEST_ASSERT(context, HasStatus(result, 0x80));
	AT_PORTABLE_TEST_ASSERT(context,
		!memcmp(preservedHalf.data(), disk->mData.data() + 2 * 512, preservedHalf.size()));
	AT_PORTABLE_TEST_ASSERT(context,
		!memcmp(halfSector.data(), disk->mData.data() + 2 * 512 + 256, halfSector.size()));
	result = initiator.Execute({ 0x08, 0, 0, 5, 1, 0 });
	AT_PORTABLE_TEST_ASSERT(context, HasStatus(result, 0x80));
	AT_PORTABLE_TEST_ASSERT(context, result.mData == halfSector);

	result = initiator.Execute({ 0x08, 0, 0, 8, 1, 0 });
	AT_PORTABLE_TEST_ASSERT(context, HasStatus(result, 0x82));
	result = initiator.Execute({ 0x03, 0, 0, 0, 4, 0 });
	AT_PORTABLE_TEST_ASSERT(context, HasStatus(result, 0x82));
	AT_PORTABLE_TEST_ASSERT(context, result.mData == std::vector<uint8>({ 0x21, 0, 0, 8 }));

	result = initiator.Execute({ 0x1F, 0, 0, 0, 0, 0 });
	AT_PORTABLE_TEST_ASSERT(context, HasStatus(result, 0x82));
	result = initiator.Execute({ 0x03, 0, 0, 0, 4, 0 });
	AT_PORTABLE_TEST_ASSERT(context, HasStatus(result, 0x82));
	AT_PORTABLE_TEST_ASSERT(context, result.mData == std::vector<uint8>({ 0x20, 0, 0, 0 }));

	bus.Shutdown();
	return true;
}
