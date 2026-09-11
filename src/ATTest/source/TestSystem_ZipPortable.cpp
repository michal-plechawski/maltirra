// Altirra portable Deflate, gzip, CRC, and ZIP archive tests

#include <algorithm>
#include <cstring>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

#include <at/attest/portabletest.h>
#include <vd2/system/file.h>
#include <vd2/system/zip.h>

namespace {
	std::vector<uint8> VDCompressRaw(
		const std::vector<uint8>& source,
		VDDeflateCompressionLevel level,
		VDDeflateChecksumMode checksumMode,
		uint32 *crc,
		uint32 *adler32) {
		VDMemoryBufferStream destination;
		VDDeflateStream compressor(destination, checksumMode, level);

		size_t offset = 0;
		bool flushed = false;
		while(offset < source.size()) {
			const size_t chunk = std::min<size_t>(
				1 + offset % 4093,
				source.size() - offset);
			compressor.Write(
				source.data() + offset,
				static_cast<sint32>(chunk));
			offset += chunk;

			if (!flushed && offset >= source.size() / 2) {
				compressor.FlushToByteBoundary();
				flushed = true;
			}
		}

		compressor.Finalize();
		if (crc)
			*crc = compressor.GetCRC();
		if (adler32)
			*adler32 = compressor.Adler32();

		const vdspan<const uint8> compressed = destination.GetBuffer();
		return std::vector<uint8>(compressed.begin(), compressed.end());
	}

	bool VDInflateRaw(
		const std::vector<uint8>& compressed,
		const std::vector<uint8>& expected) {
		VDMemoryStream source(
			compressed.data(),
			static_cast<uint32>(compressed.size()));
		VDInflateStream<false> inflater;
		inflater.Init(&source, compressed.size(), false);
		inflater.EnableCRC();
		inflater.SetExpectedCRC(VDCRCTable::CRC32.CRC(
			expected.data(), expected.size()));

		std::vector<uint8> actual(expected.size());
		size_t offset = 0;
		while(offset < actual.size()) {
			const sint32 request = static_cast<sint32>(
				std::min<size_t>(997, actual.size() - offset));
			const sint32 count = inflater.ReadData(actual.data() + offset, request);
			if (count <= 0)
				return false;
			offset += static_cast<size_t>(count);
		}

		uint8 extra = 0;
		if (inflater.ReadData(&extra, 1) != 0)
			return false;

		inflater.VerifyCRC();
		return actual == expected;
	}

	bool VDInflateKnownDeflate() {
		static constexpr uint8 kCompressed[] = {
			0x73,0xCC,0x29,0xC9,0x2C,0x2A,0x4A,0x54,
			0x28,0xC8,0x2F,0x2A,0x49,0x4C,0xCA,0x49,
			0x55,0x70,0x49,0x4D,0xCB,0x49,0x2C,0x49,
			0x55,0x28,0x49,0x2D,0x2E,0xB1,0x52,0x30,
			0x30,0x34,0x32,0x36,0x31,0x35,0x33,0xB7,
			0xB0,0x74,0xA4,0xBA,0x42,0x00
		};
		const std::string text =
			"Altirra portable Deflate test: 0123456789"
			"Altirra portable Deflate test: 0123456789"
			"Altirra portable Deflate test: 0123456789";

		return VDInflateRaw(
			std::vector<uint8>(std::begin(kCompressed), std::end(kCompressed)),
			std::vector<uint8>(text.begin(), text.end()));
	}

	bool VDInflateKnownGzip() {
		static constexpr uint8 kGzip[] = {
			0x1F,0x8B,0x08,0x00,0x00,0x00,0x00,0x00,
			0x02,0x03,0x4B,0xAF,0xCA,0x2C,0x50,0xC8,
			0xCC,0x2B,0x49,0x2D,0xCA,0x2F,0x48,0x2D,
			0x4A,0x4C,0xCA,0xCC,0xC9,0x2C,0xA9,0x04,
			0x00,0x3A,0x61,0x47,0x87,0x15,0x00,0x00,
			0x00
		};
		static constexpr char kExpected[] = "gzip interoperability";

		VDMemoryStream source(kGzip, sizeof kGzip);
		VDGUnzipStream inflater(&source, sizeof kGzip);
		inflater.EnableCRC();
		inflater.SetExpectedCRC(0x8747613A);

		char actual[sizeof kExpected - 1] {};
		inflater.Read(actual, sizeof actual);
		uint8 extra = 0;
		if (inflater.ReadData(&extra, 1) != 0)
			return false;
		inflater.VerifyCRC();

		return !memcmp(actual, kExpected, sizeof actual)
			&& !*inflater.GetFilename();
	}

	bool VDReadKnownZip() {
		static constexpr uint8 kZip[] = {
			0x50,0x4B,0x03,0x04,0x14,0x00,0x00,0x00,
			0x08,0x00,0x83,0x18,0x22,0x50,0x4D,0xD1,
			0x58,0xC3,0x16,0x00,0x00,0x00,0x14,0x00,
			0x00,0x00,0x0C,0x00,0x00,0x00,0x65,0x78,
			0x74,0x65,0x72,0x6E,0x61,0x6C,0x2E,0x74,
			0x78,0x74,0xAB,0xCA,0x2C,0x50,0xC8,0xCC,
			0x2B,0x49,0x2D,0xCA,0x2F,0x48,0x2D,0x4A,
			0x4C,0xCA,0xCC,0xC9,0x2C,0xA9,0x04,0x00,
			0x50,0x4B,0x01,0x02,0x14,0x03,0x14,0x00,
			0x00,0x00,0x08,0x00,0x83,0x18,0x22,0x50,
			0x4D,0xD1,0x58,0xC3,0x16,0x00,0x00,0x00,
			0x14,0x00,0x00,0x00,0x0C,0x00,0x00,0x00,
			0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
			0x80,0x01,0x00,0x00,0x00,0x00,0x65,0x78,
			0x74,0x65,0x72,0x6E,0x61,0x6C,0x2E,0x74,
			0x78,0x74,0x50,0x4B,0x05,0x06,0x00,0x00,
			0x00,0x00,0x01,0x00,0x01,0x00,0x3A,0x00,
			0x00,0x00,0x40,0x00,0x00,0x00,0x00,0x00
		};
		static constexpr char kExpected[] = "zip interoperability";

		VDMemoryStream source(kZip, sizeof kZip);
		VDZipArchive archive;
		archive.Init(&source);
		if (archive.GetFileCount() != 1)
			return false;

		const sint32 index = archive.FindFile("external.txt");
		if (index < 0 || archive.FindFile(L"EXTERNAL.TXT", false) != index)
			return false;

		std::unique_ptr<IVDInflateStream> decoded(
			archive.OpenDecodedStream(index, true));
		char actual[sizeof kExpected - 1] {};
		decoded->Read(actual, sizeof actual);
		decoded->VerifyCRC();
		return !memcmp(actual, kExpected, sizeof actual);
	}

	bool VDRejectsTruncatedDeflate() {
		static constexpr uint8 kTruncated[] = {
			0x01,0x10,0x00,0xEF,0xFF
		};
		VDMemoryStream source(kTruncated, sizeof kTruncated);
		VDInflateStream<false> inflater;
		inflater.Init(&source, sizeof kTruncated, false);
		uint8 output[16] {};

		try {
			inflater.Read(output, sizeof output);
		} catch(const MyError&) {
			return true;
		}

		return false;
	}

	bool VDRejectsBadCRC(
		const std::vector<uint8>& compressed,
		const std::vector<uint8>& expected) {
		VDMemoryStream source(
			compressed.data(),
			static_cast<uint32>(compressed.size()));
		VDInflateStream<false> inflater;
		inflater.Init(&source, compressed.size(), false);
		inflater.EnableCRC();
		inflater.SetExpectedCRC(
			VDCRCTable::CRC32.CRC(expected.data(), expected.size()) ^ 1);

		std::vector<uint8> actual(expected.size());
		inflater.Read(actual.data(), static_cast<sint32>(actual.size()));
		try {
			inflater.VerifyCRC();
		} catch(const MyError&) {
			return true;
		}

		return false;
	}

	bool VDTestDeflateResetChecksum() {
		static constexpr uint8 kFirst[] = { 1, 2, 3, 4, 5 };
		static constexpr uint8 kSecond[] = { 9, 8, 7 };
		VDMemoryBufferStream destination;
		VDDeflateStream compressor(
			destination,
			VDDeflateChecksumMode::Adler32,
			VDDeflateCompressionLevel::Quick);

		compressor.Write(kFirst, sizeof kFirst);
		compressor.Finalize();
		if (compressor.Adler32()
			!= VDAdler32Checker::Adler32(kFirst, sizeof kFirst))
			return false;

		destination.Clear();
		compressor.Reset(VDDeflateCompressionLevel::Best);
		compressor.Write(kSecond, sizeof kSecond);
		compressor.Finalize();
		if (compressor.Adler32()
			!= VDAdler32Checker::Adler32(kSecond, sizeof kSecond))
			return false;

		const vdspan<const uint8> compressed = destination.GetBuffer();
		return VDInflateRaw(
			std::vector<uint8>(compressed.begin(), compressed.end()),
			std::vector<uint8>(std::begin(kSecond), std::end(kSecond)));
	}

	bool VDTestInflateReinitialization() {
		const std::vector<uint8> first { 1, 3, 5, 7, 9 };
		const std::vector<uint8> second { 2, 4, 6, 8 };
		const std::vector<uint8> firstCompressed = VDCompressRaw(
			first,
			VDDeflateCompressionLevel::Quick,
			VDDeflateChecksumMode::None,
			nullptr,
			nullptr);
		const std::vector<uint8> secondCompressed = VDCompressRaw(
			second,
			VDDeflateCompressionLevel::Best,
			VDDeflateChecksumMode::None,
			nullptr,
			nullptr);
		VDInflateStream<false> inflater;

		const auto inflate = [&inflater](
			const std::vector<uint8>& compressed,
			const std::vector<uint8>& expected) {
			VDMemoryStream source(
				compressed.data(),
				static_cast<uint32>(compressed.size()));
			inflater.Init(&source, compressed.size(), false);
			if (inflater.Pos())
				return false;
			inflater.EnableCRC();
			inflater.SetExpectedCRC(
				VDCRCTable::CRC32.CRC(expected.data(), expected.size()));
			std::vector<uint8> actual(expected.size());
			inflater.Read(actual.data(), static_cast<sint32>(actual.size()));
			inflater.VerifyCRC();
			return actual == expected
				&& inflater.Pos() == static_cast<sint64>(expected.size());
		};

		return inflate(firstCompressed, first)
			&& inflate(secondCompressed, second);
	}

	bool VDTestZipArchive(const std::vector<uint8>& payload) {
		VDMemoryBufferStream stream;
		std::unique_ptr<IVDZipArchiveWriter> writer(
			VDCreateZipArchiveWriter(stream));

		VDDeflateStream& dataFile = writer->BeginFile(
			L"//Folder\\\\\u017B\u00F3\u0142\u0107.bin",
			VDDeflateCompressionLevel::Best);
		dataFile.Write(payload.data(), static_cast<sint32>(payload.size()));
		writer->EndFile();

		writer->BeginFile(L"empty.txt", VDDeflateCompressionLevel::Store);
		writer->EndFile();
		writer->Finalize();
		writer.reset();

		const vdspan<const uint8> bytes = stream.GetBuffer();
		if (bytes.size() < 22
			|| memcmp(bytes.data(), "PK\x03\x04", 4)
			|| memcmp(bytes.data() + bytes.size() - 22, "PK\x05\x06", 4))
			return false;

		stream.Seek(0);
		VDZipArchive archive;
		archive.Init(&stream);
		if (archive.GetFileCount() != 2)
			return false;

		const sint32 dataIndex = archive.FindFile(L"folder/\u017B\u00F3\u0142\u0107.bin", false);
		if (dataIndex < 0
			|| archive.FindFile(L"folder/\u017B\u00F3\u0142\u0107.bin", true) >= 0)
			return false;

		const VDZipArchive::FileInfo& info = archive.GetFileInfo(dataIndex);
		if (!info.mbSupported || !info.mbPacked
			|| info.mUncompressedSize != payload.size())
			return false;

		std::unique_ptr<IVDInflateStream> decoded(
			archive.OpenDecodedStream(dataIndex, true));
		std::vector<uint8> decodedPayload(payload.size());
		decoded->Read(
			decodedPayload.data(),
			static_cast<sint32>(decodedPayload.size()));
		decoded->VerifyCRC();
		if (decodedPayload != payload)
			return false;
		decoded.reset();

		vdfastvector<uint8> rawData;
		if (archive.ReadRawStream(dataIndex, rawData, true))
			return false;
		archive.DecompressStream(dataIndex, rawData);
		if (rawData.size() != payload.size()
			|| memcmp(rawData.data(), payload.data(), payload.size()))
			return false;

		const sint32 emptyIndex = archive.FindFile("empty.txt");
		if (emptyIndex < 0 || archive.GetFileInfo(emptyIndex).mUncompressedSize)
			return false;
		decoded.reset(archive.OpenDecodedStream(emptyIndex, true));
		uint8 extra = 0;
		if (decoded->ReadData(&extra, 1) != 0)
			return false;
		decoded->VerifyCRC();

		return true;
	}
}

bool ATTestSystemZip(ATPortableTestContext& context) {
	try {
		static constexpr char kCheckText[] = "123456789";
		AT_PORTABLE_TEST_ASSERT(context,
			VDCRCTable::CRC32.CRC(kCheckText, 9) == 0xCBF43926);
		AT_PORTABLE_TEST_ASSERT(context,
			VDAdler32Checker::Adler32(kCheckText, 9) == 0x091E01DE);

		uint8 crcInput[1024];
		for(size_t i = 0; i < std::size(crcInput); ++i)
			crcInput[i] = static_cast<uint8>(i + 1);
		for(size_t length = 0; length <= std::size(crcInput); ++length) {
			VDCRCChecker checker(VDCRCTable::CRC32);
			checker.Process(crcInput, static_cast<sint32>(length));
			AT_PORTABLE_TEST_ASSERT(context,
				checker.CRC() == VDCRCTable::CRC32.CRC(crcInput, length));
		}
		for(size_t offset = 0; offset < 8; ++offset) {
			const size_t length = 1001 - offset;
			VDCRCChecker checker(VDCRCTable::CRC32);
			checker.Process(
				crcInput + offset,
				static_cast<sint32>(length));
			AT_PORTABLE_TEST_ASSERT(context,
				checker.CRC()
					== VDCRCTable::CRC32.CRC(crcInput + offset, length));

			checker.Init();
			for(size_t i = 0; i < length; ++i)
				checker.Process(crcInput + offset + i, 1);
			AT_PORTABLE_TEST_ASSERT(context,
				checker.CRC()
					== VDCRCTable::CRC32.CRC(crcInput + offset, length));
		}

		std::vector<uint8> payload(180000);
		uint32 randomState = 1;
		for(size_t i = 0; i < payload.size(); ++i) {
			randomState = randomState * 1664525 + 1013904223;
			payload[i] = (i / 4096) & 1
				? static_cast<uint8>(randomState >> 24)
				: static_cast<uint8>(i % 37);
		}

		for(const VDDeflateCompressionLevel level : {
			VDDeflateCompressionLevel::Quick,
			VDDeflateCompressionLevel::Best,
			VDDeflateCompressionLevel::Store }) {
			uint32 crc = 0;
			uint32 adler32 = 0;
			const VDDeflateChecksumMode checksumMode =
				level == VDDeflateCompressionLevel::Quick
					? VDDeflateChecksumMode::Adler32
					: VDDeflateChecksumMode::CRC32;
			const std::vector<uint8> compressed = VDCompressRaw(
				payload, level, checksumMode, &crc, &adler32);
			AT_PORTABLE_TEST_ASSERT(context, !compressed.empty());
			AT_PORTABLE_TEST_ASSERT(context, VDInflateRaw(compressed, payload));
			if (checksumMode == VDDeflateChecksumMode::CRC32) {
				AT_PORTABLE_TEST_ASSERT(context,
					crc == VDCRCTable::CRC32.CRC(payload.data(), payload.size()));
			} else {
				AT_PORTABLE_TEST_ASSERT(context,
					adler32 == VDAdler32Checker::Adler32(
						payload.data(), payload.size()));
			}

			if (level == VDDeflateCompressionLevel::Best)
				AT_PORTABLE_TEST_ASSERT(context,
					VDRejectsBadCRC(compressed, payload));
		}

		const std::vector<uint8> empty;
		AT_PORTABLE_TEST_ASSERT(context,
			VDInflateRaw(
				VDCompressRaw(
					empty,
					VDDeflateCompressionLevel::Best,
					VDDeflateChecksumMode::None,
					nullptr,
					nullptr),
				empty));
		AT_PORTABLE_TEST_ASSERT(context, VDInflateKnownDeflate());
		AT_PORTABLE_TEST_ASSERT(context, VDInflateKnownGzip());
		AT_PORTABLE_TEST_ASSERT(context, VDReadKnownZip());
		AT_PORTABLE_TEST_ASSERT(context, VDRejectsTruncatedDeflate());
		AT_PORTABLE_TEST_ASSERT(context, VDTestDeflateResetChecksum());
		AT_PORTABLE_TEST_ASSERT(context, VDTestInflateReinitialization());
		AT_PORTABLE_TEST_ASSERT(context, VDTestZipArchive(payload));
	} catch(...) {
		AT_PORTABLE_TEST_ASSERT(context, false);
	}

	return true;
}
