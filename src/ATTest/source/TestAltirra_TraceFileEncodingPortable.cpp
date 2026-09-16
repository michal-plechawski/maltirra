// Portable tests for saved trace codecs, predictors, and CPU history decoding.

#include <algorithm>
#include <cstring>
#include <memory>
#include <vector>

#include <at/atcore/snapshotimpl.h>
#include <at/attest/portabletest.h>
#include <vd2/system/binary.h>

#include <tracefileencoding.h>
#include <tracefileformat.h>

namespace {
	class PredictorInput final : public IATSerializationInput {
	public:
		bool OpenObject(const char *) override { return false; }
		uint32 OpenArray(const char *) override { return 0; }
		void Close() override {}
		bool ReadStringA(const char *, VDStringA&) override { return false; }
		bool ReadStringW(const char *, VDStringW&) override { return false; }
		bool ReadBool(const char *, bool&) override { return false; }
		bool ReadInt64(const char *, sint64&) override { return false; }
		bool ReadUint64(const char *key, uint64& value) override {
			if (!std::strcmp(key, "pc_offset"))
				value = 0;
			else if (!std::strcmp(key, "insn_offset"))
				value = 2;
			else if (!std::strcmp(key, "insn_size"))
				value = mInsnSize;
			else if (!std::strcmp(key, "flags_bit_offset"))
				value = mFlagsBitOffset;
			else
				return false;
			return true;
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

		uint32 mInsnSize = 4;
		uint32 mFlagsBitOffset = 13;
	};

	template<typename T_Call>
	bool ThrowsInvalidSaveState(T_Call&& call) {
		try {
			call();
		} catch(const ATInvalidSaveStateException&) {
			return true;
		}

		return false;
	}

	std::vector<uint8> MakeRows(uint32 rowSize, size_t rowCount) {
		std::vector<uint8> rows(rowSize * rowCount);

		for(size_t row = 0; row < rowCount; ++row) {
			for(uint32 column = 0; column < rowSize; ++column) {
				const uint32 seed = static_cast<uint32>(row * 37 + column * 19);
				rows[row * rowSize + column] = seed % 5 ? static_cast<uint8>(seed + 1) : 0;
			}
		}

		return rows;
	}

	bool CodecRoundTrips(const IATTraceFmtCodec& codec, uint32 rowSize) {
		constexpr size_t kRowCount = 37;
		const std::vector<uint8> source = MakeRows(rowSize, kRowCount);
		ATSaveStateMemoryBuffer encoded;
		codec.Encode(encoded, source.data(), rowSize, kRowCount);

		std::vector<uint8> decoded(source.size(), 0xCD);
		codec.Decode(encoded, decoded.data(), rowSize, kRowCount);
		return decoded == source;
	}

	bool PredictorRoundTrips(IATTraceFmtPredictor& predictor, uint32 rowSize) {
		constexpr size_t kRowCount = 41;
		const std::vector<uint8> source = MakeRows(rowSize, kRowCount);
		std::vector<uint8> transformed(source);

		ATTraceFmtAccessMask accessMask(rowSize);
		predictor.Validate(accessMask);
		predictor.Reset();
		predictor.Encode(transformed.data(), rowSize, kRowCount);
		predictor.Reset();
		predictor.Decode(transformed.data(), rowSize, kRowCount);
		return transformed == source;
	}

	ATSavedTraceCPUColumnInfo Column(ATSavedTraceCPUColumnType type, uint32 bitOffset, uint8 bitWidth) {
		ATSavedTraceCPUColumnInfo column;
		column.mType = type;
		column.mBitOffset = bitOffset;
		column.mBitWidth = bitWidth;
		return column;
	}
}

bool ATTestAltirraTraceFileEncoding(ATPortableTestContext& context) {
	ATTraceFmtRowMask rowMask(96);
	rowMask.MarkCount(31, 34);
	AT_PORTABLE_TEST_ASSERT(context, rowMask.mMask.size() == 3);
	AT_PORTABLE_TEST_ASSERT(context, rowMask.mMask[0] == 0x80000000U);
	AT_PORTABLE_TEST_ASSERT(context, rowMask.mMask[1] == 0xFFFFFFFFU);
	AT_PORTABLE_TEST_ASSERT(context, rowMask.mMask[2] == 0x00000001U);

	ATTraceFmtAccessMask accessMask(8);
	AT_PORTABLE_TEST_ASSERT(context, ThrowsInvalidSaveState([&] { accessMask.MarkRead(7, 2); }));
	AT_PORTABLE_TEST_ASSERT(context, ThrowsInvalidSaveState([&] { accessMask.MarkWrite(9, 1); }));
	accessMask.MarkRead(0, 2);
	accessMask.MarkWrite(4, 2);

	ATTraceFmtAccessMask independentMask(8);
	independentMask.MarkReadWrite(2, 2);
	AT_PORTABLE_TEST_ASSERT(context, accessMask.CanSwapWith(independentMask));
	independentMask.MarkRead(4, 1);
	AT_PORTABLE_TEST_ASSERT(context, !accessMask.CanSwapWith(independentMask));

	ATSavedTraceCodecNull nullCodec;
	AT_PORTABLE_TEST_ASSERT(context, CodecRoundTrips(nullCodec, 24));
	ATSaveStateMemoryBuffer malformedNull;
	malformedNull.GetWriteBuffer().resize(3);
	uint8 nullOutput[4] {};
	AT_PORTABLE_TEST_ASSERT(context, ThrowsInvalidSaveState([&] { nullCodec.Decode(malformedNull, nullOutput, 2, 2); }));

	ATSavedTraceCodecSparse sparseCodec;
	for(uint32 rowSize : { 8U, 16U, 24U, 32U })
		AT_PORTABLE_TEST_ASSERT(context, CodecRoundTrips(sparseCodec, rowSize));
	AT_PORTABLE_TEST_ASSERT(context, ThrowsInvalidSaveState([&] { sparseCodec.Validate(33); }));
	AT_PORTABLE_TEST_ASSERT(context, ThrowsInvalidSaveState([&] { sparseCodec.Validate(0); }));

	ATSaveStateMemoryBuffer truncatedSparse;
	const std::vector<uint8> sparseSource = MakeRows(24, 4);
	sparseCodec.Encode(truncatedSparse, sparseSource.data(), 24, 4);
	truncatedSparse.GetWriteBuffer().pop_back();
	std::vector<uint8> sparseOutput(sparseSource.size());
	AT_PORTABLE_TEST_ASSERT(context, ThrowsInvalidSaveState([&] { sparseCodec.Decode(truncatedSparse, sparseOutput.data(), 24, 4); }));

	ATSavedTracePredictorXOR xorPredictor(2, 4);
	AT_PORTABLE_TEST_ASSERT(context, PredictorRoundTrips(xorPredictor, 16));
	ATSavedTracePredictorXOR oversizedXor(0, 33);
	ATTraceFmtAccessMask wideAccessMask(64);
	AT_PORTABLE_TEST_ASSERT(context, ThrowsInvalidSaveState([&] { oversizedXor.Validate(wideAccessMask); }));
	ATSavedTracePredictorEA eaPredictor(4);
	AT_PORTABLE_TEST_ASSERT(context, PredictorRoundTrips(eaPredictor, 16));

	auto pcPredictor = std::make_unique<ATSavedTracePredictorPC>(6);
	AT_PORTABLE_TEST_ASSERT(context, PredictorRoundTrips(*pcPredictor, 16));
	pcPredictor.reset();

	ATSavedTracePredictorHorizDelta16 horizontal16(2, 0);
	AT_PORTABLE_TEST_ASSERT(context, PredictorRoundTrips(horizontal16, 16));
	ATSavedTracePredictorHorizDelta32 horizontal32(4, 0);
	AT_PORTABLE_TEST_ASSERT(context, PredictorRoundTrips(horizontal32, 16));
	ATSavedTracePredictorVertDelta16 vertical16(0, 3);
	AT_PORTABLE_TEST_ASSERT(context, PredictorRoundTrips(vertical16, 16));
	ATSavedTracePredictorHVDelta16x2 horizontalVertical16(0, 5);
	AT_PORTABLE_TEST_ASSERT(context, PredictorRoundTrips(horizontalVertical16, 16));
	ATSavedTracePredictorVertDelta32 vertical32(0, 7);
	AT_PORTABLE_TEST_ASSERT(context, PredictorRoundTrips(vertical32, 16));
	ATSavedTracePredictorVertDelta8 vertical8(2, 4);
	AT_PORTABLE_TEST_ASSERT(context, PredictorRoundTrips(vertical8, 16));
	ATSavedTracePredictorVertDelta8 oversizedVertical8(0, 33);
	AT_PORTABLE_TEST_ASSERT(context, ThrowsInvalidSaveState([&] { oversizedVertical8.Validate(wideAccessMask); }));

	PredictorInput invalidInsnInput;
	ATSavedTracePredictorInsn invalidInsn;
	ATDeserializer invalidInsnReader(invalidInsnInput, 0);
	AT_PORTABLE_TEST_ASSERT(context, ThrowsInvalidSaveState([&] { invalidInsn.Exchange(invalidInsnReader); }));
	PredictorInput validInsnInput;
	validInsnInput.mFlagsBitOffset = 10;
	ATSavedTracePredictorInsn validInsn;
	ATDeserializer validInsnReader(validInsnInput, 0);
	validInsn.Exchange(validInsnReader);

	ATSavedTracePredictorDelta32TablePrev8 delta32Table(0, 4);
	AT_PORTABLE_TEST_ASSERT(context, PredictorRoundTrips(delta32Table, 16));
	ATSavedTracePredictorDelta16TablePrev8 delta16Table(0, 4);
	AT_PORTABLE_TEST_ASSERT(context, PredictorRoundTrips(delta16Table, 16));
	ATSavedTracePredictorXor32Table8 xor32Table(0, 4);
	AT_PORTABLE_TEST_ASSERT(context, PredictorRoundTrips(xor32Table, 16));

	auto xor32PC = std::make_unique<ATSavedTracePredictorXor32TablePrev16>(0, 4);
	AT_PORTABLE_TEST_ASSERT(context, PredictorRoundTrips(*xor32PC, 16));
	xor32PC.reset();
	auto xor32VerticalPC = std::make_unique<ATSavedTracePredictorXor32VertDeltaTablePrev16>(0, 4);
	AT_PORTABLE_TEST_ASSERT(context, PredictorRoundTrips(*xor32VerticalPC, 16));
	xor32VerticalPC.reset();

	ATSavedTracePredictorSignMag16 signMagnitude16(0);
	AT_PORTABLE_TEST_ASSERT(context, PredictorRoundTrips(signMagnitude16, 16));
	ATSavedTracePredictorSignMag32 signMagnitude32(4);
	AT_PORTABLE_TEST_ASSERT(context, PredictorRoundTrips(signMagnitude32, 16));

	ATSavedTraceCPUChannelDetail channel;
	channel.mRowSize = 16;
	channel.mRowGroupSize = 1;
	channel.mpCodec = new ATSavedTraceCodecNull;
	channel.mColumns = {
		Column(ATSavedTraceCPUColumnType::Cycle, 0, 8),
		Column(ATSavedTraceCPUColumnType::UnhaltedCycle, 8, 8),
		Column(ATSavedTraceCPUColumnType::A, 16, 8),
		Column(ATSavedTraceCPUColumnType::PC, 24, 16),
		Column(ATSavedTraceCPUColumnType::Irq, 40, 1),
		Column(ATSavedTraceCPUColumnType::Nmi, 41, 1),
		Column(ATSavedTraceCPUColumnType::EffectiveAddress, 48, 32),
		Column(ATSavedTraceCPUColumnType::GlobalPCBase, 80, 32),
		Column(ATSavedTraceCPUColumnType::Opcode, 112, 16)
	};

	uint8 historyRows[3][16] {};
	const uint8 cycles[3] { 254, 255, 0 };
	const uint8 unhaltedCycles[3] { 255, 0, 1 };
	for(uint32 i = 0; i < 3; ++i) {
		historyRows[i][0] = cycles[i];
		historyRows[i][1] = unhaltedCycles[i];
		historyRows[i][2] = static_cast<uint8>(0x10 + i);
		VDWriteUnalignedLEU16(&historyRows[i][3], static_cast<uint16>(0x2000 + i));
		historyRows[i][5] = static_cast<uint8>(i);
		VDWriteUnalignedLEU32(&historyRows[i][6], 0x12340000U + i);
		VDWriteUnalignedLEU32(&historyRows[i][10], 0xABC00000U + i);
		VDWriteUnalignedLEU16(&historyRows[i][14], static_cast<uint16>(0xD000 + i));
	}

	ATSavedTraceCPUHistoryDecoder historyDecoder;
	historyDecoder.Init(channel);
	ATCPUHistoryEntry history[3] {};
	historyDecoder.Decode(&historyRows[0][0], history, 3);
	AT_PORTABLE_TEST_ASSERT(context, history[0].mCycle == 254);
	AT_PORTABLE_TEST_ASSERT(context, history[1].mCycle == 255);
	AT_PORTABLE_TEST_ASSERT(context, history[2].mCycle == 256);
	AT_PORTABLE_TEST_ASSERT(context, history[0].mUnhaltedCycle == 255);
	AT_PORTABLE_TEST_ASSERT(context, history[1].mUnhaltedCycle == 256);
	AT_PORTABLE_TEST_ASSERT(context, history[2].mUnhaltedCycle == 257);
	for(uint32 i = 0; i < 3; ++i) {
		AT_PORTABLE_TEST_ASSERT(context, history[i].mA == 0x10 + i);
		AT_PORTABLE_TEST_ASSERT(context, history[i].mPC == 0x2000 + i);
		AT_PORTABLE_TEST_ASSERT(context, history[i].mbIRQ == ((i & 1) != 0));
		AT_PORTABLE_TEST_ASSERT(context, history[i].mbNMI == ((i & 2) != 0));
		AT_PORTABLE_TEST_ASSERT(context, history[i].mEA == 0x12340000U + i);
		AT_PORTABLE_TEST_ASSERT(context, history[i].mGlobalPCBase == 0xABC00000U + i);
		AT_PORTABLE_TEST_ASSERT(context, VDReadUnalignedLEU16(history[i].mOpcode) == 0xD000 + i);
	}

	ATCPUHistoryEntry repeatedHistory[3] {};
	historyDecoder.Init(channel);
	historyDecoder.Decode(&historyRows[0][0], repeatedHistory, 3);
	AT_PORTABLE_TEST_ASSERT(context, repeatedHistory[2].mCycle == 256);
	AT_PORTABLE_TEST_ASSERT(context, repeatedHistory[2].mUnhaltedCycle == 257);

	ATSavedTraceCPUChannelDetail invalidChannel;
	invalidChannel.mRowSize = 16;
	invalidChannel.mRowCount = 1;
	invalidChannel.mRowGroupSize = 0;
	invalidChannel.mpCodec = new ATSavedTraceCodecNull;
	AT_PORTABLE_TEST_ASSERT(context, ThrowsInvalidSaveState([&] {
		ATSavedTraceCPUHistoryDecoder invalidDecoder;
		invalidDecoder.Init(invalidChannel);
	}));

	return true;
}
