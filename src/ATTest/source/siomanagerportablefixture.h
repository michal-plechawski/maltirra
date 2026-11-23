// Real CPU, memory, POKEY, PIA, tape and scheduler behind the production SIO manager.
#ifndef f_ATTEST_SIOMANAGERPORTABLEFIXTURE_H
#define f_ATTEST_SIOMANAGERPORTABLEFIXTURE_H
#include <array>
#include <memory>
#include <vector>
#include <vd2/system/vdtypes.h>
#include <at/atcore/ksyms.h>
#include <at/atcore/sioutils.h>
#include <at/atcore/snapshot.h>
#include <cpu.h>
#include <cpuhookmanager.h>
#include <siomanager.h>
#include "cassetteportablefixture.h"

class SIOPortableFixture final : public ATCPUEmulatorCallbacks {
public:
	struct ROMMapping {
		bool IsKernelROMEnabled() const { return mbEnabled; }
		bool IsROMOverlayActive() const { return false; }
		bool mbEnabled = true;
	};
	SIOPortableFixture() {
		auto& chips = mMachine.mChips;
		mMachine.mTape->Shutdown();
		mHooks.Init(&mCPU, &mROM, &mROM);
		mCPU.Init(&mMachine.mMemory, &mHooks, this);
		mCPU.SetCPUMode(kATCPUMode_65C816, 1);
		mCPU.SetCPUMode(kATCPUMode_6502, 1);
		mCPU.SetP(0x20); mCPU.SetPC(0x2000); mCPU.SetS(0xFD); mCPU.SetK(0); mCPU.SetB(0);
		mHooks.EnableOSHooks(true);
		ATMemoryHandlerTable h {};
		h.mpThis = &chips.mPOKEY;
		h.mpDebugReadHandler = [](void *p, uint32 a) -> sint32 { return static_cast<ATPokeyEmulator *>(p)->DebugReadByte(a & 15); };
		h.mpReadHandler = [](void *p, uint32 a) -> sint32 { return static_cast<ATPokeyEmulator *>(p)->ReadByte(a & 15); };
		h.mpWriteHandler = [](void *p, uint32 a, uint8 v) { static_cast<ATPokeyEmulator *>(p)->WriteByte(a & 15, v); return true; };
		mpPokeyLayer = mMachine.mMemory.CreateLayer(kATMemoryPri_Hardware, h, 0xD2, 1);
		mMachine.mMemory.EnableLayer(mpPokeyLayer, true);
		ATSIOManagerConnections c;
		c.mpIsFastBootEnabled = [this] { return mbFastBoot; };
		c.mpIsCassetteSIOPatchEnabled = [this] { return mbCassettePatch; };
		c.mpIsDiskSIOOverrideDetectEnabled = [this] { return mbDetectOverride; };
		c.mpIsDiskDriveEnabled = [this](uint32 i) { return mDiskEnabled.at(i); };
		c.mpGetDiskClientCount = [this](uint32 i) { return mDiskClients.at(i); };
		c.mpPulseStatusFlags = [this](uint32 flags) { mActivity.push_back(flags); };
		mManager.Init(mCPU, chips.mVideo.mScheduler, chips.mPOKEY, chips.mPIA, *mMachine.mTape, std::move(c));
		mManager.SetFrameTime(262 * 114);
		mMachine.mTape->Init(&chips.mPOKEY, &chips.mVideo.mScheduler, &mMachine.mSlowScheduler,
			&mMachine.mMixer, &mMachine.mDeferred, &mManager, &chips.mPorts);
		chips.mPOKEY.SetCassette(mMachine.mTape.get());
		Mem(ATKernelSymbols::BRKKEY, 0xFF);
	}
	~SIOPortableFixture() {
		mMachine.mChips.mPOKEY.SetCassette(nullptr);
		mMachine.mTape->Shutdown();
		mManager.Shutdown();
		mHooks.Shutdown();
		mMachine.mMemory.DeleteLayer(mpPokeyLayer);
	}
	void Tick(uint32 cycles) { mMachine.Tick(cycles); }
	void Mem(uint16 address, uint8 value) { mMachine.mMemory.WriteByte(address, value); }
	uint8 Mem(uint16 address) { return mMachine.mMemory.ReadByte(address); }
	void ReceiveClock() { // linked high-frequency timers 3+4, approximately 19200 baud
		auto& p = mMachine.mChips.mPOKEY;
		p.WriteByte(8, 0x28); p.WriteByte(4, 40); p.WriteByte(6, 0); p.WriteByte(0x0F, 0x13);
		p.WriteByte(0x0E, 0x20); Tick(114);
	}
	void Command(uint8 dev, uint8 cmd = 0x53, uint8 aux1 = 0, uint8 aux2 = 0, bool good = true, bool early = false) {
		auto& p = mMachine.mChips.mPOKEY;
		std::array<uint8, 5> frame {dev, cmd, aux1, aux2, 0};
		frame[4] = ATComputeSIOChecksum(frame.data(), 4) ^ (good ? 0 : 1);
		p.SetCommandLine(true);
		for (size_t i = 0; i < frame.size(); ++i) {
			Tick(1);
			if (early && i == 4) { p.SetCommandLine(false); Tick(1); }
			mManager.PokeyWriteSIO(frame[i], i != 4 || !early, 93, mMachine.mChips.mVideo.mScheduler.GetTick64(), false, false);
		}
		p.SetCommandLine(false);
	}
	ATSIORequest Request(uint8 dev = 0x71, uint8 mode = 0x40, uint16 len = 4) {
		return {dev, 0x52, mode, 1, 0x4000, len, 1, {1, 0}};
	}
	std::vector<uint8> Collect(uint32 cycles) {
		std::vector<uint8> result;
		auto& p = mMachine.mChips.mPOKEY;
		while (cycles--) {
			Tick(1);
			if (!(p.ReadByte(0x0E) & 0x20)) {
				result.push_back(p.ReadByte(0x0D)); p.WriteByte(0x0E, 0); p.WriteByte(0x0E, 0x20);
			}
		}
		return result;
	}
	uint32 CPUGetCycle() override { return mMachine.mChips.mVideo.mScheduler.GetTick(); }
	uint32 CPUGetUnhaltedCycle() override { return CPUGetCycle(); }
	uint32 CPUGetUnhaltedAndRDYCycle() override { return CPUGetCycle(); }
	void CPUGetHistoryTimes(ATCPUHistoryEntry *he) const override {
		he->mCycle = he->mUnhaltedCycle = mMachine.mChips.mVideo.mScheduler.GetTick();
	}
	CassettePortableFixture mMachine;
	ATCPUHookManager mHooks;
	ATCPUEmulator mCPU;
	ROMMapping mROM;
	ATSIOManager mManager;
	ATMemoryLayer *mpPokeyLayer = nullptr;
	std::array<bool, 15> mDiskEnabled {};
	std::array<uint32, 15> mDiskClients {};
	std::vector<uint32> mActivity;
	bool mbFastBoot = false, mbCassettePatch = false, mbDetectOverride = false;
};

class SIOTestDevice final : public IATDeviceSIO {
public:
	void InitSIO(IATDeviceSIOManager *p) override { mInterface = p->AddDevice(this); }
	CmdResponse OnSerialBeginCommand(const ATDeviceSIOCommand& cmd) override {
		mCommands.push_back(cmd); return mpCommand ? mpCommand(cmd) : kCmdResponse_NotHandled;
	}
	CmdResponse OnSerialAccelCommand(const ATDeviceSIORequest& req) override {
		mRequests.push_back(req); return mpAccel ? mpAccel(req) : kCmdResponse_NotHandled;
	}
	void OnSerialAbortCommand() override { ++mAborts; }
	void OnSerialReceiveComplete(uint32 id, const void *data, uint32 len, bool checksumOK) override {
		mReceiveIds.push_back(id); const auto *p = static_cast<const uint8 *>(data);
		mReceived.emplace_back(p, p + len); mChecksums.push_back(checksumOK);
	}
	void OnSerialFence(uint32 id) override { mFences.push_back(id); }
	vdrefptr<IATDeviceSIOInterface> mInterface;
	vdfunction<CmdResponse(const ATDeviceSIOCommand&)> mpCommand;
	vdfunction<CmdResponse(const ATDeviceSIORequest&)> mpAccel;
	std::vector<ATDeviceSIOCommand> mCommands;
	std::vector<ATDeviceSIORequest> mRequests;
	std::vector<std::vector<uint8>> mReceived;
	std::vector<bool> mChecksums;
	std::vector<uint32> mReceiveIds, mFences;
	uint32 mAborts = 0;
};

class SIORawProbe final : public IATDeviceRawSIO {
public:
	void OnCommandStateChanged(bool v) override { mCommands.push_back(v); if (mpCommand) mpCommand(); }
	void OnMotorStateChanged(bool v) override { mMotors.push_back(v); }
	void OnBreakStateChanged(bool v) override { mBreaks.push_back(v); }
	void OnReadyStateChanged(bool v) override { mReady.push_back(v); }
	void OnBeginReceiveByte(uint8 c, bool, uint32) override { mBegun.push_back(c); }
	void OnReceiveByte(uint8 c, bool, uint32) override { mBytes.push_back(c); }
	void OnTruncateByte() override { ++mTruncates; }
	void OnSendReady() override { ++mSendReady; }
	void OnSerialBiClockChanged(uint32 rate) override { mRates.push_back(rate); }
	vdfunction<void()> mpCommand;
	std::vector<bool> mCommands, mMotors, mBreaks, mReady;
	std::vector<uint8> mBegun, mBytes;
	std::vector<uint32> mRates;
	uint32 mTruncates = 0, mSendReady = 0;
};
#endif
