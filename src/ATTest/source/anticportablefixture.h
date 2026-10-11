// Shared real ANTIC/GTIA timing fixture for portable and native-frame tests.

#ifndef f_ATTEST_ANTICPORTABLEFIXTURE_H
#define f_ATTEST_ANTICPORTABLEFIXTURE_H

#include <array>
#include <cstdint>
#include <initializer_list>
#include <vector>

#include <at/atcore/scheduler.h>
#include <vd2/system/VDString.h>
#include <antic.h>
#include <gtia.h>
#include <simeventmanager.h>

class ANTICPortableFixture final : public ATAnticEmulatorConnections, public IATGTIAEmulatorConnections {
public:
	enum class CallbackType {
		NMI_DLI,
		NMI_VBI,
		NMI_RES,
		RDYAssert,
		RDYNegate,
		EndFrame,
		EndScanline,
		VBlank
	};

	struct CallbackRecord {
		CallbackType mType;
		uint32 mTime;
		uint32 mBeamX;
		uint32 mBeamY;
	};

	struct BusReadRecord {
		uint16 mAddress;
		uint8 mValue;
		uint32 mTime;
		uint32 mBeamX;
		uint32 mBeamY;
	};

	ANTICPortableFixture();
	~ANTICPortableFixture();

	ANTICPortableFixture(const ANTICPortableFixture&) = delete;
	ANTICPortableFixture& operator=(const ANTICPortableFixture&) = delete;

	void Tick();
	void Tick(uint32 cycles);
	bool RunToBeam(uint32 y, uint32 x, uint32 maxCycles = 40000);
	void WriteANTIC(uint8 reg, uint8 value) { mAntic.WriteByte(reg, value); }
	void SetDisplayList(uint16 address);
	void WriteDisplayList(uint16 address, std::initializer_list<uint8> bytes);
	void AttachVideoDisplay(IVDVideoDisplay *display) { mGTIA.SetVideoOutput(display); }
	bool BeginVideoFrame(bool force = true, bool drop = false);

	uint8 AnticReadByte(uint32 address) override;
	void AnticAssertNMI_DLI() override;
	void AnticAssertNMI_VBI() override;
	void AnticAssertNMI_RES() override;
	void AnticAssertRDY() override;
	void AnticNegateRDY() override;
	void AnticEndFrame() override;
	void AnticEndScanline() override;
	bool AnticIsNextCPUCycleWrite() override { return mbNextCPUCycleWrite; }
	uint8 AnticGetCPUHeldCycleValue() override { return mBusValue; }
	void AnticForceNextCPUCycleSlow() override { ++mForcedSlowCycles; }
	void AnticOnVBlank() override;

	uint32 GTIAGetXClock() override { return mAntic.GetBeamX() * 2; }
	uint32 GTIAGetTimestamp() const override { return ATSCHEDULER_GETTIME(&mScheduler); }
	uint64 GTIAGetTimestamp64() const override { return mScheduler.GetTick64(); }
	void GTIASetSpeaker(bool state) override { mbSpeakerOn = state; }
	void GTIASelectController(uint8 index, bool potsEnabled) override {
		mControllerIndex = index;
		mbPotsEnabled = potsEnabled;
	}
	void GTIARequestAnticSync(int offset) override { mAntic.SyncWithGTIA(offset); }
	uint32 GTIAGetLineEdgeTimingId(uint32 offset) const override;

	ATScheduler mScheduler;
	ATSimulatorEventManager mEventManager;
	std::array<uint8, 65536> mRAM {};
	uint8 mBusValue = 0xFF;
	ATGTIAEmulator mGTIA;
	ATAnticEmulator mAntic;
	std::vector<CallbackRecord> mCallbacks;
	std::vector<BusReadRecord> mBusReads;
	uint32 mForcedSlowCycles = 0;
	uint8 mControllerIndex = 0xFF;
	bool mbPotsEnabled = false;
	bool mbSpeakerOn = false;
	bool mbNextCPUCycleWrite = false;

private:
	void RecordCallback(CallbackType type);

	std::array<uintptr, 256> mReadPageMap {};
};

inline ANTICPortableFixture::ANTICPortableFixture() {
	mScheduler.SetRate(VDFraction(17897725, 10));
	mReadPageMap.fill(1); // route the real fast-fetch contract through our timed bus callback
	mpAnticReadPageMap = mReadPageMap.data();
	mGTIA.Init(this);
	mAntic.Init(this, &mGTIA, &mScheduler, &mEventManager, &mBusValue);
	mAntic.ColdReset();
}

inline ANTICPortableFixture::~ANTICPortableFixture() {
	mAntic.Shutdown();
}

inline void ANTICPortableFixture::Tick() {
	ATSCHEDULER_ADVANCE(&mScheduler);
	const uint8 fetchMode = mAntic.PreAdvance();
	mAntic.PostAdvance(fetchMode);
}

inline void ANTICPortableFixture::Tick(uint32 cycles) {
	while(cycles--)
		Tick();
}

inline bool ANTICPortableFixture::RunToBeam(uint32 y, uint32 x, uint32 maxCycles) {
	while(maxCycles-- && (mAntic.GetBeamY() != y || mAntic.GetBeamX() != x))
		Tick();
	return mAntic.GetBeamY() == y && mAntic.GetBeamX() == x;
}

inline void ANTICPortableFixture::SetDisplayList(uint16 address) {
	mAntic.WriteByte(0x02, static_cast<uint8>(address));
	mAntic.WriteByte(0x03, static_cast<uint8>(address >> 8));
}

inline void ANTICPortableFixture::WriteDisplayList(uint16 address, std::initializer_list<uint8> bytes) {
	uint16 p = address;
	for(uint8 byte : bytes)
		mRAM[p++] = byte;
	SetDisplayList(address);
}

inline bool ANTICPortableFixture::BeginVideoFrame(bool force, bool drop) {
	return mGTIA.BeginFrame(mAntic.GetRawFrameCounter(), mAntic.GetBeamY(), force, drop);
}

inline uint8 ANTICPortableFixture::AnticReadByte(uint32 address) {
	const uint16 busAddress = static_cast<uint16>(address);
	const uint8 value = mRAM[busAddress];
	mBusValue = value;
	mBusReads.push_back({ busAddress, value, ATSCHEDULER_GETTIME(&mScheduler), mAntic.GetBeamX(), mAntic.GetBeamY() });
	return value;
}

inline void ANTICPortableFixture::RecordCallback(CallbackType type) {
	mCallbacks.push_back({ type, ATSCHEDULER_GETTIME(&mScheduler), mAntic.GetBeamX(), mAntic.GetBeamY() });
}

inline void ANTICPortableFixture::AnticAssertNMI_DLI() { RecordCallback(CallbackType::NMI_DLI); }
inline void ANTICPortableFixture::AnticAssertNMI_VBI() { RecordCallback(CallbackType::NMI_VBI); }
inline void ANTICPortableFixture::AnticAssertNMI_RES() { RecordCallback(CallbackType::NMI_RES); }
inline void ANTICPortableFixture::AnticAssertRDY() { RecordCallback(CallbackType::RDYAssert); }
inline void ANTICPortableFixture::AnticNegateRDY() { RecordCallback(CallbackType::RDYNegate); }
inline void ANTICPortableFixture::AnticEndFrame() { RecordCallback(CallbackType::EndFrame); }
inline void ANTICPortableFixture::AnticEndScanline() { RecordCallback(CallbackType::EndScanline); }
inline void ANTICPortableFixture::AnticOnVBlank() { RecordCallback(CallbackType::VBlank); }

inline uint32 ANTICPortableFixture::GTIAGetLineEdgeTimingId(uint32 offset) const {
	const uint32 beamX = mAntic.GetBeamX();
	uint32 timingId = GTIAGetTimestamp() - beamX;
	if (beamX >= offset)
		timingId += 114;
	return timingId;
}

#endif
