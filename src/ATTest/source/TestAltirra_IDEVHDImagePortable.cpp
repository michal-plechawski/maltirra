// Portable VHD block device tests.

#include <array>
#include <cstring>

#include <at/atcore/propertyset.h>
#include <at/attest/portabletest.h>
#include <vd2/system/filesys.h>
#include <vd2/system/time.h>

#include <idevhdimage.h>

namespace {
	class ATIDEVHDTestFiles {
	public:
		ATIDEVHDTestFiles() {
			VDStringW prefix;
			prefix.sprintf(
				L"altirra-vhd-test-%u-%llu-\u00E9",
				static_cast<unsigned>(VDGetCurrentProcessId()),
				static_cast<unsigned long long>(VDGetCurrentTick64()));

			mFixedPath = VDGetFullPath((prefix + L"-fixed.vhd").c_str());
			mDynamicPath = VDGetFullPath((prefix + L"-dynamic.vhd").c_str());
			mChildPath = VDGetFullPath((prefix + L"-child.vhd").c_str());
		}

		~ATIDEVHDTestFiles() {
			VDRemoveFile(mChildPath.c_str());
			VDRemoveFile(mDynamicPath.c_str());
			VDRemoveFile(mFixedPath.c_str());
		}

		VDStringW mFixedPath;
		VDStringW mDynamicPath;
		VDStringW mChildPath;
	};

	template<size_t N>
	void FillPattern(std::array<uint8, N>& data, uint8 seed) {
		for(size_t i = 0; i < data.size(); ++i)
			data[i] = static_cast<uint8>(seed + i * 37 + i / 251);
	}
}

bool ATTestAltirraIDEVHDImage(ATPortableTestContext& context) {
	ATIDEVHDTestFiles files;
	std::array<uint8, 1024> parentData {};
	FillPattern(parentData, 11);

	ATIDEVHDImage fixedImage;
	fixedImage.InitNew(files.mFixedPath.c_str(), 4, 8, 128, false, nullptr);
	AT_PORTABLE_TEST_ASSERT(context, fixedImage.GetSectorCount() == 128);
	AT_PORTABLE_TEST_ASSERT(context, fixedImage.GetVHDHeads() == 4);
	AT_PORTABLE_TEST_ASSERT(context, fixedImage.GetVHDSectorsPerTrack() == 8);
	AT_PORTABLE_TEST_ASSERT(context, !fixedImage.IsReadOnly());
	AT_PORTABLE_TEST_ASSERT(context,
		fixedImage.AsInterface(IATBlockDeviceDirectAccess::kTypeID) != nullptr);
	fixedImage.WriteSectors(parentData.data(), 7, 2);
	fixedImage.Flush();
	fixedImage.Shutdown();

	fixedImage.Init(files.mFixedPath.c_str(), false, true);
	AT_PORTABLE_TEST_ASSERT(context, fixedImage.IsReadOnly());
	AT_PORTABLE_TEST_ASSERT(context, fixedImage.GetGeometry().mbSolidState);
	AT_PORTABLE_TEST_ASSERT(context, fixedImage.GetVHDDirectAccessPath() == files.mFixedPath);
	std::array<uint8, 1024> readback {};
	fixedImage.ReadSectors(readback.data(), 7, 2);
	AT_PORTABLE_TEST_ASSERT(context, readback == parentData);
	fixedImage.Shutdown();

	std::array<uint8, 1536> dynamicData {};
	FillPattern(dynamicData, 29);
	std::array<uint8, 512> secondBlockData {};
	FillPattern(secondBlockData, 73);

	ATIDEVHDImage dynamicImage;
	dynamicImage.InitNew(files.mDynamicPath.c_str(), 8, 16, 5000, true, nullptr);
	dynamicImage.WriteSectors(dynamicData.data(), 10, 3);
	dynamicImage.WriteSectors(secondBlockData.data(), 4097, 1);
	dynamicImage.Flush();
	dynamicImage.Shutdown();

	dynamicImage.Init(files.mDynamicPath.c_str(), true, false);
	readback.fill(0xCD);
	std::array<uint8, 1536> dynamicReadback {};
	dynamicImage.ReadSectors(dynamicReadback.data(), 10, 3);
	AT_PORTABLE_TEST_ASSERT(context, dynamicReadback == dynamicData);
	std::array<uint8, 512> secondBlockReadback {};
	dynamicImage.ReadSectors(secondBlockReadback.data(), 4097, 1);
	AT_PORTABLE_TEST_ASSERT(context, secondBlockReadback == secondBlockData);

	std::array<uint8, 512> zeroSector {};
	dynamicImage.WriteSectors(zeroSector.data(), 11, 1);
	dynamicImage.Flush();
	dynamicReadback.fill(0xCD);
	dynamicImage.ReadSectors(dynamicReadback.data(), 10, 3);
	AT_PORTABLE_TEST_ASSERT(context,
		!memcmp(dynamicReadback.data(), dynamicData.data(), 512));
	AT_PORTABLE_TEST_ASSERT(context,
		!memcmp(dynamicReadback.data() + 512, zeroSector.data(), 512));
	AT_PORTABLE_TEST_ASSERT(context,
		!memcmp(dynamicReadback.data() + 1024, dynamicData.data() + 1024, 512));
	dynamicImage.Shutdown();

	ATIDEVHDImage parentImage;
	parentImage.Init(files.mFixedPath.c_str(), false, false);
	ATIDEVHDImage childImage;
	childImage.InitNew(files.mChildPath.c_str(), 0, 0, 0, true, &parentImage);
	childImage.Shutdown();
	parentImage.Shutdown();

	childImage.Init(files.mChildPath.c_str(), true, false);
	readback.fill(0);
	childImage.ReadSectors(readback.data(), 7, 2);
	AT_PORTABLE_TEST_ASSERT(context, readback == parentData);

	std::array<uint8, 512> childData {};
	FillPattern(childData, 151);
	childImage.WriteSectors(childData.data(), 7, 1);
	childImage.Flush();
	childImage.Shutdown();

	childImage.Init(files.mChildPath.c_str(), false, false);
	readback.fill(0);
	childImage.ReadSectors(readback.data(), 7, 2);
	AT_PORTABLE_TEST_ASSERT(context,
		!memcmp(readback.data(), childData.data(), 512));
	AT_PORTABLE_TEST_ASSERT(context,
		!memcmp(readback.data() + 512, parentData.data() + 512, 512));
	childImage.Shutdown();

	ATPropertySet settings;
	settings.SetString("path", files.mDynamicPath.c_str());
	settings.SetBool("write_enabled", false);
	settings.SetBool("solid_state", true);
	IATDevice *device = nullptr;
	ATCreateDeviceHardDiskVHDImage(settings, &device);
	AT_PORTABLE_TEST_ASSERT(context, device != nullptr);
	auto *blockDevice = static_cast<IATBlockDevice *>(
		device->AsInterface(IATBlockDevice::kTypeID));
	AT_PORTABLE_TEST_ASSERT(context, blockDevice != nullptr);
	AT_PORTABLE_TEST_ASSERT(context, blockDevice->IsReadOnly());
	AT_PORTABLE_TEST_ASSERT(context, blockDevice->GetSectorCount() == 5000);
	AT_PORTABLE_TEST_ASSERT(context, blockDevice->GetGeometry().mbSolidState);
	device->Release();
	return true;
}
