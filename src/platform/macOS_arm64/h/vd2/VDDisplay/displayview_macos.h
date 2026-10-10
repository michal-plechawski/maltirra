// Altirra - Atari 800/800XL/5200 emulator
// Copyright (C) 2026 Avery Lee

#ifndef f_VD2_VDDISPLAY_DISPLAYVIEW_MACOS_H
#define f_VD2_VDDISPLAY_DISPLAYVIEW_MACOS_H

#include <CoreGraphics/CoreGraphics.h>

#include <vd2/system/vdtypes.h>
#include <vd2/Kasumi/pixmap.h>

// Creates an immutable Core Graphics image from a positive-pitch XRGB8888
// pixmap. The returned image owns its pixel snapshot and must be released by
// the caller with CGImageRelease().
CGImageRef VDCreateDisplayImageMac(const VDPixmap& pixmap);

// The view is returned retained. Creation must run on the main thread;
// destruction and frame updates may run on any thread. The caller must finish
// all frame updates before destroying the view and must not reuse the handle
// once destruction has begun.
VDGUIHandle VDCreateDisplayViewMac();
void VDDestroyDisplayViewMac(VDGUIHandle view);
bool VDDisplayViewSetSourceMac(VDGUIHandle view, const VDPixmap& source, bool allowConversion);
void VDDisplayViewClearMac(VDGUIHandle view);

#endif
