// Altirra - Atari 800/800XL/5200 emulator
// Copyright (C) 2026 Avery Lee

#ifndef f_VD2_VDDISPLAY_DISPLAY_MACOS_H
#define f_VD2_VDDISPLAY_DISPLAY_MACOS_H

#include <vd2/VDDisplay/display.h>

// Baseline SDR software backend: owned frame snapshots, frame recycling,
// software screen FX, capture, source cropping, destination layout, and point
// or bilinear filtering. Bicubic falls back to bilinear. GPU-only preferences
// (HDR, precise vsync, pixel sharpening, custom refresh modes) are not supported.
// Compositors render after video scaling in output-pixel coordinates. Captures
// include video, borders and overlays. Custom shaders are not supported.
//
// Create on the main thread with an existing VDCreateDisplayViewMac() view.
// The backend retains the view independently of the caller's reference.
// A null view creates a headless backend using the same frame/capture pipeline.
// Its optional width/height select output pixels; zero defaults to source size.
// Other methods may run on any thread; callbacks run on the main thread.
// Cross-thread synchronous methods require a running main event loop. Source
// pixels/FX engines must remain valid until the source is replaced, flushed,
// or destroyed (or Cache() is called for a nonpersistent source).
// Before Destroy(), stop producers and finish all other calls. Destroy() cancels
// queued main-thread work and pending capture; it releases only the backend's
// view reference. The caller still owns its view and must release it separately.
IVDVideoDisplay *VDCreateVideoDisplayMac(VDGUIHandle view, sint32 width = 0, sint32 height = 0);

#endif
