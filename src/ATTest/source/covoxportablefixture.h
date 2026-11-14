// Real Covox bus, scheduler, and edge renderer with a recording audio host.
#ifndef f_ATTEST_COVOXPORTABLEFIXTURE_H
#define f_ATTEST_COVOXPORTABLEFIXTURE_H

#include <array>
#include <cstdlib>
#include <string>
#include <vector>
#include <at/atcore/scheduler.h>
#include <at/ataudio/internal/audioedgeplayer.h>
#include <memorymanager.h>

class CovoxAudioHost final : public IATAudioMixer, public IATSyncAudioEdgePlayer {
public:
	struct Submission {
		std::vector<ATSyncAudioEdge> mEdges;
		float mLeft, mRight;
		std::string mLabel;
	};
	static constexpr uint32 kSamples = 8;
	using Buffer = std::array<float, kSamples + ATSyncAudioEdgePlayer::kTailLength>;
	struct Output { Buffer mLeft {}, mRight {}; };
	void AddSyncAudioSource(IATSyncAudioSource *p) override {
		if (mpSource) std::abort();
		mpSource = p; ++mAdds;
	}
	void RemoveSyncAudioSource(IATSyncAudioSource *p) override {
		if (mpSource != p) std::abort();
		mpSource = nullptr; ++mRemoves;
	}
	void AddAsyncAudioSource(IATAudioAsyncSource&) override { std::abort(); }
	void RemoveAsyncAudioSource(IATAudioAsyncSource&) override { std::abort(); }
	IATSyncAudioSamplePlayer& GetSamplePlayer() override { std::abort(); }
	IATSyncAudioSamplePlayer& GetEdgeSamplePlayer() override { std::abort(); }
	IATSyncAudioSamplePlayer& GetAsyncSamplePlayer() override { std::abort(); }
	IATSyncAudioEdgePlayer& GetEdgePlayer() override { return *this; }
	void AddInternalAudioTap(IATInternalAudioTap *) override { std::abort(); }
	void RemoveInternalAudioTap(IATInternalAudioTap *) override { std::abort(); }
	void BlockInternalAudio() override { std::abort(); }
	void UnblockInternalAudio() override { std::abort(); }
	void AddEdges(const ATSyncAudioEdge *, size_t, float) override { std::abort(); }
	void AddEdgeBuffer(ATSyncAudioEdgeBuffer *p) override {
		mPending.emplace_back(p);
		mPlayer.AddEdgeBuffer(p);
	}
	Output Render(uint32 start, bool stereo = true) {
		if (!mpSource) std::abort();
		ATSyncAudioMixInfo info {};
		info.mStartTime = start;
		info.mNumCycles = kSamples * kATCyclesPerSyncSample;
		mpSource->WriteAudio(info);
		// The edge-player API retains mutable buffers until the mixing phase.
		// Observe them when the production renderer is about to consume them.
		for (const auto& p : mPending)
			mSubmissions.push_back({ {p->mEdges.begin(), p->mEdges.end()}, p->mLeftVolume,
				p->mRightVolume, p->mpDebugLabel ? p->mpDebugLabel : "" });
		mPending.clear();
		Output output;
		mPlayer.RenderEdges(output.mLeft.data(), stereo ? output.mRight.data() : nullptr, kSamples, start);
		return output;
	}
	IATSyncAudioSource *mpSource = nullptr;
	uint32 mAdds = 0, mRemoves = 0;
	std::vector<Submission> mSubmissions;
	ATSyncAudioEdgePlayer mPlayer;
	std::vector<vdrefptr<ATSyncAudioEdgeBuffer>> mPending;
};

class CovoxPortableHost {
public:
	CovoxPortableHost() : mRAM(0x10000, 0xEA) {
		mScheduler.SetRate(VDFraction(17897725, 10));
		mMemory.Init();
		mpRAMLayer = mMemory.CreateLayer(kATMemoryPri_BaseRAM, mRAM.data(), 0, 0x100, false);
		mMemory.EnableLayer(mpRAMLayer, true);
	}
	~CovoxPortableHost() { mMemory.DeleteLayer(mpRAMLayer); }
	void Advance(uint32 cycles) { while (cycles--) ATSCHEDULER_ADVANCE(&mScheduler); }
	uint32 Time() const { return mScheduler.GetTick(); }
	std::vector<uint8> mRAM;
	ATScheduler mScheduler;
	ATMemoryManager mMemory;
	CovoxAudioHost mAudio;
private:
	ATMemoryLayer *mpRAMLayer;
};
#endif
