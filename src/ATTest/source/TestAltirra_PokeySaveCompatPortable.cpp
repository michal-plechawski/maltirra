// Portable conversion tests for legacy binary POKEY save states.

#include <memory>
#include <vector>

#include <at/ataudio/pokey.h>
#include <at/ataudio/pokeysavestate.h>
#include <at/ataudio/pokeytables.h>
#include <at/atcore/scheduler.h>
#include <at/atcore/serialization.h>
#include <at/attest/portabletest.h>
#include <vd2/system/binary.h>

#include <pokeysavecompat.h>
#include <savestate.h>

namespace {
	class PokeySaveTestConnections final : public IATPokeyEmulatorConnections {
	public:
		void PokeyAssertIRQ(bool) override {}
		void PokeyNegateIRQ(bool) override {}
		void PokeyBreak() override {}
		bool PokeyIsInInterrupt() const override { return false; }
		bool PokeyIsKeyPushOK(uint8, bool) const override { return false; }
	};

	void AppendU8(std::vector<uint8>& data, uint8 value) {
		data.push_back(value);
	}

	void AppendU16(std::vector<uint8>& data, uint16 value) {
		const size_t offset = data.size();
		data.resize(offset + 2);
		VDWriteUnalignedLEU16(data.data() + offset, value);
	}

	void AppendU32(std::vector<uint8>& data, uint32 value) {
		const size_t offset = data.size();
		data.resize(offset + 4);
		VDWriteUnalignedLEU32(data.data() + offset, value);
	}

	void AppendU64(std::vector<uint8>& data, uint64 value) {
		const size_t offset = data.size();
		data.resize(offset + 8);
		VDWriteUnalignedLEU64(data.data() + offset, value);
	}
}

bool ATTestAltirraPokeySaveCompat(ATPortableTestContext& context) {
	std::vector<uint8> data;
	for(uint8 channel = 0; channel < 4; ++channel) {
		AppendU8(data, static_cast<uint8>(0x10 + channel));
		AppendU8(data, static_cast<uint8>(0xA0 + channel));
	}
	AppendU8(data, 0x02);       // AUDCTL (9-bit polynomial, no linked timers)
	AppendU8(data, 0x00);       // IRQEN
	AppendU8(data, 0xA5);       // IRQST
	AppendU8(data, 0x03);       // SKCTL

	AppendU8(data, 0x5A);       // ALLPOT
	AppendU8(data, 0x37);       // KBCODE
	AppendU32(data, 0);         // timer counters are clamped to [1, 256]
	AppendU32(data, 1);
	AppendU32(data, 256);
	AppendU32(data, 999);
	AppendU32(data, 0);         // borrow counters retain only two bits
	AppendU32(data, 1);
	AppendU32(data, 4);
	AppendU32(data, 7);
	AppendU8(data, 10);         // 15KHz clock offset
	AppendU8(data, 20);         // 64KHz clock offset
	AppendU16(data, 700);       // polynomial offsets are reduced modulo period
	AppendU32(data, 200000);
	AppendU64(data, UINT64_C(0x100000002));
	AppendU8(data, 17);
	AppendU8(data, 34);
	AppendU16(data, 700);
	AppendU32(data, 200000);
	for(uint8 value : { 0, 1, 2, 3, 255, 4 })
		AppendU8(data, value);
	for(uint8 value : { 9, 8, 7, 6 })
		AppendU8(data, value);    // obsolete output bytes

	PokeySaveTestConnections connections;
	ATScheduler scheduler;
	auto tables = std::make_unique<ATPokeyTables>();
	ATPokeyEmulator pokey(false);
	pokey.Init(&connections, &scheduler, nullptr, tables.get());

	ATSaveStateReader reader(data.data(), data.size());
	ATPokeyEmulatorOldStateLoader loader(pokey);
	loader.BeginLoadState(reader);
	static constexpr uint32 kPokeyChunk = VDMAKEFOURCC('P', 'O', 'K', 'Y');
	reader.DispatchChunk(kATSaveStateSection_Arch, kPokeyChunk);
	reader.DispatchChunk(kATSaveStateSection_Private, kPokeyChunk);
	reader.DispatchChunk(kATSaveStateSection_ResetPrivate, 0);
	reader.DispatchChunk(kATSaveStateSection_End, 0);
	AT_PORTABLE_TEST_ASSERT(context, reader.GetAvailable() == 0);

	vdrefptr<IATObjectState> savedState;
	pokey.SaveState(~savedState);
	AT_PORTABLE_TEST_ASSERT(context, savedState != nullptr);
	const auto& state = atser_cast<const ATSaveStatePokey&>(*savedState);
	for(uint8 channel = 0; channel < 4; ++channel) {
		AT_PORTABLE_TEST_ASSERT(context, state.mAUDF[channel] == 0x10 + channel);
		AT_PORTABLE_TEST_ASSERT(context, state.mAUDC[channel] == 0xA0 + channel);
	}
	AT_PORTABLE_TEST_ASSERT(context, state.mAUDCTL == 0x02);
	AT_PORTABLE_TEST_ASSERT(context, state.mIRQEN == 0);
	AT_PORTABLE_TEST_ASSERT(context, state.mIRQST == 0xA5);
	AT_PORTABLE_TEST_ASSERT(context, state.mSKCTL == 3);
	AT_PORTABLE_TEST_ASSERT(context, state.mKBCODE == 0x37);
	AT_PORTABLE_TEST_ASSERT(context, state.mpInternalState != nullptr);
	const auto& internal = *state.mpInternalState;
	AT_PORTABLE_TEST_ASSERT(context, internal.mTimerCounters[0] == 1);
	// A pending single-cycle borrow is consumed by POKEY timer restoration.
	AT_PORTABLE_TEST_ASSERT(context, internal.mTimerCounters[1] == 256);
	AT_PORTABLE_TEST_ASSERT(context, internal.mTimerCounters[2] == 256);
	AT_PORTABLE_TEST_ASSERT(context, internal.mTimerCounters[3] == 256);
	AT_PORTABLE_TEST_ASSERT(context, internal.mTimerBorrowCounters[0] == 0);
	AT_PORTABLE_TEST_ASSERT(context, internal.mTimerBorrowCounters[1] == 1);
	AT_PORTABLE_TEST_ASSERT(context, internal.mTimerBorrowCounters[2] == 0);
	AT_PORTABLE_TEST_ASSERT(context, internal.mTimerBorrowCounters[3] == 3);
	AT_PORTABLE_TEST_ASSERT(context, internal.mClock15Offset == 10);
	AT_PORTABLE_TEST_ASSERT(context, internal.mClock64Offset == 20);
	AT_PORTABLE_TEST_ASSERT(context, internal.mPoly9Offset == 700 % 511);
	AT_PORTABLE_TEST_ASSERT(context, internal.mPoly17Offset == 200000 % 131071);
	AT_PORTABLE_TEST_ASSERT(context, internal.mPolyShutOffTime == 2);
	AT_PORTABLE_TEST_ASSERT(context, internal.mRendererState.mPoly4Offset == 17 % 15);
	AT_PORTABLE_TEST_ASSERT(context, internal.mRendererState.mPoly5Offset == 34 % 31);
	AT_PORTABLE_TEST_ASSERT(context, internal.mRendererState.mPoly9Offset == 700 % 511);
	AT_PORTABLE_TEST_ASSERT(context, internal.mRendererState.mPoly17Offset == 200000 % 131071);
	AT_PORTABLE_TEST_ASSERT(context, internal.mRendererState.mOutputFlipFlops == 0x1A);
	return true;
}
