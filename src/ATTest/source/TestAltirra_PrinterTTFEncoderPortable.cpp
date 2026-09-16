// Altirra portable TrueType printer font encoder tests

#include <cstring>

#include <at/attest/portabletest.h>
#include <printerttfencoder.h>

namespace {
	constexpr uint32 MakeTag(char a, char b, char c, char d) {
		return ((uint32)(uint8)a << 24) | ((uint32)(uint8)b << 16) |
			((uint32)(uint8)c << 8) | (uint8)d;
	}

	uint16 ReadU16(vdspan<const uint8> data, size_t offset) {
		return ((uint16)data[offset] << 8) | data[offset + 1];
	}

	uint32 ReadU32(vdspan<const uint8> data, size_t offset) {
		return ((uint32)ReadU16(data, offset) << 16) | ReadU16(data, offset + 2);
	}

	uint64 ReadU64(vdspan<const uint8> data, size_t offset) {
		return ((uint64)ReadU32(data, offset) << 32) | ReadU32(data, offset + 4);
	}

	struct TableInfo {
		uint32 mTag = 0;
		uint32 mChecksum = 0;
		uint32 mOffset = 0;
		uint32 mLength = 0;
	};

	bool FindTable(vdspan<const uint8> data, uint32 tag, TableInfo& table) {
		if (data.size() < 12)
			return false;

		const uint16 tableCount = ReadU16(data, 4);
		if (data.size() < 12 + (size_t)tableCount * 16)
			return false;

		for(uint16 i = 0; i < tableCount; ++i) {
			const size_t pos = 12 + (size_t)i * 16;
			if (ReadU32(data, pos) != tag)
				continue;

			table.mTag = tag;
			table.mChecksum = ReadU32(data, pos + 4);
			table.mOffset = ReadU32(data, pos + 8);
			table.mLength = ReadU32(data, pos + 12);
			return (size_t)table.mOffset + table.mLength <= data.size();
		}

		return false;
	}

	uint32 SumTable(vdspan<const uint8> data, const TableInfo& table, bool clearChecksumAdjustment) {
		uint32 sum = 0;
		const size_t wordCount = ((size_t)table.mLength + 3) / 4;

		for(size_t i = 0; i < wordCount; ++i) {
			uint32 word = 0;

			for(size_t j = 0; j < 4; ++j) {
				const size_t relativeOffset = i * 4 + j;
				uint8 value = relativeOffset < table.mLength
					? data[table.mOffset + relativeOffset]
					: 0;

				if (clearChecksumAdjustment && relativeOffset >= 8 && relativeOffset < 12)
					value = 0;

				word = (word << 8) | value;
			}

			sum += word;
		}

		return sum;
	}

	uint16 MapCharacter(vdspan<const uint8> data, const TableInfo& cmap, uint16 ch) {
		if (cmap.mLength < 12)
			return 0;

		const size_t subtable = cmap.mOffset + ReadU32(data, cmap.mOffset + 8);
		if (subtable + 16 > data.size() || ReadU16(data, subtable) != 4)
			return 0;

		const uint16 segmentCount = ReadU16(data, subtable + 6) / 2;
		const size_t endCodes = subtable + 14;
		const size_t startCodes = endCodes + (size_t)segmentCount * 2 + 2;
		const size_t deltas = startCodes + (size_t)segmentCount * 2;
		const size_t rangeOffsets = deltas + (size_t)segmentCount * 2;

		for(uint16 i = 0; i < segmentCount; ++i) {
			if (ch > ReadU16(data, endCodes + (size_t)i * 2))
				continue;

			const uint16 start = ReadU16(data, startCodes + (size_t)i * 2);
			if (ch < start)
				return 0;

			const uint16 delta = ReadU16(data, deltas + (size_t)i * 2);
			const size_t rangeOffsetPos = rangeOffsets + (size_t)i * 2;
			const uint16 rangeOffset = ReadU16(data, rangeOffsetPos);
			if (!rangeOffset)
				return (uint16)(ch + delta);

			const size_t glyphPos = rangeOffsetPos + rangeOffset + (size_t)(ch - start) * 2;
			if (glyphPos + 2 > data.size())
				return 0;

			const uint16 glyph = ReadU16(data, glyphPos);
			return glyph ? (uint16)(glyph + delta) : 0;
		}

		return 0;
	}

	bool ContainsUTF16BE(vdspan<const uint8> data, const TableInfo& table, const char *text) {
		const size_t length = std::strlen(text);
		if (!length || table.mLength < length * 2)
			return false;

		for(size_t pos = table.mOffset; pos + length * 2 <= table.mOffset + table.mLength; ++pos) {
			bool match = true;
			for(size_t i = 0; i < length; ++i) {
				if (data[pos + i * 2] != 0 || data[pos + i * 2 + 1] != (uint8)text[i]) {
					match = false;
					break;
				}
			}

			if (match)
				return true;
		}

		return false;
	}
}

bool ATTestAltirraPrinterTTFEncoder(ATPortableTestContext& context) {
	ATTrueTypeEncoder encoder;
	encoder.SetTimestamps(
		VDDate { 95616288000000000ULL + 1230000000ULL },
		VDDate { 95616288000000000ULL + 4560000000ULL });
	encoder.SetDefaultAdvanceWidth(640);
	encoder.SetDefaultChar('?');
	encoder.SetBreakChar(' ');

	encoder.BeginSimpleGlyph();
	encoder.EndSimpleGlyph();

	const auto spaceGlyph = encoder.BeginSimpleGlyph();
	encoder.EndSimpleGlyph();

	const auto shapeGlyph = encoder.BeginSimpleGlyph();
	encoder.AddGlyphPoint(-20, -30, true);
	encoder.AddGlyphPoint(300, -30, true);
	encoder.AddGlyphPoint(300, 700, false);
	encoder.AddGlyphPoint(-20, 700, true);
	encoder.EndContour();
	encoder.EndSimpleGlyph();

	const auto compositeGlyph = encoder.BeginCompositeGlyph();
	encoder.AddGlyphReference(shapeGlyph, 10, 20);
	encoder.AddGlyphReference(shapeGlyph, 300, -200);
	encoder.EndCompositeGlyph();

	encoder.MapCharacter(' ', spaceGlyph);
	encoder.MapCharacter('A', compositeGlyph);
	encoder.MapCharacter('B', shapeGlyph);
	encoder.MapCharacter('C', compositeGlyph);
	encoder.MapCharacterRange('P', spaceGlyph, 3);

	encoder.SetName(ATTrueTypeName::Copyright, "Public domain");
	encoder.SetName(ATTrueTypeName::FontFamily, "Portable TTF");
	encoder.SetName(ATTrueTypeName::FontSubfamily, "Regular");
	encoder.SetName(ATTrueTypeName::FullFontName, "Portable TTF Regular");
	encoder.SetName(ATTrueTypeName::UniqueFontIdentifier, "Portable-TTF-Regular");
	encoder.SetName(ATTrueTypeName::Version, "Version 1.0");
	encoder.SetName(ATTrueTypeName::PostScriptName, "PortableTTF-Regular");

	const vdspan<const uint8> data = encoder.Finalize();
	AT_PORTABLE_TEST_ASSERT(context, data.size() > 400);
	AT_PORTABLE_TEST_ASSERT(context, !(data.size() & 3));
	AT_PORTABLE_TEST_ASSERT(context, ReadU32(data, 0) == 0x00010000);
	AT_PORTABLE_TEST_ASSERT(context, ReadU16(data, 4) == 10);

	uint32 fontChecksum = 0;
	for(size_t pos = 0; pos < data.size(); pos += 4)
		fontChecksum += ReadU32(data, pos);
	AT_PORTABLE_TEST_ASSERT(context, fontChecksum == 0xB1B0AFBAU);

	uint32 previousTag = 0;
	for(uint16 i = 0; i < ReadU16(data, 4); ++i) {
		const size_t directoryPos = 12 + (size_t)i * 16;
		TableInfo table {
			ReadU32(data, directoryPos),
			ReadU32(data, directoryPos + 4),
			ReadU32(data, directoryPos + 8),
			ReadU32(data, directoryPos + 12)
		};
		AT_PORTABLE_TEST_ASSERT(context, i == 0 || table.mTag > previousTag);
		AT_PORTABLE_TEST_ASSERT(context, !(table.mOffset & 3));
		AT_PORTABLE_TEST_ASSERT(context, (size_t)table.mOffset + table.mLength <= data.size());
		AT_PORTABLE_TEST_ASSERT(context,
			SumTable(data, table, table.mTag == MakeTag('h', 'e', 'a', 'd')) == table.mChecksum);
		previousTag = table.mTag;
	}

	TableInfo head;
	AT_PORTABLE_TEST_ASSERT(context, FindTable(data, MakeTag('h', 'e', 'a', 'd'), head));
	AT_PORTABLE_TEST_ASSERT(context, head.mLength == 54);
	AT_PORTABLE_TEST_ASSERT(context, ReadU32(data, head.mOffset + 12) == 0x5F0F3CF5);
	AT_PORTABLE_TEST_ASSERT(context, ReadU16(data, head.mOffset + 18) == 1024);
	AT_PORTABLE_TEST_ASSERT(context, ReadU64(data, head.mOffset + 20) == 123);
	AT_PORTABLE_TEST_ASSERT(context, ReadU64(data, head.mOffset + 28) == 456);
	AT_PORTABLE_TEST_ASSERT(context, (sint16)ReadU16(data, head.mOffset + 36) == -20);
	AT_PORTABLE_TEST_ASSERT(context, (sint16)ReadU16(data, head.mOffset + 38) == -230);
	AT_PORTABLE_TEST_ASSERT(context, (sint16)ReadU16(data, head.mOffset + 40) == 600);
	AT_PORTABLE_TEST_ASSERT(context, (sint16)ReadU16(data, head.mOffset + 42) == 720);

	TableInfo maxp;
	AT_PORTABLE_TEST_ASSERT(context, FindTable(data, MakeTag('m', 'a', 'x', 'p'), maxp));
	AT_PORTABLE_TEST_ASSERT(context, ReadU16(data, maxp.mOffset + 4) == 4);
	AT_PORTABLE_TEST_ASSERT(context, ReadU16(data, maxp.mOffset + 6) == 4);
	AT_PORTABLE_TEST_ASSERT(context, ReadU16(data, maxp.mOffset + 8) == 1);
	AT_PORTABLE_TEST_ASSERT(context, ReadU16(data, maxp.mOffset + 10) == 8);
	AT_PORTABLE_TEST_ASSERT(context, ReadU16(data, maxp.mOffset + 12) == 2);
	AT_PORTABLE_TEST_ASSERT(context, ReadU16(data, maxp.mOffset + 28) == 2);
	AT_PORTABLE_TEST_ASSERT(context, ReadU16(data, maxp.mOffset + 30) == 1);

	TableInfo hmtx;
	AT_PORTABLE_TEST_ASSERT(context, FindTable(data, MakeTag('h', 'm', 't', 'x'), hmtx));
	AT_PORTABLE_TEST_ASSERT(context, hmtx.mLength == 16);
	for(size_t pos = 0; pos < hmtx.mLength; pos += 4)
		AT_PORTABLE_TEST_ASSERT(context, ReadU16(data, hmtx.mOffset + pos) == 640);

	TableInfo cmap;
	AT_PORTABLE_TEST_ASSERT(context, FindTable(data, MakeTag('c', 'm', 'a', 'p'), cmap));
	AT_PORTABLE_TEST_ASSERT(context, MapCharacter(data, cmap, ' ') == (uint16)spaceGlyph);
	AT_PORTABLE_TEST_ASSERT(context, MapCharacter(data, cmap, 'A') == (uint16)compositeGlyph);
	AT_PORTABLE_TEST_ASSERT(context, MapCharacter(data, cmap, 'B') == (uint16)shapeGlyph);
	AT_PORTABLE_TEST_ASSERT(context, MapCharacter(data, cmap, 'C') == (uint16)compositeGlyph);
	AT_PORTABLE_TEST_ASSERT(context, MapCharacter(data, cmap, 'D') == 0);
	AT_PORTABLE_TEST_ASSERT(context, MapCharacter(data, cmap, 'P') == (uint16)spaceGlyph);
	AT_PORTABLE_TEST_ASSERT(context, MapCharacter(data, cmap, 'Q') == (uint16)shapeGlyph);
	AT_PORTABLE_TEST_ASSERT(context, MapCharacter(data, cmap, 'R') == (uint16)compositeGlyph);

	TableInfo name;
	AT_PORTABLE_TEST_ASSERT(context, FindTable(data, MakeTag('n', 'a', 'm', 'e'), name));
	AT_PORTABLE_TEST_ASSERT(context, ReadU16(data, name.mOffset + 2) == 14);
	AT_PORTABLE_TEST_ASSERT(context, ContainsUTF16BE(data, name, "Portable TTF Regular"));

	TableInfo loca;
	AT_PORTABLE_TEST_ASSERT(context, FindTable(data, MakeTag('l', 'o', 'c', 'a'), loca));
	AT_PORTABLE_TEST_ASSERT(context, loca.mLength == 10);
	return true;
}
