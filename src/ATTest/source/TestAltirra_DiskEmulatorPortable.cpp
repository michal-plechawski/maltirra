// Full disk commands through the production SIO accelerator and real images.
#include <at/attest/portabletest.h>
#include "diskemulatorportablefixture.h"

bool ATTestAltirraDiskStatus(ATPortableTestContext& context) {
	for (int i = 0; i < kATDiskEmulationModeCount; ++i) {
		auto f = std::make_unique<DiskEmulatorPortableFixture>(); f->mDrive.SetEmulationMode(static_cast<ATDiskEmulationMode>(i)); f->mDrive.Reset();
		AT_PORTABLE_TEST_ASSERT(context, f->Accel(0x53, 0x40, 4) && f->Status() == 1);
		auto status = f->Buffer(4);
		AT_PORTABLE_TEST_ASSERT(context, !(status[0] & 0x08) && status[2] == (i == kATDiskEmulationMode_XF551 ? 0xFE : 0xE0) && status[3] == 0);
		f->mSlot.mDisk.SetWriteMode(kATMediaWriteMode_RO);
		AT_PORTABLE_TEST_ASSERT(context, f->Accel(0x53, 0x40, 4) && (f->Buffer(4)[0] & 0x08));
		f->mDrive.SetEnabled(false);
		AT_PORTABLE_TEST_ASSERT(context, !f->Accel(0x53, 0x40, 4));
	}
	return true;
}

bool ATTestAltirraDiskReadWrite(ATPortableTestContext& context) {
	for (uint32 size : {128U, 256U, 512U, 8192U}) {
		auto f = std::make_unique<DiskEmulatorPortableFixture>(true, size, size > 256 ? 18 : 720);
		const auto data = f->Pattern(size); f->Fill(3, data);
		AT_PORTABLE_TEST_ASSERT(context, f->Accel(0x52, 0x40, static_cast<uint16>(size), 4) && f->Status() == 1 && f->Buffer(size) == data);
		const auto changed = f->Pattern(size, 0xA6); f->Buffer(changed);
		AT_PORTABLE_TEST_ASSERT(context, f->Accel(0x57, 0x80, static_cast<uint16>(size), 4) && f->Status() == 1);
		AT_PORTABLE_TEST_ASSERT(context, f->Accel(0x52, 0x40, static_cast<uint16>(size), 4) && f->Buffer(size) == changed);
		f->mSlot.mDisk.SetAccurateSectorTimingEnabled(true);
		AT_PORTABLE_TEST_ASSERT(context, f->Accel(0x52, 0x40, static_cast<uint16>(size), 4) && f->Status() == 1 && f->Buffer(size) == changed);
		f->mSlot.mDisk.SetAccurateSectorTimingEnabled(false);
		if (size == 256) {
			const auto boot = f->Pattern(128, 0x55); f->Fill(2, boot);
			AT_PORTABLE_TEST_ASSERT(context, f->Accel(0x52, 0x40, 128, 3) && f->Buffer(128) == boot);
		}
	}
	return true;
}

bool ATTestAltirraDiskProtection(ATPortableTestContext& context) {
	auto f = std::make_unique<DiskEmulatorPortableFixture>(); const auto original = f->Pattern(128); f->Fill(3, original);
	f->mSlot.mDisk.SetWriteMode(kATMediaWriteMode_RO); f->Buffer(f->Pattern(128, 0x66));
	AT_PORTABLE_TEST_ASSERT(context, f->Accel(0x57, 0x80, 128, 4) && f->Status() == 0x90);
	std::array<uint8, 128> actual {}; f->mSlot.mDisk.GetDiskImage()->ReadVirtualSector(3, actual.data(), actual.size());
	AT_PORTABLE_TEST_ASSERT(context, std::equal(actual.begin(), actual.end(), original.begin()));
	f->mSlot.mDisk.SetWriteMode(kATMediaWriteMode_VRWSafe);
	AT_PORTABLE_TEST_ASSERT(context, f->Accel(0x21, 0x40, 128) && f->Status() == 0x90);
	AT_PORTABLE_TEST_ASSERT(context, f->Accel(0x52, 0x40, 128, 0) && f->Status() == 0x8B);
	AT_PORTABLE_TEST_ASSERT(context, f->Accel(0x52, 0x40, 128, 721) && f->Status() == 0x8B);
	f->mSlot.mDisk.UnloadDisk();
	AT_PORTABLE_TEST_ASSERT(context, f->Accel(0x52, 0x40, 128) && f->Status() == 0x90);
	return true;
}

bool ATTestAltirraDiskPERCOM(ATPortableTestContext& context) {
	auto f = std::make_unique<DiskEmulatorPortableFixture>();
	AT_PORTABLE_TEST_ASSERT(context, f->Accel(0x4E, 0x40, 12) && f->Status() == 1);
	auto percom = f->Buffer(12);
	AT_PORTABLE_TEST_ASSERT(context, percom[0] == 40 && percom[2] == 0 && percom[3] == 18 && percom[4] == 0 && percom[6] == 0 && percom[7] == 128);
	percom[6] = 1; percom[7] = 0; percom[5] = 4; f->Buffer(percom);
	AT_PORTABLE_TEST_ASSERT(context, f->Accel(0x4F, 0x80, 12) && f->Status() == 1);
	AT_PORTABLE_TEST_ASSERT(context, f->Accel(0x4E, 0x40, 12) && f->Buffer(12) == percom);
	for (int field : {0, 3, 4, 6}) {
		auto invalid = percom; invalid[field] = field == 4 ? 2 : field == 6 ? 3 : 0; f->Buffer(invalid);
		AT_PORTABLE_TEST_ASSERT(context, f->Accel(0x4F, 0x80, 12) && f->Status() == 0x90);
	}
	return true;
}

bool ATTestAltirraDiskFormat(ATPortableTestContext& context) {
	for (auto mode : {kATDiskEmulationMode_Generic, kATDiskEmulationMode_810, kATDiskEmulationMode_1050, kATDiskEmulationMode_XF551}) {
		auto f = std::make_unique<DiskEmulatorPortableFixture>(); f->mDrive.SetEmulationMode(mode);
		f->Fill(0, f->Pattern(128));
		AT_PORTABLE_TEST_ASSERT(context, f->Accel(0x21, 0x40, 128) && f->Status() == 1);
		auto result = f->Buffer(128);
		AT_PORTABLE_TEST_ASSERT(context, result[0] == 0xFF && result[1] == 0xFF && f->mSlot.mDisk.GetDiskImage()->GetVirtualSectorCount() == 720);
		std::array<uint8, 128> zero {}; f->mSlot.mDisk.GetDiskImage()->ReadVirtualSector(0, zero.data(), zero.size());
		AT_PORTABLE_TEST_ASSERT(context, std::all_of(zero.begin(), zero.end(), [](uint8 c) { return c == 0; }));
	}
	return true;
}

bool ATTestAltirraDiskHighSpeed(ATPortableTestContext& context) {
	for (int i = 0; i < kATDiskEmulationModeCount; ++i) {
		auto f = std::make_unique<DiskEmulatorPortableFixture>(); const auto mode = static_cast<ATDiskEmulationMode>(i); f->mDrive.SetEmulationMode(mode);
		const auto& profile = ATGetDiskProfile(mode);
		AT_PORTABLE_TEST_ASSERT(context, f->Accel(0x3F, 0x40, 1));
		AT_PORTABLE_TEST_ASSERT(context, f->Status() == (profile.mbSupportedCmdGetHighSpeedIndex ? 1 : 0x8B));
		if (profile.mbSupportedCmdGetHighSpeedIndex) AT_PORTABLE_TEST_ASSERT(context, f->Buffer(1)[0] == profile.mHighSpeedIndex);
	}
	return true;
}
