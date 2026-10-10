// Altirra - Atari 800/800XL/5200 emulator
// Copyright (C) 2026 Avery Lee

#include <cstring>
#include <limits>
#include <mutex>
#include <new>

#import <AppKit/AppKit.h>
#import <CoreGraphics/CoreGraphics.h>
#import <dispatch/dispatch.h>

#include <vd2/VDDisplay/displayview_macos.h>
#include <vd2/VDDisplay/internal/presentationbuffer.h>

CGImageRef VDCreateDisplayImageMac(const VDPixmap& pixmap) {
	using namespace nsVDPixmap;

	if (!pixmap.data || pixmap.w <= 0 || pixmap.h <= 0
		|| pixmap.format != kPixFormat_XRGB8888 || pixmap.pitch <= 0)
		return nullptr;

	const size_t width = (size_t)pixmap.w;
	const size_t height = (size_t)pixmap.h;
	if (width > std::numeric_limits<size_t>::max() / sizeof(uint32))
		return nullptr;

	const size_t rowBytes = width * sizeof(uint32);
	const size_t sourcePitch = (size_t)pixmap.pitch;
	if (sourcePitch < rowBytes
		|| height > std::numeric_limits<size_t>::max() / rowBytes)
		return nullptr;
	if (height > 1
		&& sourcePitch > (std::numeric_limits<size_t>::max() - rowBytes) / (height - 1))
		return nullptr;

	const size_t dataSize = rowBytes * height;
	if (dataSize > (size_t)std::numeric_limits<CFIndex>::max())
		return nullptr;

	CFMutableDataRef data = CFDataCreateMutable(kCFAllocatorDefault, (CFIndex)dataSize);
	if (!data)
		return nullptr;

	CFDataSetLength(data, (CFIndex)dataSize);
	UInt8 *destination = CFDataGetMutableBytePtr(data);
	if (!destination) {
		CFRelease(data);
		return nullptr;
	}

	const uint8 *source = static_cast<const uint8 *>(pixmap.data);
	for(size_t y = 0; y < height; ++y)
		std::memcpy(destination + y * rowBytes, source + y * sourcePitch, rowBytes);

	CGDataProviderRef provider = CGDataProviderCreateWithCFData(data);
	CFRelease(data);
	if (!provider)
		return nullptr;

	CGColorSpaceRef colorSpace = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
	if (!colorSpace)
		colorSpace = CGColorSpaceCreateDeviceRGB();

	CGImageRef image = nullptr;
	if (colorSpace) {
		const CGBitmapInfo bitmapInfo = (CGBitmapInfo)(
			(uint32)kCGBitmapByteOrder32Little | (uint32)kCGImageAlphaNoneSkipFirst);
		image = CGImageCreate(
			width,
			height,
			8,
			32,
			rowBytes,
			colorSpace,
			bitmapInfo,
			provider,
			nullptr,
			false,
			kCGRenderingIntentDefault);
		CGColorSpaceRelease(colorSpace);
	}

	CGDataProviderRelease(provider);
	return image;
}

@interface VDMacVideoDisplayView : NSView {
@private
	std::mutex *_mutex;
	VDDisplayPresentationBuffer *_presentationBuffer;
	CGImageRef _image;
	bool _redisplayPending;
}

- (bool)setSource:(const VDPixmap&)source allowConversion:(bool)allowConversion;
- (void)clearFrame;
- (void)requestRedisplay;

@end

@implementation VDMacVideoDisplayView

- (id)initWithFrame:(NSRect)frameRect {
	self = [super initWithFrame:frameRect];
	if (self) {
		_mutex = new(std::nothrow) std::mutex;
		_presentationBuffer = new(std::nothrow) VDDisplayPresentationBuffer;
		if (!_mutex || !_presentationBuffer) {
			[self release];
			return nil;
		}
	}

	return self;
}

- (void)dealloc {
	if (_image)
		CGImageRelease(_image);

	delete _presentationBuffer;
	delete _mutex;
	[super dealloc];
}

- (BOOL)isOpaque {
	return YES;
}

- (BOOL)isFlipped {
	return YES;
}

- (bool)setSource:(const VDPixmap&)source allowConversion:(bool)allowConversion {
	CGImageRef oldImage = nullptr;
	{
		std::lock_guard<std::mutex> lock(*_mutex);
		if (!_presentationBuffer->Update(source, allowConversion))
			return false;

		CGImageRef newImage = VDCreateDisplayImageMac(_presentationBuffer->GetPixmap());
		if (!newImage)
			return false;

		oldImage = _image;
		_image = newImage;
	}

	if (oldImage)
		CGImageRelease(oldImage);
	[self requestRedisplay];
	return true;
}

- (void)clearFrame {
	CGImageRef oldImage = nullptr;
	{
		std::lock_guard<std::mutex> lock(*_mutex);
		_presentationBuffer->Clear();
		oldImage = _image;
		_image = nullptr;
	}

	if (oldImage)
		CGImageRelease(oldImage);
	[self requestRedisplay];
}

- (void)requestRedisplay {
	bool scheduleRedisplay = false;
	{
		std::lock_guard<std::mutex> lock(*_mutex);
		if (!_redisplayPending) {
			_redisplayPending = true;
			scheduleRedisplay = true;
		}
	}

	if (!scheduleRedisplay)
		return;

	void (^invalidateView)(void) = ^{
		{
			std::lock_guard<std::mutex> lock(*_mutex);
			_redisplayPending = false;
		}
		[self setNeedsDisplay:YES];
	};

	if ([NSThread isMainThread])
		invalidateView();
	else
		dispatch_async(dispatch_get_main_queue(), invalidateView);
}

- (void)drawRect:(NSRect)dirtyRect {
	(void)dirtyRect;

	CGContextRef context = [[NSGraphicsContext currentContext] CGContext];
	if (!context)
		return;

	const CGRect bounds = NSRectToCGRect([self bounds]);
	CGContextSetRGBFillColor(context, 0, 0, 0, 1);
	CGContextFillRect(context, bounds);

	CGImageRef image = nullptr;
	{
		std::lock_guard<std::mutex> lock(*_mutex);
		if (_image)
			image = CGImageRetain(_image);
	}

	if (!image)
		return;

	CGContextSaveGState(context);
	CGContextSetInterpolationQuality(context, kCGInterpolationNone);
	CGContextSetBlendMode(context, kCGBlendModeCopy);
	CGContextTranslateCTM(context, CGRectGetMinX(bounds), CGRectGetMaxY(bounds));
	CGContextScaleCTM(context, 1, -1);
	CGContextDrawImage(
		context,
		CGRectMake(0, 0, CGRectGetWidth(bounds), CGRectGetHeight(bounds)),
		image);
	CGContextRestoreGState(context);
	CGImageRelease(image);
}

@end

VDGUIHandle VDCreateDisplayViewMac() {
	if (![NSThread isMainThread])
		return nullptr;

	VDMacVideoDisplayView *view = [[VDMacVideoDisplayView alloc]
		initWithFrame:NSMakeRect(0, 0, 1, 1)];
	return reinterpret_cast<VDGUIHandle>(view);
}

void VDDestroyDisplayViewMac(VDGUIHandle view) {
	VDMacVideoDisplayView *displayView = reinterpret_cast<VDMacVideoDisplayView *>(view);
	if (!displayView)
		return;

	if ([NSThread isMainThread])
		[displayView release];
	else
		dispatch_async(dispatch_get_main_queue(), ^{ [displayView release]; });
}

bool VDDisplayViewSetSourceMac(VDGUIHandle view, const VDPixmap& source, bool allowConversion) {
	VDMacVideoDisplayView *displayView = reinterpret_cast<VDMacVideoDisplayView *>(view);
	return displayView && [displayView setSource:source allowConversion:allowConversion];
}

void VDDisplayViewClearMac(VDGUIHandle view) {
	VDMacVideoDisplayView *displayView = reinterpret_cast<VDMacVideoDisplayView *>(view);
	if (displayView)
		[displayView clearFrame];
}
