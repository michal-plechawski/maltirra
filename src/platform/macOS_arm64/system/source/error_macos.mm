// Altirra native macOS exception presentation

#import <AppKit/AppKit.h>
#include <dispatch/dispatch.h>
#include <wchar.h>

#include <vd2/system/Error.h>

namespace {
	NSString *VDNSStringFromNarrow(const char *text) {
		if (!text)
			return @"";

		NSString *value = [NSString stringWithUTF8String:text];
		if (!value)
			value = [NSString stringWithCString:text encoding:NSISOLatin1StringEncoding];

		return value ?: @"";
	}

	NSString *VDNSStringFromWide(const wchar_t *text) {
		if (!text)
			return @"";

		NSMutableString *value = [NSMutableString string];

		while(*text) {
			const uint32 codePoint = (uint32)*text++;

			if (codePoint <= 0xFFFF && !(codePoint >= 0xD800 && codePoint <= 0xDFFF)) {
				const unichar character = (unichar)codePoint;
				[value appendString:[NSString stringWithCharacters:&character length:1]];
			} else if (codePoint <= 0x10FFFF) {
				const uint32 adjusted = codePoint - 0x10000;
				const unichar characters[] = {
					(unichar)(0xD800 + (adjusted >> 10)),
					(unichar)(0xDC00 + (adjusted & 0x3FF)),
				};
				[value appendString:[NSString stringWithCharacters:characters length:2]];
			} else {
				[value appendString:@"\uFFFD"];
			}
		}

		return value;
	}

	void VDShowNativeError(VDExceptionPostContext context, NSString *message, NSString *title) {
		void (^showAlert)(void) = ^{
			@autoreleasepool {
				[NSApplication sharedApplication];

				NSAlert *alert = [[[NSAlert alloc] init] autorelease];
				alert.alertStyle = NSAlertStyleCritical;
				alert.messageText = title.length ? title : @"Altirra";
				alert.informativeText = message ?: @"";

				NSWindow *parentWindow = (NSWindow *)(void *)context;
				if (parentWindow)
					[alert beginSheetModalForWindow:parentWindow completionHandler:nil];
				else
					[alert runModal];
			}
		};

		if ([NSThread isMainThread])
			showAlert();
		else
			dispatch_sync(dispatch_get_main_queue(), showAlert);
	}
}

void VDPostException(VDExceptionPostContext context, const char *message, const char *title) {
	@autoreleasepool {
		VDShowNativeError(context, VDNSStringFromNarrow(message), VDNSStringFromNarrow(title));
	}
}

void VDPostException(VDExceptionPostContext context, const wchar_t *message, const wchar_t *title) {
	@autoreleasepool {
		VDShowNativeError(context, VDNSStringFromWide(message), VDNSStringFromWide(title));
	}
}
