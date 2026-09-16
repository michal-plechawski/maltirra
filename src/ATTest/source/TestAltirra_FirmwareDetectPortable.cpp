// Portable tests for firmware image recognition and OS ROM heuristics.

#include <array>
#include <cstring>
#include <vector>

#include <at/attest/portabletest.h>
#include <firmwaredetect.h>

namespace {
	uint32 ReferenceCRC32(const void *data, size_t len) {
		const auto *src = static_cast<const uint8 *>(data);
		uint32 crc = UINT32_C(0xFFFFFFFF);

		while(len--) {
			crc ^= *src++;

			for(int i = 0; i < 8; ++i)
				crc = (crc >> 1) ^ (UINT32_C(0xEDB88320) & (0 - (crc & 1)));
		}

		return ~crc;
	}

	bool ForgeCRC32(std::vector<uint8>& data, size_t patchOffset, uint32 targetCRC) {
		if (patchOffset + 4 > data.size())
			return false;

		const uint32 baseCRC = ReferenceCRC32(data.data(), data.size());
		std::array<uint32, 32> basis {};
		std::array<uint32, 32> basisSelection {};

		for(uint32 bit = 0; bit < 32; ++bit) {
			data[patchOffset + bit / 8] ^= (uint8)(1U << (bit & 7));
			uint32 value = ReferenceCRC32(data.data(), data.size()) ^ baseCRC;
			data[patchOffset + bit / 8] ^= (uint8)(1U << (bit & 7));
			uint32 selection = UINT32_C(1) << bit;

			for(int row = 31; row >= 0; --row) {
				if (!(value & (UINT32_C(1) << row)))
					continue;

				if (basis[row]) {
					value ^= basis[row];
					selection ^= basisSelection[row];
				} else {
					basis[row] = value;
					basisSelection[row] = selection;
					break;
				}
			}
		}

		uint32 value = targetCRC ^ baseCRC;
		uint32 selection = 0;
		for(int row = 31; row >= 0; --row) {
			if (!(value & (UINT32_C(1) << row)))
				continue;

			if (!basis[row])
				return false;

			value ^= basis[row];
			selection ^= basisSelection[row];
		}

		if (value)
			return false;

		for(uint32 bit = 0; bit < 32; ++bit) {
			if (selection & (UINT32_C(1) << bit))
				data[patchOffset + bit / 8] ^= (uint8)(1U << (bit & 7));
		}

		return ReferenceCRC32(data.data(), data.size()) == targetCRC;
	}

	void WriteLE16(uint8 *dst, uint16 value) {
		dst[0] = (uint8)value;
		dst[1] = (uint8)(value >> 8);
	}

	void MakePlausibleOSROM(uint8 *d800Segment, uint16 baseAddress) {
		for(int offset = 0; offset < 6; offset += 2)
			WriteLE16(d800Segment + 0x27FA + offset, baseAddress);

		for(int deviceOffset = 0; deviceOffset < 0x50; deviceOffset += 0x10) {
			for(int handlerOffset = 0; handlerOffset < 12; handlerOffset += 2)
				WriteLE16(d800Segment + 0xC00 + deviceOffset + handlerOffset, (uint16)(baseAddress - 1));
		}

		for(int vectorOffset = 0xC50; vectorOffset < 0xC80; vectorOffset += 3) {
			d800Segment[vectorOffset] = 0x4C;
			WriteLE16(d800Segment + vectorOffset + 1, baseAddress);
		}
	}
}

bool ATTestAltirraFirmwareDetect(ATPortableTestContext& context) {
	static constexpr uint32 kAcceptedSizes[] {
		1024, 2048, 4096, 6144, 8192, 10240, 16384, 32768, 65536
	};

	for(uint32 size : kAcceptedSizes)
		AT_PORTABLE_TEST_ASSERT(context, ATFirmwareAutodetectCheckSize(size));

	static constexpr uint64 kRejectedSizes[] {
		0, 1, 1023, 1025, 3072, 12288, 65535, 65537, UINT64_C(0x100000000)
	};

	for(uint64 size : kRejectedSizes)
		AT_PORTABLE_TEST_ASSERT(context, !ATFirmwareAutodetectCheckSize(size));

	size_t knownCount = 0;
	while(const ATKnownFirmware *known = ATFirmwareGetKnownByIndex(knownCount)) {
		AT_PORTABLE_TEST_ASSERT(context, known->mpDesc && *known->mpDesc);
		AT_PORTABLE_TEST_ASSERT(context, known->mType > kATFirmwareType_Unknown && known->mType < kATFirmwareTypeCount);
		AT_PORTABLE_TEST_ASSERT(context, ATFirmwareAutodetectCheckSize(known->mSize));
		++knownCount;
	}
	AT_PORTABLE_TEST_ASSERT(context, knownCount >= 70);
	AT_PORTABLE_TEST_ASSERT(context, !ATFirmwareGetKnownByIndex(knownCount + 100));

	const ATKnownFirmware *firstKnown = ATFirmwareGetKnownByIndex(0);
	AT_PORTABLE_TEST_ASSERT(context, firstKnown && firstKnown->mSize == 2048);
	std::vector<uint8> knownImage(firstKnown->mSize);
	AT_PORTABLE_TEST_ASSERT(context, ForgeCRC32(knownImage, knownImage.size() - 4, firstKnown->mCRC));

	ATFirmwareInfo info {};
	ATSpecificFirmwareType specificType = kATSpecificFirmwareTypeCount;
	sint32 knownIndex = -2;
	AT_PORTABLE_TEST_ASSERT(context, ATFirmwareAutodetect(knownImage.data(), (uint32)knownImage.size(), info, specificType, knownIndex) == ATFirmwareDetection::SpecificImage);
	AT_PORTABLE_TEST_ASSERT(context, knownIndex == 0);
	AT_PORTABLE_TEST_ASSERT(context, info.mType == kATFirmwareType_Kernel5200);
	AT_PORTABLE_TEST_ASSERT(context, info.mName == L"Atari 5200 OS (4-port)");
	AT_PORTABLE_TEST_ASSERT(context, info.mbVisible && info.mFlags == 0);
	AT_PORTABLE_TEST_ASSERT(context, specificType == kATSpecificFirmwareType_None);

	knownImage[0] ^= 1;
	ATFirmwareInfo changedInfo {};
	AT_PORTABLE_TEST_ASSERT(context, ATFirmwareAutodetect(knownImage.data(), (uint32)knownImage.size(), changedInfo, specificType, knownIndex) == ATFirmwareDetection::None);
	AT_PORTABLE_TEST_ASSERT(context, knownIndex == -1 && specificType == kATSpecificFirmwareType_None);

	std::vector<uint8> os800(10 * 1024);
	MakePlausibleOSROM(os800.data(), 0xD800);
	ATFirmwareInfo os800Info {};
	AT_PORTABLE_TEST_ASSERT(context, ATFirmwareAutodetect(os800.data(), (uint32)os800.size(), os800Info, specificType, knownIndex) == ATFirmwareDetection::TypeOnly);
	AT_PORTABLE_TEST_ASSERT(context, os800Info.mType == kATFirmwareType_Kernel800_OSB);

	std::vector<uint8> osxl(16 * 1024);
	MakePlausibleOSROM(osxl.data() + 6 * 1024, 0xC000);
	ATFirmwareInfo osxlInfo {};
	AT_PORTABLE_TEST_ASSERT(context, ATFirmwareAutodetect(osxl.data(), (uint32)osxl.size(), osxlInfo, specificType, knownIndex) == ATFirmwareDetection::TypeOnly);
	AT_PORTABLE_TEST_ASSERT(context, osxlInfo.mType == kATFirmwareType_KernelXL);

	os800[0xC50] = 0;
	ATFirmwareInfo malformedInfo {};
	AT_PORTABLE_TEST_ASSERT(context, ATFirmwareAutodetect(os800.data(), (uint32)os800.size(), malformedInfo, specificType, knownIndex) == ATFirmwareDetection::None);

	std::vector<uint8> happyFunctional(3072);
	AT_PORTABLE_TEST_ASSERT(context, ForgeCRC32(happyFunctional, happyFunctional.size() - 4, UINT32_C(0xE52A98B2)));
	happyFunctional[0x3FE] = 0x34;
	happyFunctional[0x3FF] = 0x12;
	std::vector<uint8> happyImage(4096);
	memcpy(happyImage.data() + 1024, happyFunctional.data(), happyFunctional.size());

	ATFirmwareInfo happyInfo {};
	AT_PORTABLE_TEST_ASSERT(context, ATFirmwareAutodetect(happyImage.data(), (uint32)happyImage.size(), happyInfo, specificType, knownIndex) == ATFirmwareDetection::SpecificImage);
	AT_PORTABLE_TEST_ASSERT(context, happyInfo.mType == kATFirmwareType_Happy810);
	AT_PORTABLE_TEST_ASSERT(context, happyInfo.mName == L"Happy 810 firmware (pre-v7 serial $1234; 4K non-original)");
	AT_PORTABLE_TEST_ASSERT(context, knownIndex == -1 && specificType == kATSpecificFirmwareType_None);

	return true;
}
