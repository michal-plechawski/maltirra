// Altirra portable synchronous audio edge renderer

#include <stdafx.h>
#include <at/ataudio/internal/audioedgeplayer.h>

bool ATSyncAudioEdgePlayer::IsStereoMixingRequired() const {
	if (mbTailHasStereo)
		return true;

	if (mBuffers.empty())
		return false;

	for(ATSyncAudioEdgeBuffer *buf : mBuffers) {
		if (!buf->mEdges.empty()
			&& buf->mLeftVolume != buf->mRightVolume
			&& (buf->mLeftVolume != 0 || buf->mRightVolume != 0))
			return true;
	}

	return false;
}

void ATSyncAudioEdgePlayer::RenderEdges(
	float *dstLeft, float *dstRight, uint32 n, uint32 timestamp) {
	memset(dstLeft + n, 0, sizeof(*dstLeft) * kTailLength);

	for(int i = 0; i < kTailLength; ++i)
		dstLeft[i] += mLeftTail[i];

	if (dstRight) {
		memset(dstRight + n, 0, sizeof(*dstRight) * kTailLength);

		for(int i = 0; i < kTailLength; ++i)
			dstRight[i] += mRightTail[i];
	}

	RenderEdgeBuffer(
		dstLeft, dstRight, n, timestamp, mEdges.data(), mEdges.size(), 1.0f);
	mEdges.clear();

	while(!mBuffers.empty()) {
		ATSyncAudioEdgeBuffer *buf = mBuffers.back();
		mBuffers.pop_back();

		if (buf->mLeftVolume == buf->mRightVolume) {
			if (buf->mLeftVolume != 0) {
				RenderEdgeBuffer(
					dstLeft, dstRight, n, timestamp,
					buf->mEdges.data(), buf->mEdges.size(), buf->mLeftVolume);
			}
		} else {
			if (buf->mLeftVolume != 0) {
				RenderEdgeBuffer(
					dstLeft, nullptr, n, timestamp,
					buf->mEdges.data(), buf->mEdges.size(), buf->mLeftVolume);
			}

			if (buf->mRightVolume != 0) {
				if (dstRight) {
					RenderEdgeBuffer(
						dstRight, nullptr, n, timestamp,
						buf->mEdges.data(), buf->mEdges.size(),
						buf->mRightVolume);
				} else {
					VDFAIL("Stereo edge buffer submitted without stereo being active.");
					RenderEdgeBuffer(
						dstLeft, nullptr, n, timestamp,
						buf->mEdges.data(), buf->mEdges.size(),
						buf->mRightVolume);
				}
			}
		}

		buf->mEdges.clear();
		buf->Release();
	}

	for(int i = 0; i < kTailLength; ++i)
		mLeftTail[i] = dstLeft[n + i];

	if (dstRight) {
		for(int i = 0; i < kTailLength; ++i)
			mRightTail[i] = dstRight[n + i];
	} else {
		for(int i = 0; i < kTailLength; ++i)
			mRightTail[i] = dstLeft[n + i];
	}

	mbTailHasStereo =
		memcmp(mLeftTail, mRightTail, sizeof mLeftTail) != 0;
}

void ATSyncAudioEdgePlayer::AddEdges(
	const ATSyncAudioEdge *edges, size_t numEdges, float volume) {
	if (!numEdges)
		return;

	mEdges.resize(mEdges.size() + numEdges);

	const ATSyncAudioEdge *VDRESTRICT src = edges;
	ATSyncAudioEdge *VDRESTRICT dst = &*(mEdges.end() - numEdges);

	while(numEdges--) {
		dst->mTime = src->mTime;
		dst->mDeltaValue = src->mDeltaValue * volume;
		++dst;
		++src;
	}
}

void ATSyncAudioEdgePlayer::AddEdgeBuffer(ATSyncAudioEdgeBuffer *buffer) {
	if (buffer) {
		mBuffers.push_back(buffer);
		buffer->AddRef();
	}
}

void ATSyncAudioEdgePlayer::RenderEdgeBuffer(
	float *dstLeft, float *dstRight, uint32 n, uint32 timestamp,
	const ATSyncAudioEdge *edges, size_t numEdges, float volume) {
	if (dstRight) {
		RenderEdgeBuffer2<true>(
			dstLeft, dstRight, n, timestamp, edges, numEdges, volume);
	} else {
		RenderEdgeBuffer2<false>(
			dstLeft, dstRight, n, timestamp, edges, numEdges, volume);
	}
}

template<bool T_RightEnabled>
void ATSyncAudioEdgePlayer::RenderEdgeBuffer2(
	float *dstLeft, float *dstRight, uint32 n, uint32 timestamp,
	const ATSyncAudioEdge *edges, size_t numEdges, float volume) {
	const ATSyncAudioEdge *VDRESTRICT src = edges;
	float *VDRESTRICT dstL2 = dstLeft;
	float *VDRESTRICT dstR2 = dstRight;
	const uint32 timeWindow = (n + 2) * kATCyclesPerSyncSample;

	while(numEdges--) {
		const uint32 cycleOffset = src->mTime - timestamp;
		if (cycleOffset < timeWindow) {
			const uint32 sampleOffset =
				cycleOffset / kATCyclesPerSyncSample;
			const uint32 phaseOffset =
				cycleOffset % kATCyclesPerSyncSample;
			const float shift = static_cast<float>(phaseOffset)
				* (1.0f / static_cast<float>(kATCyclesPerSyncSample));
			const float delta = src->mDeltaValue * volume;
			const float v1 = delta * shift;
			const float v0 = delta - v1;

			dstL2[sampleOffset] += v0;
			dstL2[sampleOffset + 1] += v1;

			if constexpr (T_RightEnabled) {
				dstR2[sampleOffset] += v0;
				dstR2[sampleOffset + 1] += v1;
			}
		} else {
			if (cycleOffset & UINT32_C(0x80000000))
				VDFAIL("Edge player has sample before allowed frame window.");
			else
				VDFAIL("Edge player has sample after allowed frame window.");
		}

		++src;
	}
}
