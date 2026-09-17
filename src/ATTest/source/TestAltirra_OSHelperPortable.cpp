// Altirra portable OS helper tests

#include <cstring>
#include <memory>
#include <at/attest/portabletest.h>
#include <vd2/Kasumi/pixmap.h>
#include <vd2/Kasumi/pixmaputils.h>
#include <vd2/system/error.h>
#include <vd2/system/file.h>
#include <vd2/system/filesys.h>
#include <vd2/system/process.h>
#include <vd2/system/time.h>
#include <vd2/system/vdstring.h>
#include <oshelper.h>
#include <encode_png.h>
#include "../../Altirra/res/resource.h"

namespace {
	class ATOSHelperTestFile {
	public:
		ATOSHelperTestFile() {
			mPath.sprintf(
				L"altirra-oshelper-test-%u-%llu.tmp",
				static_cast<unsigned>(VDGetCurrentProcessId()),
				static_cast<unsigned long long>(VDGetCurrentTick64()));

			VDFile file(mPath.c_str(), nsVDFile::kWrite | nsVDFile::kCreateAlways);
			file.write("test", 4);
		}

		~ATOSHelperTestFile() {
			try {
				ATFileSetReadOnlyAttribute(mPath.c_str(), false);
			} catch(...) {
			}

			VDRemoveFile(mPath.c_str());
		}

		VDStringW mPath;
	};
}

bool ATTestAltirraOSHelper(ATPortableTestContext& context) {
	AT_PORTABLE_TEST_ASSERT(context, !std::strcmp(ATEnumToString(ATProcessEfficiencyMode::Default), "default"));
	AT_PORTABLE_TEST_ASSERT(context, !std::strcmp(ATEnumToString(ATProcessEfficiencyMode::Performance), "performance"));
	AT_PORTABLE_TEST_ASSERT(context, !std::strcmp(ATEnumToString(ATProcessEfficiencyMode::Efficiency), "efficiency"));

	const auto performance = ATParseEnum<ATProcessEfficiencyMode>(VDStringSpanA("performance"));
	AT_PORTABLE_TEST_ASSERT(context, performance.mValid);
	AT_PORTABLE_TEST_ASSERT(context, performance.mValue == ATProcessEfficiencyMode::Performance);

	const auto invalid = ATParseEnum<ATProcessEfficiencyMode>(VDStringSpanA("invalid"));
	AT_PORTABLE_TEST_ASSERT(context, !invalid.mValid);
	AT_PORTABLE_TEST_ASSERT(context, invalid.mValue == ATProcessEfficiencyMode::Default);

	{
		const wchar_t *args[] { L"/portable", L"plain", L"path/to/file" };
		AT_PORTABLE_TEST_ASSERT(context, ATBuildEscapedCommandLine(args) == L"/portable plain path/to/file");
	}

	{
		const wchar_t *args[] { L"", L"two words", L"tab\tvalue", L"say\"hello" };
		AT_PORTABLE_TEST_ASSERT(context,
			ATBuildEscapedCommandLine(args) == L"\"\" \"two words\" \"tab\tvalue\" \"say\\\"hello\"");
	}

	{
		const wchar_t *args[] { L"C:\\plain\\path", L"C:\\space path\\", L"a\\\\\"b" };
		AT_PORTABLE_TEST_ASSERT(context,
			ATBuildEscapedCommandLine(args) == L"C:\\plain\\path \"C:\\space path\\\\\" \"a\\\\\\\\\\\"b\"");
	}

	{
		const wchar_t *args[] { nullptr };
		AT_PORTABLE_TEST_ASSERT(context, ATBuildEscapedCommandLine(args) == L"\"\"");
	}

	{
		ATOSHelperTestFile file;
		AT_PORTABLE_TEST_ASSERT(context,
			!(VDFileGetAttributes(file.mPath.c_str()) & kVDFileAttr_ReadOnly));

		ATFileSetReadOnlyAttribute(file.mPath.c_str(), true);
		AT_PORTABLE_TEST_ASSERT(context,
			VDFileGetAttributes(file.mPath.c_str()) & kVDFileAttr_ReadOnly);

		ATFileSetReadOnlyAttribute(file.mPath.c_str(), false);
		AT_PORTABLE_TEST_ASSERT(context,
			!(VDFileGetAttributes(file.mPath.c_str()) & kVDFileAttr_ReadOnly));
	}

	{
		uint8 firstGuid[16] {};
		uint8 secondGuid[16] {};
		ATGenerateGuid(firstGuid);
		ATGenerateGuid(secondGuid);

		const uint8 zeroGuid[16] {};
		AT_PORTABLE_TEST_ASSERT(context, std::memcmp(firstGuid, zeroGuid, sizeof firstGuid));
		AT_PORTABLE_TEST_ASSERT(context, std::memcmp(secondGuid, zeroGuid, sizeof secondGuid));
		AT_PORTABLE_TEST_ASSERT(context, std::memcmp(firstGuid, secondGuid, sizeof firstGuid));
	}

	// This is environment-dependent, but both paths through the native
	// implementation must be safe to query without elevated privileges.
	(void)ATIsUserAdministrator();

	{
		static constexpr uint8 packed[] {
			5, 0, 0, 0,
			10, 'h', 'e', 'l', 'l', 'o',
			0
		};
		vdfastvector<uint8> decoded;
		AT_PORTABLE_TEST_ASSERT(context, ATDecodeLZPackedResource(packed, sizeof packed, decoded));
		AT_PORTABLE_TEST_ASSERT(context, decoded.size() == 5);
		AT_PORTABLE_TEST_ASSERT(context, !std::memcmp(decoded.data(), "hello", 5));
	}

	{
		static constexpr uint8 packed[] {
			9, 0, 0, 0,
			6, 'a', 'b', 'c',
			0x61, 2,
			0
		};
		vdfastvector<uint8> decoded;
		AT_PORTABLE_TEST_ASSERT(context, ATDecodeLZPackedResource(packed, sizeof packed, decoded));
		AT_PORTABLE_TEST_ASSERT(context, decoded.size() == 9);
		AT_PORTABLE_TEST_ASSERT(context, !std::memcmp(decoded.data(), "abcabcabc", 9));
	}

	{
		static constexpr uint8 truncatedHeader[] { 1, 0, 0 };
		static constexpr uint8 truncatedLiteral[] { 5, 0, 0, 0, 10, 'h', 'i' };
		static constexpr uint8 invalidBackReference[] { 3, 0, 0, 0, 1, 0, 0 };
		static constexpr uint8 earlyTerminator[] { 1, 0, 0, 0, 0 };
		static constexpr uint8 missingTerminator[] { 1, 0, 0, 0, 2, 'x' };
		static constexpr uint8 oversizedOutput[] { 0x01, 0x00, 0x00, 0x10, 0 };
		vdfastvector<uint8> decoded { 1, 2, 3 };

		AT_PORTABLE_TEST_ASSERT(context, !ATDecodeLZPackedResource(nullptr, 0, decoded));
		AT_PORTABLE_TEST_ASSERT(context, decoded.empty());
		AT_PORTABLE_TEST_ASSERT(context, !ATDecodeLZPackedResource(truncatedHeader, sizeof truncatedHeader, decoded));
		AT_PORTABLE_TEST_ASSERT(context, !ATDecodeLZPackedResource(truncatedLiteral, sizeof truncatedLiteral, decoded));
		AT_PORTABLE_TEST_ASSERT(context, !ATDecodeLZPackedResource(invalidBackReference, sizeof invalidBackReference, decoded));
		AT_PORTABLE_TEST_ASSERT(context, !ATDecodeLZPackedResource(earlyTerminator, sizeof earlyTerminator, decoded));
		AT_PORTABLE_TEST_ASSERT(context, !ATDecodeLZPackedResource(missingTerminator, sizeof missingTerminator, decoded));
		AT_PORTABLE_TEST_ASSERT(context, !ATDecodeLZPackedResource(oversizedOutput, sizeof oversizedOutput, decoded));
		AT_PORTABLE_TEST_ASSERT(context, decoded.empty());
	}

	{
		vdfastvector<uint8> about;
		AT_PORTABLE_TEST_ASSERT(context, ATLoadMiscResource(IDR_ABOUT, about));
		AT_PORTABLE_TEST_ASSERT(context, !about.empty());

		static constexpr uint8 marker[] {
			0xFF, 0xFE, 'A', 0, 'l', 0, 't', 0, 'i', 0, 'r', 0, 'r', 0, 'a', 0
		};
		AT_PORTABLE_TEST_ASSERT(context, about.size() >= sizeof marker);
		AT_PORTABLE_TEST_ASSERT(context, !std::memcmp(about.data(), marker, sizeof marker));

		size_t compatSize = 0;
		const void *const compatData = ATLockResource(IDR_COMPATDB, compatSize);
		AT_PORTABLE_TEST_ASSERT(context, compatData);
		AT_PORTABLE_TEST_ASSERT(context, compatSize > 16);
		AT_PORTABLE_TEST_ASSERT(context, ATLockResource(999, compatSize) == nullptr);
		AT_PORTABLE_TEST_ASSERT(context, !ATLoadMiscResource(999, about));
	}

	{
		const uint32 pixels[] {
			0x00112233, 0x00445566,
			0x00778899, 0x00AABBCC
		};
		VDPixmap source {};
		source.data = const_cast<uint32 *>(pixels);
		source.w = 2;
		source.h = 2;
		source.pitch = 2 * sizeof(uint32);
		source.format = nsVDPixmap::kPixFormat_XRGB8888;

		std::unique_ptr<IVDImageEncoderPNG> encoder(VDCreateImageEncoderPNG());
		const void *encodedData = nullptr;
		uint32 encodedSize = 0;
		encoder->Encode(source, encodedData, encodedSize, false);

		auto validatePixels = [&](const VDPixmap& decoded) {
			if (decoded.w != 2 || decoded.h != 2 || decoded.format != nsVDPixmap::kPixFormat_XRGB8888)
				return false;

			for(sint32 y = 0; y < 2; ++y) {
				const uint32 *const row = decoded.GetPixelRow<uint32>(y);
				for(sint32 x = 0; x < 2; ++x) {
					if ((row[x] & UINT32_C(0x00FFFFFF)) != pixels[y * 2 + x])
						return false;
				}
			}

			return true;
		};

		VDPixmapBuffer memoryDecoded;
		ATLoadFrameFromMemory(memoryDecoded, encodedData, encodedSize);
		AT_PORTABLE_TEST_ASSERT(context, validatePixels(memoryDecoded));

		ATOSHelperTestFile file;
		ATSaveFrame(source, file.mPath.c_str());
		VDPixmapBuffer fileDecoded;
		ATLoadFrame(fileDecoded, file.mPath.c_str());
		AT_PORTABLE_TEST_ASSERT(context, validatePixels(fileDecoded));

		VDPixmapBuffer resourceImage;
		AT_PORTABLE_TEST_ASSERT(context, ATLoadImageResource(IDB_WARNING, resourceImage));
		AT_PORTABLE_TEST_ASSERT(context, resourceImage.w > 0 && resourceImage.h > 0);

		bool invalidImageRejected = false;
		try {
			const uint8 invalidImage[] { 0 };
			ATLoadFrameFromMemory(memoryDecoded, invalidImage, sizeof invalidImage);
		} catch(const MyError&) {
			invalidImageRejected = true;
		}
		AT_PORTABLE_TEST_ASSERT(context, invalidImageRejected);
	}

	return true;
}
