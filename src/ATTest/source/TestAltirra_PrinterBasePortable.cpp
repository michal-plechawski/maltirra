// Altirra portable printer sound source tests

#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

#include <at/atcore/scheduler.h>
#include <at/attest/portabletest.h>
#include <printerbase.h>

namespace {
	class RecordingEdgePlayer final : public IATSyncAudioEdgePlayer {
	public:
		struct Batch {
			std::vector<ATSyncAudioEdge> mEdges;
			float mLeftVolume = 0;
			float mRightVolume = 0;
			std::string mDebugLabel;
		};

		void AddEdges(const ATSyncAudioEdge *, size_t, float) override { std::abort(); }

		void AddEdgeBuffer(ATSyncAudioEdgeBuffer *buffer) override {
			Batch batch;
			batch.mEdges.assign(buffer->mEdges.begin(), buffer->mEdges.end());
			batch.mLeftVolume = buffer->mLeftVolume;
			batch.mRightVolume = buffer->mRightVolume;
			batch.mDebugLabel = buffer->mpDebugLabel ? buffer->mpDebugLabel : "";
			mBatches.push_back(std::move(batch));
			buffer->mEdges.clear();
		}

		std::vector<Batch> mBatches;
	};

	class TestSoundGroup final : public vdrefcounted<IATAudioSoundGroup> {
	public:
		bool IsAnySoundQueued() const override { return false; }
		void StopAllSounds() override { ++mStopCalls; }

		uint32 mStopCalls = 0;
	};

	class RecordingSamplePlayer final : public IATSyncAudioSamplePlayer {
	public:
		struct SoundCall {
			bool mbLooping = false;
			uint32 mDelay = 0;
			ATAudioSampleId mSampleId = kATAudioSampleId_None;
			float mVolume = 0;
			ATSoundId mSoundId {};
		};

		struct TimedStop {
			ATSoundId mSoundId {};
			uint64 mTime = 0;
		};

		IATSyncAudioSource& AsSource() override { std::abort(); }
		vdrefptr<IATAudioSampleHandle> RegisterSample(vdspan<const sint16>, const ATAudioSoundSamplingRate&, float) override { std::abort(); }

		ATSoundId AddSound(IATAudioSoundGroup&, uint32 delay, ATAudioSampleId sampleId, float volume) override {
			return RecordSound(false, delay, sampleId, volume);
		}

		ATSoundId AddLoopingSound(IATAudioSoundGroup&, uint32 delay, ATAudioSampleId sampleId, float volume) override {
			return RecordSound(true, delay, sampleId, volume);
		}

		ATSoundId AddSound(IATAudioSoundGroup&, uint32, IATAudioSampleSource *, IVDRefCount *, uint32, float) override { std::abort(); }
		ATSoundId AddLoopingSound(IATAudioSoundGroup&, uint32, IATAudioSampleSource *, IVDRefCount *, float) override { std::abort(); }
		ATSoundId AddSound(IATAudioSoundGroup&, uint32, IATAudioSampleHandle&, const ATSoundParams&) override { std::abort(); }

		vdrefptr<IATAudioSoundGroup> CreateGroup(const ATAudioGroupDesc& desc) override {
			mGroupDesc = desc;
			++mGroupsCreated;
			return vdrefptr<IATAudioSoundGroup>(new TestSoundGroup);
		}

		void ForceStopSound(ATSoundId) override { std::abort(); }
		void StopSound(ATSoundId id) override { mStoppedSounds.push_back(id); }
		void StopSound(ATSoundId id, uint64 time) override { mTimedStops.push_back({ id, time }); }
		vdrefptr<IATSyncAudioConvolutionPlayer> CreateConvolutionPlayer(ATAudioSampleId) override { std::abort(); }
		vdrefptr<IATSyncAudioConvolutionPlayer> CreateConvolutionPlayer(const sint16 *, uint32) override { std::abort(); }

		ATSoundId RecordSound(bool looping, uint32 delay, ATAudioSampleId sampleId, float volume) {
			const ATSoundId id = static_cast<ATSoundId>(++mNextSoundId);
			mSounds.push_back({ looping, delay, sampleId, volume, id });
			return id;
		}

		uint32 mNextSoundId = 0;
		uint32 mGroupsCreated = 0;
		ATAudioGroupDesc mGroupDesc {};
		std::vector<SoundCall> mSounds;
		std::vector<ATSoundId> mStoppedSounds;
		std::vector<TimedStop> mTimedStops;
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
		IATSyncAudioSamplePlayer& GetSamplePlayer() override { return mSamplePlayer; }
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
		RecordingSamplePlayer mSamplePlayer;
		RecordingEdgePlayer mEdgePlayer;
	};
}

bool ATTestAltirraPrinterBase(ATPortableTestContext& context) {
	ATScheduler scheduler;
	scheduler.SetRate(VDFraction(1000, 1));

	TestAudioMixer mixer;
	ATPrinterSoundSource source;
	AT_PORTABLE_TEST_ASSERT(context, !source.RequiresStereoMixingNow());

	// Pin events queued before initialization are emitted as edge pulses. Duplicate
	// events at the same time are deliberately suppressed.
	source.AddPinSound(100, 2);
	source.AddPinSound(100, 4);
	source.AddPinSound(130, 3);
	source.Init(mixer, scheduler, "printer-test");
	AT_PORTABLE_TEST_ASSERT(context, mixer.mSource == &source);
	AT_PORTABLE_TEST_ASSERT(context, mixer.mAdds == 1);
	AT_PORTABLE_TEST_ASSERT(context, mixer.mSamplePlayer.mGroupsCreated == 1);
	AT_PORTABLE_TEST_ASSERT(context, mixer.mSamplePlayer.mGroupDesc.mAudioMix == kATAudioMix_Other);
	AT_PORTABLE_TEST_ASSERT(context, !mixer.mSamplePlayer.mGroupDesc.mbRemoveSupercededSounds);

	ATSyncAudioMixInfo mixInfo {};
	mixInfo.mStartTime = 200;
	mixInfo.mNumCycles = 50;
	source.WriteAudio(mixInfo);
	AT_PORTABLE_TEST_ASSERT(context, mixer.mEdgePlayer.mBatches.size() == 1);
	const auto& firstBatch = mixer.mEdgePlayer.mBatches[0];
	AT_PORTABLE_TEST_ASSERT(context, firstBatch.mLeftVolume == 0.5f);
	AT_PORTABLE_TEST_ASSERT(context, firstBatch.mRightVolume == 0.5f);
	AT_PORTABLE_TEST_ASSERT(context, firstBatch.mDebugLabel == "printer-test");
	AT_PORTABLE_TEST_ASSERT(context, firstBatch.mEdges.size() == 1);
	AT_PORTABLE_TEST_ASSERT(context, firstBatch.mEdges[0].mTime == 228);
	AT_PORTABLE_TEST_ASSERT(context, firstBatch.mEdges[0].mDeltaValue == 2.0f);

	mixInfo.mStartTime = 250;
	mixInfo.mNumCycles = 400;
	source.WriteAudio(mixInfo);
	AT_PORTABLE_TEST_ASSERT(context, mixer.mEdgePlayer.mBatches.size() == 2);
	const auto& secondBatch = mixer.mEdgePlayer.mBatches[1];
	AT_PORTABLE_TEST_ASSERT(context, secondBatch.mEdges.size() == 3);
	AT_PORTABLE_TEST_ASSERT(context, secondBatch.mEdges[0].mTime == 528);
	AT_PORTABLE_TEST_ASSERT(context, secondBatch.mEdges[0].mDeltaValue == -2.0f);
	AT_PORTABLE_TEST_ASSERT(context, secondBatch.mEdges[1].mTime == 258);
	AT_PORTABLE_TEST_ASSERT(context, secondBatch.mEdges[1].mDeltaValue == 3.0f);
	AT_PORTABLE_TEST_ASSERT(context, secondBatch.mEdges[2].mTime == 558);
	AT_PORTABLE_TEST_ASSERT(context, secondBatch.mEdges[2].mDeltaValue == -3.0f);

	const uint32 now = scheduler.GetTick();
	source.AddPinSound(now + 10, 4);
	AT_PORTABLE_TEST_ASSERT(context, mixer.mSamplePlayer.mSounds.size() == 1);
	AT_PORTABLE_TEST_ASSERT(context, mixer.mSamplePlayer.mSounds[0].mDelay == 138);
	AT_PORTABLE_TEST_ASSERT(context, mixer.mSamplePlayer.mSounds[0].mSampleId == kATAudioSampleId_Printer1029Pin);

	source.ScheduleSound(kATAudioSampleId_Printer1029Home, false, 0.25f, 0, 0.5f);
	AT_PORTABLE_TEST_ASSERT(context, mixer.mSamplePlayer.mSounds.size() == 2);
	AT_PORTABLE_TEST_ASSERT(context, mixer.mSamplePlayer.mSounds[1].mDelay == 250);
	AT_PORTABLE_TEST_ASSERT(context, !mixer.mSamplePlayer.mSounds[1].mbLooping);
	AT_PORTABLE_TEST_ASSERT(context, mixer.mSamplePlayer.mSounds[1].mVolume == 0.5f);

	source.ScheduleSound(kATAudioSampleId_Printer1029Platen, true, 0.1f, 0.5f, 0.75f);
	AT_PORTABLE_TEST_ASSERT(context, mixer.mSamplePlayer.mSounds.size() == 3);
	const auto scheduledLoopId = mixer.mSamplePlayer.mSounds[2].mSoundId;
	AT_PORTABLE_TEST_ASSERT(context, mixer.mSamplePlayer.mSounds[2].mbLooping);
	AT_PORTABLE_TEST_ASSERT(context, mixer.mSamplePlayer.mSounds[2].mDelay == 100);
	AT_PORTABLE_TEST_ASSERT(context, mixer.mSamplePlayer.mTimedStops.size() == 1);
	AT_PORTABLE_TEST_ASSERT(context, mixer.mSamplePlayer.mTimedStops[0].mSoundId == scheduledLoopId);
	AT_PORTABLE_TEST_ASSERT(context, mixer.mSamplePlayer.mTimedStops[0].mTime == scheduler.GetTick64() + 500);

	// A looping request too short to occupy one scheduler cycle is ignored.
	source.ScheduleSound(kATAudioSampleId_Printer1029Platen, true, 0, 0.0001f, 1.0f);
	AT_PORTABLE_TEST_ASSERT(context, mixer.mSamplePlayer.mSounds.size() == 3);

	source.EnablePlatenSound(true);
	source.EnablePlatenSound(true);
	AT_PORTABLE_TEST_ASSERT(context, mixer.mSamplePlayer.mSounds.size() == 4);
	const auto platenId = mixer.mSamplePlayer.mSounds.back().mSoundId;
	source.EnablePlatenSound(false);
	source.EnablePlatenSound(false);
	AT_PORTABLE_TEST_ASSERT(context, mixer.mSamplePlayer.mStoppedSounds.size() == 1);
	AT_PORTABLE_TEST_ASSERT(context, mixer.mSamplePlayer.mStoppedSounds[0] == platenId);

	source.EnableRetractSound(true);
	AT_PORTABLE_TEST_ASSERT(context, mixer.mSamplePlayer.mSounds.size() == 5);
	const auto retractId = mixer.mSamplePlayer.mSounds.back().mSoundId;
	AT_PORTABLE_TEST_ASSERT(context, mixer.mSamplePlayer.mSounds.back().mSampleId == kATAudioSampleId_Printer1029Retract);
	source.EnableRetractSound(false);
	AT_PORTABLE_TEST_ASSERT(context, mixer.mSamplePlayer.mStoppedSounds.size() == 2);
	AT_PORTABLE_TEST_ASSERT(context, mixer.mSamplePlayer.mStoppedSounds[1] == retractId);

	source.PlayHomeSound();
	AT_PORTABLE_TEST_ASSERT(context, mixer.mSamplePlayer.mSounds.size() == 6);
	AT_PORTABLE_TEST_ASSERT(context, mixer.mSamplePlayer.mSounds.back().mSampleId == kATAudioSampleId_Printer1029Home);

	source.Shutdown();
	source.Shutdown();
	AT_PORTABLE_TEST_ASSERT(context, mixer.mSource == nullptr);
	AT_PORTABLE_TEST_ASSERT(context, mixer.mRemoves == 1);
	return true;
}
