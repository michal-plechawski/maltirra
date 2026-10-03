// Portable VDDisplay bitmap font tests.

#include <algorithm>
#include <cstring>
#include <vector>

#include <at/attest/portabletest.h>
#include <vd2/system/refcount.h>
#include <vd2/Kasumi/pixmap.h>
#include <vd2/VDDisplay/font.h>

bool ATTestVDDisplayFontBitmap(ATPortableTestContext& context) {
	const uint32 atlasPixels[] = {
		0xFF101112, 0xFF202122,
		0xFF303132,
		0xFF404142, 0xFF505152, 0xFF606162
	};

	VDPixmap atlas {};
	atlas.data = const_cast<uint32 *>(atlasPixels);
	atlas.pitch = sizeof atlasPixels;
	atlas.w = 6;
	atlas.h = 1;
	atlas.format = nsVDPixmap::kPixFormat_XRGB8888;

	const wchar_t glyphChars[] = { L'Z', L'A', L'M' };
	const VDDisplayBitmapFontGlyphInfo glyphInfos[] = {
		{ 3, 0, -2, -3, 3, 1, 7 },
		{ 0, 0,  1, -2, 2, 1, 3 },
		{ 2, 0,  0, -1, 1, 1, 5 }
	};
	const VDDisplayFontMetrics fontMetrics { 4, 2 };

	vdrefptr<IVDDisplayFont> font;
	VDCreateDisplayBitmapFont(fontMetrics, 3, glyphChars, glyphInfos, atlas, 0, nullptr, ~font);
	AT_PORTABLE_TEST_ASSERT(context, font != nullptr);

	VDDisplayFontMetrics actualFontMetrics {};
	font->GetMetrics(actualFontMetrics);
	AT_PORTABLE_TEST_ASSERT(context, actualFontMetrics.mAscent == 4);
	AT_PORTABLE_TEST_ASSERT(context, actualFontMetrics.mDescent == 2);

	vdfastvector<VDDisplayFontGlyphPlacement> placements;
	placements.push_back({ 99, 99, 99, 99, 99 });
	vdrect32 cellBounds;
	vdrect32 glyphBounds;
	vdpoint32 nextPos;
	font->ShapeText(L"AM?", 3, placements, &cellBounds, &glyphBounds, &nextPos);

	AT_PORTABLE_TEST_ASSERT(context, placements.size() == 4);
	AT_PORTABLE_TEST_ASSERT(context, placements[1].mGlyphIndex == 0);
	AT_PORTABLE_TEST_ASSERT(context, placements[1].mCellX == 0);
	AT_PORTABLE_TEST_ASSERT(context, placements[1].mX == 1);
	AT_PORTABLE_TEST_ASSERT(context, placements[1].mY == -2);
	AT_PORTABLE_TEST_ASSERT(context, placements[1].mOriginalOffset == 0);
	AT_PORTABLE_TEST_ASSERT(context, placements[2].mGlyphIndex == 1);
	AT_PORTABLE_TEST_ASSERT(context, placements[2].mCellX == 3);
	AT_PORTABLE_TEST_ASSERT(context, placements[2].mX == 3);
	AT_PORTABLE_TEST_ASSERT(context, placements[2].mOriginalOffset == 1);
	AT_PORTABLE_TEST_ASSERT(context, placements[3].mGlyphIndex == 2);
	AT_PORTABLE_TEST_ASSERT(context, placements[3].mCellX == 8);
	AT_PORTABLE_TEST_ASSERT(context, placements[3].mX == 6);
	AT_PORTABLE_TEST_ASSERT(context, placements[3].mOriginalOffset == 2);
	AT_PORTABLE_TEST_ASSERT(context, cellBounds == vdrect32(0, -4, 15, 2));
	AT_PORTABLE_TEST_ASSERT(context, glyphBounds == vdrect32(0, -4, 15, 2));
	AT_PORTABLE_TEST_ASSERT(context, nextPos == vdpoint32(15, 0));

	VDDisplayFontGlyphMetrics glyphMetrics {};
	font->GetGlyphMetrics(2, glyphMetrics);
	AT_PORTABLE_TEST_ASSERT(context, glyphMetrics.mAdvance == 7);
	AT_PORTABLE_TEST_ASSERT(context, glyphMetrics.mX == -2);
	AT_PORTABLE_TEST_ASSERT(context, glyphMetrics.mY == -3);
	AT_PORTABLE_TEST_ASSERT(context, glyphMetrics.mWidth == 3);
	AT_PORTABLE_TEST_ASSERT(context, glyphMetrics.mHeight == 1);

	font->GetGlyphMetrics(3, glyphMetrics);
	AT_PORTABLE_TEST_ASSERT(context, glyphMetrics.mAdvance == 0);
	AT_PORTABLE_TEST_ASSERT(context, glyphMetrics.mWidth == 0);

	uint32 copiedPixels[3] = {};
	VDPixmap copiedPixmap {};
	copiedPixmap.data = copiedPixels;
	copiedPixmap.pitch = sizeof copiedPixels;
	copiedPixmap.w = 3;
	copiedPixmap.h = 1;
	copiedPixmap.format = nsVDPixmap::kPixFormat_XRGB8888;
	AT_PORTABLE_TEST_ASSERT(context, font->GetGlyphImage(2, false, copiedPixmap));
	AT_PORTABLE_TEST_ASSERT(context, !memcmp(copiedPixels, atlasPixels + 3, sizeof copiedPixels));
	AT_PORTABLE_TEST_ASSERT(context, !font->GetGlyphImage(3, false, copiedPixmap));

	uint32 fitCount = 99;
	AT_PORTABLE_TEST_ASSERT(context, font->FitString(L"AMZ", 3, 8, &fitCount) == vdsize32(8, 6));
	AT_PORTABLE_TEST_ASSERT(context, fitCount == 2);
	AT_PORTABLE_TEST_ASSERT(context, font->FitString(L"AMZ", 3, 2, &fitCount) == vdsize32(0, 6));
	AT_PORTABLE_TEST_ASSERT(context, fitCount == 0);
	AT_PORTABLE_TEST_ASSERT(context, font->MeasureString(L"AMZ", 3, false) == vdsize32(15, 6));

	vdrefptr<IVDDisplayFont> systemFont;
	AT_PORTABLE_TEST_ASSERT(context, VDCreateDisplaySystemFont(-18, false, "Helvetica", ~systemFont));
	AT_PORTABLE_TEST_ASSERT(context, systemFont != nullptr);

	VDDisplayFontMetrics systemMetrics {};
	systemFont->GetMetrics(systemMetrics);
	AT_PORTABLE_TEST_ASSERT(context, systemMetrics.mAscent > 0);
	AT_PORTABLE_TEST_ASSERT(context, systemMetrics.mDescent >= 0);
	vdrefptr<IVDDisplayFont> positiveHeightFont;
	AT_PORTABLE_TEST_ASSERT(context, VDCreateDisplaySystemFont(18, false, "Helvetica", ~positiveHeightFont));
	VDDisplayFontMetrics positiveHeightMetrics {};
	positiveHeightFont->GetMetrics(positiveHeightMetrics);
	AT_PORTABLE_TEST_ASSERT(context,
		positiveHeightMetrics.mAscent + positiveHeightMetrics.mDescent
			<= systemMetrics.mAscent + systemMetrics.mDescent);

	vdfastvector<VDDisplayFontGlyphPlacement> systemPlacements;
	vdrect32 systemCellBounds;
	vdrect32 systemGlyphBounds;
	vdpoint32 systemNextPos;
	systemFont->ShapeText(
		L"Ag", 2, systemPlacements,
		&systemCellBounds, &systemGlyphBounds, &systemNextPos);
	AT_PORTABLE_TEST_ASSERT(context, systemPlacements.size() == 2);
	AT_PORTABLE_TEST_ASSERT(context, systemPlacements[0].mOriginalOffset == 0);
	AT_PORTABLE_TEST_ASSERT(context, systemPlacements[1].mOriginalOffset == 1);
	AT_PORTABLE_TEST_ASSERT(context, systemNextPos.x > 0);
	AT_PORTABLE_TEST_ASSERT(context, systemCellBounds.height() == systemMetrics.mAscent + systemMetrics.mDescent);
	AT_PORTABLE_TEST_ASSERT(context, systemGlyphBounds.width() > 0);

	VDDisplayFontGlyphMetrics systemGlyphMetrics {};
	systemFont->GetGlyphMetrics(systemPlacements[0].mGlyphIndex, systemGlyphMetrics);
	AT_PORTABLE_TEST_ASSERT(context, systemGlyphMetrics.mWidth > 0);
	AT_PORTABLE_TEST_ASSERT(context, systemGlyphMetrics.mHeight == systemMetrics.mAscent + systemMetrics.mDescent);
	AT_PORTABLE_TEST_ASSERT(context, systemGlyphMetrics.mAdvance > 0);

	const int glyphWidth = systemGlyphMetrics.mWidth;
	const int glyphHeight = systemGlyphMetrics.mHeight;
	const int glyphPitch = glyphWidth + 2;
	std::vector<uint32> systemGlyphPixels((glyphPitch * (glyphHeight + 2)), 0x12345678);
	VDPixmap systemGlyphPixmap {};
	systemGlyphPixmap.data = systemGlyphPixels.data() + glyphPitch + 1;
	systemGlyphPixmap.pitch = glyphPitch * sizeof(uint32);
	systemGlyphPixmap.w = glyphWidth;
	systemGlyphPixmap.h = glyphHeight;
	systemGlyphPixmap.format = nsVDPixmap::kPixFormat_XRGB8888;
	AT_PORTABLE_TEST_ASSERT(context, systemFont->GetGlyphImage(
		systemPlacements[0].mGlyphIndex, false, systemGlyphPixmap));
	bool hasLitPixel = false;
	for(int y = 0; y < glyphHeight; ++y) {
		const uint32 *row = systemGlyphPixmap.GetPixelRow<uint32>(y);
		for(int x = 0; x < glyphWidth; ++x)
			hasLitPixel |= (row[x] & 0x00FFFFFF) != 0;
	}
	AT_PORTABLE_TEST_ASSERT(context, hasLitPixel);
	AT_PORTABLE_TEST_ASSERT(context, systemGlyphPixels.front() == 0x12345678);
	AT_PORTABLE_TEST_ASSERT(context, systemGlyphPixels.back() == 0x12345678);

	std::fill(systemGlyphPixels.begin(), systemGlyphPixels.end(), 0x12345678);
	AT_PORTABLE_TEST_ASSERT(context, systemFont->GetGlyphImage(
		systemPlacements[0].mGlyphIndex, true, systemGlyphPixmap));
	bool hasInvertedLitPixel = false;
	bool hasInvertedBackgroundPixel = false;
	for(int y = 0; y < glyphHeight; ++y) {
		const uint32 *row = systemGlyphPixmap.GetPixelRow<uint32>(y);
		for(int x = 0; x < glyphWidth; ++x) {
			hasInvertedLitPixel |= (row[x] & 0x00FFFFFF) != 0;
			hasInvertedBackgroundPixel |= (row[x] & 0x00FFFFFF) == 0;
		}
	}
	AT_PORTABLE_TEST_ASSERT(context, hasInvertedLitPixel);
	AT_PORTABLE_TEST_ASSERT(context, hasInvertedBackgroundPixel);

	uint32 systemFitCount = 99;
	const vdsize32 systemMeasured = systemFont->MeasureString(L"Ag", 2, false);
	AT_PORTABLE_TEST_ASSERT(context, systemMeasured.w > 0);
	AT_PORTABLE_TEST_ASSERT(context, systemMeasured.h == systemMetrics.mAscent + systemMetrics.mDescent);
	const vdsize32 systemFitted = systemFont->FitString(L"Ag", 2, (uint32)systemMeasured.w, &systemFitCount);
	AT_PORTABLE_TEST_ASSERT(context, systemFitCount == 2);
	AT_PORTABLE_TEST_ASSERT(context, systemFitted.w <= systemMeasured.w);

	return true;
}
