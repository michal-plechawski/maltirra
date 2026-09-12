// Altirra portable raw audio source tests

#include <cstdlib>
#include <utility>
#include <vector>

#include <at/attest/portabletest.h>
#include <audiorawsource.h>

namespace {
	class RecordingEdgePlayer final : public IATSyncAudioEdgePlayer {
	public:
		struct Batch {
			std::vector<ATSyncAudioEdge> mEdges;
			float mVolume = 0;
		};

		void AddEdges(const ATSyncAudioEdge *edges, size_t count, float volume) override {
			Batch batch;
			batch.mVolume = volume;
			if (count)
				batch.mEdges.assign(edges, edges + count);
			mBatches.push_back(std::move(batch));
		}
		void AddEdgeBuffer(ATSyncAudioEdgeBuffer *) override { std::abort(); }

		std::vector<Batch> mBatches;
	};

	class TestAudioMixer final : public IATAudioMixer {
	public:
		void AddSyncAudioSource(IATSyncAudioSource *source) override {
			mSource = source;
			++mAdds;
		}
		void RemoveSyncAudioSource(IATSyncAudioSource *source) override {
			if (mSource != source)
				std::abort();
			mSource = nullptr;
			++mRemoves;
		}
		void AddAsyncAudioSource(IATAudioAsyncSource&) override { std::abort(); }
		void RemoveAsyncAudioSource(IATAudioAsyncSource&) override { std::abort(); }
		IATSyncAudioSamplePlayer& GetSamplePlayer() override { std::abort(); }
		IATSyncAudioSamplePlayer& GetEdgeSamplePlayer() override { std::abort(); }
		IATSyncAudioEdgePlayer& GetEdgePlayer() override { return mEdgePlayer; }
		IATSyncAudioSamplePlayer& GetAsyncSamplePlayer() override { std::abort(); }
		void AddInternalAudioTap(IATInternalAudioTap *) override { std::abort(); }
		void RemoveInternalAudioTap(IATInternalAudioTap *) override { std::abort(); }
		void BlockInternalAudio() override { std::abort(); }
		void UnblockInternalAudio() override { std::abort(); }

		IATSyncAudioSource *mSource = nullptr;
		uint32 mAdds = 0;
		uint32 mRemoves = 0;
		RecordingEdgePlayer mEdgePlayer;
	};
}

bool ATTestAltirraAudioRawSource(ATPortableTestContext& context) {
	TestAudioMixer mixer;
	ATAudioRawSource source;
	AT_PORTABLE_TEST_ASSERT(context, !source.RequiresStereoMixingNow());
	source.Init(&mixer);
	AT_PORTABLE_TEST_ASSERT(context, mixer.mSource == &source);
	AT_PORTABLE_TEST_ASSERT(context, mixer.mAdds == 1);

	float mixLevels[kATAudioMixCount] {};
	mixLevels[kATAudioMix_Other] = 0.4f;
	ATSyncAudioMixInfo mixInfo {};
	mixInfo.mStartTime = 90;
	mixInfo.mCount = 2;
	mixInfo.mpMixLevels = mixLevels;

	source.SetOutput(100, 0.25f);
	source.SetOutput(100, 0.50f);
	source.SetOutput(120, 0.50f);
	source.SetOutput(130, -0.25f);
	source.WriteAudio(mixInfo);
	AT_PORTABLE_TEST_ASSERT(context, mixer.mEdgePlayer.mBatches.size() == 1);
	const auto& first = mixer.mEdgePlayer.mBatches[0];
	AT_PORTABLE_TEST_ASSERT(context, first.mVolume == 0.4f);
	AT_PORTABLE_TEST_ASSERT(context, first.mEdges.size() == 2);
	AT_PORTABLE_TEST_ASSERT(context, first.mEdges[0].mTime == 100);
	AT_PORTABLE_TEST_ASSERT(context, first.mEdges[0].mDeltaValue == 0.50f);
	AT_PORTABLE_TEST_ASSERT(context, first.mEdges[1].mTime == 130);
	AT_PORTABLE_TEST_ASSERT(context, first.mEdges[1].mDeltaValue == -0.75f);

	source.SetOutput(170, 0.75f);
	mixInfo.mStartTime = 146;
	source.WriteAudio(mixInfo);
	AT_PORTABLE_TEST_ASSERT(context, mixer.mEdgePlayer.mBatches.size() == 2);
	AT_PORTABLE_TEST_ASSERT(context, mixer.mEdgePlayer.mBatches[1].mEdges.size() == 1);
	AT_PORTABLE_TEST_ASSERT(context, mixer.mEdgePlayer.mBatches[1].mEdges[0].mTime == 170);
	AT_PORTABLE_TEST_ASSERT(context, mixer.mEdgePlayer.mBatches[1].mEdges[0].mDeltaValue == 1.0f);

	source.SetOutput(80, 1.0f);
	mixInfo.mStartTime = 200;
	mixInfo.mCount = 1;
	source.WriteAudio(mixInfo);
	AT_PORTABLE_TEST_ASSERT(context, mixer.mEdgePlayer.mBatches.size() == 3);
	AT_PORTABLE_TEST_ASSERT(context, mixer.mEdgePlayer.mBatches[2].mEdges.size() == 1);
	AT_PORTABLE_TEST_ASSERT(context, mixer.mEdgePlayer.mBatches[2].mEdges[0].mTime == 200);
	AT_PORTABLE_TEST_ASSERT(context, mixer.mEdgePlayer.mBatches[2].mEdges[0].mDeltaValue == 0.25f);

	source.SetOutput(300, 0.0f);
	mixInfo.mStartTime = 220;
	source.WriteAudio(mixInfo);
	AT_PORTABLE_TEST_ASSERT(context, mixer.mEdgePlayer.mBatches.size() == 4);
	AT_PORTABLE_TEST_ASSERT(context, mixer.mEdgePlayer.mBatches[3].mEdges.empty());
	mixInfo.mStartTime = 280;
	source.WriteAudio(mixInfo);
	AT_PORTABLE_TEST_ASSERT(context, mixer.mEdgePlayer.mBatches.size() == 5);
	AT_PORTABLE_TEST_ASSERT(context, mixer.mEdgePlayer.mBatches[4].mEdges.size() == 1);
	AT_PORTABLE_TEST_ASSERT(context, mixer.mEdgePlayer.mBatches[4].mEdges[0].mTime == 300);
	AT_PORTABLE_TEST_ASSERT(context, mixer.mEdgePlayer.mBatches[4].mEdges[0].mDeltaValue == -1.0f);

	source.Shutdown();
	source.Shutdown();
	AT_PORTABLE_TEST_ASSERT(context, mixer.mSource == nullptr);
	AT_PORTABLE_TEST_ASSERT(context, mixer.mRemoves == 1);
	return true;
}
