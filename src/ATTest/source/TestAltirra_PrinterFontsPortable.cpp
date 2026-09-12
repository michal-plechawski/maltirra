// Altirra portable raster printer font tests

#include <cstddef>

#include <at/attest/portabletest.h>
#include <printerfont.h>

namespace {
	template<typename Font>
	bool ValidMonoFont(const Font& font) {
		if (font.mDesc.mWidth != Font::kWidth ||
			font.mDesc.mHeight != Font::kHeight ||
			font.mDesc.mCharFirst != Font::kCharFirst ||
			font.mDesc.mCharLast != Font::kCharLast)
			return false;

		const uint32 validBits = (1u << Font::kHeight) - 1;
		for(uint8 column : font.mColumns) {
			if (column & ~validBits)
				return false;
		}

		const size_t aOffset = ('A' - Font::kCharFirst) * Font::kWidth;
		for(size_t i = 0; i < Font::kWidth; ++i) {
			if (font.mColumns[aOffset + i])
				return true;
		}

		return false;
	}

	template<typename Font>
	bool BlankSpace(const Font& font) {
		const size_t offset = (' ' - Font::kCharFirst) * Font::kWidth;
		for(size_t i = 0; i < Font::kWidth; ++i) {
			if (font.mColumns[offset + i])
				return false;
		}
		return true;
	}

	bool ValidProportionalFont(const ATPrinterFont825Prop& font) {
		if (font.mDesc.mWidth != ATPrinterFont825Prop::kWidth ||
			font.mDesc.mHeight != ATPrinterFont825Prop::kHeight ||
			font.mDesc.mCharFirst != ATPrinterFont825Prop::kCharFirst ||
			font.mDesc.mCharLast != ATPrinterFont825Prop::kCharLast)
			return false;

		for(uint16 column : font.mColumns) {
			if (column & ~((1u << ATPrinterFont825Prop::kHeight) - 1))
				return false;
		}

		for(uint8 advance : font.mAdvanceWidths) {
			if (advance > ATPrinterFont825Prop::kWidth)
				return false;
		}

		const size_t aIndex = 'A' - ATPrinterFont825Prop::kCharFirst;
		if (!font.mAdvanceWidths[aIndex])
			return false;
		for(size_t i = 0; i < ATPrinterFont825Prop::kWidth; ++i) {
			if (font.mColumns[aIndex * ATPrinterFont825Prop::kWidth + i])
				return true;
		}

		return false;
	}
}

bool ATTestAltirraPrinterFonts(ATPortableTestContext& context) {
	AT_PORTABLE_TEST_ASSERT(context, ValidMonoFont(g_ATPrinterFont820));
	AT_PORTABLE_TEST_ASSERT(context, ValidMonoFont(g_ATPrinterFont820S));
	AT_PORTABLE_TEST_ASSERT(context, ValidMonoFont(g_ATPrinterFont1025));
	AT_PORTABLE_TEST_ASSERT(context, ValidMonoFont(g_ATPrinterFont1029));
	AT_PORTABLE_TEST_ASSERT(context, ValidMonoFont(g_ATPrinterFont825Mono));
	AT_PORTABLE_TEST_ASSERT(context, ValidProportionalFont(g_ATPrinterFont825Prop));

	AT_PORTABLE_TEST_ASSERT(context, BlankSpace(g_ATPrinterFont820));
	AT_PORTABLE_TEST_ASSERT(context, BlankSpace(g_ATPrinterFont825Mono));

	// The 820's five columns for 'A' form its two legs and crossbar.
	const uint8 expectedA[] {0x7C, 0x12, 0x11, 0x12, 0x7C};
	const size_t aOffset = ('A' - ATPrinterFont820::kCharFirst) * ATPrinterFont820::kWidth;
	for(size_t i = 0; i < ATPrinterFont820::kWidth; ++i)
		AT_PORTABLE_TEST_ASSERT(context, g_ATPrinterFont820.mColumns[aOffset + i] == expectedA[i]);

	return true;
}
