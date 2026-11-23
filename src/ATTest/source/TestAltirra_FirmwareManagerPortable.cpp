// Firmware catalog, ROM bytes and special VFS tested through production services.
#include <algorithm>
#include <array>
#include <cstring>
#include <cwchar>
#include <memory>
#include <utility>
#include <vector>
#include <at/atcore/vfs.h>
#include <at/attest/portabletest.h>
#include <vd2/system/binary.h>
#include <vd2/system/error.h>
#include <vd2/system/file.h>
#include <vd2/system/filesys.h>
#include <vd2/system/process.h>
#include <vd2/system/registry.h>
#include <vd2/system/registrymemory.h>
#include <vd2/system/time.h>
#include <vd2/system/zip.h>
#include <firmwaremanager.h>
#include <oshelper.h>
#include "../../Altirra/res/resource.h"

namespace {
	struct RegistryScope {
		IVDRegistryProvider *mpPrevious = VDGetRegistryProvider();
		VDStringA mAppKey {VDRegistryAppKey::getDefaultKey()};
		VDRegistryProviderMemory mProvider;
		RegistryScope() { VDSetRegistryProvider(&mProvider); VDRegistryAppKey::setDefaultKey("FirmwarePortableTests"); }
		~RegistryScope() { VDRegistryAppKey::setDefaultKey(mAppKey.c_str()); VDSetRegistryProvider(mpPrevious); }
	};
	struct FirmwareFile {
		VDStringW mPath;
		std::vector<uint8> mData;
		explicit FirmwareFile(uint32 size = 8192, uint8 fill = 0) : mData(size, fill) {
			static uint32 serial = 0;
			VDStringW name;
			name.sprintf(L"altirra-firmware-%u-%llu-%u.rom", static_cast<unsigned>(VDGetCurrentProcessId()),
				static_cast<unsigned long long>(VDGetCurrentTick64()), ++serial);
			mPath = VDGetFullPath(name.c_str());
			if (!fill) for (uint32 i = 0; i < size; ++i) mData[i] = static_cast<uint8>(i * 17 + 3);
			VDFile file(mPath.c_str(), nsVDFile::kWrite | nsVDFile::kCreateAlways);
			if (size) file.write(mData.data(), size);
		}
		~FirmwareFile() { VDRemoveFile(mPath.c_str()); }
		uint64 Add(ATFirmwareManager& manager, const wchar_t *name = L"Synthetic BASIC") const {
			ATFirmwareInfo info {};
			info.mPath = mPath; info.mName = name; info.mType = kATFirmwareType_Basic;
			info.mFlags = kATFirmwareFlags_InvertOPTION;
			manager.AddFirmware(info);
			return ATGetFirmwareIdFromPath(mPath.c_str());
		}
	};
	std::vector<uint8> ReadView(ATVFSFileView& view) {
		auto& stream = view.GetStream();
		std::vector<uint8> result(static_cast<size_t>(stream.Length()));
		if (!result.empty()) stream.Read(result.data(), static_cast<sint32>(result.size()));
		return result;
	}
}

bool ATTestAltirraFirmwareTypes(ATPortableTestContext& context) {
	for (uint32 i = 1; i < kATFirmwareTypeCount; ++i) {
		const auto type = static_cast<ATFirmwareType>(i);
		AT_PORTABLE_TEST_ASSERT(context, ATGetFirmwareTypeFromName(ATGetFirmwareTypeName(type)) == type);
		AT_PORTABLE_TEST_ASSERT(context, ATIsKernelFirmwareType(type) == (i >= kATFirmwareType_Kernel800_OSA && i <= kATFirmwareType_Kernel1200XL));
	}
	AT_PORTABLE_TEST_ASSERT(context, ATGetFirmwareTypeFromName("missing") == kATFirmwareType_Unknown);
	AT_PORTABLE_TEST_ASSERT(context, !*ATGetFirmwareTypeName(static_cast<ATFirmwareType>(0xFFFFFFFF)));
	AT_PORTABLE_TEST_ASSERT(context, ATIsSpecificFirmwareTypeCompatible(kATFirmwareType_Basic, kATSpecificFirmwareType_BASICRevC));
	AT_PORTABLE_TEST_ASSERT(context, !ATIsSpecificFirmwareTypeCompatible(kATFirmwareType_KernelXL, kATSpecificFirmwareType_BASICRevC));
	AT_PORTABLE_TEST_ASSERT(context, ATIsSpecificFirmwareTypeCompatible(kATFirmwareType_Kernel1200XL, kATSpecificFirmwareType_XLOSr4));
	return true;
}

bool ATTestAltirraFirmwarePathIds(ATPortableTestContext& context) {
	// Golden FNV-1a IDs use lower-case UTF-16 code units, including surrogate
	// pairs. They must agree on Windows wchar_t=16 and macOS wchar_t=32.
	AT_PORTABLE_TEST_ASSERT(context, ATGetFirmwareIdFromPath(L"Altirra.ROM") == ATGetFirmwareIdFromPath(L"altirra.rom"));
	AT_PORTABLE_TEST_ASSERT(context, ATGetFirmwareIdFromPath(L"Altirra.ROM") == UINT64_C(0xD89E469A52D56346));
	AT_PORTABLE_TEST_ASSERT(context, ATGetFirmwareIdFromPath(L"rom-\U0001F600.bin") == UINT64_C(0xF4B8EFD641D8EB3E));
	AT_PORTABLE_TEST_ASSERT(context, ATGetFirmwareIdFromPath(L"rom-\U0001F600.bin") == ATGetFirmwareIdFromPath(L"rom-\xD83D\xDE00.bin"));
	AT_PORTABLE_TEST_ASSERT(context, ATGetFirmwareIdFromPath(L"Altirra.ROM") & (UINT64_C(1) << 63));
	const VDStringW absolute = VDMakePath(VDGetProgramPath().c_str(), L"portable-firmware.rom");
	AT_PORTABLE_TEST_ASSERT(context, ATGetFirmwareIdFromPath(absolute.c_str()) == ATGetFirmwareIdFromPath(L"portable-firmware.rom"));
	return true;
}

bool ATTestAltirraFirmwareInternal(ATPortableTestContext& context) {
	constexpr std::array<std::pair<uint32,uint32>, 17> resources {{
		{kATFirmwareId_NoKernel, IDR_NOKERNEL}, {kATFirmwareId_Kernel_LLE, IDR_KERNEL},
		{kATFirmwareId_Kernel_LLEXL, IDR_KERNELXL}, {kATFirmwareId_Basic_ATBasic, IDR_BASIC},
		{kATFirmwareId_5200_LLE, IDR_5200KERNEL}, {kATFirmwareId_5200_NoCartridge, IDR_NOCARTRIDGE},
		{kATFirmwareId_U1MB_Recovery, IDR_U1MBBIOS}, {kATFirmwareId_KMKJZIDE_NoBIOS, IDR_NOHDBIOS},
		{kATFirmwareId_850Relocator, IDR_850RELOCATOR}, {kATFirmwareId_850Handler, IDR_850HANDLER},
		{kATFirmwareId_1030Firmware, IDR_1030HANDLER}, {kATFirmwareId_NoMIO, IDR_NOMIO},
		{kATFirmwareId_NoBlackBox, IDR_NOBLACKBOX}, {kATFirmwareId_NoGame, IDR_NOGAME},
		{kATFirmwareId_RapidusFlash, IDR_RAPIDUSFLASH}, {kATFirmwareId_RapidusPBI16, IDR_RAPIDUSPBI16},
		{kATFirmwareId_Kernel_816, IDR_KERNEL816}
	}};
	for (const auto [id, resource] : resources) {
		vdfastvector<uint8> expected, actual;
		const bool packed = resource == IDR_U1MBBIOS || resource == IDR_NOMIO || resource == IDR_NOBLACKBOX || resource == IDR_RAPIDUSFLASH;
		AT_PORTABLE_TEST_ASSERT(context, packed ? ATLoadKernelResourceLZPacked(resource, expected) : ATLoadKernelResource(resource, expected));
		uint32 length = 999; bool changed = false, usable = false;
		AT_PORTABLE_TEST_ASSERT(context, ATLoadInternalFirmware(id, nullptr, 0, 0, &changed, &length, &actual, &usable));
		AT_PORTABLE_TEST_ASSERT(context, actual == expected && length == expected.size() && changed);
		changed = false;
		AT_PORTABLE_TEST_ASSERT(context, ATLoadInternalFirmware(id, nullptr, 0, 0, &changed, &length, &actual));
		AT_PORTABLE_TEST_ASSERT(context, !changed);
		std::array<uint8, 19> part {}; changed = false;
		const uint32 offset = static_cast<uint32>(expected.size()) - 7;
		AT_PORTABLE_TEST_ASSERT(context, ATLoadInternalFirmware(id, part.data(), offset, part.size(), &changed, &length));
		AT_PORTABLE_TEST_ASSERT(context, length == 7 && !memcmp(part.data(), expected.data() + offset, 7));
		AT_PORTABLE_TEST_ASSERT(context, !ATLoadInternalFirmware(id, part.data(), static_cast<uint32>(expected.size()) + 1, part.size()));
		AT_PORTABLE_TEST_ASSERT(context, usable == (id != kATFirmwareId_NoKernel && id != kATFirmwareId_5200_NoCartridge && id != kATFirmwareId_NoMIO && id != kATFirmwareId_NoBlackBox && id != kATFirmwareId_NoGame));
	}
	vdfastvector<uint8> xl, charset;
	AT_PORTABLE_TEST_ASSERT(context, ATLoadInternalFirmware(kATFirmwareId_Kernel_LLEXL, nullptr, 0, 0, nullptr, nullptr, &xl));
	AT_PORTABLE_TEST_ASSERT(context, ATLoadInternalFirmware(kATFirmwareId_1090Charset, nullptr, 0, 0, nullptr, nullptr, &charset));
	AT_PORTABLE_TEST_ASSERT(context, charset.size() == 2048 && !memcmp(charset.data(), xl.data() + 0x2000, 1024) && !memcmp(charset.data() + 1024, xl.data() + 0xC00, 1024));
	for (uint64 id : {uint64(0), uint64(kATFirmwareId_Kernel_HLE), uint64(kATFirmwareId_1090Firmware), uint64(kATFirmwareId_PredefCount1), UINT64_MAX}) {
		uint32 length = 999; bool usable = true;
		AT_PORTABLE_TEST_ASSERT(context, !ATLoadInternalFirmware(id, nullptr, 0, 0, nullptr, &length, &charset, &usable));
		AT_PORTABLE_TEST_ASSERT(context, length == 0 && !usable);
	}
	return true;
}

bool ATTestAltirraFirmwareCatalog(ATPortableTestContext& context) {
	RegistryScope registry; ATFirmwareManager manager;
	vdvector<ATFirmwareInfo> catalog; manager.GetFirmwareList(catalog);
	AT_PORTABLE_TEST_ASSERT(context, catalog.size() == kATFirmwareId_PredefCount - 1);
	uint64 previous = 0;
	for (const auto& info : catalog) {
		AT_PORTABLE_TEST_ASSERT(context, info.mId > previous && info.mId != kATFirmwareId_Kernel_HLE && info.mType != kATFirmwareType_Unknown);
		previous = info.mId;
		AT_PORTABLE_TEST_ASSERT(context, manager.GetFirmwareByRefString(manager.GetFirmwareRefString(info.mId).c_str()) == info.mId);
	}
	AT_PORTABLE_TEST_ASSERT(context, manager.GetCompatibleFirmware(kATFirmwareType_Kernel800_OSA) == kATFirmwareId_Kernel_LLE);
	AT_PORTABLE_TEST_ASSERT(context, manager.GetCompatibleFirmware(kATFirmwareType_KernelXEGS) == kATFirmwareId_Kernel_LLEXL);
	FirmwareFile file; const uint64 id = file.Add(manager);
	ATFirmwareInfo info {};
	AT_PORTABLE_TEST_ASSERT(context, manager.GetFirmwareInfo(id, info) && info.mFlags == kATFirmwareFlags_InvertOPTION && info.mType == kATFirmwareType_Basic);
	AT_PORTABLE_TEST_ASSERT(context, manager.GetFirmwareOfType(kATFirmwareType_Basic, false) == id);
	AT_PORTABLE_TEST_ASSERT(context, manager.GetFirmwareByRefString(L"Synthetic BASIC") == id);
	AT_PORTABLE_TEST_ASSERT(context, manager.GetFirmwareByRefString(file.mPath.c_str()) == id);
	manager.SetDefaultFirmware(kATFirmwareType_Basic, id);
	manager.SetSpecificFirmware(kATSpecificFirmwareType_BASICRevC, id);
	{
		ATFirmwareManager reloaded;
		AT_PORTABLE_TEST_ASSERT(context, reloaded.GetDefaultFirmware(kATFirmwareType_Basic) == id && reloaded.GetSpecificFirmware(kATSpecificFirmwareType_BASICRevC) == id);
	}
	for (const wchar_t *bad : {L"internal:", L"internal:xyz", L"internal:00000003junk", L"internal:4", L"internal:10000"})
		AT_PORTABLE_TEST_ASSERT(context, manager.GetFirmwareByRefString(bad) == 0);
	AT_PORTABLE_TEST_ASSERT(context, manager.GetFirmwareByRefString(L"internal:3", [](ATFirmwareType t) { return t == kATFirmwareType_Basic; }) == 0);
	VDStringW checksum;
	checksum.sprintf(L"[%08X]", VDCRCTable::CRC32.CRC(file.mData.data(), file.mData.size()));
	AT_PORTABLE_TEST_ASSERT(context, manager.GetFirmwareByRefString(checksum.c_str(), [](ATFirmwareType t) { return t == kATFirmwareType_Basic; }) == id);
	manager.RemoveFirmware(id);
	AT_PORTABLE_TEST_ASSERT(context, !manager.GetFirmwareInfo(id, info));
	return true;
}

bool ATTestAltirraFirmwareFiles(ATPortableTestContext& context) {
	RegistryScope registry; ATFirmwareManager manager; FirmwareFile file;
	const uint64 id = file.Add(manager);
	std::array<uint8,16> data {}; uint32 length = 999; bool changed = false, usable = false;
	AT_PORTABLE_TEST_ASSERT(context, manager.LoadFirmware(id, data.data(), 5, data.size(), &changed, &length, nullptr, nullptr, &usable));
	AT_PORTABLE_TEST_ASSERT(context, changed && usable && length == 16 && !memcmp(data.data(), file.mData.data() + 5, 16));
	changed = false;
	AT_PORTABLE_TEST_ASSERT(context, manager.LoadFirmware(id, data.data(), 5, data.size(), &changed));
	AT_PORTABLE_TEST_ASSERT(context, !changed);
	changed = true;
	AT_PORTABLE_TEST_ASSERT(context, manager.LoadFirmware(id, data.data(), 5, data.size(), &changed));
	AT_PORTABLE_TEST_ASSERT(context, changed); // shared caller can accumulate changes across ROM loads
	const uint8 fill = 0x5A; changed = false;
	AT_PORTABLE_TEST_ASSERT(context, manager.LoadFirmware(id, data.data(), 8190, data.size(), &changed, &length, nullptr, &fill));
	AT_PORTABLE_TEST_ASSERT(context, length == 2 && data[0] == file.mData[8190] && data[1] == file.mData[8191] && std::all_of(data.begin() + 2, data.end(), [fill](uint8 c) { return c == fill; }));
	vdfastvector<uint8> suffix;
	AT_PORTABLE_TEST_ASSERT(context, manager.LoadFirmware(id, nullptr, 8189, 0, nullptr, &length, &suffix));
	AT_PORTABLE_TEST_ASSERT(context, length == 3 && suffix.size() == 3 && !memcmp(suffix.data(), file.mData.data() + 8189, 3));
	changed = false;
	AT_PORTABLE_TEST_ASSERT(context, manager.LoadFirmware(id, nullptr, 8189, 0, &changed, &length, &suffix) && !changed);
	FirmwareFile empty(0);
	const uint64 emptyId = empty.Add(manager, L"Empty");
	AT_PORTABLE_TEST_ASSERT(context, manager.LoadFirmware(emptyId, nullptr, 0, 0, &changed, &length, &suffix, nullptr, &usable));
	AT_PORTABLE_TEST_ASSERT(context, changed && suffix.empty() && length == 0 && !usable);
	FirmwareFile blank(16, 0xFF); const uint64 blankId = blank.Add(manager, L"Blank");
	AT_PORTABLE_TEST_ASSERT(context, manager.LoadFirmware(blankId, data.data(), 0, data.size(), nullptr, &length, nullptr, nullptr, &usable) && !usable);
	AT_PORTABLE_TEST_ASSERT(context, !manager.LoadFirmware(UINT64_MAX, data.data(), 0, data.size(), nullptr, &length) && length == 0);
	AT_PORTABLE_TEST_ASSERT(context, manager.LoadFirmware(kATFirmwareId_Basic_ATBasic, data.data(), 8190, data.size(), nullptr, &length, nullptr, &fill));
	AT_PORTABLE_TEST_ASSERT(context, length == 2 && std::all_of(data.begin() + 2, data.end(), [fill](uint8 c) { return c == fill; }));
	return true;
}

bool ATTestAltirraFirmwareVFS(ATPortableTestContext& context) {
	RegistryScope registry;
	vdrefptr<ATVFSFileView> retained;
	auto first = std::make_unique<ATFirmwareManager>();
	FirmwareFile file; const uint64 id = file.Add(*first);
	first->SetDefaultFirmware(kATFirmwareType_Basic, id);
	first->SetSpecificFirmware(kATSpecificFirmwareType_BASICRevA, id);
	{
		auto second = std::make_unique<ATFirmwareManager>();
		first.reset(); // must not unregister the later manager's callback
		ATVFSOpenFileView(L"special://basic.bin", false, ~retained);
		AT_PORTABLE_TEST_ASSERT(context, retained->IsSourceReadOnly() && ReadView(*retained) == file.mData);
		vdrefptr<ATVFSFileView> cart;
		ATVFSOpenFileView(L"special://ataribas-a.car", false, ~cart);
		const auto bytes = ReadView(*cart);
		AT_PORTABLE_TEST_ASSERT(context, bytes.size() == 8208 && !memcmp(bytes.data(), "CART", 4) && VDReadUnalignedBEU32(bytes.data() + 4) == 1);
		uint32 sum = 0; for (uint8 c : file.mData) sum += c;
		AT_PORTABLE_TEST_ASSERT(context, VDReadUnalignedBEU32(bytes.data() + 8) == sum && !memcmp(bytes.data() + 16, file.mData.data(), 8192));
		FirmwareFile alternate(8192, 0x6B);
		{
			ATFirmwareManager third;
			const uint64 alternateId = alternate.Add(third, L"Alternate");
			third.SetSpecificFirmware(kATSpecificFirmwareType_BASICRevA, alternateId);
			vdrefptr<ATVFSFileView> view;
			ATVFSOpenFileView(L"special://ataribas-a.bin", false, ~view);
			AT_PORTABLE_TEST_ASSERT(context, ReadView(*view) == alternate.mData);
		}
		// The previous live manager retains its cached selection. Restoring its
		// callback must not keep using the third manager or read its cached ID.
		vdrefptr<ATVFSFileView> restored;
		ATVFSOpenFileView(L"special://ataribas-a.bin", false, ~restored);
		AT_PORTABLE_TEST_ASSERT(context, ReadView(*restored) == file.mData);
		bool rejected = false;
		try { ATVFSOpenFileView(L"special://basic.bin", true, ~cart); } catch (const ATVFSReadOnlyException&) { rejected = true; }
		AT_PORTABLE_TEST_ASSERT(context, rejected);
	}
	retained->GetStream().Seek(0);
	AT_PORTABLE_TEST_ASSERT(context, ReadView(*retained) == file.mData); // views own their data after manager destruction
	// A Windows application may have its own manager registered before tests.
	// Access after these managers die must either use that live handler or fail
	// cleanly. Selecting a cached firmware also detects stale callbacks in ASan.
	try { vdrefptr<ATVFSFileView> view; ATVFSOpenFileView(L"special://ataribas-a.bin", false, ~view); }
	catch (const MyError&) {}
	return true;
}
