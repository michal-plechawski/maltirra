// Portable tests for SAP-to-XEX conversion.

#include <algorithm>
#include <array>
#include <cstring>
#include <string>
#include <vector>

#include <at/attest/portabletest.h>
#include <vd2/system/error.h>

#include <sapconverter.h>

namespace {
	std::vector<uint8> MakeSAP(const char *header, const std::vector<uint8>& payload = {}) {
		std::vector<uint8> sap(header, header + strlen(header));
		sap.insert(sap.end(), payload.begin(), payload.end());
		return sap;
	}

	bool ConversionFails(const std::vector<uint8>& sap) {
		vdfastvector<uint8> result;
		try {
			ATConvertSAPToPlayer(sap.data(), (uint32)sap.size(), result);
		} catch(const MyError&) {
			return true;
		}

		return false;
	}
}

bool ATTestAltirraSAPConverter(ATPortableTestContext& context) {
	const auto typeB = MakeSAP(
		"SAP\r\n"
		"TYPE B\r\n"
		"INIT 2000\r\n"
		"PLAYER 2100\r\n"
		"NAME \"Tune\"\r\n"
		"AUTHOR \"Coder\"\r\n"
		"SONGS 12\r\n"
		"DEFSONG 3\r\n"
		"FASTPLAY 100\r\n"
		"\r\n",
		{ 0xff, 0xff, 0x00, 0x20, 0x00, 0x20, 0xaa });

	vdfastvector<uint8> result;
	ATConvertSAPToPlayer(typeB.data(), (uint32)typeB.size(), result);
	AT_PORTABLE_TEST_ASSERT(context, result.size() > 100);
	AT_PORTABLE_TEST_ASSERT(context, result[7] == 0x00 && result[8] == 0x20);
	AT_PORTABLE_TEST_ASSERT(context, result[10] == 0x00 && result[11] == 0x21);
	AT_PORTABLE_TEST_ASSERT(context, result[12] == 50);
	AT_PORTABLE_TEST_ASSERT(context, result[13] == 3 && result[14] == 12);
	const std::array<uint8, 4> internalName { 0x34, 0x75, 0x6e, 0x65 };
	AT_PORTABLE_TEST_ASSERT(context, std::equal(internalName.begin(), internalName.end(), result.begin() + 25));
	const std::array<uint8, 5> appendedSegment { 0x00, 0x20, 0x00, 0x20, 0xaa };
	AT_PORTABLE_TEST_ASSERT(context, std::equal(appendedSegment.begin(), appendedSegment.end(), result.end() - appendedSegment.size()));

	std::vector<uint8> registerFrames(36);
	for(size_t i = 0; i < registerFrames.size(); ++i)
		registerFrames[i] = (uint8)(i * 7 + 3);
	const auto typeR = MakeSAP(
		"SAP\r\n"
		"TYPE R\r\n"
		"STEREO\r\n"
		"NTSC\r\n"
		"FASTPLAY 4\r\n"
		"NAME \"Raw\"\r\n"
		"AUTHOR \"Test\"\r\n"
		"\r\n",
		registerFrames);
	result.clear();
	ATConvertSAPToPlayer(typeR.data(), (uint32)typeR.size(), result);
	AT_PORTABLE_TEST_ASSERT(context, result.size() > 100);
	AT_PORTABLE_TEST_ASSERT(context, result[6] == 2);
	AT_PORTABLE_TEST_ASSERT(context, result[7] == 0x80);
	AT_PORTABLE_TEST_ASSERT(context, result.back() == 0x81);

	AT_PORTABLE_TEST_ASSERT(context, ConversionFails(MakeSAP("not a SAP file")));
	AT_PORTABLE_TEST_ASSERT(context, ConversionFails(MakeSAP("SAP\r\nTYPE Z\r\n\r\n")));
	AT_PORTABLE_TEST_ASSERT(context, ConversionFails(MakeSAP(
		"SAP\r\nTYPE B\r\nINIT 2000\r\nPLAYER 2100\r\n\r\n",
		{ 0xff, 0xff, 0x00, 0x04, 0x00, 0x04, 0x42 })));

	return true;
}
