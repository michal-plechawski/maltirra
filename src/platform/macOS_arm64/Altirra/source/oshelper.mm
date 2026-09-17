// Altirra core OS helpers for macOS

#include <algorithm>
#include <cstring>
#include <map>
#include <mutex>
#include <vector>

#import <AppKit/AppKit.h>
#import <ImageIO/ImageIO.h>
#import <objc/runtime.h>

#include <grp.h>
#include <unistd.h>
#include <uuid/uuid.h>

#include "oshelper.h"
#include <vd2/system/error.h>
#include <vd2/system/filesys.h>
#include <vd2/system/text.h>
#include <vd2/system/vdstring.h>
#include <vd2/Kasumi/pixmap.h>
#include <vd2/Kasumi/pixmaputils.h>
#include "../../../../Altirra/res/resource.h"

@interface ATPathCompletionDelegate : NSObject <NSTextFieldDelegate> {
	id<NSTextFieldDelegate> _forwardDelegate;
}

- (instancetype)initWithForwardDelegate:(id<NSTextFieldDelegate>)delegate;

@end

@implementation ATPathCompletionDelegate

- (instancetype)initWithForwardDelegate:(id<NSTextFieldDelegate>)delegate {
	self = [super init];
	if (self)
		_forwardDelegate = delegate;
	return self;
}

- (NSArray<NSString *> *)control:(NSControl *)control
	textView:(NSTextView *)textView
	completions:(NSArray<NSString *> *)words
	forPartialWordRange:(NSRange)charRange
	indexOfSelectedItem:(NSInteger *)index {
	(void)control;
	(void)words;

	NSString *const text = textView.string ?: @"";
	if (NSMaxRange(charRange) > text.length)
		return @[];

	NSString *const typedPath = [text substringWithRange:charRange];
	NSString *const expandedPath = typedPath.stringByExpandingTildeInPath;
	NSString *directory = expandedPath.stringByDeletingLastPathComponent;
	if (!directory.length)
		directory = NSFileManager.defaultManager.currentDirectoryPath;

	NSString *const fragment = expandedPath.lastPathComponent;
	NSArray<NSString *> *const names = [NSFileManager.defaultManager
		contentsOfDirectoryAtPath:directory
		error:nil];
	if (!names)
		return @[];

	NSMutableArray<NSString *> *const matches = [NSMutableArray array];
	for(NSString *name in names) {
		if (![name hasPrefix:fragment])
			continue;

		NSString *candidate;
		if (typedPath.isAbsolutePath || [typedPath hasPrefix:@"~"]) {
			candidate = [directory stringByAppendingPathComponent:name];
			if ([typedPath hasPrefix:@"~"])
				candidate = candidate.stringByAbbreviatingWithTildeInPath;
		} else {
			NSString *const typedDirectory = typedPath.stringByDeletingLastPathComponent;
			candidate = typedDirectory.length
				? [typedDirectory stringByAppendingPathComponent:name]
				: name;
		}

		BOOL isDirectory = NO;
		[NSFileManager.defaultManager
			fileExistsAtPath:[directory stringByAppendingPathComponent:name]
			isDirectory:&isDirectory];
		if (isDirectory)
			candidate = [candidate stringByAppendingString:@"/"];

		[matches addObject:candidate];
	}

	[matches sortUsingSelector:@selector(localizedStandardCompare:)];
	if (index && matches.count)
		*index = 0;
	return matches;
}

- (BOOL)respondsToSelector:(SEL)selector {
	return [super respondsToSelector:selector]
		|| [_forwardDelegate respondsToSelector:selector];
}

- (id)forwardingTargetForSelector:(SEL)selector {
	if ([_forwardDelegate respondsToSelector:selector])
		return _forwardDelegate;
	return [super forwardingTargetForSelector:selector];
}

@end

namespace {
	char g_ATPathCompletionDelegateKey;
	id g_ATProcessActivityToken;

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

	const ATResourceLocation *ATGetImageResourceLocation(int id) {
		static constexpr ATResourceLocation kTraceViewerToolbar { "PNG", "traceViewerToolbar.png", "src/Altirra/res/traceViewerToolbar.png" };
		static constexpr ATResourceLocation kProfilerToolbar { "PNG", "profilerToolbar.png", "src/Altirra/res/profilerToolbar.png" };
		static constexpr ATResourceLocation kWarning { "PNG", "warning.png", "src/Altirra/res/warning.png" };
		static constexpr ATResourceLocation kFirmwareIcons { "PNG", "firmware.png", "src/Altirra/res/firmware.png" };

		switch(id) {
			case IDB_TOOLBAR_TRACEVIEWER: return &kTraceViewerToolbar;
			case IDB_TOOLBAR_PROFILER2: return &kProfilerToolbar;
			case IDB_WARNING: return &kWarning;
			case IDB_FIRMWARE_ICONS: return &kFirmwareIcons;
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

bool ATLoadImageResource(uint32 id, VDPixmapBuffer& image) {
	vdfastvector<uint8> data;
	if (!ATLoadResource(ATGetImageResourceLocation(id), data))
		return false;

	try {
		ATLoadFrameFromMemory(image, data.data(), data.size());
		return true;
	} catch(const MyError&) {
		return false;
	}
}

void ATLoadFrameFromMemory(VDPixmapBuffer& px, const void *mem, size_t len) {
	if (!mem || !len)
		throw MyError("Unable to decode image.");

	@autoreleasepool {
		NSData *const data = [NSData dataWithBytesNoCopy:const_cast<void *>(mem)
			length:len
			freeWhenDone:NO];
		CGImageSourceRef source = CGImageSourceCreateWithData((__bridge CFDataRef)data, nullptr);
		if (!source)
			throw MyError("Unable to decode image.");

		CGImageRef image = CGImageSourceCreateImageAtIndex(source, 0, nullptr);
		CFRelease(source);
		if (!image)
			throw MyError("Unable to decode image.");

		const size_t width = CGImageGetWidth(image);
		const size_t height = CGImageGetHeight(image);
		if (!width || !height || width > INT32_MAX || height > INT32_MAX) {
			CGImageRelease(image);
			throw MyError("Invalid image dimensions.");
		}

		try {
			px.init(static_cast<sint32>(width), static_cast<sint32>(height), nsVDPixmap::kPixFormat_XRGB8888);
		} catch(...) {
			CGImageRelease(image);
			throw;
		}

		CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
		if (!colorSpace) {
			CGImageRelease(image);
			throw MyError("Unable to create image color space.");
		}

		CGContextRef context = CGBitmapContextCreate(
			px.data,
			width,
			height,
			8,
			static_cast<size_t>(px.pitch),
			colorSpace,
			static_cast<CGBitmapInfo>(
				static_cast<uint32>(kCGBitmapByteOrder32Little)
				| static_cast<uint32>(kCGImageAlphaNoneSkipFirst)));
		CGColorSpaceRelease(colorSpace);

		if (!context) {
			CGImageRelease(image);
			throw MyError("Unable to create image buffer.");
		}

		CGContextDrawImage(context, CGRectMake(0, 0, width, height), image);
		CGContextRelease(context);
		CGImageRelease(image);
	}
}

void ATCopyFrameToClipboard(const VDPixmap& px) {
	vdfastvector<uint8> pngData;
	ATEncodeFrameAsPNG(px, pngData);

	@autoreleasepool {
		NSData *const data = [NSData dataWithBytes:pngData.data() length:pngData.size()];
		NSPasteboard *const pasteboard = [NSPasteboard generalPasteboard];
		[pasteboard clearContents];
		[pasteboard setData:data forType:NSPasteboardTypePNG];
	}
}

void ATCopyTextToClipboard(void *hwnd, const char *s) {
	(void)hwnd;
	if (!s)
		return;

	@autoreleasepool {
		NSString *text = [[[NSString alloc]
			initWithBytes:s
			length:strlen(s)
			encoding:NSUTF8StringEncoding] autorelease];
		if (!text)
			text = [NSString stringWithCString:s encoding:NSISOLatin1StringEncoding];

		if (text) {
			NSPasteboard *const pasteboard = [NSPasteboard generalPasteboard];
			[pasteboard clearContents];
			[pasteboard setString:text forType:NSPasteboardTypeString];
		}
	}
}

void ATCopyTextToClipboard(void *hwnd, const wchar_t *s) {
	if (!s)
		return;

	const VDStringA text = VDTextWToU8(s, -1);
	ATCopyTextToClipboard(hwnd, text.c_str());
}

void ATUISaveWindowPlacement(void *hwnd, const char *name) {
	NSWindow *const window = static_cast<NSWindow *>(hwnd);
	if (!window)
		return;

	const NSRect frame = window.frame;
	const uint32 dpi = static_cast<uint32>(window.backingScaleFactor * 96.0 + 0.5);
	ATUISaveWindowPlacement(
		name,
		vdrect32 {
			static_cast<sint32>(frame.origin.x),
			static_cast<sint32>(frame.origin.y),
			static_cast<sint32>(NSMaxX(frame)),
			static_cast<sint32>(NSMaxY(frame)),
		},
		window.zoomed,
		dpi);
}

void ATUIRestoreWindowPlacement(void *hwnd, const char *name, int nCmdShow, bool sizeOnly) {
	(void)nCmdShow;
	NSWindow *const window = static_cast<NSWindow *>(hwnd);
	if (!window || window.zoomed || window.miniaturized)
		return;

	vdrect32 savedRect {};
	bool wasMaximized = false;
	uint32 savedDpi = 0;
	if (!ATUILoadWindowPlacement(name, savedRect, wasMaximized, savedDpi))
		return;

	NSRect frame = window.frame;
	double width = savedRect.width();
	double height = savedRect.height();
	const uint32 currentDpi = static_cast<uint32>(window.backingScaleFactor * 96.0 + 0.5);
	if (savedDpi && currentDpi) {
		const double scale = static_cast<double>(currentDpi) / savedDpi;
		width *= scale;
		height *= scale;
	}

	if (!sizeOnly) {
		frame.origin.x = savedRect.left;
		frame.origin.y = savedRect.top;
	}
	frame.size.width = width;
	frame.size.height = height;
	[window setFrame:frame display:NO];

	if (wasMaximized && !window.zoomed)
		[window zoom:nil];
}

void ATUIEnableEditControlAutoComplete(void *hwnd) {
	if (!hwnd)
		return;

	NSTextField *const field = static_cast<NSTextField *>(hwnd);
	if (![field isKindOfClass:[NSTextField class]])
		return;

	if (objc_getAssociatedObject(field, &g_ATPathCompletionDelegateKey))
		return;

	ATPathCompletionDelegate *const delegate = [[ATPathCompletionDelegate alloc]
		initWithForwardDelegate:field.delegate];
	objc_setAssociatedObject(
		field,
		&g_ATPathCompletionDelegateKey,
		delegate,
		OBJC_ASSOCIATION_RETAIN_NONATOMIC);
	field.delegate = delegate;
	[delegate release];
}

VDStringW ATGetHelpPath() {
	@autoreleasepool {
		NSURL *const bundleURL = [[NSBundle mainBundle]
			URLForResource:@"contents"
			withExtension:@"html"
			subdirectory:@"Help"];
		if (bundleURL) {
			const char *const path = bundleURL.path.fileSystemRepresentation;
			if (path)
				return VDTextU8ToW(path, -1);
		}
	}

	return VDMakePath(VDGetProgramPath().c_str(), L"Help/contents.html");
}

void ATShowHelp(void *hwnd, const wchar_t *filename) {
	try {
		const VDStringW target = ATResolveWebHelpPath(ATGetHelpPath().c_str(), filename);
		const wchar_t *const anchor = wcschr(target.c_str(), L'#');
		VDStringW filePath;
		if (anchor)
			filePath.assign(target.c_str(), anchor);
		else
			filePath = target;

		if (!VDDoesPathExist(filePath.c_str()))
			throw VDException(L"Cannot find help topic: %ls", filePath.c_str());

		@autoreleasepool {
			const VDStringA pathUTF8 = VDTextWToU8(filePath.c_str(), -1);
			NSString *const path = [NSString stringWithUTF8String:pathUTF8.c_str()];
			NSURL *targetURL = path ? [NSURL fileURLWithPath:path] : nil;

			if (targetURL && anchor && anchor[1]) {
				const VDStringA fragmentUTF8 = VDTextWToU8(anchor + 1, -1);
				NSURLComponents *const components = [NSURLComponents
					componentsWithURL:targetURL
					resolvingAgainstBaseURL:NO];
				components.fragment = [NSString stringWithUTF8String:fragmentUTF8.c_str()];
				targetURL = components.URL;
			}

			if (!targetURL || ![[NSWorkspace sharedWorkspace] openURL:targetURL])
				throw MyError("Unable to open Altirra help.");
		}
	} catch(const MyError& error) {
		error.post(reinterpret_cast<VDExceptionPostContext>(hwnd), "Altirra Error");
	}
}

void ATLaunchURL(const wchar_t *url) {
	if (!url)
		return;

	@autoreleasepool {
		const VDStringA urlUTF8 = VDTextWToU8(url, -1);
		NSString *const urlString = [NSString stringWithUTF8String:urlUTF8.c_str()];
		NSURL *const targetURL = urlString ? [NSURL URLWithString:urlString] : nil;
		if (targetURL)
			[[NSWorkspace sharedWorkspace] openURL:targetURL];
	}
}

void ATLaunchFileForEdit(const wchar_t *file) {
	if (!file)
		return;

	@autoreleasepool {
		const VDStringA pathUTF8 = VDTextWToU8(file, -1);
		NSString *const path = [NSString stringWithUTF8String:pathUTF8.c_str()];
		if (path)
			[[NSWorkspace sharedWorkspace] openURL:[NSURL fileURLWithPath:path]];
	}
}

void ATShowFileInSystemExplorer(const wchar_t *filename) {
	if (!filename)
		return;

	@autoreleasepool {
		const VDStringW fullPath = VDGetFullPath(filename);
		const VDStringA pathUTF8 = VDTextWToU8(fullPath.c_str(), -1);
		NSString *const path = [NSString stringWithUTF8String:pathUTF8.c_str()];
		if (path) {
			NSURL *const fileURL = [NSURL fileURLWithPath:path];
			[[NSWorkspace sharedWorkspace] activateFileViewerSelectingURLs:@[fileURL]];
		}
	}
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

void ATSetProcessEfficiencyMode(ATProcessEfficiencyMode mode) {
	@autoreleasepool {
		NSProcessInfo *const processInfo = NSProcessInfo.processInfo;
		@synchronized(processInfo) {
			if (g_ATProcessActivityToken) {
				[processInfo endActivity:g_ATProcessActivityToken];
				[g_ATProcessActivityToken release];
				g_ATProcessActivityToken = nil;
			}

			NSActivityOptions options = 0;
			NSString *reason = nil;
			switch(mode) {
				case ATProcessEfficiencyMode::Default:
					break;

				case ATProcessEfficiencyMode::Performance:
					options = NSActivityUserInitiated | NSActivityLatencyCritical;
					reason = @"Altirra performance mode";
					break;

				case ATProcessEfficiencyMode::Efficiency:
					options = NSActivityBackground;
					reason = @"Altirra efficiency mode";
					break;
			}

			if (options)
				g_ATProcessActivityToken = [[processInfo
					beginActivityWithOptions:options
					reason:reason] retain];
		}
	}
}
