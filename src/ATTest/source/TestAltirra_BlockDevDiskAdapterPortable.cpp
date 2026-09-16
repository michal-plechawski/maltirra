// Portable tests for the disk-image-to-block-device adapter.

#include <array>
#include <cstring>
#include <cwchar>

#include <at/atcore/propertyset.h>
#include <at/atio/diskfssdx2util.h>
#include <at/attest/portabletest.h>
#include <vd2/system/binary.h>
#include <vd2/system/error.h>
#include <vd2/system/file.h>
#include <vd2/system/filesys.h>
#include <vd2/system/time.h>

#include <blockdevdiskadapter.h>

namespace {
	class BlockAdapterTestSandbox {
	public:
		explicit BlockAdapterTestSandbox(const VDStringW& path)
			: mPath(path)
			, mFilePath(VDMakePath(path.c_str(), L"HELLO.TXT")) {
		}

		~BlockAdapterTestSandbox() {
			VDRemoveFile(mFilePath.c_str());
			try {
				VDRemoveDirectory(mPath.c_str());
			} catch(...) {
			}
		}

		VDStringW mPath;
		VDStringW mFilePath;
	};
}

bool ATTestAltirraBlockDevDiskAdapter(ATPortableTestContext& context) {
	static uint32 sequence = 0;
	VDStringW baseName;
	baseName.sprintf(
		L"altirra-block-adapter-test-%u-%llu-%u",
		static_cast<unsigned>(VDGetCurrentProcessId()),
		static_cast<unsigned long long>(VDGetCurrentTick64()),
		static_cast<unsigned>(++sequence));
	BlockAdapterTestSandbox sandbox(VDGetFullPath(baseName.c_str()));
	VDCreateDirectory(sandbox.mPath.c_str());
	{
		VDFileStream stream(
			sandbox.mFilePath.c_str(),
			nsVDFile::kWrite | nsVDFile::kCreateAlways);
		static constexpr char kContents[] = "Block adapter data";
		stream.Write(kContents, sizeof kContents - 1);
	}

	ATBlockDeviceVirtSDFS adapter;
	AT_PORTABLE_TEST_ASSERT(context, adapter.IsReadOnly());
	AT_PORTABLE_TEST_ASSERT(context, adapter.GetSectorCount() == 65535 + 8);
	AT_PORTABLE_TEST_ASSERT(context, adapter.GetSerialNumber() == 0);
	const ATBlockDeviceGeometry geometry = adapter.GetGeometry();
	AT_PORTABLE_TEST_ASSERT(context, geometry.mCylinders == 0);
	AT_PORTABLE_TEST_ASSERT(context, geometry.mHeads == 0);
	AT_PORTABLE_TEST_ASSERT(context, geometry.mSectorsPerTrack == 0);
	AT_PORTABLE_TEST_ASSERT(context, !geometry.mbSolidState);

	ATPropertySet settings;
	settings.SetString("path", sandbox.mPath.c_str());
	AT_PORTABLE_TEST_ASSERT(context, !adapter.SetSettings(settings));
	AT_PORTABLE_TEST_ASSERT(context, adapter.SetSettings(settings));
	adapter.Init();
	AT_PORTABLE_TEST_ASSERT(context, adapter.GetSectorCount() == 65535 + 8);

	std::array<uint8, 512 * 9> sectors;
	sectors.fill(0xCD);
	adapter.ReadSectors(sectors.data(), 0, 9);
	const uint8 *mbr = sectors.data();
	AT_PORTABLE_TEST_ASSERT(context, mbr[0x1BE] == 0x80);
	AT_PORTABLE_TEST_ASSERT(context, mbr[0x1C2] == 0x7F);
	AT_PORTABLE_TEST_ASSERT(context, VDReadUnalignedLEU32(mbr + 0x1C6) == 1);
	AT_PORTABLE_TEST_ASSERT(context,
		VDReadUnalignedLEU32(mbr + 0x1CA) == adapter.GetSectorCount() - 1);
	AT_PORTABLE_TEST_ASSERT(context, mbr[0x1FE] == 0x55 && mbr[0x1FF] == 0xAA);

	const uint8 *apt = sectors.data() + 512;
	AT_PORTABLE_TEST_ASSERT(context, !memcmp(apt + 1, "APT", 3));
	AT_PORTABLE_TEST_ASSERT(context, apt[5] == 2);
	AT_PORTABLE_TEST_ASSERT(context, apt[0x10] == 3);
	AT_PORTABLE_TEST_ASSERT(context, VDReadUnalignedLEU32(apt + 0x12) == 8);
	AT_PORTABLE_TEST_ASSERT(context, VDReadUnalignedLEU32(apt + 0x16) == 65535);
	AT_PORTABLE_TEST_ASSERT(context, VDReadUnalignedLEU16(apt + 0x1A) == 1);
	AT_PORTABLE_TEST_ASSERT(context, apt[0x1C] == 0xC0);

	for(size_t i = 2 * 512; i < 8 * 512; ++i)
		AT_PORTABLE_TEST_ASSERT(context, sectors[i] == 0);

	const uint8 *sdfsBootSector = sectors.data() + 8 * 512;
	std::array<uint8, 512> expectedBootSector {};
	memcpy(expectedBootSector.data(), kATSDFSBootSector0_512b, 128);
	// Volume name, sequence, and random ID are assigned dynamically.
	memcpy(expectedBootSector.data() + 22, sdfsBootSector + 22, 8);
	expectedBootSector[38] = sdfsBootSector[38];
	expectedBootSector[39] = sdfsBootSector[39];
	AT_PORTABLE_TEST_ASSERT(context,
		!memcmp(sdfsBootSector, expectedBootSector.data(), expectedBootSector.size()));

	std::array<uint8, 512> outsideSector;
	outsideSector.fill(0xCD);
	adapter.ReadSectors(outsideSector.data(), adapter.GetSectorCount(), 1);
	for(uint8 value : outsideSector)
		AT_PORTABLE_TEST_ASSERT(context, value == 0);

	bool rejectedWrite = false;
	try {
		adapter.WriteSectors(expectedBootSector.data(), 8, 1);
	} catch(const MyError&) {
		rejectedWrite = true;
	}
	AT_PORTABLE_TEST_ASSERT(context, rejectedWrite);

	ATPropertySet capturedSettings;
	adapter.GetSettings(capturedSettings);
	AT_PORTABLE_TEST_ASSERT(context,
		!wcscmp(capturedSettings.GetString("path", L""), sandbox.mPath.c_str()));
	adapter.Shutdown();
	return true;
}
