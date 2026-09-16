// Portable serialization and validation tests for saved trace metadata.

#include <cstring>

#include <at/atcore/savestate.h>
#include <at/atcore/serialization.h>
#include <at/attest/portabletest.h>
#include <vd2/system/VDString.h>

#include <tracefileformat.h>

namespace {
	class TraceInput final : public IATSerializationInput {
	public:
		bool OpenObject(const char *) override { return true; }

		uint32 OpenArray(const char *key) override {
			if (!key)
				return 0;
			if (!std::strcmp(key, "frame_boundaries"))
				return mFrameBoundaryCount;
			if (!std::strcmp(key, "columns"))
				return mColumnCount;
			return 0;
		}

		void Close() override {}

		bool ReadStringA(const char *key, VDStringA& value) override {
			if (!key || std::strcmp(key, "type"))
				return false;
			value = mColumnType;
			return true;
		}

		bool ReadStringW(const char *key, VDStringW& value) override {
			if (!key || std::strcmp(key, "format"))
				return false;
			value = L"raw";
			return true;
		}

		bool ReadBool(const char *, bool&) override { return false; }
		bool ReadInt64(const char *, sint64&) override { return false; }

		bool ReadUint64(const char *key, uint64& value) override {
			if (!key)
				return false;

			static constexpr struct Member {
				const char *mpName;
				uint32 TraceInput::*mpValue;
			} kMembers[] = {
				{ "offset", &TraceInput::mFrameOffset },
				{ "period", &TraceInput::mFramePeriod },
				{ "count", &TraceInput::mFrameCount },
				{ "row_count", &TraceInput::mRowCount },
				{ "row_group_size", &TraceInput::mRowGroupSize },
				{ "bit_offset", &TraceInput::mColumnBitOffset },
				{ "bit_width", &TraceInput::mColumnBitWidth },
				{ "byte_count", &TraceInput::mRowSize },
				{ "frame_buffers_per_group", &TraceInput::mFrameBuffersPerGroup }
			};

			for(const Member& member : kMembers) {
				if (!std::strcmp(key, member.mpName)) {
					value = this->*member.mpValue;
					return true;
				}
			}

			value = 0;
			return true;
		}

		bool ReadDouble(const char *key, double& value) override {
			if (!key || std::strcmp(key, "ticks_per_second"))
				return false;
			value = mTicksPerSecond;
			return true;
		}

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

		double mTicksPerSecond = 1780000.0;
		uint32 mFrameBoundaryCount = 1;
		uint32 mFrameOffset = 123;
		uint32 mFramePeriod = 456;
		uint32 mFrameCount = 7;
		uint32 mRowCount = 10;
		uint32 mRowGroupSize = 5;
		uint32 mColumnCount = 1;
		const char *mColumnType = "pc";
		uint32 mColumnBitOffset = 0;
		uint32 mColumnBitWidth = 16;
		uint32 mRowSize = 2;
		uint32 mFrameBuffersPerGroup = 4;
	};

	template<typename T>
	bool DeserializeThrowsInvalid(TraceInput& input) {
		try {
			T value;
			ATDeserializer reader(input, 0);
			value.Exchange(reader);
		} catch(const ATInvalidSaveStateException&) {
			return true;
		}

		return false;
	}
}

bool ATTestAltirraTraceFileFormat(ATPortableTestContext& context) {
	AT_PORTABLE_TEST_ASSERT(context, !std::strcmp(ATEnumToString(ATSavedTraceCPUColumnType::EffectiveAddress), "effective_address"));
	AT_PORTABLE_TEST_ASSERT(context, !std::strcmp(ATEnumToString(ATSavedTraceCPUColumnType::GlobalPCBase), "global_pc_base"));

	TraceInput frameInput;
	ATSavedTraceFrameChannelDetail frame;
	ATDeserializer frameReader(frameInput, 0);
	frame.Exchange(frameReader);
	AT_PORTABLE_TEST_ASSERT(context, frame.mTicksPerSecond == 1780000.0);
	AT_PORTABLE_TEST_ASSERT(context, frame.mFrameBoundaries.size() == 1);
	AT_PORTABLE_TEST_ASSERT(context, frame.mFrameBoundaries[0].mOffset == 123);
	AT_PORTABLE_TEST_ASSERT(context, frame.mFrameBoundaries[0].mPeriod == 456);
	AT_PORTABLE_TEST_ASSERT(context, frame.mFrameBoundaries[0].mCount == 7);

	TraceInput invalidBoundary;
	invalidBoundary.mFramePeriod = 0;
	AT_PORTABLE_TEST_ASSERT(context, DeserializeThrowsInvalid<ATSavedTraceFrameChannelDetail>(invalidBoundary));

	TraceInput cpuInput;
	ATSavedTraceCPUChannelDetail cpu;
	ATDeserializer cpuReader(cpuInput, 0);
	cpu.Exchange(cpuReader);
	AT_PORTABLE_TEST_ASSERT(context, cpu.mRowCount == 10);
	AT_PORTABLE_TEST_ASSERT(context, cpu.mRowGroupSize == 5);
	AT_PORTABLE_TEST_ASSERT(context, cpu.mRowSize == 2);
	AT_PORTABLE_TEST_ASSERT(context, cpu.mColumns.size() == 1);
	AT_PORTABLE_TEST_ASSERT(context, cpu.mColumns[0].mType == ATSavedTraceCPUColumnType::PC);
	AT_PORTABLE_TEST_ASSERT(context, cpu.mColumns[0].mBitWidth == 16);

	TraceInput zeroWidthRows;
	zeroWidthRows.mRowSize = 0;
	AT_PORTABLE_TEST_ASSERT(context, DeserializeThrowsInvalid<ATSavedTraceCPUChannelDetail>(zeroWidthRows));

	TraceInput unalignedPC;
	unalignedPC.mColumnBitOffset = 1;
	unalignedPC.mColumnBitWidth = 8;
	AT_PORTABLE_TEST_ASSERT(context, DeserializeThrowsInvalid<ATSavedTraceCPUChannelDetail>(unalignedPC));

	TraceInput invalidVideo;
	invalidVideo.mFrameBuffersPerGroup = 0;
	AT_PORTABLE_TEST_ASSERT(context, DeserializeThrowsInvalid<ATSavedTraceVideoChannelDetail>(invalidVideo));

	TraceInput validVideo;
	ATSavedTraceVideoChannelDetail video;
	ATDeserializer videoReader(validVideo, 0);
	video.Exchange(videoReader);
	AT_PORTABLE_TEST_ASSERT(context, video.mFormat == L"raw");
	AT_PORTABLE_TEST_ASSERT(context, video.mFrameBuffersPerGroup == 4);
	return true;
}
