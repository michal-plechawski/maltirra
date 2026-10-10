// Altirra - Atari 800/800XL/5200 emulator
// Copyright (C) 2026 Avery Lee

#ifndef f_VD2_VDDISPLAY_INTERNAL_SOFTWARECOMPOSITION_H
#define f_VD2_VDDISPLAY_INTERNAL_SOFTWARECOMPOSITION_H

#include <vd2/system/function.h>
#include <vd2/Kasumi/pixmaputils.h>
#include <vd2/VDDisplay/compositor.h>
#include <vd2/VDDisplay/renderersoft.h>

// Serialized software composition in output-pixel coordinates. Like the GDI
// backend, this engine supports overlays but not GPU custom effects. The
// backend paints video/borders between PreComposite() and Composite().
class VDDisplaySoftwareComposition final : public IVDDisplayCompositionEngine {
public:
	VDDisplaySoftwareComposition();
	~VDDisplaySoftwareComposition();

	void SetCompositor(IVDDisplayCompositor *compositor);
	void Shutdown();
	void Clear();
	void Invalidate() { ++mGeneration; }
	void LoadCustomEffect(const wchar_t *) override {}

	// Failed, recursively entered, or cancelled renders preserve the last
	// output. paint() and finalize() see a borrowed XRGB8888 staging target.
	// finalize() can prepare a platform image/upload before the output commits.
	bool Render(sint32 width, sint32 height,
		const vdfunction<bool(const VDPixmap&)>& paint,
		const vdfunction<bool(const VDPixmap&)>& finalize = {});
	const VDPixmap& GetPixmap() const { return mOutput; }
	bool IsRendering() const { return mbRendering; }

private:
	void ApplyCompositor(IVDDisplayCompositor *compositor);
	void ClearBuffers();

	VDDisplayRendererSoft mRenderer;
	VDPixmapBuffer mOutput;
	VDPixmapBuffer mStaging;
	vdrefptr<IVDDisplayCompositor> mpCompositor;
	vdrefptr<IVDDisplayCompositor> mpRequested;
	uint64 mGeneration = 0;
	bool mbChangePending = false;
	bool mbChanging = false;
	bool mbRendering = false;
	bool mbClearPending = false;
	bool mbShutdown = false;
};

#endif
