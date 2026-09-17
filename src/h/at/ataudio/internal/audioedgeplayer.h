// Altirra portable synchronous audio edge renderer

#ifndef f_AT_ATAUDIO_INTERNAL_AUDIOEDGEPLAYER_H
#define f_AT_ATAUDIO_INTERNAL_AUDIOEDGEPLAYER_H

#include <at/atcore/audiomixer.h>
#include <at/atcore/audiosource.h>

class ATSyncAudioEdgePlayer final : public IATSyncAudioEdgePlayer {
public:
	// We need one sample extra for the triangle filter, one to accommodate
	// frame start cycle-to-sample jitter, and another for end jitter.
	static constexpr int kTailLength = 3;

	bool IsStereoMixingRequired() const;

	// Important: Both buffers need kTailLength temporary entries after n.
	void RenderEdges(
		float *dstLeft, float *dstRight, uint32 n, uint32 timestamp);

	void AddEdges(
		const ATSyncAudioEdge *edges, size_t numEdges, float volume) override;
	void AddEdgeBuffer(ATSyncAudioEdgeBuffer *buffer) override;

private:
	void RenderEdgeBuffer(
		float *dstLeft, float *dstRight, uint32 n, uint32 timestamp,
		const ATSyncAudioEdge *edges, size_t numEdges, float volume);

	template<bool T_RightEnabled>
	void RenderEdgeBuffer2(
		float *dstLeft, float *dstRight, uint32 n, uint32 timestamp,
		const ATSyncAudioEdge *edges, size_t numEdges, float volume);

	vdfastvector<ATSyncAudioEdge> mEdges;
	vdfastvector<ATSyncAudioEdgeBuffer *> mBuffers;
	bool mbTailHasStereo = false;

	float mLeftTail[kTailLength] {};
	float mRightTail[kTailLength] {};
};

#endif
