// Altirra - Atari 800/800XL/5200 emulator
// Copyright (C) 2026 Avery Lee

#ifndef f_VD2_VDDISPLAY_INTERNAL_FRAMEQUEUE_H
#define f_VD2_VDDISPLAY_INTERNAL_FRAMEQUEUE_H

#include <deque>
#include <mutex>
#include <vd2/VDDisplay/display.h>

struct VDDisplayFrameLease {
	vdrefptr<VDVideoDisplayFrame> mpFrame;
	uint64 mSequence = 0;
};

// Thread-safe ownership queue for software display backends. Frames must not
// be modified after Post() until Revoke() transfers them back to the producer.
// The consumer must stop reading a lease before Complete(). Flush() cancels
// leases, but their strong references keep in-flight reads safe. It does not
// wait for the consumer; the backend must serialize rendering with its flush.
class VDDisplayFrameQueue {
public:
	~VDDisplayFrameQueue();

	bool Post(VDVideoDisplayFrame *frame);
	VDDisplayFrameLease Begin();
	bool Complete(const VDDisplayFrameLease& lease, bool presented = true);
	bool Revoke(bool allowFrameSkip, VDVideoDisplayFrame **frame);
	void Flush();

	int GetQueuedFrames() const;
	bool IsFramePending() const;
	vdrefptr<VDVideoDisplayFrame> GetLastFrame() const;

private:
	mutable std::mutex mMutex;
	std::deque<vdrefptr<VDVideoDisplayFrame>> mPending;
	std::deque<vdrefptr<VDVideoDisplayFrame>> mIdle;
	vdrefptr<VDVideoDisplayFrame> mpActive;
	vdrefptr<VDVideoDisplayFrame> mpLast;
	uint64 mSequence = 0;
};

#endif
