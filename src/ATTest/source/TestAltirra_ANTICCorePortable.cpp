// Portable behavioral tests for the production ANTIC core.

#include <memory>
#include <algorithm>
#include <string>
#include <string_view>

#include <at/atcore/snapshotimpl.h>
#include <at/atcore/savestate.h>
#include <at/attest/portabletest.h>
#include "anticportablefixture.h"
#include <savestateio.h>
#include <vd2/system/file.h>

namespace {
	bool SameRegisters(const ATAnticRegisterState& a, const ATAnticRegisterState& b) {
		return a.mDMACTL == b.mDMACTL && a.mCHACTL == b.mCHACTL && a.mDLISTL == b.mDLISTL &&
			a.mDLISTH == b.mDLISTH && a.mHSCROL == b.mHSCROL && a.mVSCROL == b.mVSCROL &&
			a.mPMBASE == b.mPMBASE && a.mCHBASE == b.mCHBASE && a.mNMIEN == b.mNMIEN;
	}

	vdrefptr<IATObjectState> RoundTripSnapshot(ATAnticEmulator& antic) {
		vdrefptr<IATObjectState> state;
		antic.SaveState(~state);
		if (!state)
			return {};

		VDMemoryBufferStream stream;
		std::unique_ptr<IATSaveStateSerializer> writer(ATCreateSaveStateSerializer());
		writer->Serialize(stream, *state, L"ANTIC portable core");
		stream.Seek(0);
		std::unique_ptr<IATSaveStateDeserializer> reader(ATCreateSaveStateDeserializer());
		vdrefptr<IATSerializable> decoded;
		reader->Deserialize(stream, ~decoded);
		if (!decoded || &decoded->GetSerializationType() != &state->GetSerializationType())
			return {};
		return vdrefptr<IATObjectState>(static_cast<IATObjectState *>(decoded.get()));
	}

	std::string SerializeSnapshot(ATAnticEmulator& antic) {
		vdrefptr<IATObjectState> state;
		antic.SaveState(~state);
		VDMemoryBufferStream stream;
		std::unique_ptr<IATSaveStateSerializer> writer(ATCreateSaveStateSerializer());
		writer->Serialize(stream, *state, L"ANTIC portable core buffers");
		const vdspan<const uint8> buffer = stream.GetBuffer();
		return std::string(reinterpret_cast<const char *>(buffer.data()), buffer.size());
	}

	bool ReadJSONValue(std::string_view json, std::string_view key, std::string_view& value) {
		const std::string quotedKey = "\"" + std::string(key) + "\"";
		const size_t keyPos = json.find(quotedKey);
		if (keyPos == std::string_view::npos)
			return false;
		const size_t colonPos = json.find(':', keyPos + quotedKey.size());
		if (colonPos == std::string_view::npos)
			return false;
		size_t begin = colonPos + 1;
		while (begin < json.size() && (json[begin] == ' ' || json[begin] == '\t' || json[begin] == '\r' || json[begin] == '\n'))
			++begin;
		if (begin >= json.size())
			return false;
		const char opening = json[begin];
		if (opening != '[' && opening != '{') {
			size_t end = json.find_first_of(",}\r\n", begin);
			if (end == std::string_view::npos)
				end = json.size();
			value = json.substr(begin, end - begin);
			return true;
		}
		const char closing = opening == '[' ? ']' : '}';
		int depth = 0;
		bool quoted = false;
		for(size_t i = begin; i < json.size(); ++i) {
			const char c = json[i];
			if (c == '"' && (i == 0 || json[i - 1] != '\\'))
				quoted = !quoted;
			if (quoted)
				continue;
			if (c == opening)
				++depth;
			else if (c == closing && !--depth) {
				value = json.substr(begin, i - begin + 1);
				return true;
			}
		}
		return false;
	}

	bool ReplaceJSONValue(std::string& json, std::string_view key, std::string_view replacement) {
		std::string_view value;
		if (!ReadJSONValue(json, key, value))
			return false;
		const size_t offset = static_cast<size_t>(value.data() - json.data());
		json.replace(offset, value.size(), replacement);
		return true;
	}

	bool RejectSnapshotJSON(std::string_view json) {
		VDMemoryBufferStream stream;
		stream.Write(json.data(), static_cast<sint32>(json.size()));
		stream.Seek(0);
		std::unique_ptr<IATSaveStateDeserializer> reader(ATCreateSaveStateDeserializer());
		vdrefptr<IATSerializable> decoded;
		try {
			reader->Deserialize(stream, ~decoded);
		} catch(const ATInvalidSaveStateException&) {
			return true;
		}
		return false;
	}

	bool TestRegisters(ATPortableTestContext& context) {
		auto storage = std::make_unique<ANTICPortableFixture>();
		auto& fixture = *storage;
		auto& antic = fixture.mAntic;
		ATAnticRegisterState state {};
		antic.GetRegisterState(state);

		fixture.WriteANTIC(0x00, 0xff); // DMACTL has only six implemented bits.
		fixture.WriteANTIC(0x01, 0xff); // CHACTL has only three implemented bits.
		fixture.WriteANTIC(0x04, 0xff);
		fixture.WriteANTIC(0x05, 0xff);
		fixture.WriteANTIC(0x06, 0xff);
		fixture.WriteANTIC(0x07, 0xff);
		fixture.WriteANTIC(0x0e, 0xff);
		antic.GetRegisterState(state);
		AT_PORTABLE_TEST_ASSERT(context, state.mDMACTL == 0x3f);
		AT_PORTABLE_TEST_ASSERT(context, state.mCHACTL == 0x07);
		AT_PORTABLE_TEST_ASSERT(context, state.mHSCROL == 0x0f && state.mVSCROL == 0x0f);
		AT_PORTABLE_TEST_ASSERT(context, state.mPMBASE == 0xfc);
		AT_PORTABLE_TEST_ASSERT(context, state.mNMIEN == 0xc0);
		AT_PORTABLE_TEST_ASSERT(context, antic.ReadBackWriteRegister(0) == 0x3f);
		AT_PORTABLE_TEST_ASSERT(context, antic.ReadByte(0x0e) == 0xff);

		// CHBASE takes effect on the real delayed-register event, not at write time.
		const uint8 oldCHBASE = state.mCHBASE;
		fixture.WriteANTIC(0x09, 0x40);
		antic.GetRegisterState(state);
		AT_PORTABLE_TEST_ASSERT(context, state.mCHBASE == oldCHBASE);
		fixture.Tick();
		antic.GetRegisterState(state);
		AT_PORTABLE_TEST_ASSERT(context, state.mCHBASE == oldCHBASE);
		fixture.Tick(2);
		antic.GetRegisterState(state);
		AT_PORTABLE_TEST_ASSERT(context, state.mCHBASE == 0x40);
		return true;
	}

	bool TestTiming(ATPortableTestContext& context) {
		auto storage = std::make_unique<ANTICPortableFixture>();
		auto& fixture = *storage;
		auto& antic = fixture.mAntic;

		fixture.WriteANTIC(0x0a, 0); // WSYNC stalls until the ANTIC's real line-end event.
		fixture.Tick(3);
		AT_PORTABLE_TEST_ASSERT(context, antic.GetWSYNCFlag() != 0);
		AT_PORTABLE_TEST_ASSERT(context, std::any_of(fixture.mCallbacks.begin(), fixture.mCallbacks.end(), [](const auto& c) {
			return c.mType == ANTICPortableFixture::CallbackType::RDYAssert;
		}));
		AT_PORTABLE_TEST_ASSERT(context, fixture.RunToBeam(1, 105));
		AT_PORTABLE_TEST_ASSERT(context, antic.GetWSYNCFlag() == 0);
		AT_PORTABLE_TEST_ASSERT(context, std::any_of(fixture.mCallbacks.begin(), fixture.mCallbacks.end(), [](const auto& c) {
			return c.mType == ANTICPortableFixture::CallbackType::RDYNegate;
		}));

		auto vbiStorage = std::make_unique<ANTICPortableFixture>();
		auto& vbi = *vbiStorage;
		vbi.WriteANTIC(0x0e, 0x40);
		AT_PORTABLE_TEST_ASSERT(context, vbi.RunToBeam(248, 9));
		AT_PORTABLE_TEST_ASSERT(context, std::any_of(vbi.mCallbacks.begin(), vbi.mCallbacks.end(), [](const auto& c) {
			return c.mType == ANTICPortableFixture::CallbackType::NMI_VBI;
		}));

		auto dliStorage = std::make_unique<ANTICPortableFixture>();
		auto& dli = *dliStorage;
		dli.mRAM[0x3000] = 0x42; // LMS + DLI, mode 2 graphics.
		dli.mRAM[0x3001] = 0x00;
		dli.mRAM[0x3002] = 0x40;
		dli.WriteDisplayList(0x2000, { 0xC2, 0x00, 0x40, 0x41, 0x00, 0x20 });
		dli.WriteANTIC(0x00, 0x22);
		dli.WriteANTIC(0x0e, 0x80);
		AT_PORTABLE_TEST_ASSERT(context, dli.RunToBeam(16, 0));
		auto dliEvent = std::find_if(dli.mCallbacks.begin(), dli.mCallbacks.end(), [](const auto& c) {
			return c.mType == ANTICPortableFixture::CallbackType::NMI_DLI;
		});
		AT_PORTABLE_TEST_ASSERT(context, dliEvent != dli.mCallbacks.end());
		AT_PORTABLE_TEST_ASSERT(context, dliEvent->mBeamY == 15);
		AT_PORTABLE_TEST_ASSERT(context, dliEvent->mBeamX == 7 || dliEvent->mBeamX == 8);

		auto requestStorage = std::make_unique<ANTICPortableFixture>();
		auto& request = *requestStorage;
		request.mAntic.RequestNMI();
		AT_PORTABLE_TEST_ASSERT(context, request.RunToBeam(248, 9));
		AT_PORTABLE_TEST_ASSERT(context, std::any_of(request.mCallbacks.begin(), request.mCallbacks.end(), [](const auto& c) {
			return c.mType == ANTICPortableFixture::CallbackType::NMI_RES;
		}));
		return true;
	}

	bool TestSnapshot(ATPortableTestContext& context) {
		auto sourceStorage = std::make_unique<ANTICPortableFixture>();
		auto restoredStorage = std::make_unique<ANTICPortableFixture>();
		auto& source = *sourceStorage;
		auto& restored = *restoredStorage;
		for(uint16 i = 0; i < 40; ++i) {
			source.mRAM[0x3000 + i] = static_cast<uint8>(i + 1);
			for(uint16 row = 0; row < 8; ++row)
				source.mRAM[0x4000 + (i + 1) * 8 + row] = static_cast<uint8>((i * 13 + row * 7) ^ 0xA5);
		}
		source.WriteDisplayList(0x2000, { 0x44, 0x00, 0x30, 0x41, 0x00, 0x20 });
		source.WriteANTIC(0x00, 0x22);
		source.WriteANTIC(0x09, 0x40);
		source.Tick(3);
		restored.mRAM = source.mRAM;
		AT_PORTABLE_TEST_ASSERT(context, source.RunToBeam(8, 40));
		// Two delayed writes ensure snapshots retain every queued CHBASE update.
		source.WriteANTIC(0x09, 0x80);
		source.Tick();
		source.WriteANTIC(0x09, 0xc0);
		auto state = RoundTripSnapshot(source.mAntic);
		AT_PORTABLE_TEST_ASSERT(context, !!state);
		const std::string stateJSON = SerializeSnapshot(source.mAntic);
		std::string malformedQueue = stateJSON;
		AT_PORTABLE_TEST_ASSERT(context, ReplaceJSONValue(malformedQueue, "register_updates", "[1, 2]"));
		AT_PORTABLE_TEST_ASSERT(context, RejectSnapshotJSON(malformedQueue));
		std::string malformedOffset = stateJSON;
		AT_PORTABLE_TEST_ASSERT(context, ReplaceJSONValue(malformedOffset, "pf_data_read", "115"));
		AT_PORTABLE_TEST_ASSERT(context, RejectSnapshotJSON(malformedOffset));
		restored.mAntic.LoadState(state.get());
		AT_PORTABLE_TEST_ASSERT(context, restored.mAntic.GetBeamX() == source.mAntic.GetBeamX());
		AT_PORTABLE_TEST_ASSERT(context, restored.mAntic.GetBeamY() == source.mAntic.GetBeamY());
		source.mBusReads.clear();
		restored.mBusReads.clear();
		const std::string sourceBufferState = SerializeSnapshot(source.mAntic);
		const std::string restoredBufferState = SerializeSnapshot(restored.mAntic);
		for(std::string_view name : { "pf_data_read", "pf_data_write", "pf_char_read", "pf_data_buffer", "pf_char_buffer", "pf_decode_buffer" }) {
			std::string_view expectedBuffer, actualBuffer;
			AT_PORTABLE_TEST_ASSERT(context, ReadJSONValue(sourceBufferState, name, expectedBuffer));
			AT_PORTABLE_TEST_ASSERT(context, ReadJSONValue(restoredBufferState, name, actualBuffer));
			AT_PORTABLE_TEST_ASSERT(context, expectedBuffer == actualBuffer);
		}
		source.Tick(12);
		restored.Tick(12);
		ATAnticRegisterState expected {}, actual {};
		source.mAntic.GetRegisterState(expected);
		restored.mAntic.GetRegisterState(actual);
		AT_PORTABLE_TEST_ASSERT(context, SameRegisters(expected, actual));
		AT_PORTABLE_TEST_ASSERT(context, source.mAntic.GetDisplayListPointer() == restored.mAntic.GetDisplayListPointer());
		AT_PORTABLE_TEST_ASSERT(context, source.mAntic.GetBeamX() == restored.mAntic.GetBeamX());
		AT_PORTABLE_TEST_ASSERT(context, source.mAntic.GetBeamY() == restored.mAntic.GetBeamY());
		AT_PORTABLE_TEST_ASSERT(context, source.mBusReads.size() == restored.mBusReads.size());
		for(size_t i = 0; i < source.mBusReads.size(); ++i) {
			AT_PORTABLE_TEST_ASSERT(context, source.mBusReads[i].mAddress == restored.mBusReads[i].mAddress);
			AT_PORTABLE_TEST_ASSERT(context, source.mBusReads[i].mValue == restored.mBusReads[i].mValue);
		}

		// Re-loading the same decoded object after advancing must cancel/rebuild
		// the scheduled work and restore the same mid-line buffers and cursors.
		restored.mAntic.LoadState(state.get());
		restored.mBusReads.clear();
		restored.Tick(12);
		AT_PORTABLE_TEST_ASSERT(context, restored.mBusReads.size() == source.mBusReads.size());
		for(size_t i = 0; i < source.mBusReads.size(); ++i) {
			AT_PORTABLE_TEST_ASSERT(context, source.mBusReads[i].mAddress == restored.mBusReads[i].mAddress);
			AT_PORTABLE_TEST_ASSERT(context, source.mBusReads[i].mValue == restored.mBusReads[i].mValue);
		}

		// A clean snapshot replaces pending scheduled RNMI/WSYNC work; loading it
		// must not leave callbacks from the destination's prior execution alive.
		auto cleanStorage = std::make_unique<ANTICPortableFixture>();
		auto pendingStorage = std::make_unique<ANTICPortableFixture>();
		auto& clean = *cleanStorage;
		auto& pending = *pendingStorage;
		clean.Tick(7);
		auto cleanState = RoundTripSnapshot(clean.mAntic);
		AT_PORTABLE_TEST_ASSERT(context, !!cleanState);
		pending.mAntic.RequestNMI();
		pending.WriteANTIC(0x0a, 0);
		pending.Tick(3);
		pending.mAntic.LoadState(cleanState.get());
		pending.mCallbacks.clear();
		AT_PORTABLE_TEST_ASSERT(context, pending.RunToBeam(248, 9));
		AT_PORTABLE_TEST_ASSERT(context, std::none_of(pending.mCallbacks.begin(), pending.mCallbacks.end(), [](const auto& c) {
			return c.mType == ANTICPortableFixture::CallbackType::NMI_RES || c.mType == ANTICPortableFixture::CallbackType::RDYAssert;
		}));

		// Conversely, a snapshot with RNMI and active WSYNC must re-register the
		// real scheduled work when restored into another running instance.
		auto eventSourceStorage = std::make_unique<ANTICPortableFixture>();
		auto eventRestoredStorage = std::make_unique<ANTICPortableFixture>();
		auto& eventSource = *eventSourceStorage;
		auto& eventRestored = *eventRestoredStorage;
		eventSource.mAntic.RequestNMI();
		eventSource.WriteANTIC(0x0a, 0);
		eventSource.Tick(3);
		auto eventState = RoundTripSnapshot(eventSource.mAntic);
		AT_PORTABLE_TEST_ASSERT(context, !!eventState);
		eventRestored.mAntic.LoadState(eventState.get());
		AT_PORTABLE_TEST_ASSERT(context, eventRestored.RunToBeam(1, 105));
		AT_PORTABLE_TEST_ASSERT(context, eventRestored.mBusValue == 0xff);
		AT_PORTABLE_TEST_ASSERT(context, std::any_of(eventRestored.mCallbacks.begin(), eventRestored.mCallbacks.end(), [](const auto& c) {
			return c.mType == ANTICPortableFixture::CallbackType::RDYAssert;
		}));
		AT_PORTABLE_TEST_ASSERT(context, eventRestored.RunToBeam(248, 9));
		AT_PORTABLE_TEST_ASSERT(context, std::any_of(eventRestored.mCallbacks.begin(), eventRestored.mCallbacks.end(), [](const auto& c) {
			return c.mType == ANTICPortableFixture::CallbackType::NMI_RES;
		}));

		// Shutdown is idempotent and cancels delayed register, RNMI, and WSYNC
		// callbacks before scheduler advancement/destruction.
		auto shutdownStorage = std::make_unique<ANTICPortableFixture>();
		auto& shutdown = *shutdownStorage;
		shutdown.mAntic.RequestNMI();
		shutdown.WriteANTIC(0x0a, 0);
		shutdown.WriteANTIC(0x09, 0x40);
		shutdown.Tick(3);
		shutdown.mCallbacks.clear();
		shutdown.mAntic.Shutdown();
		shutdown.mAntic.Shutdown();
		const size_t callbacksAfterShutdown = shutdown.mCallbacks.size();
		for(uint32 i = 0; i < 40000; ++i)
			ATSCHEDULER_ADVANCE(&shutdown.mScheduler);
		AT_PORTABLE_TEST_ASSERT(context, shutdown.mCallbacks.size() == callbacksAfterShutdown);
		return true;
	}
}

bool ATTestAltirraANTICRegisters(ATPortableTestContext& context) { return TestRegisters(context); }
bool ATTestAltirraANTICTiming(ATPortableTestContext& context) { return TestTiming(context); }
bool ATTestAltirraANTICSnapshot(ATPortableTestContext& context) { return TestSnapshot(context); }
