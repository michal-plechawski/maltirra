// Altirra - Atari 800/800XL/5200 emulator
// Copyright (C) 2026 Avery Lee

#include <cstring>
#include <cwchar>
#include <limits>
#include <mutex>
#include <new>

#import <AppKit/AppKit.h>
#import <CoreGraphics/CoreGraphics.h>
#import <dispatch/dispatch.h>

#include <vd2/VDDisplay/displayview_macos.h>
#include <vd2/VDDisplay/internal/presentationbuffer.h>
#include <vd2/system/VDString.h>

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

void VDDrawDisplayImageMac(CGContextRef context, CGImageRef image, CGRect bounds,
	const vdrect32 *sourceRect, const vdrect32f *destRect, uint32 backgroundColor,
	bool bilinear) {
	if (!context)
		return;

	CGContextSaveGState(context);
	CGContextClipToRect(context, bounds);
	CGContextSetRGBFillColor(context, ((backgroundColor >> 16) & 255) / 255.0,
		((backgroundColor >> 8) & 255) / 255.0, (backgroundColor & 255) / 255.0, 1);
	CGContextFillRect(context, bounds);

	CGImageRef croppedImage = nullptr;
	if (image && sourceRect) {
		if (sourceRect->right > sourceRect->left && sourceRect->bottom > sourceRect->top) {
			const CGRect sourceBounds = CGRectMake(0, 0, CGImageGetWidth(image), CGImageGetHeight(image));
			const CGRect crop = CGRectIntersection(sourceBounds, CGRectMake(sourceRect->left,
				sourceRect->top, (double)sourceRect->right - sourceRect->left,
				(double)sourceRect->bottom - sourceRect->top));
			if (!CGRectIsEmpty(crop) && !CGRectIsNull(crop))
				croppedImage = CGImageCreateWithImageInRect(image, crop);
		}
		image = croppedImage;
	}

	const CGRect dest = destRect
		? CGRectMake(destRect->left, destRect->top, destRect->width(), destRect->height())
		: bounds;
	if (image && !CGRectIsEmpty(dest)) {
		CGContextSetInterpolationQuality(context, bilinear ? kCGInterpolationLow : kCGInterpolationNone);
		CGContextSetBlendMode(context, kCGBlendModeCopy);
		CGContextTranslateCTM(context, CGRectGetMinX(dest), CGRectGetMaxY(dest));
		CGContextScaleCTM(context, 1, -1);
		CGContextDrawImage(context, CGRectMake(0, 0, CGRectGetWidth(dest), CGRectGetHeight(dest)), image);
	}
	if (croppedImage)
		CGImageRelease(croppedImage);
	CGContextRestoreGState(context);
}

@interface VDMacVideoDisplayView : NSView {
@private
	std::mutex *_mutex;
	VDDisplayPresentationBuffer *_presentationBuffer;
	CGImageRef _image;
	bool _redisplayPending;
	bool _hasSourceRect;
	bool _hasDestRect;
	vdrect32 _sourceRect;
	vdrect32f _destRect;
	uint32 _backgroundColor;
	bool _bilinear;
	NSString *_message;
}

- (bool)setSource:(const VDPixmap&)source allowConversion:(bool)allowConversion;
- (void)clearFrame;
- (void)requestRedisplay;
- (void)setImage:(CGImageRef)image;
- (void)setLayout:(const vdrect32 *)sourceRect destination:(const vdrect32f *)destRect
	background:(uint32)backgroundColor bilinear:(bool)bilinear;
- (void)setMessage:(const wchar_t *)message;

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
	[_message release];
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

- (void)setImage:(CGImageRef)image {
	if (image)
		CGImageRetain(image);
	CGImageRef oldImage = nullptr;
	{
		std::lock_guard<std::mutex> lock(*_mutex);
		oldImage = _image;
		_image = image;
	}
	if (oldImage)
		CGImageRelease(oldImage);
	[self requestRedisplay];
}

- (void)setLayout:(const vdrect32 *)sourceRect destination:(const vdrect32f *)destRect
	background:(uint32)backgroundColor bilinear:(bool)bilinear {
	_hasSourceRect = sourceRect != nullptr;
	_hasDestRect = destRect != nullptr;
	if (sourceRect)
		_sourceRect = *sourceRect;
	if (destRect)
		_destRect = *destRect;
	_backgroundColor = backgroundColor;
	_bilinear = bilinear;
	[self requestRedisplay];
}

- (void)setMessage:(const wchar_t *)message {
	NSString *text = nil;
	if (message) {
		const VDStringA utf8 = VDTextWToU8(message, (int)std::wcslen(message));
		text = [[NSString alloc] initWithBytes:utf8.data() length:utf8.size()
			encoding:NSUTF8StringEncoding];
	}
	[_message release];
	_message = text;
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

	CGImageRef image = nullptr;
	{
		std::lock_guard<std::mutex> lock(*_mutex);
		if (_image)
			image = CGImageRetain(_image);
	}

	VDDrawDisplayImageMac(context, image, bounds, _hasSourceRect ? &_sourceRect : nullptr,
		_hasDestRect ? &_destRect : nullptr, _backgroundColor, _bilinear);
	if (image)
		CGImageRelease(image);
	if ([_message length]) {
		NSDictionary *attributes = @{ NSFontAttributeName: [NSFont systemFontOfSize:13],
			NSForegroundColorAttributeName: [NSColor whiteColor] };
		const NSSize size = [_message sizeWithAttributes:attributes];
		[_message drawAtPoint:NSMakePoint(CGRectGetMidX(bounds) - size.width / 2,
			CGRectGetMidY(bounds) - size.height / 2) withAttributes:attributes];
	}
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

void VDDisplayViewSetImageMac(VDGUIHandle view, CGImageRef image) {
	[reinterpret_cast<VDMacVideoDisplayView *>(view) setImage:image];
}

void VDDisplayViewSetLayoutMac(VDGUIHandle view, const vdrect32 *sourceRect,
	const vdrect32f *destRect, uint32 backgroundColor, bool bilinear) {
	[reinterpret_cast<VDMacVideoDisplayView *>(view) setLayout:sourceRect destination:destRect
		background:backgroundColor bilinear:bilinear];
}

void VDDisplayViewSetMessageMac(VDGUIHandle view, const wchar_t *message) {
	[reinterpret_cast<VDMacVideoDisplayView *>(view) setMessage:message];
}
