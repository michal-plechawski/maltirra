// Portable behavioral tests for GTIA registers and architectural snapshots.

#include <memory>
#include <string>
#include <string_view>

#include <at/atcore/snapshotimpl.h>
#include <at/atcore/savestate.h>
#include <at/attest/portabletest.h>
#include <savestateio.h>
#include <vd2/system/file.h>
#include <vd2/system/vdalloc.h>
#include <vd2/system/refcount.h>
#include <gtia.h>

namespace {
	class GTIATestConnections final : public IATGTIAEmulatorConnections {
	public:
		uint32 GTIAGetXClock() override { return mXClock; }
		uint32 GTIAGetTimestamp() const override { return mTimestamp; }
		uint64 GTIAGetTimestamp64() const override { return mTimestamp64; }
		void GTIASetSpeaker(bool state) override { mSpeaker = state; ++mSpeakerChanges; }
		void GTIASelectController(uint8 index, bool potsEnabled) override {
			mController = index;
			mPotsEnabled = potsEnabled;
			++mControllerChanges;
		}
		void GTIARequestAnticSync(int offset) override {
			mLastSyncOffset = offset;
			++mSyncRequests;
		}
		uint32 GTIAGetLineEdgeTimingId(uint32 offset) const override {
			const uint32 beamX = mXClock / 2;
			uint32 timingId = mTimestamp - beamX;
			if (beamX >= offset)
				timingId += 114;
			return timingId;
		}

		uint32 mXClock = 0;
		uint32 mTimestamp = 0;
		uint64 mTimestamp64 = 0;
		bool mSpeaker = true;
		bool mPotsEnabled = false;
		uint8 mController = 0xff;
		uint32 mSpeakerChanges = 0;
		uint32 mControllerChanges = 0;
		uint32 mSyncRequests = 0;
		int mLastSyncOffset = 0;
	};

	struct GTIAFixture {
		GTIAFixture() { gtia.Init(&connections); }
		GTIATestConnections connections;
		ATGTIAEmulator gtia;
	};

	bool ReplaceSyncOffset(std::string& json, sint32 value) {
		const size_t keyPos = json.find("\"sync_offset\"");
		if (keyPos == std::string::npos)
			return false;

		const size_t colonPos = json.find(':', keyPos);
		if (colonPos == std::string::npos)
			return false;

		size_t valueBegin = colonPos + 1;
		while (valueBegin < json.size() && (json[valueBegin] == ' ' || json[valueBegin] == '\t' || json[valueBegin] == '\r' || json[valueBegin] == '\n'))
			++valueBegin;

		size_t valueEnd = valueBegin;
		if (valueEnd < json.size() && json[valueEnd] == '-')
			++valueEnd;
		while (valueEnd < json.size() && json[valueEnd] >= '0' && json[valueEnd] <= '9')
			++valueEnd;
		if (valueBegin == valueEnd)
			return false;

		json.replace(valueBegin, valueEnd - valueBegin, std::to_string(value));
		return true;
	}

	bool ReadSyncOffset(std::string_view json, sint32& value) {
		const size_t keyPos = json.find("\"sync_offset\"");
		if (keyPos == std::string_view::npos)
			return false;

		const size_t colonPos = json.find(':', keyPos);
		if (colonPos == std::string_view::npos)
			return false;

		size_t valueBegin = colonPos + 1;
		while (valueBegin < json.size() && (json[valueBegin] == ' ' || json[valueBegin] == '\t' || json[valueBegin] == '\r' || json[valueBegin] == '\n'))
			++valueBegin;
		size_t valueEnd = valueBegin;
		if (valueEnd < json.size() && json[valueEnd] == '-')
			++valueEnd;
		while (valueEnd < json.size() && json[valueEnd] >= '0' && json[valueEnd] <= '9')
			++valueEnd;
		if (valueBegin == valueEnd)
			return false;

		try {
			value = static_cast<sint32>(std::stoi(std::string(json.substr(valueBegin, valueEnd - valueBegin))));
		} catch (...) {
			return false;
		}
		return true;
	}

	bool DeserializeGTIASnapshot(std::string_view json, vdrefptr<IATSerializable>& snapshot, bool& rejected) {
		VDMemoryBufferStream stream;
		stream.Write(json.data(), static_cast<sint32>(json.size()));
		stream.Seek(0);
		std::unique_ptr<IATSaveStateDeserializer> reader(ATCreateSaveStateDeserializer());
		rejected = false;
		try {
			reader->Deserialize(stream, ~snapshot);
		} catch(const ATInvalidSaveStateException&) {
			rejected = true;
		}
		return !!snapshot;
	}

	std::string SerializeGTIASnapshot(IATSerializable& snapshot) {
		VDMemoryBufferStream stream;
		std::unique_ptr<IATSaveStateSerializer> writer(ATCreateSaveStateSerializer());
		writer->Serialize(stream, snapshot, L"GTIA core compatibility");
		const vdspan<const uint8> buffer = stream.GetBuffer();
		return std::string(reinterpret_cast<const char *>(buffer.data()), buffer.size());
	}

	bool TestSyncOffsetCompatibility(ATPortableTestContext& context, const std::string& json) {
		std::string legacyJson = json;
		const size_t keyPos = legacyJson.find("\"sync_offset\"");
		AT_PORTABLE_TEST_ASSERT(context, keyPos != std::string::npos);
		legacyJson.replace(keyPos, sizeof("\"sync_offset\"") - 1, "\"unknown_sync_offset\"");

		vdrefptr<IATSerializable> legacySnapshot;
		bool rejected = false;
		AT_PORTABLE_TEST_ASSERT(context, DeserializeGTIASnapshot(legacyJson, legacySnapshot, rejected));
		AT_PORTABLE_TEST_ASSERT(context, !rejected);
		std::string upgradedJson = SerializeGTIASnapshot(*legacySnapshot);
		sint32 upgradedOffset = -1;
		AT_PORTABLE_TEST_ASSERT(context, ReadSyncOffset(upgradedJson, upgradedOffset));
		AT_PORTABLE_TEST_ASSERT(context, upgradedOffset == 0);

		for(sint32 invalidOffset : { sint32(-1), sint32(231) }) {
			std::string invalidJson = json;
			AT_PORTABLE_TEST_ASSERT(context, ReplaceSyncOffset(invalidJson, invalidOffset));
			vdrefptr<IATSerializable> invalidSnapshot;
			AT_PORTABLE_TEST_ASSERT(context, !DeserializeGTIASnapshot(invalidJson, invalidSnapshot, rejected));
			AT_PORTABLE_TEST_ASSERT(context, rejected);
		}
		return true;
	}

	bool TestRegisterBehavior(ATPortableTestContext& context) {
		auto fixtureStorage = std::make_unique<GTIAFixture>();
		auto& fixture = *fixtureStorage;
		auto& gtia = fixture.gtia;
		auto& conn = fixture.connections;
		gtia.UpdatePlayfield160(0, nullptr, 0);
		gtia.UpdatePlayfield320(0, nullptr, 0);

		AT_PORTABLE_TEST_ASSERT(context, conn.mControllerChanges == 1);
		AT_PORTABLE_TEST_ASSERT(context, conn.mController == 0);
		AT_PORTABLE_TEST_ASSERT(context, !conn.mPotsEnabled);
		AT_PORTABLE_TEST_ASSERT(context, gtia.ReadByte(0x1f) == 0);

		gtia.SetConsoleSwitch(1, false);
		gtia.SetConsoleSwitch(2, true);
		AT_PORTABLE_TEST_ASSERT(context, gtia.ReadConsoleSwitchInputs() == 13);
		AT_PORTABLE_TEST_ASSERT(context, gtia.ReadByte(0x3f) == 0);
		gtia.SetForcedConsoleSwitches(0x0b);
		AT_PORTABLE_TEST_ASSERT(context, gtia.ReadConsoleSwitchInputs() == 9);
		AT_PORTABLE_TEST_ASSERT(context, gtia.ReadByte(0x1f) == 0);

		gtia.WriteByte(0x3f, 0x02);
		AT_PORTABLE_TEST_ASSERT(context, conn.mController == 2);
		AT_PORTABLE_TEST_ASSERT(context, !conn.mPotsEnabled);
		AT_PORTABLE_TEST_ASSERT(context, conn.mControllerChanges == 2);
		AT_PORTABLE_TEST_ASSERT(context, !conn.mSpeaker);
		AT_PORTABLE_TEST_ASSERT(context, conn.mSpeakerChanges == 1);
		gtia.WriteByte(0x1f, 0x06);
		AT_PORTABLE_TEST_ASSERT(context, conn.mController == 2);
		AT_PORTABLE_TEST_ASSERT(context, conn.mPotsEnabled);
		AT_PORTABLE_TEST_ASSERT(context, conn.mControllerChanges == 3);
		gtia.WriteByte(0x1f, 0x02);
		AT_PORTABLE_TEST_ASSERT(context, !conn.mPotsEnabled);
		AT_PORTABLE_TEST_ASSERT(context, conn.mControllerChanges == 4);
		gtia.WriteByte(0x1f, 0x02);
		AT_PORTABLE_TEST_ASSERT(context, conn.mControllerChanges == 4);
		AT_PORTABLE_TEST_ASSERT(context, conn.mSpeakerChanges == 1);
		AT_PORTABLE_TEST_ASSERT(context, gtia.ReadByte(0x1f) == 9);

		gtia.SetControllerTrigger(0, true);
		AT_PORTABLE_TEST_ASSERT(context, gtia.ReadByte(0x10) == 0);
		gtia.WriteByte(0x1d, 0x04);
		gtia.SetControllerTrigger(0, false);
		AT_PORTABLE_TEST_ASSERT(context, gtia.ReadByte(0x10) == 0);
		gtia.WriteByte(0x1d, 0x00);
		AT_PORTABLE_TEST_ASSERT(context, gtia.ReadByte(0x10) == 1);

		// The core's actual power-on collision latches are visible through the
		// debug reads. Collision masks suppress one class and clear it on re-enable.
		AT_PORTABLE_TEST_ASSERT(context, gtia.ReadByte(0x08) == 0x0f);
		AT_PORTABLE_TEST_ASSERT(context, gtia.ReadByte(0x0f) == 0x07);
		gtia.SetPMCollisionsEnabled(false);
		AT_PORTABLE_TEST_ASSERT(context, gtia.ReadByte(0x08) == 0);
		gtia.SetPMCollisionsEnabled(true);
		AT_PORTABLE_TEST_ASSERT(context, gtia.ReadByte(0x08) == 0);

		gtia.SetPALMode(true);
		AT_PORTABLE_TEST_ASSERT(context, gtia.ReadByte(0x14) == 0x01);
		gtia.SetPALMode(false);
		AT_PORTABLE_TEST_ASSERT(context, gtia.ReadByte(0x14) == 0x0f);
		gtia.SetSECAMMode(true);
		conn.mTimestamp = 107;
		conn.mXClock = 214;
		gtia.SetControllerTrigger(1, true);
		AT_PORTABLE_TEST_ASSERT(context, gtia.ReadByte(0x11) == 1);
		conn.mTimestamp = 108;
		conn.mXClock = 216;
		AT_PORTABLE_TEST_ASSERT(context, gtia.ReadByte(0x11) == 0);
		conn.mTimestamp = 109;
		conn.mXClock = 218;
		gtia.SetControllerTrigger(1, false);
		AT_PORTABLE_TEST_ASSERT(context, gtia.ReadByte(0x11) == 0);
		conn.mTimestamp = 222;
		conn.mXClock = 216;
		AT_PORTABLE_TEST_ASSERT(context, gtia.ReadByte(0x11) == 1);
		gtia.SetSECAMMode(false);

		ATColorSettings colors = gtia.GetDefaultColorSettings();
		AT_PORTABLE_TEST_ASSERT(context, colors.mNTSCParams.mPresetTag == "default_ntsc");
		AT_PORTABLE_TEST_ASSERT(context, colors.mPALParams.mPresetTag == "default_pal");
		AT_PORTABLE_TEST_ASSERT(context, colors.mbUsePALParams);
		colors.mbUsePALParams = false;
		gtia.SetColorSettings(colors);
		AT_PORTABLE_TEST_ASSERT(context, !gtia.GetColorSettings().mbUsePALParams);
		AT_PORTABLE_TEST_ASSERT(context, conn.mSyncRequests > 0);
		return true;
	}

	bool TestSnapshotRestore(ATPortableTestContext& context) {
		auto originalStorage = std::make_unique<GTIAFixture>();
		auto restoredStorage = std::make_unique<GTIAFixture>();
		auto& original = originalStorage->gtia;
		auto& restored = restoredStorage->gtia;

		original.WriteByte(0x00, 0x35);
		original.WriteByte(0x08, 0x03);
		original.WriteByte(0x0d, 0xa5);
		original.WriteByte(0x12, 0x71);
		original.WriteByte(0x16, 0x82);
		original.WriteByte(0x1a, 0x94);
		original.WriteByte(0x1b, 0x40);
		original.WriteByte(0x1c, 0x24);
		original.WriteByte(0x1d, 0x03);
		original.WriteByte(0x1f, 0x05);
		original.Sync();

		vdrefptr<IATObjectState> snapshot;
		original.SaveState(~snapshot);
		AT_PORTABLE_TEST_ASSERT(context, !!snapshot);
		VDMemoryBufferStream stream;
		std::unique_ptr<IATSaveStateSerializer> writer(ATCreateSaveStateSerializer());
		writer->Serialize(stream, *snapshot, L"GTIA core");
		const vdspan<const uint8> serializedBuffer = stream.GetBuffer();
		const std::string serializedJson(reinterpret_cast<const char *>(serializedBuffer.data()), serializedBuffer.size());
		AT_PORTABLE_TEST_ASSERT(context, TestSyncOffsetCompatibility(context, serializedJson));
		stream.Seek(0);
		std::unique_ptr<IATSaveStateDeserializer> reader(ATCreateSaveStateDeserializer());
		vdrefptr<IATSerializable> decodedSnapshot;
		reader->Deserialize(stream, ~decodedSnapshot);
		AT_PORTABLE_TEST_ASSERT(context, !!decodedSnapshot);
		AT_PORTABLE_TEST_ASSERT(context, &decodedSnapshot->GetSerializationType() == &snapshot->GetSerializationType());
		auto *roundTrippedSnapshot = static_cast<IATObjectState *>(decodedSnapshot.get());

		restored.WriteByte(0x12, 0x22);
		restored.WriteByte(0x1a, 0x44);
		restored.WriteByte(0x1f, 0x0f);
		originalStorage->connections.mXClock = 10;
		original.Sync();
		ATGTIARegisterState continuedOriginal {};
		original.GetRegisterState(continuedOriginal);
		AT_PORTABLE_TEST_ASSERT(context, continuedOriginal.mReg[0] == 0x35);

		restored.LoadState(roundTrippedSnapshot);
		restoredStorage->connections.mXClock = 10;
		restored.Sync();

		ATGTIARegisterState after {};
		restored.GetRegisterState(after);
		AT_PORTABLE_TEST_ASSERT(context, after.mReg[0] == 0x35);
		for(size_t i = 0; i < sizeof after.mReg; ++i)
			AT_PORTABLE_TEST_ASSERT(context, continuedOriginal.mReg[i] == after.mReg[i]);
		AT_PORTABLE_TEST_ASSERT(context, restored.ReadByte(0x10) == 1);
		AT_PORTABLE_TEST_ASSERT(context, restored.ReadByte(0x14) == 0x0f);
		AT_PORTABLE_TEST_ASSERT(context, restored.ReadByte(0x1f) == 0x0a);

		// Loading the same architectural state a second time must be stable.
		restoredStorage->connections.mXClock = 0;
		restored.LoadState(roundTrippedSnapshot);
		restoredStorage->connections.mXClock = 10;
		restored.Sync();
		ATGTIARegisterState afterReload {};
		restored.GetRegisterState(afterReload);
		for(size_t i = 0; i < sizeof afterReload.mReg; ++i)
			AT_PORTABLE_TEST_ASSERT(context, continuedOriginal.mReg[i] == afterReload.mReg[i]);
		return true;
	}
}

bool ATTestAltirraGTIARegisters(ATPortableTestContext& context) {
	return TestRegisterBehavior(context);
}

bool ATTestAltirraGTIASnapshot(ATPortableTestContext& context) {
	return TestSnapshotRestore(context);
}
