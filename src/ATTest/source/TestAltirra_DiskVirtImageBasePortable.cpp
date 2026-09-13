// Altirra portable virtual disk image base tests

#include <at/attest/portabletest.h>
#include <diskvirtimagebase.h>

namespace {
	class TestVirtualDiskImage final : public ATDiskImageVirtualFolderBase {
	public:
		ATDiskGeometryInfo GetGeometry() const override { return {}; }
		uint32 GetSectorSize() const override { return 128; }
		uint32 GetSectorSize(uint32) const override { return 128; }
		uint32 GetBootSectorCount() const override { return 0; }
		uint32 GetPhysicalSectorCount() const override { return 0; }
		void GetPhysicalSectorInfo(uint32, ATDiskPhysicalSectorInfo&) const override {}
		void ReadPhysicalSector(uint32, void *, uint32) override {}
		void WritePhysicalSector(uint32, const void *, uint32, uint8) override {}
		uint32 GetVirtualSectorCount() const override { return 0; }
		void GetVirtualSectorInfo(uint32, ATDiskVirtualSectorInfo&) const override {}
		uint32 ReadVirtualSector(uint32, void *, uint32) override { return 0; }
		bool WriteVirtualSector(uint32, const void *, uint32) override { return false; }
		void Resize(uint32) override {}
		void FormatTrack(uint32, uint32, const ATDiskVirtualSectorInfo *, uint32,
			const ATDiskPhysicalSectorInfo *, const uint8 *) override {}
		bool IsSafeToReinterleave() const override { return false; }
		void Reinterleave(ATDiskInterleave) override {}
	};
}

bool ATTestAltirraDiskVirtImageBase(ATPortableTestContext& context) {
	vdrefptr<TestVirtualDiskImage> image { new TestVirtualDiskImage };
	IATDiskImage *disk = image;
	AT_PORTABLE_TEST_ASSERT(context, image->AsInterface(IATDiskImage::kTypeID) == disk);
	AT_PORTABLE_TEST_ASSERT(context, image->AsInterface(0) == nullptr);
	AT_PORTABLE_TEST_ASSERT(context, disk->GetImageType() == kATImageType_Disk);
	AT_PORTABLE_TEST_ASSERT(context, disk->GetTimingMode() == kATDiskTimingMode_Any);
	AT_PORTABLE_TEST_ASSERT(context, !disk->IsDirty());
	AT_PORTABLE_TEST_ASSERT(context, !disk->IsUpdatable());
	AT_PORTABLE_TEST_ASSERT(context, disk->IsDynamic());
	AT_PORTABLE_TEST_ASSERT(context, disk->GetImageFormat() == kATDiskImageFormat_None);
	AT_PORTABLE_TEST_ASSERT(context, disk->GetImageChecksum() == 0);
	AT_PORTABLE_TEST_ASSERT(context, !disk->GetImageFileCRC().has_value());
	AT_PORTABLE_TEST_ASSERT(context, !disk->GetImageFileSHA256().has_value());
	disk->Flush();
	disk->SetPath(L"ignored.atr", kATDiskImageFormat_ATR);
	disk->Save(L"ignored.atr", kATDiskImageFormat_ATR);
	AT_PORTABLE_TEST_ASSERT(context, !disk->IsDirty());
	AT_PORTABLE_TEST_ASSERT(context, !disk->IsUpdatable());
	return true;
}
