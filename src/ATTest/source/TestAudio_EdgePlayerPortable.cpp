// Altirra portable synchronous audio edge renderer tests

#include <array>

#include <at/attest/portabletest.h>
#include <at/ataudio/internal/audioedgeplayer.h>

namespace {
	constexpr uint32 kBlockSamples = 8;
	constexpr uint32 kTimestamp = 1000;
	using ATTestBuffer = std::array<
		float, kBlockSamples + ATSyncAudioEdgePlayer::kTailLength>;

	bool ATCheckLooseEdgesAndTail(ATPortableTestContext& context) {
		ATSyncAudioEdgePlayer player;
		player.AddEdges(nullptr, 0, 1.0f);
		AT_PORTABLE_TEST_ASSERT(context, !player.IsStereoMixingRequired());

		const ATSyncAudioEdge edges[] {
			{ kTimestamp, 2.0f },
			{ kTimestamp + 14, 4.0f },
			{ kTimestamp + kBlockSamples * kATCyclesPerSyncSample, 6.0f }
		};
		player.AddEdges(edges, std::size(edges), 0.5f);

		ATTestBuffer firstBlock {};
		player.RenderEdges(
			firstBlock.data(), nullptr, kBlockSamples, kTimestamp);

		AT_PORTABLE_TEST_ASSERT(context, firstBlock[0] == 2.0f);
		AT_PORTABLE_TEST_ASSERT(context, firstBlock[1] == 1.0f);
		for(size_t i = 2; i < kBlockSamples; ++i)
			AT_PORTABLE_TEST_ASSERT(context, firstBlock[i] == 0.0f);
		AT_PORTABLE_TEST_ASSERT(context, firstBlock[kBlockSamples] == 3.0f);
		AT_PORTABLE_TEST_ASSERT(context, !player.IsStereoMixingRequired());

		ATTestBuffer secondBlock {};
		player.RenderEdges(
			secondBlock.data(), nullptr, kBlockSamples,
			kTimestamp + kBlockSamples * kATCyclesPerSyncSample);
		AT_PORTABLE_TEST_ASSERT(context, secondBlock[0] == 3.0f);
		for(size_t i = 1; i < secondBlock.size(); ++i)
			AT_PORTABLE_TEST_ASSERT(context, secondBlock[i] == 0.0f);

		return true;
	}

	bool ATCheckStereoEdgeBuffer(ATPortableTestContext& context) {
		ATSyncAudioEdgePlayer player;
		vdrefptr<ATSyncAudioEdgeBuffer> edgeBuffer {
			new ATSyncAudioEdgeBuffer
		};
		edgeBuffer->mEdges.push_back({
			kTimestamp + kATCyclesPerSyncSample, 2.0f
		});
		edgeBuffer->mLeftVolume = 0.25f;
		edgeBuffer->mRightVolume = 0.75f;

		player.AddEdgeBuffer(edgeBuffer);
		AT_PORTABLE_TEST_ASSERT(context, player.IsStereoMixingRequired());

		ATTestBuffer left {};
		ATTestBuffer right {};
		player.RenderEdges(
			left.data(), right.data(), kBlockSamples, kTimestamp);

		AT_PORTABLE_TEST_ASSERT(context, left[1] == 0.5f);
		AT_PORTABLE_TEST_ASSERT(context, right[1] == 1.5f);
		AT_PORTABLE_TEST_ASSERT(context, edgeBuffer->mEdges.empty());
		AT_PORTABLE_TEST_ASSERT(context, !player.IsStereoMixingRequired());

		edgeBuffer->mEdges.push_back({
			kTimestamp + 2 * kATCyclesPerSyncSample, 2.0f
		});
		edgeBuffer->mLeftVolume = 0.0f;
		edgeBuffer->mRightVolume = 1.0f;
		player.AddEdgeBuffer(edgeBuffer);
		AT_PORTABLE_TEST_ASSERT(context, player.IsStereoMixingRequired());

		left.fill(0.0f);
		right.fill(0.0f);
		player.RenderEdges(
			left.data(), right.data(), kBlockSamples, kTimestamp);
		AT_PORTABLE_TEST_ASSERT(context, left[2] == 0.0f);
		AT_PORTABLE_TEST_ASSERT(context, right[2] == 2.0f);
		AT_PORTABLE_TEST_ASSERT(context, !player.IsStereoMixingRequired());

		return true;
	}

	bool ATCheckStereoTail(ATPortableTestContext& context) {
		ATSyncAudioEdgePlayer player;
		vdrefptr<ATSyncAudioEdgeBuffer> edgeBuffer {
			new ATSyncAudioEdgeBuffer
		};
		edgeBuffer->mEdges.push_back({
			kTimestamp + kBlockSamples * kATCyclesPerSyncSample, 4.0f
		});
		edgeBuffer->mLeftVolume = 0.25f;
		edgeBuffer->mRightVolume = 0.75f;
		player.AddEdgeBuffer(edgeBuffer);

		ATTestBuffer firstLeft {};
		ATTestBuffer firstRight {};
		player.RenderEdges(
			firstLeft.data(), firstRight.data(), kBlockSamples, kTimestamp);
		AT_PORTABLE_TEST_ASSERT(context, firstLeft[kBlockSamples] == 1.0f);
		AT_PORTABLE_TEST_ASSERT(context, firstRight[kBlockSamples] == 3.0f);
		AT_PORTABLE_TEST_ASSERT(context, player.IsStereoMixingRequired());

		ATTestBuffer secondLeft {};
		ATTestBuffer secondRight {};
		player.RenderEdges(
			secondLeft.data(), secondRight.data(), kBlockSamples,
			kTimestamp + kBlockSamples * kATCyclesPerSyncSample);
		AT_PORTABLE_TEST_ASSERT(context, secondLeft[0] == 1.0f);
		AT_PORTABLE_TEST_ASSERT(context, secondRight[0] == 3.0f);
		AT_PORTABLE_TEST_ASSERT(context, !player.IsStereoMixingRequired());

		return true;
	}
}

bool ATTestAudioEdgePlayer(ATPortableTestContext& context) {
	return ATCheckLooseEdgesAndTail(context)
		&& ATCheckStereoEdgeBuffer(context)
		&& ATCheckStereoTail(context);
}
