// Altirra portable SpartaDOS virtual folder disk tests

#include <cstring>

#include <vd2/system/binary.h>
#include <vd2/system/file.h>
#include <vd2/system/filesys.h>
#include <vd2/system/time.h>

#include <at/atio/diskimage.h>
#include <at/attest/portabletest.h>

namespace {
	class VirtualSDFSTestSandbox {
	public:
		explicit VirtualSDFSTestSandbox(const VDStringW& path)
			: mPath(path)
			, mFilePath(VDMakePath(path.c_str(), L"HELLO.TXT")) {
		}

		~VirtualSDFSTestSandbox() {
			VDRemoveFile(mFilePath.c_str());
			try {
				VDRemoveDirectory(mPath.c_str());
			} catch(...) {
			}
		}

		VDStringW mPath;
		VDStringW mFilePath;
	};

	bool CheckVirtualSDFS(ATPortableTestContext& context, const wchar_t *path, uint32 sectorSize) {
		vdrefptr<IATDiskImage> image;
		ATMountDiskImageVirtualFolderSDFS(path, sectorSize, 0x1234, ~image);
		AT_PORTABLE_TEST_ASSERT(context, image);
		AT_PORTABLE_TEST_ASSERT(context, image->GetSectorSize() == sectorSize);
		AT_PORTABLE_TEST_ASSERT(context, image->GetPhysicalSectorCount() == 65535);
		AT_PORTABLE_TEST_ASSERT(context, image->GetBootSectorCount() == (sectorSize == 512 ? 0 : 3));
		AT_PORTABLE_TEST_ASSERT(context, image->IsDynamic());

		uint8 sector[512] {};
		const uint32 bootSectorSize = sectorSize == 512 ? 512 : 128;
		AT_PORTABLE_TEST_ASSERT(context, image->GetSectorSize(0) == bootSectorSize);
		AT_PORTABLE_TEST_ASSERT(context, image->ReadVirtualSector(0, sector, sizeof sector) == bootSectorSize);
		const uint32 bootSectorCount = sectorSize == 512 ? 1 : 3;
		const uint32 bitmapSectorCount = 8191 / sectorSize + 1;
		const uint32 rootMapIndex = bootSectorCount + bitmapSectorCount;

		AT_PORTABLE_TEST_ASSERT(context, image->GetSectorSize(rootMapIndex) == sectorSize);
		AT_PORTABLE_TEST_ASSERT(context, image->ReadVirtualSector(rootMapIndex, sector, sizeof sector) == sectorSize);
		const uint32 rootDataSector = VDReadUnalignedLEU16(sector + 4);
		AT_PORTABLE_TEST_ASSERT(context, rootDataSector > 0);
		AT_PORTABLE_TEST_ASSERT(context, image->ReadVirtualSector(rootDataSector - 1, sector, sizeof sector) == sectorSize);

		const uint8 *fileEntry = sector + 23;
		AT_PORTABLE_TEST_ASSERT(context, (fileEntry[0] & 0x08) != 0);
		AT_PORTABLE_TEST_ASSERT(context, !memcmp(fileEntry + 6, "HELLO   ", 8));
		AT_PORTABLE_TEST_ASSERT(context, !memcmp(fileEntry + 14, "TXT", 3));
		const uint32 fileMapSector = VDReadUnalignedLEU16(fileEntry + 1);
		AT_PORTABLE_TEST_ASSERT(context, fileMapSector > 0);

		AT_PORTABLE_TEST_ASSERT(context, image->ReadVirtualSector(fileMapSector - 1, sector, sizeof sector) == sectorSize);
		const uint32 fileDataSector = VDReadUnalignedLEU16(sector + 4);
		AT_PORTABLE_TEST_ASSERT(context, fileDataSector > 0);
		AT_PORTABLE_TEST_ASSERT(context, image->ReadVirtualSector(fileDataSector - 1, sector, sizeof sector) == sectorSize);
		AT_PORTABLE_TEST_ASSERT(context, !memcmp(sector, "SDFS data", 9));
		AT_PORTABLE_TEST_ASSERT(context, !image->WriteVirtualSector(fileDataSector - 1, sector, sectorSize));
		return true;
	}
}

bool ATTestAltirraDiskVirtImageSDFS(ATPortableTestContext& context) {
	static uint32 sequence = 0;
	VDStringW baseName;
	baseName.sprintf(
		L"altirra-virtual-sdfs-test-%u-%llu-%u",
		static_cast<unsigned>(VDGetCurrentProcessId()),
		static_cast<unsigned long long>(VDGetCurrentTick64()),
		static_cast<unsigned>(++sequence));
	VirtualSDFSTestSandbox sandbox(VDGetFullPath(baseName.c_str()));
	VDCreateDirectory(sandbox.mPath.c_str());
	{
		VDFileStream stream(sandbox.mFilePath.c_str(), nsVDFile::kWrite | nsVDFile::kCreateAlways);
		static constexpr char kFileContents[] = "SDFS data";
		stream.Write(kFileContents, sizeof kFileContents - 1);
	}

	if (!CheckVirtualSDFS(context, sandbox.mPath.c_str(), 128))
		return false;
	if (!CheckVirtualSDFS(context, sandbox.mPath.c_str(), 256))
		return false;
	if (!CheckVirtualSDFS(context, sandbox.mPath.c_str(), 512))
		return false;
	return true;
}
