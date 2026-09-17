// Altirra core OS helpers for macOS

#include <algorithm>
#include <cstring>
#include <map>
#include <mutex>
#include <vector>

#import <Foundation/Foundation.h>

#include <grp.h>
#include <unistd.h>
#include <uuid/uuid.h>

#include "oshelper.h"
#include "../../../../Altirra/res/resource.h"

namespace {
	struct ATResourceLocation {
		const char *mpBundleDirectory;
		const char *mpFilename;
		const char *mpSourcePath;
	};

	const ATResourceLocation *ATGetKernelResourceLocation(int id) {
		static constexpr ATResourceLocation kKernel { "KERNEL", "kernel.rom", "out/release/kernel.rom" };
		static constexpr ATResourceLocation kKernelXL { "KERNEL", "kernelxl.rom", "out/release/kernelxl.rom" };
		static constexpr ATResourceLocation kKernel816 { "KERNEL", "kernel816.rom", "out/release/kernel816.rom" };
		static constexpr ATResourceLocation kNoKernel { "KERNEL", "nokernel.rom", "out/release/nokernel.rom" };
		static constexpr ATResourceLocation kBasic { "KERNEL", "atbasic.bin", "out/release/atbasic.bin" };
		static constexpr ATResourceLocation k5200Kernel { "KERNEL", "superkernel.rom", "out/Release/superkernel.rom" };
		static constexpr ATResourceLocation kNoCartridge { "KERNEL", "nocartridge.rom", "out/Release/nocartridge.rom" };
		static constexpr ATResourceLocation kNoHDBios { "KERNEL", "nohdbios.rom", "out/Release/nohdbios.rom" };
		static constexpr ATResourceLocation kNoGame { "KERNEL", "nogame.rom", "out/Release/nogame.rom" };
		static constexpr ATResourceLocation kU1MBBios { "KERNEL", "ultimate.lzrom", "out/release/ultimate.lzrom" };
		static constexpr ATResourceLocation k850Relocator { "KERNEL", "850relocator.bin", "out/release/850relocator.bin" };
		static constexpr ATResourceLocation k850Handler { "KERNEL", "850handler.bin", "out/release/850handler.bin" };
		static constexpr ATResourceLocation k1030Firmware { "KERNEL", "1030firmware.bin", "out/release/1030firmware.bin" };
		static constexpr ATResourceLocation kNoMIO { "KERNEL", "nomio.lzrom", "out/release/nomio.lzrom" };
		static constexpr ATResourceLocation kNoBlackBox { "KERNEL", "noblackbox.lzrom", "out/release/noblackbox.lzrom" };
		static constexpr ATResourceLocation kRapidusFlash { "KERNEL", "rapidflash.lzrom", "obj/Release/Kernel/rapidflash.lzrom" };
		static constexpr ATResourceLocation kRapidusPBI16 { "KERNEL", "rapidpbi16.bin", "out/Release/rapidpbi16.bin" };

		switch(id) {
			case IDR_KERNEL: return &kKernel;
			case IDR_KERNELXL: return &kKernelXL;
			case IDR_KERNEL816: return &kKernel816;
			case IDR_NOKERNEL: return &kNoKernel;
			case IDR_BASIC: return &kBasic;
			case IDR_5200KERNEL: return &k5200Kernel;
			case IDR_NOCARTRIDGE: return &kNoCartridge;
			case IDR_NOHDBIOS: return &kNoHDBios;
			case IDR_NOGAME: return &kNoGame;
			case IDR_U1MBBIOS: return &kU1MBBios;
			case IDR_850RELOCATOR: return &k850Relocator;
			case IDR_850HANDLER: return &k850Handler;
			case IDR_1030FIRMWARE: return &k1030Firmware;
			case IDR_NOMIO: return &kNoMIO;
			case IDR_NOBLACKBOX: return &kNoBlackBox;
			case IDR_RAPIDUSFLASH: return &kRapidusFlash;
			case IDR_RAPIDUSPBI16: return &kRapidusPBI16;
			default: return nullptr;
		}
	}

	const ATResourceLocation *ATGetMiscResourceLocation(int id) {
		static constexpr ATResourceLocation kChanges { "STUFF", "changes.txt", "src/Altirra/res/changes.txt" };
		static constexpr ATResourceLocation kDebugHelp { "STUFF", "dbghelp.txt", "src/Altirra/res/dbghelp.txt" };
		static constexpr ATResourceLocation kTrackStep { "STUFF", "TrackStep.pcm", "src/Altirra/res/TrackStep.pcm" };
		static constexpr ATResourceLocation kTrackStep2 { "STUFF", "TrackStep2.pcm", "src/Altirra/res/TrackStep2.pcm" };
		static constexpr ATResourceLocation kTrackStep3 { "STUFF", "TrackStep3.pcm", "src/Altirra/res/TrackStep3.pcm" };
		static constexpr ATResourceLocation kDiskSpin { "STUFF", "DiskSpin.pcm", "src/Altirra/res/DiskSpin.pcm" };
		static constexpr ATResourceLocation kDiskLoader128 { "STUFF", "atdiskloader128.bin", "out/Release/atdiskloader128.bin" };
		static constexpr ATResourceLocation kAbout { "STUFF", "about.txt", "src/Altirra/res/about.txt" };
		static constexpr ATResourceLocation kMenuDefault { "STUFF", "menu_default.txt", "src/Altirra/res/menu_default.txt" };
		static constexpr ATResourceLocation kCompatDB { "STUFF", "compatdb.atcpengine", "src/Altirra/res/compatdb.atcpengine" };
		static constexpr ATResourceLocation kRomSetReadme { "STUFF", "romset.html", "src/Altirra/res/romset.html" };
		static constexpr ATResourceLocation kCommandLineHelp { "STUFF", "cmdhelp.txt", "src/Altirra/res/cmdhelp.txt" };
		static constexpr ATResourceLocation kSpeakerStep { "STUFF", "speaker-click.pcm", "src/Altirra/res/speaker-click.pcm" };
		static constexpr ATResourceLocation kDebugHelpTemplate { "STUFF", "dbghelp-template.html", "src/Altirra/res/dbghelp-template.html" };
		static constexpr ATResourceLocation k1030Relay { "STUFF", "1030relay.pcm", "src/Altirra/res/1030relay.pcm" };
		static constexpr ATResourceLocation kPrinter1029Pin { "STUFF", "printer-1029-pin.pcm", "src/Altirra/res/printer-1029-pin.pcm" };
		static constexpr ATResourceLocation kPrinter1029Platen { "STUFF", "printer-1029-platen.pcm", "src/Altirra/res/printer-1029-platen.pcm" };
		static constexpr ATResourceLocation kPrinter1029Retract { "STUFF", "printer-1029-retract.pcm", "src/Altirra/res/printer-1029-retract.pcm" };
		static constexpr ATResourceLocation kPrinter1029Home { "STUFF", "printer-1029-home.pcm", "src/Altirra/res/printer-1029-home.pcm" };
		static constexpr ATResourceLocation kPrinter1025Feed { "STUFF", "printer-1025-feed.pcm", "src/Altirra/res/printer-1025-feed.pcm" };

		switch(id) {
			case IDR_CHANGES: return &kChanges;
			case IDR_DEBUG_HELP: return &kDebugHelp;
			case IDR_TRACK_STEP: return &kTrackStep;
			case IDR_TRACK_STEP_2: return &kTrackStep2;
			case IDR_TRACK_STEP_3: return &kTrackStep3;
			case IDR_DISK_SPIN: return &kDiskSpin;
			case IDR_DISKLOADER128: return &kDiskLoader128;
			case IDR_ABOUT: return &kAbout;
			case IDR_MENU_DEFAULT: return &kMenuDefault;
			case IDR_COMPATDB: return &kCompatDB;
			case IDR_ROMSETREADME: return &kRomSetReadme;
			case IDR_CMDLINEHELP: return &kCommandLineHelp;
			case IDR_SPEAKER_STEP: return &kSpeakerStep;
			case IDR_DEBUG_HELP_TEMPLATE: return &kDebugHelpTemplate;
			case IDR_1030RELAY: return &k1030Relay;
			case IDR_PRINTER_1029_PIN: return &kPrinter1029Pin;
			case IDR_PRINTER_1029_PLATEN: return &kPrinter1029Platen;
			case IDR_PRINTER_1029_RETRACT: return &kPrinter1029Retract;
			case IDR_PRINTER_1029_HOME: return &kPrinter1029Home;
			case IDR_PRINTER_1025_FEED: return &kPrinter1025Feed;
			default: return nullptr;
		}
	}

	bool ATLoadResource(const ATResourceLocation *location, vdfastvector<uint8>& data) {
		if (!location)
			return false;

		@autoreleasepool {
			NSString *const directory = [NSString stringWithUTF8String:location->mpBundleDirectory];
			NSString *const filename = [NSString stringWithUTF8String:location->mpFilename];
			NSURL *const bundleURL = [[NSBundle mainBundle]
				URLForResource:filename
				withExtension:nil
				subdirectory:directory];
			NSData *resourceData = bundleURL ? [NSData dataWithContentsOfURL:bundleURL] : nil;

			if (!resourceData) {
				NSString *const sourcePath = [NSString stringWithUTF8String:location->mpSourcePath];
				resourceData = [NSData dataWithContentsOfFile:sourcePath];

				if (!resourceData) {
					NSString *const executableDirectory = [[[NSBundle mainBundle] executablePath]
						stringByDeletingLastPathComponent];
					NSString *const repositoryPath = [executableDirectory
						stringByAppendingPathComponent:[@"../.." stringByAppendingPathComponent:sourcePath]];
					resourceData = [NSData dataWithContentsOfFile:repositoryPath];
				}
			}

			if (!resourceData)
				return false;

			const NSUInteger length = resourceData.length;
			if (!length) {
				data.clear();
				return true;
			}

			const uint8 *const bytes = static_cast<const uint8 *>(resourceData.bytes);
			data.assign(bytes, bytes + length);
			return true;
		}
	}
}

const void *ATLockResource(uint32 id, size_t& size) {
	static std::mutex cacheMutex;
	static std::map<uint32, vdfastvector<uint8>> cache;
	const std::lock_guard lock(cacheMutex);

	auto [it, inserted] = cache.try_emplace(id);
	if (inserted && !ATLoadResource(ATGetMiscResourceLocation(id), it->second)) {
		cache.erase(it);
		return nullptr;
	}

	size = it->second.size();
	return it->second.data();
}

bool ATLoadKernelResource(int id, void *dst, uint32 offset, uint32 size, bool allowPartial) {
	vdfastvector<uint8> data;
	if (!ATLoadResource(ATGetKernelResourceLocation(id), data) || offset > data.size())
		return false;

	const size_t available = data.size() - offset;
	if (size > available) {
		if (!allowPartial)
			return false;

		size = static_cast<uint32>(available);
	}

	if (size)
		std::memcpy(dst, data.data() + offset, size);
	return true;
}

bool ATLoadKernelResource(int id, vdfastvector<uint8>& data) {
	return ATLoadResource(ATGetKernelResourceLocation(id), data);
}

bool ATLoadKernelResourceLZPacked(int id, vdfastvector<uint8>& data) {
	vdfastvector<uint8> packedData;
	if (!ATLoadResource(ATGetKernelResourceLocation(id), packedData))
		return false;

	return ATDecodeLZPackedResource(packedData.data(), packedData.size(), data);
}

bool ATLoadMiscResource(int id, vdfastvector<uint8>& data) {
	return ATLoadResource(ATGetMiscResourceLocation(id), data);
}

bool ATIsUserAdministrator() {
	const group *adminGroup = getgrnam("admin");
	if (!adminGroup)
		return false;

	const gid_t adminGroupId = adminGroup->gr_gid;
	if (getgid() == adminGroupId || getegid() == adminGroupId)
		return true;

	const int groupCount = getgroups(0, nullptr);
	if (groupCount <= 0)
		return false;

	std::vector<gid_t> groups(static_cast<size_t>(groupCount));
	const int groupsRead = getgroups(groupCount, groups.data());
	if (groupsRead <= 0)
		return false;

	return std::find(groups.begin(), groups.begin() + groupsRead, adminGroupId) != groups.begin() + groupsRead;
}

void ATGenerateGuid(uint8 guid[16]) {
	uuid_generate_random(guid);
}
