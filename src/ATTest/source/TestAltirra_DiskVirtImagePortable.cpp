// Altirra portable DOS 2 virtual folder disk tests

#include <cstring>

#include <vd2/system/file.h>
#include <vd2/system/filesys.h>
#include <vd2/system/time.h>

#include <at/atio/diskimage.h>
#include <at/attest/portabletest.h>

namespace {
	class VirtualDiskTestSandbox {
	public:
		explicit VirtualDiskTestSandbox(const VDStringW& path)
			: mPath(path)
			, mFilePath(VDMakePath(path.c_str(), L"HELLO.TXT")) {
		}

		~VirtualDiskTestSandbox() {
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

bool ATTestAltirraDiskVirtImage(ATPortableTestContext& context) {
	static uint32 sequence = 0;
	VDStringW baseName;
	baseName.sprintf(
		L"altirra-virtual-disk-test-%u-%llu-%u",
		static_cast<unsigned>(VDGetCurrentProcessId()),
		static_cast<unsigned long long>(VDGetCurrentTick64()),
		static_cast<unsigned>(++sequence));
	VirtualDiskTestSandbox sandbox(VDGetFullPath(baseName.c_str()));
	VDCreateDirectory(sandbox.mPath.c_str());
	{
		VDFileStream stream(sandbox.mFilePath.c_str(), nsVDFile::kWrite | nsVDFile::kCreateAlways);
		char contents[130];
		memset(contents, 'X', sizeof contents);
		memcpy(contents, "Hello", 5);
		memcpy(contents + 125, "World", 5);
		stream.Write(contents, sizeof contents);
	}

	vdrefptr<IATDiskImage> image;
	ATMountDiskImageVirtualFolder(sandbox.mPath.c_str(), 720, ~image);
	AT_PORTABLE_TEST_ASSERT(context, image);
	AT_PORTABLE_TEST_ASSERT(context, image->GetSectorSize() == 128);
	AT_PORTABLE_TEST_ASSERT(context, image->GetPhysicalSectorCount() == 720);
	AT_PORTABLE_TEST_ASSERT(context, image->GetBootSectorCount() == 3);
	AT_PORTABLE_TEST_ASSERT(context, image->IsDynamic());

	uint8 sector[128] {};
	image->ReadPhysicalSector(360, sector, sizeof sector);
	AT_PORTABLE_TEST_ASSERT(context, sector[0] == 0x42);
	AT_PORTABLE_TEST_ASSERT(context, sector[1] == 2 && sector[2] == 0);
	AT_PORTABLE_TEST_ASSERT(context, sector[3] == 4 && sector[4] == 0);
	AT_PORTABLE_TEST_ASSERT(context, !memcmp(sector + 5, "HELLO   TXT", 11));

	AT_PORTABLE_TEST_ASSERT(context, image->ReadVirtualSector(3, sector, sizeof sector) == 128);
	AT_PORTABLE_TEST_ASSERT(context, !memcmp(sector, "Hello", 5));
	AT_PORTABLE_TEST_ASSERT(context, sector[127] == 125);
	const uint32 nextSector = ((sector[125] & 3) << 8) + sector[126];
	AT_PORTABLE_TEST_ASSERT(context, nextSector >= 1 && nextSector <= 720);
	AT_PORTABLE_TEST_ASSERT(context, image->ReadVirtualSector(nextSector - 1, sector, sizeof sector) == 128);
	AT_PORTABLE_TEST_ASSERT(context, !memcmp(sector, "World", 5));
	AT_PORTABLE_TEST_ASSERT(context, sector[125] == 0 && sector[126] == 0);
	AT_PORTABLE_TEST_ASSERT(context, sector[127] == 5);
	AT_PORTABLE_TEST_ASSERT(context, !image->WriteVirtualSector(3, sector, sizeof sector));
	return true;
}
