// Altirra - Atari 800/800XL/5200 emulator
// Copyright (C) 2026 Avery Lee

#ifndef f_VD2_VDDISPLAY_DISPLAYVIEW_MACOS_H
#define f_VD2_VDDISPLAY_DISPLAYVIEW_MACOS_H

#include <CoreGraphics/CoreGraphics.h>

#include <vd2/system/vdtypes.h>
#include <vd2/system/vectors.h>
#include <vd2/system/function.h>
#include <vd2/Kasumi/pixmap.h>

// Creates an immutable Core Graphics image from a positive-pitch XRGB8888
// pixmap. The returned image owns its pixel snapshot and must be released by
// the caller with CGImageRelease().
CGImageRef VDCreateDisplayImageMac(const VDPixmap& pixmap);

// Draws into a context whose origin is at the top left, as in a flipped
// NSView. Shared with offscreen rendering tests. Source coordinates are pixels;
// destination coordinates are points in the view.
void VDDrawDisplayImageMac(CGContextRef context, CGImageRef image, CGRect bounds,
	const vdrect32 *sourceRect, const vdrect32f *destRect, uint32 backgroundColor,
	bool bilinear);

// The view is returned retained. Creation must run on the main thread;
// destruction and frame updates may run on any thread. The caller must finish
// all frame updates before destroying the view and must not reuse the handle
// once destruction has begun.
VDGUIHandle VDCreateDisplayViewMac();
void VDDestroyDisplayViewMac(VDGUIHandle view);
bool VDDisplayViewSetSourceMac(VDGUIHandle view, const VDPixmap& source, bool allowConversion);
void VDDisplayViewClearMac(VDGUIHandle view);

// The view retains the image; the caller keeps its own ownership. Layout and
// message updates must run on the main thread.
void VDDisplayViewSetImageMac(VDGUIHandle view, CGImageRef image);
void VDDisplayViewSetLayoutMac(VDGUIHandle view, const vdrect32 *sourceRect,
	const vdrect32f *destRect, uint32 backgroundColor, bool bilinear);
void VDDisplayViewSetMessageMac(VDGUIHandle view, const wchar_t *message);

struct VDDisplayViewOutputInfoMac {
	sint32 mWidth = 0;
	sint32 mHeight = 0;
	float mScaleX = 1;
	float mScaleY = 1;
};

// Output dimensions are backing pixels; layout coordinates remain view points.
// These functions and the refresh callback run on the main thread.
VDDisplayViewOutputInfoMac VDDisplayViewGetOutputInfoMac(VDGUIHandle view);
void VDDisplayViewSetRefreshCallbackMac(VDGUIHandle view, vdfunction<void()> callback);

#endif
