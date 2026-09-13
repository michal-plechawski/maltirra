// Portable serialization tests for save-state thumbnail metadata.

#include <cstring>
#include <cwchar>
#include <map>
#include <string>
#include <vector>

#include <at/atcore/serialization.h>
#include <at/atcore/snapshotimpl.h>
#include <at/attest/portabletest.h>
#include <vd2/system/binary.h>

#include <savestatetypes.h>

namespace {
	class SaveStateCapture final : public IATSerializationOutput {
	public:
		void CreateMember(const char *key) override { mKey = key ? key : ""; }
		void OpenArray(bool) override {}
		void CloseArray() override {}
		void OpenObject(const char *) override {}
		void CloseObject() override {}
		void WriteStringA(VDStringSpanA) override {}
		void WriteStringW(VDStringSpanW) override {}
		void WriteBool(bool) override {}
		void WriteInt64(sint64) override {}
		void WriteUint64(uint64 value) override { mIntegers[mKey] = value; }
		void WriteDouble(double value) override { mDoubles[mKey] = value; }
		void WriteNullInlineObject() override {}
		void WriteBulkData(const void *, uint32) override {}

		void WriteObject(IATSerializable *object) override {
			if (mKey != "thumbnail")
				return;

			const auto *image = dynamic_cast<ATSaveStateMemoryBuffer *>(object);
			if (!image)
				return;

			mHasThumbnail = true;
			mThumbnailName = image->mpDirectName;
			const auto& bytes = image->GetReadBuffer();
			mThumbnail.assign(bytes.begin(), bytes.end());
		}

		std::string mKey;
		std::map<std::string, double> mDoubles;
		std::map<std::string, uint64> mIntegers;
		std::vector<uint8> mThumbnail;
		const wchar_t *mThumbnailName = nullptr;
		bool mHasThumbnail = false;
	};

	class SaveStateInput final : public IATSerializationInput {
	public:
		bool OpenObject(const char *) override { return false; }
		uint32 OpenArray(const char *) override { return 0; }
		void Close() override {}
		bool ReadStringA(const char *, VDStringA&) override { return false; }
		bool ReadStringW(const char *, VDStringW&) override { return false; }
		bool ReadBool(const char *, bool&) override { return false; }
		bool ReadInt64(const char *, sint64&) override { return false; }
		bool ReadUint64(const char *key, uint64& value) override {
			if (strcmp(key, "cold_start_id"))
				return false;
			value = 0x12345678;
			return true;
		}
		bool ReadDouble(const char *key, double& value) override {
			static constexpr struct { const char *key; double value; } kValues[] = {
				{ "thumbnail_x1", 0.125 },
				{ "thumbnail_y1", 0.25 },
				{ "thumbnail_x2", 0.75 },
				{ "thumbnail_y2", 0.875 },
				{ "thumbnail_pixel_aspect", 1.5 },
				{ "run_time_seconds", 123.5 }
			};
			for (const auto& entry : kValues) {
				if (!strcmp(key, entry.key)) {
					value = entry.value;
					return true;
				}
			}
			return false;
		}
		bool ReadObject(const char *key, const ATSerializationTypeDef *, IATSerializable *& value) override {
			mRequestedThumbnail = !strcmp(key, "thumbnail");
			value = nullptr;
			return false;
		}
		bool ReadInlineObject(const char *, const ATSerializationTypeDef *, int, IATSerializable *& value) override {
			value = nullptr;
			return false;
		}
		bool ReadFixedBulkData(void *, uint32) override { return false; }
		bool ReadVariableBulkData(vdfastvector<uint8>&) override { return false; }

		bool mRequestedThumbnail = false;
	};
}

bool ATTestAltirraSaveStateTypes(ATPortableTestContext& context) {
	ATSaveStateInfo state;
	state.mImage.init(1, 1, nsVDPixmap::kPixFormat_XRGB8888);
	*static_cast<uint32 *>(state.mImage.data) = 0x00112233;
	state.mImageX1 = 0.125f;
	state.mImageY1 = 0.25f;
	state.mImageX2 = 0.75f;
	state.mImageY2 = 0.875f;
	state.mImagePAR = 1.5f;
	state.mSimRunTimeSeconds = 123.5;
	state.mColdStartId = 0x12345678;

	SaveStateCapture capture;
	ATSerializer writer(capture);
	state.Exchange(writer);
	AT_PORTABLE_TEST_ASSERT(context, capture.mHasThumbnail);
	AT_PORTABLE_TEST_ASSERT(context, capture.mThumbnailName != nullptr);
	AT_PORTABLE_TEST_ASSERT(context, !std::wcscmp(capture.mThumbnailName, L"thumbnail.png"));
	AT_PORTABLE_TEST_ASSERT(context, capture.mThumbnail.size() >= 33);
	static constexpr uint8 kSignature[8] = { 137, 80, 78, 71, 13, 10, 26, 10 };
	AT_PORTABLE_TEST_ASSERT(context, !memcmp(capture.mThumbnail.data(), kSignature, sizeof kSignature));
	AT_PORTABLE_TEST_ASSERT(context, !memcmp(capture.mThumbnail.data() + 12, "IHDR", 4));
	AT_PORTABLE_TEST_ASSERT(context, VDReadUnalignedBEU32(capture.mThumbnail.data() + 16) == 1);
	AT_PORTABLE_TEST_ASSERT(context, VDReadUnalignedBEU32(capture.mThumbnail.data() + 20) == 1);
	AT_PORTABLE_TEST_ASSERT(context, capture.mDoubles["thumbnail_x1"] == 0.125);
	AT_PORTABLE_TEST_ASSERT(context, capture.mDoubles["thumbnail_y1"] == 0.25);
	AT_PORTABLE_TEST_ASSERT(context, capture.mDoubles["thumbnail_x2"] == 0.75);
	AT_PORTABLE_TEST_ASSERT(context, capture.mDoubles["thumbnail_y2"] == 0.875);
	AT_PORTABLE_TEST_ASSERT(context, capture.mDoubles["thumbnail_pixel_aspect"] == 1.5);
	AT_PORTABLE_TEST_ASSERT(context, capture.mDoubles["run_time_seconds"] == 123.5);
	AT_PORTABLE_TEST_ASSERT(context, capture.mIntegers["cold_start_id"] == 0x12345678);

	SaveStateInput input;
	ATDeserializer reader(input, 0);
	ATSaveStateInfo restored;
	restored.Exchange(reader);
	AT_PORTABLE_TEST_ASSERT(context, input.mRequestedThumbnail);
	AT_PORTABLE_TEST_ASSERT(context, restored.mImageX1 == 0.125f);
	AT_PORTABLE_TEST_ASSERT(context, restored.mImageY1 == 0.25f);
	AT_PORTABLE_TEST_ASSERT(context, restored.mImageX2 == 0.75f);
	AT_PORTABLE_TEST_ASSERT(context, restored.mImageY2 == 0.875f);
	AT_PORTABLE_TEST_ASSERT(context, restored.mImagePAR == 1.5f);
	AT_PORTABLE_TEST_ASSERT(context, restored.mSimRunTimeSeconds == 123.5);
	AT_PORTABLE_TEST_ASSERT(context, restored.mColdStartId == 0x12345678);
	return true;
}
