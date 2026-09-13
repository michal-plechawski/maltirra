// Portable end-to-end tests for the PNG image encoder.

#include <array>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <vector>

#include <at/attest/portabletest.h>
#include <vd2/Kasumi/pixmap.h>
#include <vd2/system/binary.h>
#include <vd2/system/file.h>
#include <vd2/system/zip.h>

#include <encode_png.h>

bool ATTestAltirraPNGEncoder(ATPortableTestContext& context) {
	const uint32 pixels[4] = {
		0x00112233, 0x00445566,
		0x00778899, 0x00AABBCC
	};
	VDPixmap pixmap {};
	pixmap.data = const_cast<uint32 *>(pixels);
	pixmap.w = 2;
	pixmap.h = 2;
	pixmap.pitch = 2 * sizeof(uint32);
	pixmap.format = nsVDPixmap::kPixFormat_XRGB8888;

	std::unique_ptr<IVDImageEncoderPNG> encoder(VDCreateImageEncoderPNG());
	AT_PORTABLE_TEST_ASSERT(context, encoder != nullptr);

	for (bool quick : { false, true }) {
		if (quick)
			encoder->SetPAR(2.0);

		const void *encoded = nullptr;
		uint32 encodedLength = 0;
		encoder->Encode(pixmap, encoded, encodedLength, quick);
		AT_PORTABLE_TEST_ASSERT(context, encoded != nullptr);
		AT_PORTABLE_TEST_ASSERT(context, encodedLength > 8);

		const auto *png = static_cast<const uint8 *>(encoded);
		static constexpr uint8 kSignature[8] = {
			137, 80, 78, 71, 13, 10, 26, 10
		};
		AT_PORTABLE_TEST_ASSERT(context, !memcmp(png, kSignature, sizeof kSignature));

		bool sawHeader = false;
		bool sawPixelsPerUnit = false;
		bool sawEnd = false;
		std::vector<uint8> compressed;
		size_t offset = sizeof kSignature;
		while(offset < encodedLength) {
			AT_PORTABLE_TEST_ASSERT(context, encodedLength - offset >= 12);
			const uint32 chunkLength = VDReadUnalignedBEU32(png + offset);
			AT_PORTABLE_TEST_ASSERT(context, chunkLength <= encodedLength - offset - 12);
			const uint8 *chunkType = png + offset + 4;
			const uint8 *chunkData = chunkType + 4;
			AT_PORTABLE_TEST_ASSERT(context,
				VDCRCTable::CRC32.CRC(chunkType, chunkLength + 4)
					== VDReadUnalignedBEU32(chunkData + chunkLength));

			if (!memcmp(chunkType, "IHDR", 4)) {
				AT_PORTABLE_TEST_ASSERT(context, !sawHeader && chunkLength == 13);
				AT_PORTABLE_TEST_ASSERT(context, VDReadUnalignedBEU32(chunkData) == 2);
				AT_PORTABLE_TEST_ASSERT(context, VDReadUnalignedBEU32(chunkData + 4) == 2);
				AT_PORTABLE_TEST_ASSERT(context, chunkData[8] == 8 && chunkData[9] == 2);
				AT_PORTABLE_TEST_ASSERT(context,
					chunkData[10] == 0 && chunkData[11] == 0 && chunkData[12] == 0);
				sawHeader = true;
			} else if (!memcmp(chunkType, "pHYs", 4)) {
				AT_PORTABLE_TEST_ASSERT(context, !sawPixelsPerUnit && chunkLength == 9);
				AT_PORTABLE_TEST_ASSERT(context, VDReadUnalignedBEU32(chunkData) == 1);
				AT_PORTABLE_TEST_ASSERT(context, VDReadUnalignedBEU32(chunkData + 4) == 2);
				AT_PORTABLE_TEST_ASSERT(context, chunkData[8] == 0);
				sawPixelsPerUnit = true;
			} else if (!memcmp(chunkType, "IDAT", 4)) {
				compressed.insert(compressed.end(), chunkData, chunkData + chunkLength);
			} else if (!memcmp(chunkType, "IEND", 4)) {
				AT_PORTABLE_TEST_ASSERT(context, chunkLength == 0);
				sawEnd = true;
			}
			offset += static_cast<size_t>(chunkLength) + 12;
		}
		AT_PORTABLE_TEST_ASSERT(context, offset == encodedLength);
		AT_PORTABLE_TEST_ASSERT(context, sawHeader && sawEnd);
		AT_PORTABLE_TEST_ASSERT(context, sawPixelsPerUnit == quick);
		AT_PORTABLE_TEST_ASSERT(context, compressed.size() >= 6);
		AT_PORTABLE_TEST_ASSERT(context, (compressed[0] & 15) == 8);
		AT_PORTABLE_TEST_ASSERT(context, ((compressed[0] << 8) + compressed[1]) % 31 == 0);

		VDMemoryStream source(compressed.data() + 2,
			static_cast<uint32>(compressed.size() - 6));
		VDInflateStream<false> inflater;
		inflater.Init(&source, compressed.size() - 6, false);
		std::array<uint8, 14> filtered {};
		size_t filled = 0;
		while(filled < filtered.size()) {
			const sint32 count = inflater.ReadData(
				filtered.data() + filled,
				static_cast<sint32>(filtered.size() - filled));
			AT_PORTABLE_TEST_ASSERT(context, count > 0);
			filled += count;
		}
		uint8 extra = 0;
		AT_PORTABLE_TEST_ASSERT(context, inflater.ReadData(&extra, 1) == 0);
		AT_PORTABLE_TEST_ASSERT(context,
			VDAdler32Checker::Adler32(filtered.data(), filtered.size())
				== VDReadUnalignedBEU32(compressed.data() + compressed.size() - 4));

		std::array<uint8, 12> decoded {};
		for (size_t row = 0; row < 2; ++row) {
			const uint8 filter = filtered[row * 7];
			AT_PORTABLE_TEST_ASSERT(context, filter <= 4);
			for (size_t col = 0; col < 6; ++col) {
				const uint8 left = col >= 3 ? decoded[row * 6 + col - 3] : 0;
				const uint8 up = row ? decoded[col] : 0;
				const uint8 upperLeft = row && col >= 3 ? decoded[col - 3] : 0;
				uint8 predictor = 0;
				switch(filter) {
				case 1: predictor = left; break;
				case 2: predictor = up; break;
				case 3: predictor = (static_cast<unsigned>(left) + up) / 2; break;
				case 4: {
					const int p = static_cast<int>(left) + up - upperLeft;
					const int a = std::abs(p - left);
					const int b = std::abs(p - up);
					const int c = std::abs(p - upperLeft);
					predictor = a <= b && a <= c ? left : b <= c ? up : upperLeft;
					break;
				}
				}
				decoded[row * 6 + col] = filtered[row * 7 + col + 1] + predictor;
			}
		}
		static constexpr std::array<uint8, 12> kExpected {
			0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
			0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC
		};
		AT_PORTABLE_TEST_ASSERT(context, decoded == kExpected);
	}

	return true;
}
