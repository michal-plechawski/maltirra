// Portable tests for the legacy binary save-state reader.

#include <array>
#include <cstring>
#include <vector>

#include <at/atcore/savestate.h>
#include <at/attest/portabletest.h>
#include <vd2/system/binary.h>
#include <vd2/system/VDString.h>

#include <savestate.h>

namespace {
	bool ThrowsInvalidChunk(const uint8 *data, uint32 length, uint32 chunkLength) {
		ATSaveStateReader reader(data, length);
		try {
			reader.OpenChunk(chunkLength);
		} catch(const ATInvalidSaveStateException&) {
			return true;
		}
		return false;
	}

	bool ThrowsInvalidRead(const uint8 *data, uint32 length, uint32 readLength) {
		ATSaveStateReader reader(data, length);
		std::vector<uint8> destination(readLength);
		try {
			reader.ReadData(destination.data(), readLength);
		} catch(const ATInvalidSaveStateException&) {
			return true;
		}
		return false;
	}

	bool ThrowsInvalidString(const uint8 *data, uint32 length) {
		ATSaveStateReader reader(data, length);
		VDStringW value;
		try {
			reader.ReadString(value);
		} catch(const ATInvalidSaveStateException&) {
			return true;
		}
		return false;
	}

	struct HandlerTracker {
		void OnFirst(ATSaveStateReader& reader) {
			mCalls.push_back(100 + reader.ReadUint8());
		}

		void OnSecond(ATSaveStateReader& reader) {
			mCalls.push_back(200 + reader.ReadUint8());
		}

		std::vector<int> mCalls;
	};
}

bool ATTestAltirraSaveStateReader(ATPortableTestContext& context) {
	std::array<uint8, 28> primitiveData {};
	primitiveData[0] = 2;
	primitiveData[1] = 0xFE;
	VDWriteUnalignedLEU16(primitiveData.data() + 2, 0x8123);
	VDWriteUnalignedLEU32(primitiveData.data() + 4, 0x89ABCDEF);
	VDWriteUnalignedLEU64(primitiveData.data() + 8, UINT64_C(0x0123456789ABCDEF));
	primitiveData[16] = 4;
	memcpy(primitiveData.data() + 17, "M\xC3\xB3j", 4);
	primitiveData[21] = 0x82;
	primitiveData[22] = 0x01;
	memset(primitiveData.data() + 23, 'x', 5);

	ATSaveStateReader reader(primitiveData.data(), primitiveData.size());
	AT_PORTABLE_TEST_ASSERT(context, reader.GetAvailable() == primitiveData.size());
	AT_PORTABLE_TEST_ASSERT(context, reader.CheckAvailable(primitiveData.size()));
	AT_PORTABLE_TEST_ASSERT(context, !reader.CheckAvailable(primitiveData.size() + 1));
	AT_PORTABLE_TEST_ASSERT(context, reader.ReadBool());
	AT_PORTABLE_TEST_ASSERT(context, reader.ReadSint8() == -2);
	AT_PORTABLE_TEST_ASSERT(context, reader.ReadUint16() == 0x8123);
	AT_PORTABLE_TEST_ASSERT(context, reader.ReadUint32() == 0x89ABCDEF);
	AT_PORTABLE_TEST_ASSERT(context,
		reader.ReadUint64() == UINT64_C(0x0123456789ABCDEF));
	VDStringW text;
	reader.ReadString(text);
	AT_PORTABLE_TEST_ASSERT(context, text == L"M\u00F3j");
	AT_PORTABLE_TEST_ASSERT(context, reader.GetAvailable() == 7);

	// A two-byte varint length claims 130 bytes, but only five remain.
	AT_PORTABLE_TEST_ASSERT(context,
		ThrowsInvalidString(primitiveData.data() + 21, 7));

	static constexpr uint8 kChunkData[] = { 10, 20, 30, 40 };
	ATSaveStateReader chunkReader(kChunkData, sizeof kChunkData);
	AT_PORTABLE_TEST_ASSERT(context, chunkReader.ReadUint8() == 10);
	chunkReader.OpenChunk(2);
	AT_PORTABLE_TEST_ASSERT(context, chunkReader.GetAvailable() == 2);
	AT_PORTABLE_TEST_ASSERT(context, chunkReader.ReadUint8() == 20);
	AT_PORTABLE_TEST_ASSERT(context, chunkReader.ReadUint8() == 30);
	AT_PORTABLE_TEST_ASSERT(context, !chunkReader.CheckAvailable(1));
	chunkReader.CloseChunk();
	AT_PORTABLE_TEST_ASSERT(context, chunkReader.ReadUint8() == 40);
	AT_PORTABLE_TEST_ASSERT(context, !chunkReader.GetAvailable());
	AT_PORTABLE_TEST_ASSERT(context, ThrowsInvalidChunk(kChunkData, 4, 5));
	AT_PORTABLE_TEST_ASSERT(context, ThrowsInvalidRead(kChunkData, 4, 5));

	static constexpr uint32 kChunkId = VDMAKEFOURCC('T', 'E', 'S', 'T');
	static constexpr uint8 kHandlerData[] = { 1, 2, 3, 4 };
	HandlerTracker tracker;
	ATSaveStateReader handlerReader(kHandlerData, sizeof kHandlerData);
	handlerReader.RegisterHandlerMethod(
		kATSaveStateSection_Arch, kChunkId, &tracker, &HandlerTracker::OnFirst);
	handlerReader.RegisterHandlerMethod(
		kATSaveStateSection_Arch, kChunkId, &tracker, &HandlerTracker::OnSecond);
	handlerReader.RegisterHandlerMethod(
		kATSaveStateSection_End, 0, &tracker, &HandlerTracker::OnFirst);
	handlerReader.RegisterHandlerMethod(
		kATSaveStateSection_End, 0, &tracker, &HandlerTracker::OnSecond);

	// Named chunks dispatch one handler at a time; zero is a broadcast ID.
	handlerReader.DispatchChunk(kATSaveStateSection_Arch, kChunkId);
	handlerReader.DispatchChunk(kATSaveStateSection_Arch, kChunkId);
	handlerReader.DispatchChunk(kATSaveStateSection_Arch, kChunkId);
	handlerReader.DispatchChunk(kATSaveStateSection_End, 0);
	AT_PORTABLE_TEST_ASSERT(context, tracker.mCalls.size() == 4);
	AT_PORTABLE_TEST_ASSERT(context, tracker.mCalls[0] == 101);
	AT_PORTABLE_TEST_ASSERT(context, tracker.mCalls[1] == 202);
	AT_PORTABLE_TEST_ASSERT(context, tracker.mCalls[2] == 103);
	AT_PORTABLE_TEST_ASSERT(context, tracker.mCalls[3] == 204);
	AT_PORTABLE_TEST_ASSERT(context, !handlerReader.GetAvailable());
	return true;
}
