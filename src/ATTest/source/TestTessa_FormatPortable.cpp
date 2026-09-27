// Portable Tessa pixel-format tests.

#include <array>

#include <at/attest/portabletest.h>
#include <vd2/Tessa/Format.h>

bool ATTestTessaFormat(ATPortableTestContext& context) {
	struct FormatCase {
		VDTFormat mFormat;
		uint32 mBytesPerPixel;
	};

	constexpr std::array<FormatCase, 14> kSupportedFormats {{
		{ kVDTF_R8G8B8A8, 4 },
		{ kVDTF_R8G8B8A8_sRGB, 4 },
		{ kVDTF_R8G8B8A8_GammaToSRGB, 4 },
		{ kVDTF_B8G8R8A8, 4 },
		{ kVDTF_B8G8R8A8_sRGB, 4 },
		{ kVDTF_U8V8, 2 },
		{ kVDTF_L8A8, 2 },
		{ kVDTF_R8G8, 2 },
		{ kVDTF_B5G6R5, 2 },
		{ kVDTF_B5G5R5A1, 2 },
		{ kVDTF_R8, 1 },
		{ kVDTF_L8, 1 },
		{ kVDTF_R16G16B16A16F, 8 },
		{ kVDTF_R32G32B32A32F, 16 },
	}};

	for(const FormatCase& testCase : kSupportedFormats) {
		AT_PORTABLE_TEST_ASSERT(context,
			VDTGetBytesPerBlockRow(testCase.mFormat, 13) == 13 * testCase.mBytesPerPixel);
		AT_PORTABLE_TEST_ASSERT(context,
			VDTGetNumBlockRows(testCase.mFormat, 17) == 17);
	}

	constexpr std::array<VDTFormat, 1> kUnsupportedFormats {{
		kVDTF_Unknown,
	}};

	for(VDTFormat format : kUnsupportedFormats) {
		AT_PORTABLE_TEST_ASSERT(context, VDTGetBytesPerBlockRow(format, 13) == 0);
		AT_PORTABLE_TEST_ASSERT(context, VDTGetNumBlockRows(format, 17) == 0);
	}

	return true;
}
