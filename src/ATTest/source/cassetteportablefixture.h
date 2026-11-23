// Real tape image, POKEY, PIA, memory and clocks. SIO control/audio boundary probes only.
#ifndef f_ATTEST_CASSETTEPORTABLEFIXTURE_H
#define f_ATTEST_CASSETTEPORTABLEFIXTURE_H
#include <algorithm>
#include <array>
#include <cstdlib>
#include <memory>
#include <set>
#include <vector>
#include <at/atcore/audiomixer.h>
#include <cassette.h>
#include <memorymanager.h>
#include "portmanagerportablefixture.h"

class CassetteMixerProbe final : public IATAudioMixer {
public:
	void AddSyncAudioSource(IATSyncAudioSource *p) override { if (mpSource) std::abort(); mpSource = p; ++mAdds; }
	void RemoveSyncAudioSource(IATSyncAudioSource *p) override { if (mpSource != p) std::abort(); mpSource = nullptr; ++mRemoves; }
	void AddAsyncAudioSource(IATAudioAsyncSource&) override { std::abort(); }
	void RemoveAsyncAudioSource(IATAudioAsyncSource&) override { std::abort(); }
	IATSyncAudioSamplePlayer& GetSamplePlayer() override { std::abort(); }
	IATSyncAudioSamplePlayer& GetEdgeSamplePlayer() override { std::abort(); }
	IATSyncAudioEdgePlayer& GetEdgePlayer() override { std::abort(); }
	IATSyncAudioSamplePlayer& GetAsyncSamplePlayer() override { std::abort(); }
	void AddInternalAudioTap(IATInternalAudioTap *) override { std::abort(); }
	void RemoveInternalAudioTap(IATInternalAudioTap *) override { std::abort(); }
	void BlockInternalAudio() override { std::abort(); }
	void UnblockInternalAudio() override { std::abort(); }
	IATSyncAudioSource *mpSource = nullptr;
	uint32 mAdds = 0, mRemoves = 0;
};

class CassetteSIOProbe final : public IATDeviceSIOManager {
public:
	explicit CassetteSIOProbe(PortManagerPortableFixture& chips) : mChips(chips) {}
	vdrefptr<IATDeviceSIOInterface> AddDevice(IATDeviceSIO *) override { std::abort(); }
	sint32 GetHighSpeedIndex() const override { std::abort(); }
	uint32 GetCyclesPerBitRecv() const override { std::abort(); }
	uint32 GetRecvResetCounter() const override { std::abort(); }
	uint32 GetCyclesPerBitSend() const override { std::abort(); }
	uint32 GetCyclesPerBitBiClock() const override { std::abort(); }
	void AddRawDevice(IATDeviceRawSIO *p) override { if (!mDevices.insert(p).second) std::abort(); ++mAdds; }
	void RemoveRawDevice(IATDeviceRawSIO *p) override { SetSIOInterrupt(p, false); SetSIOProceed(p, false); if (!mDevices.erase(p)) std::abort(); ++mRemoves; }
	void SendRawByte(uint8, uint32, bool, bool, bool) override { std::abort(); }
	void SetRawInput(bool) override { std::abort(); }
	bool IsSIOCommandAsserted() const override { return mbCommand; }
	bool IsSIOMotorAsserted() const override { return !(mChips.mPIA.GetOutputState() & kATPIAOutput_CA2); }
	bool IsSIOReadyAsserted() const override { return true; }
	bool IsSIOForceBreakAsserted() const override { return mChips.mPOKEY.IsSerialForceBreakEnabled(); }
	void SetSIOInterrupt(IATDeviceRawSIO *, bool state) override { mbInterrupt = state; mChips.mPIA.SetCB1(!state); }
	void SetSIOProceed(IATDeviceRawSIO *, bool state) override { mbProceed = state; mChips.mPIA.SetCA1(!state); }
	void SetBiClockNotifyEnabled(IATDeviceRawSIO *, bool) override { std::abort(); }
	void SetExternalClock(IATDeviceRawSIO *, uint32, uint32) override { std::abort(); }
	void MotorChanged() { for (auto *p : mDevices) p->OnMotorStateChanged(IsSIOMotorAsserted()); }
	void Command(bool asserted) { mbCommand = asserted; for (auto *p : mDevices) p->OnCommandStateChanged(asserted); }
	PortManagerPortableFixture& mChips;
	std::set<IATDeviceRawSIO *> mDevices;
	uint32 mAdds = 0, mRemoves = 0;
	bool mbCommand = false, mbProceed = false, mbInterrupt = false;
};

class CassettePortableFixture {
public:
	CassettePortableFixture() : mSIO(mChips) {
		mSlowScheduler.SetRate(VDFraction(17897725, 1140));
		mChips.mPIA.WriteByte(2, 0x3C); // CA2 high: conventional motor off
		mMemory.Init();
		mpRAMLayer = mMemory.CreateLayer(kATMemoryPri_BaseRAM, mRAM.data(), 0, 256, false);
		mMemory.EnableLayer(mpRAMLayer, true);
		ATMemoryHandlerTable handlers {};
		handlers.mpThis = &mChips.mPIA;
		handlers.mpDebugReadHandler = [](void *p, uint32 a) -> sint32 { return static_cast<ATPIAEmulator *>(p)->DebugReadByte(a & 3); };
		handlers.mpReadHandler = [](void *p, uint32 a) -> sint32 { return static_cast<ATPIAEmulator *>(p)->ReadByte(a & 3); };
		handlers.mpWriteHandler = [](void *p, uint32 a, uint8 v) { static_cast<ATPIAEmulator *>(p)->WriteByte(a & 3, v); return true; };
		mpPIALayer = mMemory.CreateLayer(kATMemoryPri_Hardware, handlers, 0xD3, 1);
		mMemory.EnableLayer(mpPIALayer, true);
		mPIAOutput = mChips.mPIA.AllocOutput([](void *p, uint32) { static_cast<CassetteSIOProbe *>(p)->MotorChanged(); }, &mSIO, kATPIAOutput_CA2);
		Attach();
	}
	~CassettePortableFixture() {
		mChips.mPOKEY.SetCassette(nullptr);
		mTape.reset();
		mDeferred.FlushPendingEvents();
		mChips.mPIA.FreeOutput(mPIAOutput);
		mMemory.DeleteLayer(mpPIALayer); mMemory.DeleteLayer(mpRAMLayer);
	}
	void Attach() {
		mTape = std::make_unique<ATCassetteEmulator>();
		mTape->Init(&mChips.mPOKEY, &mChips.mVideo.mScheduler, &mSlowScheduler, &mMixer, &mDeferred, &mSIO, &mChips.mPorts);
		mChips.mPOKEY.SetCassette(mTape.get());
	}
	void Tick(uint32 cycles) {
		while (cycles--) {
			mChips.mVideo.Tick();
			if (++mSlowPhase == 114) {
				mSlowPhase = 0;
				mChips.mPOKEY.AdvanceScanLine();
				ATSCHEDULER_ADVANCE(&mSlowScheduler);
			}
		}
	}
	void Motor(bool on) { mChips.mPIA.WriteByte(2, on ? 0x34 : 0x3C); }
	PortManagerPortableFixture mChips;
	ATScheduler mSlowScheduler;
	ATDeferredEventManager mDeferred;
	CassetteMixerProbe mMixer;
	CassetteSIOProbe mSIO;
	ATMemoryManager mMemory;
	alignas(2) std::array<uint8, 65536> mRAM {};
	std::unique_ptr<ATCassetteEmulator> mTape;
	ATMemoryLayer *mpRAMLayer = nullptr, *mpPIALayer = nullptr;
	int mPIAOutput = -1;
	uint32 mSlowPhase = 0;
};
#endif
