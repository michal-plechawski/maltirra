// Altirra portable Atari 1020 plotter tests

#include <cmath>
#include <cstdlib>
#include <initializer_list>

#include <at/atcore/device.h>
#include <at/atcore/propertyset.h>
#include <at/atcore/scheduler.h>
#include <at/attest/portabletest.h>
#include <printer1020.h>
#include <printeroutput.h>

namespace {
	bool Near(float a, float b) {
		return std::fabs(a - b) < 0.0001f;
	}

	class QuietSoundGroup final : public vdrefcounted<IATAudioSoundGroup> {
	public:
		bool IsAnySoundQueued() const override { return false; }
		void StopAllSounds() override {}
	};

	class QuietSamplePlayer final : public IATSyncAudioSamplePlayer {
	public:
		IATSyncAudioSource& AsSource() override { std::abort(); }
		vdrefptr<IATAudioSampleHandle> RegisterSample(vdspan<const sint16>, const ATAudioSoundSamplingRate&, float) override { std::abort(); }
		ATSoundId AddSound(IATAudioSoundGroup&, uint32, ATAudioSampleId, float) override { std::abort(); }
		ATSoundId AddLoopingSound(IATAudioSoundGroup&, uint32, ATAudioSampleId, float) override { std::abort(); }
		ATSoundId AddSound(IATAudioSoundGroup&, uint32, IATAudioSampleSource *, IVDRefCount *, uint32, float) override { std::abort(); }
		ATSoundId AddLoopingSound(IATAudioSoundGroup&, uint32, IATAudioSampleSource *, IVDRefCount *, float) override { std::abort(); }
		ATSoundId AddSound(IATAudioSoundGroup&, uint32, IATAudioSampleHandle&, const ATSoundParams&) override { std::abort(); }
		vdrefptr<IATAudioSoundGroup> CreateGroup(const ATAudioGroupDesc&) override {
			return vdrefptr<IATAudioSoundGroup>(new QuietSoundGroup);
		}
		void ForceStopSound(ATSoundId) override { std::abort(); }
		void StopSound(ATSoundId) override { std::abort(); }
		void StopSound(ATSoundId, uint64) override { std::abort(); }
		vdrefptr<IATSyncAudioConvolutionPlayer> CreateConvolutionPlayer(ATAudioSampleId) override { std::abort(); }
		vdrefptr<IATSyncAudioConvolutionPlayer> CreateConvolutionPlayer(const sint16 *, uint32) override { std::abort(); }
	};

	class QuietAudioMixer final : public IATAudioMixer {
	public:
		void AddSyncAudioSource(IATSyncAudioSource *source) override { mpSource = source; }
		void RemoveSyncAudioSource(IATSyncAudioSource *source) override {
			if (mpSource != source)
				std::abort();

			mpSource = nullptr;
		}
		void AddAsyncAudioSource(IATAudioAsyncSource&) override { std::abort(); }
		void RemoveAsyncAudioSource(IATAudioAsyncSource&) override { std::abort(); }
		IATSyncAudioSamplePlayer& GetSamplePlayer() override { return mSamplePlayer; }
		IATSyncAudioSamplePlayer& GetEdgeSamplePlayer() override { std::abort(); }
		IATSyncAudioEdgePlayer& GetEdgePlayer() override { std::abort(); }
		IATSyncAudioSamplePlayer& GetAsyncSamplePlayer() override { std::abort(); }
		void AddInternalAudioTap(IATInternalAudioTap *) override { std::abort(); }
		void RemoveInternalAudioTap(IATInternalAudioTap *) override { std::abort(); }
		void BlockInternalAudio() override { std::abort(); }
		void UnblockInternalAudio() override { std::abort(); }

		IATSyncAudioSource *mpSource = nullptr;
		QuietSamplePlayer mSamplePlayer;
	};

	class SchedulingService final : public IATDeviceSchedulingService {
	public:
		explicit SchedulingService(ATScheduler& scheduler) : mScheduler(scheduler) {}
		ATScheduler *GetMachineScheduler() const override { return &mScheduler; }
		ATScheduler *GetSlowScheduler() const override { return &mScheduler; }

		ATScheduler& mScheduler;
	};

	class PlotterDeviceManager final : public IATDeviceManager {
	public:
		PlotterDeviceManager(IATDeviceSchedulingService& schedulingService,
			IATAudioMixer& audioMixer, IATPrinterOutputManager& outputManager)
			: mSchedulingService(schedulingService)
			, mAudioMixer(audioMixer)
			, mOutputManager(outputManager)
		{
		}

		void *GetService(uint32 iid) override {
			if (iid == IATDeviceSchedulingService::kTypeID)
				return &mSchedulingService;
			if (iid == IATAudioMixer::kTypeID)
				return &mAudioMixer;
			if (iid == IATPrinterOutputManager::kTypeID)
				return &mOutputManager;

			return nullptr;
		}

		void NotifyDeviceStatusChanged(IATDevice&) override {}

		IATDeviceSchedulingService& mSchedulingService;
		IATAudioMixer& mAudioMixer;
		IATPrinterOutputManager& mOutputManager;
	};
}

bool ATTestAltirraPrinter1020(ATPortableTestContext& context) {
	ATDevicePrinter1020 plotter;
	AT_PORTABLE_TEST_ASSERT(context, plotter.IsSupportedDeviceId(0x40));
	AT_PORTABLE_TEST_ASSERT(context, plotter.IsSupportedDeviceId(0x43));
	AT_PORTABLE_TEST_ASSERT(context, !plotter.IsSupportedDeviceId(0x41));
	AT_PORTABLE_TEST_ASSERT(context, plotter.IsSupportedOrientation(0));
	AT_PORTABLE_TEST_ASSERT(context, plotter.GetWidthForOrientation(0) == 40);

	const ATPrinterGraphicsSpec spec = plotter.GetGraphicsSpec();
	AT_PORTABLE_TEST_ASSERT(context, Near(spec.mPageWidthMM, 114.5f));
	AT_PORTABLE_TEST_ASSERT(context, Near(spec.mPageVBorderMM, 8.0f));
	AT_PORTABLE_TEST_ASSERT(context, Near(spec.mDotRadiusMM, 0.09f));
	AT_PORTABLE_TEST_ASSERT(context, spec.mNumPins == 9);

	ATPropertySet settings;
	settings.SetUint32("pencolor1", 0xFF123456);
	AT_PORTABLE_TEST_ASSERT(context, plotter.SetSettings(settings));
	settings.Clear();
	plotter.GetSettings(settings);
	AT_PORTABLE_TEST_ASSERT(context, settings.GetUint32("pencolor1") == 0x123456);

	ATScheduler scheduler;
	scheduler.SetRate(VDFraction(1000, 1));
	SchedulingService schedulingService(scheduler);
	QuietAudioMixer audioMixer;
	ATPrinterOutputManager outputManager;
	PlotterDeviceManager deviceManager(schedulingService, audioMixer, outputManager);
	plotter.SetManager(&deviceManager);
	plotter.Init();
	AT_PORTABLE_TEST_ASSERT(context, audioMixer.mpSource != nullptr);
	AT_PORTABLE_TEST_ASSERT(context, outputManager.GetGraphicalOutputCount() == 1);

	auto send = [&](std::initializer_list<uint8> bytes) {
		vdfastvector<uint8> buffer(bytes.begin(), bytes.end());
		plotter.HandleFrameInternal(0, buffer.data(), (uint32)buffer.size(), true);
	};

	// Enter graphics mode, select pen 1, move to (10,20), and draw to (30,40).
	send({ 0x1B, 0x07 });
	send({ 'C', '1', 0x9B });
	send({ 'M', '1', '0', ',', '2', '0', 0x9B });
	send({ 'D', '3', '0', ',', '4', '0', 0x9B });

	ATPrinterGraphicalOutput& output = outputManager.GetGraphicalOutput(0);
	vdfastvector<ATPrinterGraphicalOutput::RenderVector> vectors;
	output.ExtractVectors(vectors, vdrect32f(0, -20, 120, 20));
	AT_PORTABLE_TEST_ASSERT(context, vectors.size() == 1);
	AT_PORTABLE_TEST_ASSERT(context, Near(vectors[0].mX1, 16.0f));
	AT_PORTABLE_TEST_ASSERT(context, Near(vectors[0].mY1, 2.0f));
	AT_PORTABLE_TEST_ASSERT(context, Near(vectors[0].mX2, 12.0f));
	AT_PORTABLE_TEST_ASSERT(context, Near(vectors[0].mY2, 6.0f));
	AT_PORTABLE_TEST_ASSERT(context,
		vectors[0].mLinearColor == output.ConvertColor(0x123456));

	// Clearing paper resets the plotter origin through the output callback.
	output.Clear();
	send({ 'D', '1', '0', ',', '0', 0x9B });
	vectors.clear();
	output.ExtractVectors(vectors, vdrect32f(0, 0, 120, 20));
	AT_PORTABLE_TEST_ASSERT(context, vectors.size() == 1);
	AT_PORTABLE_TEST_ASSERT(context, Near(vectors[0].mX1, 10.0f));
	AT_PORTABLE_TEST_ASSERT(context, Near(vectors[0].mY1, 10.0f));
	AT_PORTABLE_TEST_ASSERT(context, Near(vectors[0].mX2, 12.0f));
	AT_PORTABLE_TEST_ASSERT(context, Near(vectors[0].mY2, 10.0f));

	plotter.Shutdown();
	AT_PORTABLE_TEST_ASSERT(context, audioMixer.mpSource == nullptr);
	AT_PORTABLE_TEST_ASSERT(context, outputManager.GetGraphicalOutputCount() == 0);
	return true;
}
