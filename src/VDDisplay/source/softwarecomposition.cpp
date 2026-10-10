// Altirra - Atari 800/800XL/5200 emulator
// Copyright (C) 2026 Avery Lee

#include <vd2/VDDisplay/internal/softwarecomposition.h>

VDDisplaySoftwareComposition::VDDisplaySoftwareComposition() {
	mRenderer.Init();
}

VDDisplaySoftwareComposition::~VDDisplaySoftwareComposition() {
	Shutdown();
}

void VDDisplaySoftwareComposition::SetCompositor(IVDDisplayCompositor *compositor) {
	if (!mbShutdown)
		ApplyCompositor(compositor);
}

void VDDisplaySoftwareComposition::ApplyCompositor(IVDDisplayCompositor *compositor) {
	if ((!mbChanging && !mbChangePending && mpCompositor.get() == compositor)
		|| (mbChangePending && mpRequested.get() == compositor))
		return;

	mpRequested = compositor;
	mbChangePending = true;
	++mGeneration;
	if (mbChanging)
		return;

	struct ChangeScope {
		bool& mFlag;
		~ChangeScope() { mFlag = false; }
	} scope { mbChanging };
	mbChanging = true;
	while (mbChangePending) {
		auto next = std::move(mpRequested);
		mbChangePending = false;
		if (next == mpCompositor)
			continue;

		auto old = std::move(mpCompositor);
		if (old)
			old->DetachCompositor();

		// A detach callback can request a newer attachment. Do not attach a
		// stale candidate, and never detach an object before its first attach.
		if (mbChangePending)
			continue;
		mpCompositor = std::move(next);
		if (mpCompositor)
			mpCompositor->AttachCompositor(*this);
	}
}

void VDDisplaySoftwareComposition::Shutdown() {
	if (mbShutdown)
		return;
	mbShutdown = true;
	ApplyCompositor(nullptr);
	Clear();
}

void VDDisplaySoftwareComposition::ClearBuffers() {
	mOutput.clear();
	static_cast<VDPixmap&>(mOutput) = {};
	mStaging.clear();
	static_cast<VDPixmap&>(mStaging) = {};
	mbClearPending = false;
}

void VDDisplaySoftwareComposition::Clear() {
	++mGeneration;
	if (mbRendering)
		mbClearPending = true;
	else
		ClearBuffers();
}

bool VDDisplaySoftwareComposition::Render(sint32 width, sint32 height,
	const vdfunction<bool(const VDPixmap&)>& paint,
	const vdfunction<bool(const VDPixmap&)>& finalize) {
	if (mbShutdown || mbRendering || !paint || width <= 0 || height <= 0
		|| width > (INT32_MAX - 15) / 4)
		return false;
	const uint64 pitch = ((uint64)width * 4 + 15) & ~UINT64_C(15);
	if (pitch * (uint64)height > UINT32_MAX - 28)
		return false;

	struct RenderScope {
		VDDisplaySoftwareComposition& mOwner;
		~RenderScope() {
			mOwner.mbRendering = false;
			if (mOwner.mbClearPending)
				mOwner.ClearBuffers();
		}
	} scope { *this };
	mbRendering = true;
	const uint64 generation = mGeneration;
	const auto compositor = mpCompositor;
	const VDDisplayCompositeInfo info { (uint32)width, (uint32)height };
	if (compositor)
		compositor->PreComposite(info);
	if (generation != mGeneration || mbShutdown)
		return false;

	mStaging.init(width, height, nsVDPixmap::kPixFormat_XRGB8888);
	if (!paint(mStaging) || generation != mGeneration || mbShutdown)
		return false;
	if (compositor) {
		if (!mRenderer.Begin(mStaging))
			return false;
		compositor->Composite(mRenderer, info);
	}
	if (generation != mGeneration || mbShutdown)
		return false;
	if (finalize && !finalize(mStaging))
		return false;
	if (generation != mGeneration || mbShutdown)
		return false;

	mOutput.swap(mStaging);
	return true;
}
