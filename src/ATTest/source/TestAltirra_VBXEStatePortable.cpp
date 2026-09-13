// Altirra portable VBXE save-state tests

#include <cstring>
#include <utility>
#include <vector>

#include <at/atcore/savestate.h>
#include <at/atcore/serialization.h>
#include <at/attest/portabletest.h>
#include <vbxestate.h>

namespace {
	class VBXEStateTestInput final : public IATSerializationInput {
	public:
		explicit VBXEStateTestInput(std::vector<sint64> changes)
			: mChanges(std::move(changes)) {
		}

		bool OpenObject(const char *) override { return false; }

		uint32 OpenArray(const char *key) override {
			mbReadingChanges = key && !strcmp(key, "int_gtia_changes");
			mChangeCursor = 0;
			return mbReadingChanges ? static_cast<uint32>(mChanges.size()) : 0;
		}

		void Close() override { mbReadingChanges = false; }

		bool ReadStringA(const char *, VDStringA&) override { return false; }
		bool ReadStringW(const char *, VDStringW&) override { return false; }
		bool ReadBool(const char *, bool&) override { return false; }

		bool ReadInt64(const char *, sint64& value) override {
			if (!mbReadingChanges || mChangeCursor >= mChanges.size())
				return false;
			value = mChanges[mChangeCursor++];
			return true;
		}

		bool ReadUint64(const char *key, uint64& value) override {
			if (key && !strcmp(key, "arch_video_control")) {
				value = 0xA5;
				return true;
			}
			if (key && !strcmp(key, "arch_xdl_adr_x")) {
				value = 0x123456;
				return true;
			}
			return false;
		}

		bool ReadDouble(const char *, double&) override { return false; }
		bool ReadObject(const char *, const ATSerializationTypeDef *, IATSerializable *& value) override {
			value = nullptr;
			return false;
		}
		bool ReadInlineObject(const char *, const ATSerializationTypeDef *, int, IATSerializable *& value) override {
			value = nullptr;
			return false;
		}
		bool ReadFixedBulkData(void *, uint32) override { return false; }
		bool ReadVariableBulkData(vdfastvector<uint8>&) override { return false; }

	private:
		std::vector<sint64> mChanges;
		size_t mChangeCursor = 0;
		bool mbReadingChanges = false;
	};

	bool RejectsChanges(std::vector<sint64> changes) {
		VBXEStateTestInput input(std::move(changes));
		ATDeserializer reader(input, 0);
		ATSaveStateVbxe state;
		try {
			state.Exchange(reader);
		} catch(const ATInvalidSaveStateException&) {
			return true;
		}
		return false;
	}
}

bool ATTestAltirraVBXEState(ATPortableTestContext& context) {
	VBXEStateTestInput input({ 10, 0x12, 0x34, 20, 0x56, 0x78 });
	ATDeserializer reader(input, 0);
	ATSaveStateVbxe state;
	state.Exchange(reader);

	AT_PORTABLE_TEST_ASSERT(context, state.mVideoControl == 0xA5);
	AT_PORTABLE_TEST_ASSERT(context, state.mXdlAddr == 0x123456);
	AT_PORTABLE_TEST_ASSERT(context, state.mGtiaRegisterChanges.size() == 6);
	AT_PORTABLE_TEST_ASSERT(context, state.mGtiaRegisterChanges[0] == 10);
	AT_PORTABLE_TEST_ASSERT(context, state.mGtiaRegisterChanges[3] == 20);
	AT_PORTABLE_TEST_ASSERT(context, state.mGtiaRegisterChanges[5] == 0x78);
	AT_PORTABLE_TEST_ASSERT(context, RejectsChanges({ 10, 0x12 }));
	AT_PORTABLE_TEST_ASSERT(context, RejectsChanges({ 20, 1, 2, 10, 3, 4 }));
	AT_PORTABLE_TEST_ASSERT(context, !RejectsChanges({ 10, 1, 2, 10, 3, 4 }));
	return true;
}
