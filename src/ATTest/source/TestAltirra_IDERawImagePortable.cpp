// Portable raw IDE disk image tests.

#include <array>
#include <cstring>
#include <cwchar>

#include <at/atcore/propertyset.h>
#include <at/attest/portabletest.h>
#include <vd2/system/file.h>
#include <vd2/system/filesys.h>
#include <vd2/system/time.h>

#include <iderawimage.h>

namespace {
	class RawImageTestFile {
	public:
		explicit RawImageTestFile(const VDStringW& path) : mPath(path) {}

		~RawImageTestFile() {
			VDRemoveFile(mPath.c_str());
		}

		VDStringW mPath;
	};
}

bool ATTestAltirraIDERawImage(ATPortableTestContext& context) {
	static uint32 sequence = 0;
	VDStringW baseName;
	baseName.sprintf(
		L"altirra-ide-raw-image-test-%u-%llu-%u.img",
		static_cast<unsigned>(VDGetCurrentProcessId()),
		static_cast<unsigned long long>(VDGetCurrentTick64()),
		static_cast<unsigned>(++sequence));
	RawImageTestFile testFile(VDGetFullPath(baseName.c_str()));

	std::array<uint8, 600> initialData {};
	for(size_t i = 0; i < initialData.size(); ++i)
		initialData[i] = static_cast<uint8>(i * 37 + 11);
	{
		VDFileStream stream(
			testFile.mPath.c_str(),
			nsVDFile::kWrite | nsVDFile::kCreateAlways | nsVDFile::kDenyAll);
		stream.Write(initialData.data(), initialData.size());
	}

	ATIDERawImage image;
	image.Init(testFile.mPath.c_str(), true, true, 3, 10, 2, 5);
	AT_PORTABLE_TEST_ASSERT(context, !image.IsReadOnly());
	AT_PORTABLE_TEST_ASSERT(context, image.GetSectorCount() == 3);
	const ATBlockDeviceGeometry geometry = image.GetGeometry();
	AT_PORTABLE_TEST_ASSERT(context, geometry.mCylinders == 10);
	AT_PORTABLE_TEST_ASSERT(context, geometry.mHeads == 2);
	AT_PORTABLE_TEST_ASSERT(context, geometry.mSectorsPerTrack == 5);
	AT_PORTABLE_TEST_ASSERT(context, geometry.mbSolidState);

	std::array<uint8, 1024> readBuffer;
	readBuffer.fill(0xCD);
	image.ReadSectors(readBuffer.data(), 0, 2);
	AT_PORTABLE_TEST_ASSERT(context,
		!memcmp(readBuffer.data(), initialData.data(), initialData.size()));
	for(size_t i = initialData.size(); i < readBuffer.size(); ++i)
		AT_PORTABLE_TEST_ASSERT(context, readBuffer[i] == 0);

	std::array<uint8, 512> writtenSector {};
	for(size_t i = 0; i < writtenSector.size(); ++i)
		writtenSector[i] = static_cast<uint8>(255 - i);
	image.WriteSectors(writtenSector.data(), 3, 1);
	AT_PORTABLE_TEST_ASSERT(context, image.GetSectorCount() == 4);
	std::array<uint8, 512> roundTrip {};
	image.ReadSectors(roundTrip.data(), 3, 1);
	AT_PORTABLE_TEST_ASSERT(context, roundTrip == writtenSector);

	ATPropertySet capturedSettings;
	image.GetSettings(capturedSettings);
	AT_PORTABLE_TEST_ASSERT(context,
		!std::wcscmp(capturedSettings.GetString("path", L""), testFile.mPath.c_str()));
	AT_PORTABLE_TEST_ASSERT(context, capturedSettings.GetUint32("sectors") == 3);
	AT_PORTABLE_TEST_ASSERT(context, capturedSettings.GetUint32("cylinders") == 10);
	AT_PORTABLE_TEST_ASSERT(context, capturedSettings.GetUint32("heads") == 2);
	AT_PORTABLE_TEST_ASSERT(context, capturedSettings.GetUint32("sectors_per_track") == 5);
	AT_PORTABLE_TEST_ASSERT(context, capturedSettings.GetBool("write_enabled"));
	AT_PORTABLE_TEST_ASSERT(context, capturedSettings.GetBool("solid_state"));
	image.Shutdown();

	ATPropertySet factorySettings;
	factorySettings.SetString("path", testFile.mPath.c_str());
	factorySettings.SetBool("write_enabled", false);
	factorySettings.SetBool("solid_state", false);
	factorySettings.SetUint32("sectors", 2);
	IATDevice *device = nullptr;
	ATCreateDeviceHardDiskRawImage(factorySettings, &device);
	AT_PORTABLE_TEST_ASSERT(context, device != nullptr);
	auto *blockDevice = static_cast<IATBlockDevice *>(
		device->AsInterface(IATBlockDevice::kTypeID));
	AT_PORTABLE_TEST_ASSERT(context, blockDevice != nullptr);
	AT_PORTABLE_TEST_ASSERT(context, blockDevice->IsReadOnly());
	AT_PORTABLE_TEST_ASSERT(context, blockDevice->GetSectorCount() == 4);
	device->Release();
	return true;
}
