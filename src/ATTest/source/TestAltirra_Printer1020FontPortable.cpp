// Altirra portable 1020 plotter font tests

#include <cstddef>

#include <at/attest/portabletest.h>
#include <printer1020font.h>

bool ATTestAltirraPrinter1020Font(ATPortableTestContext& context) {
	const ATPrinterFont1020& font = g_ATPrinterFont1020;
	AT_PORTABLE_TEST_ASSERT(context, font.mpFontData != nullptr);
	AT_PORTABLE_TEST_ASSERT(context, font.mCharOffsets[0] == 0);

	// Every indexed glyph must occupy a nonempty, terminated byte range.
	for(size_t ch = 0; ch < 127; ++ch) {
		const size_t start = font.mCharOffsets[ch];
		const size_t end = font.mCharOffsets[ch + 1];
		AT_PORTABLE_TEST_ASSERT(context, end > start);
		AT_PORTABLE_TEST_ASSERT(context,
			(font.mpFontData[end - 1] & ATPrinterFont1020::kEndBit) != 0);

		for(size_t i = start; i < end; ++i) {
			const uint8 command = font.mpFontData[i];
			AT_PORTABLE_TEST_ASSERT(context, i == end - 1 || !(command & ATPrinterFont1020::kEndBit));
			AT_PORTABLE_TEST_ASSERT(context, (command & 0x70) <= 0x60);
		}
	}

	const uint8 *space = font.mpFontData + font.mCharOffsets[0x20];
	AT_PORTABLE_TEST_ASSERT(context, space[0] == ATPrinterFont1020::kEndBit);
	AT_PORTABLE_TEST_ASSERT(context,
		font.mCharOffsets[0x21] - font.mCharOffsets[0x20] == 1);

	// 'A' contains a move-to, the two sloping strokes, then a crossbar.
	const uint8 *a = font.mpFontData + font.mCharOffsets['A'];
	AT_PORTABLE_TEST_ASSERT(context,
		font.mCharOffsets['B'] - font.mCharOffsets['A'] == 7);
	AT_PORTABLE_TEST_ASSERT(context, a[0] == (0x01 | ATPrinterFont1020::kMoveBit));
	AT_PORTABLE_TEST_ASSERT(context, a[1] == 0x05);
	AT_PORTABLE_TEST_ASSERT(context, a[2] == 0x27);
	AT_PORTABLE_TEST_ASSERT(context, a[3] == 0x45);
	AT_PORTABLE_TEST_ASSERT(context, a[4] == 0x41);
	AT_PORTABLE_TEST_ASSERT(context, a[5] == (0x04 | ATPrinterFont1020::kMoveBit));
	AT_PORTABLE_TEST_ASSERT(context, a[6] == (0x44 | ATPrinterFont1020::kEndBit));

	const uint8 *underscore = font.mpFontData + font.mCharOffsets['_'];
	AT_PORTABLE_TEST_ASSERT(context, underscore[0] == ATPrinterFont1020::kMoveBit);
	AT_PORTABLE_TEST_ASSERT(context,
		underscore[1] == (0x60 | ATPrinterFont1020::kEndBit));

	// The final glyph has no following offset, so check its full encoded form.
	const uint8 *last = font.mpFontData + font.mCharOffsets[0x7F];
	AT_PORTABLE_TEST_ASSERT(context, last[0] == (0x11 | ATPrinterFont1020::kMoveBit));
	AT_PORTABLE_TEST_ASSERT(context, last[1] == 0x17);
	AT_PORTABLE_TEST_ASSERT(context, last[2] == 0x44);
	AT_PORTABLE_TEST_ASSERT(context, last[3] == (0x11 | ATPrinterFont1020::kEndBit));

	return true;
}
