// Timed transactions driven by real POKEY SEROUT and observed in SERIN/IRQs.
#include <at/attest/portabletest.h>
#include "diskemulatorportablefixture.h"

bool ATTestAltirraDiskWireRead(ATPortableTestContext& context) {
	auto f = std::make_unique<DiskEmulatorPortableFixture>(); const auto data = f->Pattern(128);
	f->Fill(0, data); f->mBus.ReceiveClock(); f->WireCommand(0x52);
	std::vector<uint8> expected {'A', 'C'}; expected.insert(expected.end(), data.begin(), data.end()); expected.push_back(ATComputeSIOChecksum(data.data(), data.size()));
	AT_PORTABLE_TEST_ASSERT(context, f->mBus.Collect(200000) == expected);
	AT_PORTABLE_TEST_ASSERT(context, f->mBus.mMachine.mChips.mIRQAsserts >= 131);
	return true;
}

bool ATTestAltirraDiskWireWrite(ATPortableTestContext& context) {
	for (bool bad : {false, true}) {
		auto f = std::make_unique<DiskEmulatorPortableFixture>(); const auto data = f->Pattern(128, 0x77);
		f->mBus.ReceiveClock(); f->WireCommand(0x57, 4);
		AT_PORTABLE_TEST_ASSERT(context, f->mBus.Collect(10000) == std::vector<uint8>({'A'}));
		auto& p = f->mBus.mMachine.mChips.mPOKEY; p.WriteByte(0x0F, 0x23);
		for (uint8 c : data) f->SerialByte(c);
		f->SerialByte(ATComputeSIOChecksum(data.data(), data.size()) ^ (bad ? 1 : 0)); p.WriteByte(0x0F, 0x13);
		auto reply = f->mBus.Collect(200000);
		AT_PORTABLE_TEST_ASSERT(context, reply == (bad ? std::vector<uint8>({'N'}) : std::vector<uint8>({'A', 'C'})));
		std::array<uint8, 128> actual {}; f->mSlot.mDisk.GetDiskImage()->ReadVirtualSector(3, actual.data(), actual.size());
		AT_PORTABLE_TEST_ASSERT(context, bad ? std::all_of(actual.begin(), actual.end(), [](uint8 c) { return c == 0; }) : std::equal(actual.begin(), actual.end(), data.begin()));
	}
	return true;
}

namespace {
	void ReplaceSector(DiskEmulatorPortableFixture& f, uint32 index, bool phantom, bool weak) {
		ATDiskVirtualSectorInfo v {0, phantom ? 2U : 1U}; std::array<ATDiskPhysicalSectorInfo, 2> physical {};
		std::array<uint8, 256> data {}; std::fill_n(data.begin(), 128, 0x11); std::fill_n(data.begin() + 128, 128, 0x22);
		for (uint32 i = 0; i < v.mNumPhysSectors; ++i) {
			auto& p = physical[i]; p.mOffset = i * 128; p.mDiskOffset = -1; p.mImageSize = p.mPhysicalSize = 128;
			p.mRotPos = 0.1f + i * 0.5f; p.mFDCStatus = weak ? 0xF7 : 0xFF; p.mWeakDataOffset = weak ? 64 : -1;
		}
		f.mSlot.mDisk.OnDiskChanging(); f.mSlot.mDisk.GetDiskImage()->FormatTrack(index, 1, &v, v.mNumPhysSectors, physical.data(), data.data()); f.mSlot.mDisk.OnDiskChanged(false);
	}
}

bool ATTestAltirraDiskPhantoms(ATPortableTestContext& context) {
	auto f = std::make_unique<DiskEmulatorPortableFixture>(); ReplaceSector(*f, 719, true, false);
	f->mDrive.SetForcedPhantomSector(720, 0, 1); f->mDrive.SetForcedPhantomSector(720, 1, 0);
	AT_PORTABLE_TEST_ASSERT(context, f->mDrive.GetForcedPhantomSector(720, 0) == 1 && f->mDrive.GetForcedPhantomSector(720, 1) == 0);
	AT_PORTABLE_TEST_ASSERT(context, f->Accel(0x52, 0x40, 128, 720) && f->Status() == 1 && f->Buffer(128)[0] == 0x22);
	AT_PORTABLE_TEST_ASSERT(context, f->Accel(0x52, 0x40, 128, 720) && f->Buffer(128)[0] == 0x11);
	f->mSlot.mDisk.SetAccurateSectorTimingEnabled(true);
	AT_PORTABLE_TEST_ASSERT(context, f->Accel(0x52, 0x40, 128, 720) && f->Status() == 1 && f->Buffer(128)[0] == 0x22);
	AT_PORTABLE_TEST_ASSERT(context, f->Accel(0x52, 0x40, 128, 720) && f->Status() == 1 && f->Buffer(128)[0] == 0x11);
	f->mDrive.SetForcedPhantomSector(720, 255, 0); // invalid index must not touch adjacent storage
	AT_PORTABLE_TEST_ASSERT(context, f->mDrive.GetForcedPhantomSector(720, 255) == -1 && f->mDrive.GetForcedPhantomSector(0, 0) == -1);
	return true;
}

bool ATTestAltirraDiskWeakSectors(ATPortableTestContext& context) {
	auto f = std::make_unique<DiskEmulatorPortableFixture>(); ReplaceSector(*f, 3, false, true);
	AT_PORTABLE_TEST_ASSERT(context, f->Accel(0x52, 0x40, 128, 4) && f->Status() == 0x90);
	const auto first = f->Buffer(128);
	AT_PORTABLE_TEST_ASSERT(context, std::all_of(first.begin(), first.begin() + 64, [](uint8 c) { return c == 0x11; }));
	AT_PORTABLE_TEST_ASSERT(context, f->Accel(0x52, 0x40, 128, 4) && f->Status() == 0x90);
	const auto second = f->Buffer(128);
	AT_PORTABLE_TEST_ASSERT(context, first != second && std::equal(first.begin(), first.begin() + 64, second.begin()));
	return true;
}

bool ATTestAltirraDiskHappyRAM(ATPortableTestContext& context) {
	for (auto mode : {kATDiskEmulationMode_Happy810, kATDiskEmulationMode_Happy1050}) {
		auto f = std::make_unique<DiskEmulatorPortableFixture>(); f->mDrive.SetEmulationMode(mode);
		const uint16 address = mode == kATDiskEmulationMode_Happy810 ? 0x0800 : 0x8000;
		const auto data = f->Pattern(128, 0x5A); f->Buffer(data);
		AT_PORTABLE_TEST_ASSERT(context, f->Accel(0x57, 0x80, 128, address) && f->Status() == 1);
		AT_PORTABLE_TEST_ASSERT(context, f->Accel(0x52, 0x40, 128, address) && f->Buffer(128) == data);
		f->mDrive.Reset(); AT_PORTABLE_TEST_ASSERT(context, f->Accel(0x52, 0x40, 128, address));
		const auto cleared = f->Buffer(128); AT_PORTABLE_TEST_ASSERT(context, std::all_of(cleared.begin(), cleared.end(), [](uint8 c) { return c == 0; }));
		if (mode == kATDiskEmulationMode_Happy1050) {
			AT_PORTABLE_TEST_ASSERT(context, f->Accel(0x52, 0x40, 128, 4) && f->mSlot.mbMotor); // sector 1 can be served from its dedicated buffer
			AT_PORTABLE_TEST_ASSERT(context, f->Accel(0x51, 0, 0) && f->Status() == 1 && !f->mSlot.mbMotor);
		}
	}
	return true;
}
