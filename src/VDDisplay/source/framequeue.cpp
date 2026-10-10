// Altirra - Atari 800/800XL/5200 emulator
// Copyright (C) 2026 Avery Lee

#include <algorithm>
#include <vd2/VDDisplay/internal/framequeue.h>

VDDisplayFrameQueue::~VDDisplayFrameQueue() {
	Flush();
}

bool VDDisplayFrameQueue::Post(VDVideoDisplayFrame *frame) {
	if (!frame)
		return false;

	vdrefptr<VDVideoDisplayFrame> ownedFrame(frame);
	std::lock_guard<std::mutex> lock(mMutex);
	const auto contains = [frame](const auto& frames) {
		return std::any_of(frames.begin(), frames.end(),
			[frame](const auto& entry) { return entry.get() == frame; });
	};
	if (mpActive.get() == frame || mpLast.get() == frame
		|| contains(mPending) || contains(mIdle))
		return false;

	mPending.push_back(std::move(ownedFrame));
	return true;
}

VDDisplayFrameLease VDDisplayFrameQueue::Begin() {
	std::lock_guard<std::mutex> lock(mMutex);
	if (mpActive || mPending.empty())
		return {};

	mpActive = std::move(mPending.front());
	mPending.pop_front();
	return { mpActive, ++mSequence };
}

bool VDDisplayFrameQueue::Complete(const VDDisplayFrameLease& lease, bool presented) {
	vdrefptr<VDVideoDisplayFrame> discard;
	std::lock_guard<std::mutex> lock(mMutex);
	if (!mpActive || lease.mSequence != mSequence || lease.mpFrame.get() != mpActive.get())
		return false;

	const bool cache = !(mpActive->mFlags & IVDVideoDisplay::kDoNotCache);
	if (presented) {
		if (mpLast)
			mIdle.push_front(std::move(mpLast));
		if (cache)
			mpLast = std::move(mpActive);
		else
			discard = std::move(mpActive);
	} else if (cache) {
		mIdle.push_front(std::move(mpActive));
	} else {
		discard = std::move(mpActive);
	}

	return true;
}

bool VDDisplayFrameQueue::Revoke(bool allowFrameSkip, VDVideoDisplayFrame **frame) {
	if (!frame)
		return false;

	std::lock_guard<std::mutex> lock(mMutex);
	if (allowFrameSkip && mPending.size() > 1) {
		*frame = mPending.back().release();
		mPending.pop_back();
		return true;
	}

	if (mIdle.empty())
		return false;

	*frame = mIdle.front().release();
	mIdle.pop_front();
	return true;
}

void VDDisplayFrameQueue::Flush() {
	std::deque<vdrefptr<VDVideoDisplayFrame>> pending;
	std::deque<vdrefptr<VDVideoDisplayFrame>> idle;
	vdrefptr<VDVideoDisplayFrame> active;
	vdrefptr<VDVideoDisplayFrame> last;
	{
		std::lock_guard<std::mutex> lock(mMutex);
		pending.swap(mPending);
		idle.swap(mIdle);
		active = std::move(mpActive);
		last = std::move(mpLast);
	}
	// Release outside the lock: derived frame destructors may call the backend.
}

int VDDisplayFrameQueue::GetQueuedFrames() const {
	std::lock_guard<std::mutex> lock(mMutex);
	return (int)mPending.size() + (mpActive ? 1 : 0);
}

bool VDDisplayFrameQueue::IsFramePending() const {
	std::lock_guard<std::mutex> lock(mMutex);
	return mpActive != nullptr;
}

vdrefptr<VDVideoDisplayFrame> VDDisplayFrameQueue::GetLastFrame() const {
	std::lock_guard<std::mutex> lock(mMutex);
	return mpLast;
}
