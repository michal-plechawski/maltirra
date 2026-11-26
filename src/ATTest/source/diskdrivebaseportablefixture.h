// Production schedulers/6502 core with observable SIO pins and host audio.
#ifndef f_ATTEST_DISKDRIVEBASEPORTABLEFIXTURE_H
#define f_ATTEST_DISKDRIVEBASEPORTABLEFIXTURE_H
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <memory>
#include <vector>
#include <vd2/system/vdtypes.h>
#include <at/atcore/audiosource.h>
#include <at/atcore/devicesio.h>
#include <at/atcpu/co6502.h>
#include <at/atcpu/execstate.h>
#include <at/atcpu/memorymap.h>
#include <at/ataudio/audiosampleplayer.h>
#include <at/ataudio/audiosamplepool.h>
#include <at/attest/portabletest.h>
#include <audiostocksamples.h>
#include <diskdrivefullbase.h>
namespace DriveBasePortable {
	inline void Tick(ATScheduler& s,uint32 cycles) { while(cycles--) ATSCHEDULER_ADVANCE(&s); }
	inline void Position(ATScheduler& s,uint32 value) { s.mTimeBase+=value-s.GetTick(); s.UpdateTick64(); }
	struct SIO final : IATDeviceSIOManager {
		struct Byte { uint32 time; uint8 value; uint32 rate; bool sync,framing,simulate; };
		explicit SIO(ATScheduler& s) : scheduler(s) {}
		vdrefptr<IATDeviceSIOInterface> AddDevice(IATDeviceSIO *) override { std::abort(); }
		sint32 GetHighSpeedIndex() const override { return -1; }
		uint32 GetCyclesPerBitRecv() const override { return rate; }
		uint32 GetRecvResetCounter() const override { return resetCounter; }
		uint32 GetCyclesPerBitSend() const override { return rate; }
		uint32 GetCyclesPerBitBiClock() const override { return 0; }
		void AddRawDevice(IATDeviceRawSIO *) override { std::abort(); }
		void RemoveRawDevice(IATDeviceRawSIO *) override { std::abort(); }
		void SendRawByte(uint8 b,uint32 cpb,bool sy,bool fe,bool si) override { bytes.push_back({scheduler.GetTick(),b,cpb,sy,fe,si}); if (onByte) onByte(); }
		void SetRawInput(bool v) override { pins.push_back({scheduler.GetTick(),v}); if (onPin) onPin(); }
		bool IsSIOCommandAsserted() const override { return false; }
		bool IsSIOMotorAsserted() const override { return false; }
		bool IsSIOReadyAsserted() const override { return true; }
		bool IsSIOForceBreakAsserted() const override { return false; }
		void SetSIOInterrupt(IATDeviceRawSIO *,bool) override { std::abort(); }
		void SetSIOProceed(IATDeviceRawSIO *,bool) override { std::abort(); }
		void SetBiClockNotifyEnabled(IATDeviceRawSIO *,bool) override { std::abort(); }
		void SetExternalClock(IATDeviceRawSIO *,uint32,uint32) override { std::abort(); }
		ATScheduler& scheduler; uint32 rate=93,resetCounter=1;
		std::vector<Byte> bytes; std::vector<std::pair<uint32,bool>> pins;
		vdfunction<void()> onByte,onPin;
	};
	inline void Frame(ATDiskDriveSerialBitTransmitQueue& q,uint32 start,uint8 value,uint32 cpb,bool goodStop=true) {
		q.AddTransmitBit(start,false);
		for (uint32 i=0; i<8; ++i) q.AddTransmitBit(start+(i+1)*cpb,(value>>i)&1);
		q.AddTransmitBit(start+9*cpb,goodStop);
		q.AddTransmitBit(start+10*cpb,true);
	}
	struct Mixer final : IATAudioMixer {
		Mixer() : player(pool,scheduler) { ATAudioRegisterStockSamples(pool); player.Init(); player.SetRates(63920.8f,1,1.0/28); }
		~Mixer() { player.Shutdown(); }
		void AddSyncAudioSource(IATSyncAudioSource *) override { std::abort(); }
		void RemoveSyncAudioSource(IATSyncAudioSource *) override { std::abort(); }
		void AddAsyncAudioSource(IATAudioAsyncSource&) override { std::abort(); }
		void RemoveAsyncAudioSource(IATAudioAsyncSource&) override { std::abort(); }
		IATSyncAudioSamplePlayer& GetSamplePlayer() override { return player; }
		IATSyncAudioSamplePlayer& GetEdgeSamplePlayer() override { std::abort(); }
		IATSyncAudioSamplePlayer& GetAsyncSamplePlayer() override { std::abort(); }
		IATSyncAudioEdgePlayer& GetEdgePlayer() override { std::abort(); }
		void AddInternalAudioTap(IATInternalAudioTap *) override { std::abort(); }
		void RemoveInternalAudioTap(IATInternalAudioTap *) override { std::abort(); }
		void BlockInternalAudio() override { std::abort(); }
		void UnblockInternalAudio() override { std::abort(); }
		std::array<float,256> Render() {
			std::array<float,256> out {}; std::array<float,kATAudioMixCount> levels; levels.fill(1);
			ATSyncAudioMixInfo m {}; m.mStartTime=scheduler.GetTick64(); m.mCount=out.size(); m.mNumCycles=28*out.size();
			m.mMixingRate=63920.8f; m.mpLeft=out.data(); m.mpMixLevels=levels.data(); player.WriteAudio(m); Tick(scheduler,m.mNumCycles); return out;
		}
		ATScheduler scheduler; ATAudioSamplePool pool; ATAudioSamplePlayer player;
	};
	inline bool Audible(const std::array<float,256>& b) { return std::any_of(b.begin(),b.end(),[](float v) { return std::abs(v)>1e-7f; }); }
	struct Target final : ATDiskDriveDebugTargetControl {
		struct Proxy final : ATDiskDriveDebugTargetProxyBaseT<ATCoProc6502> {
			explicit Proxy(Target& t) : owner(t) {}
			void SetHistoryBuffer(ATCPUHistoryEntry *p) override { history=p; ATDiskDriveDebugTargetProxyBaseT::SetHistoryBuffer(p); }
			uint32 GetTime() const override { return owner.mDriveScheduler.GetTick(); }
			Target& owner; ATCPUHistoryEntry *history=nullptr;
		};
		Target(uint32 masterHz=1000000,uint32 driveHz=500000) : cpu(true,false),proxy(*this) {
			master.SetRate(VDFraction(masterHz,1)); slow.SetRate(VDFraction(1000,1)); mDriveScheduler.SetRate(VDFraction(driveHz,1));
			ram.fill(0xEA); for (uint32 i=0; i<256; ++i) cpu.GetReadMap()[i]=cpu.GetWriteMap()[i]=(uintptr)ram.data();
			ram[0xFFFC]=0; ram[0xFFFD]=0x20; cpu.ColdReset(); cpu.Jump(0x2000); cpu.SetS(0xFD);
			proxy.Init(cpu);
			bp.BindBPHandler(cpu); bp.SetStepHandler(this); bp.SetBPsChangedHandler([this](const uint16 *pc) { cpu.OnBreakpointsChanged(pc); });
			InitTargetControl(proxy,driveHz,kATDebugDisasmMode_65C02,&bp,nullptr); InitScheduling(&master,&slow); ResetTargetControl();
		}
		~Target() { ShutdownTargetControl(); }
		void Sync() override {
			++syncs; if (paused) return;
			const uint32 limit=AccumSubCycles();
			if ((mDriveScheduler.GetTick()-limit)&0x80000000) { mDriveScheduler.SetStopTime(limit); if (!cpu.Run(mDriveScheduler)) ScheduleImmediateResume(); }
			FlushStepNotifications();
		}
		void Run(uint32 masterCycles) { Tick(master,masterCycles); Sync(); }
		ATScheduler& Drive() { return mDriveScheduler; }
		void Jump(uint16 address) { cpu.Jump(address); }
		ATScheduler master,slow; alignas(2) std::array<uint8,65536> ram;
		ATCoProc6502 cpu; ATDebugTargetBreakpointsImpl bp; Proxy proxy;
		uint32 syncs=0; bool paused=false;
	};
}
#endif
