// Altirra - Atari 800/800XL/5200 emulator
// Copyright (C) 2026 Avery Lee
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along
// with this program. If not, see <http://www.gnu.org/licenses/>.

#include <algorithm>
#include <cmath>
#include <cstring>
#include <strings.h>
#include <vector>

#import <CoreGraphics/CoreGraphics.h>
#import <CoreText/CoreText.h>

#include <vd2/system/refcount.h>
#include <vd2/Kasumi/pixmap.h>
#include <vd2/VDDisplay/font.h>

namespace {
	struct VDCoreTextGlyph {
		CTFontRef mpFont = nullptr;
		CGGlyph mGlyph = 0;

		~VDCoreTextGlyph() {
			if (mpFont)
				CFRelease(mpFont);
		}
	};

	CFStringRef VDCreateCoreTextString(uint32 codePoint, UniChar (&characters)[2], CFIndex& length) {
		if (codePoint > 0x10FFFF || (codePoint >= 0xD800 && codePoint <= 0xDFFF))
			codePoint = 0xFFFD;

		if (codePoint < 0x10000) {
			characters[0] = (UniChar)codePoint;
			length = 1;
		} else {
			codePoint -= 0x10000;
			characters[0] = (UniChar)(0xD800 + (codePoint >> 10));
			characters[1] = (UniChar)(0xDC00 + (codePoint & 0x3FF));
			length = 2;
		}

		return CFStringCreateWithCharacters(kCFAllocatorDefault, characters, length);
	}

	bool VDResolveCoreTextGlyph(CTFontRef baseFont, uint32 codePoint, VDCoreTextGlyph& result) {
		UniChar characters[2] {};
		CFIndex length = 0;
		CFStringRef text = VDCreateCoreTextString(codePoint, characters, length);
		if (!text)
			return false;

		result.mpFont = CTFontCreateForString(baseFont, text, CFRangeMake(0, length));
		CFRelease(text);

		if (!result.mpFont) {
			result.mpFont = baseFont;
			CFRetain(result.mpFont);
		}

		CGGlyph glyphs[2] {};
		CTFontGetGlyphsForCharacters(result.mpFont, characters, glyphs, length);
		result.mGlyph = glyphs[0] ? glyphs[0] : glyphs[1];
		return true;
	}

	class VDDisplayFontCoreText final : public vdrefcounted<IVDDisplayFont> {
	public:
		~VDDisplayFontCoreText() {
			if (mpFont)
				CFRelease(mpFont);
		}

		void *AsInterface(uint32) override { return nullptr; }

		bool Init(int height, bool bold, const char *fontName) {
			const CGFloat pointSize = (CGFloat)std::max(std::fabs((double)height), 1.0);
			const char *resolvedName = fontName;

			if (!resolvedName || !*resolvedName
				|| !strcasecmp(resolvedName, "MS Shell Dlg")
				|| !strcasecmp(resolvedName, "Tahoma")) {
				mpFont = CTFontCreateUIFontForLanguage(kCTFontUIFontSystem, pointSize, nullptr);
			} else {
				if (!strcasecmp(resolvedName, "Lucida Console"))
					resolvedName = "Menlo";
				else if (!strcasecmp(resolvedName, "Marlett"))
					resolvedName = "Apple Symbols";

				CFStringRef name = CFStringCreateWithCString(
					kCFAllocatorDefault, resolvedName, kCFStringEncodingUTF8);
				if (name) {
					mpFont = CTFontCreateWithName(name, pointSize, nullptr);
					CFRelease(name);
				}
			}

			if (!mpFont)
				return false;

			if (bold) {
				CTFontRef boldFont = CTFontCreateCopyWithSymbolicTraits(
					mpFont, 0, nullptr, kCTFontBoldTrait, kCTFontBoldTrait);
				if (boldFont) {
					CFRelease(mpFont);
					mpFont = boldFont;
				}
			}

			if (height > 0) {
				const CGFloat lineHeight = CTFontGetAscent(mpFont)
					+ CTFontGetDescent(mpFont) + CTFontGetLeading(mpFont);
				if (lineHeight > 0) {
					const CGFloat adjustedSize = pointSize * (CGFloat)height / lineHeight;
					CTFontRef adjustedFont = CTFontCreateCopyWithAttributes(
						mpFont, adjustedSize, nullptr, nullptr);
					if (adjustedFont) {
						CFRelease(mpFont);
						mpFont = adjustedFont;
					}
				}
			}

			mMetrics.mAscent = std::max(1, (int)ceil(CTFontGetAscent(mpFont)));
			mMetrics.mDescent = std::max(0, (int)ceil(CTFontGetDescent(mpFont)));
			return true;
		}

		void GetMetrics(VDDisplayFontMetrics& metrics) override {
			metrics = mMetrics;
		}

		void GetGlyphMetrics(uint32 glyphIndex, VDDisplayFontGlyphMetrics& metrics) override {
			VDCoreTextGlyph glyph;
			if (!VDResolveCoreTextGlyph(mpFont, glyphIndex, glyph)) {
				metrics = {};
				return;
			}

			CGRect bounds = CTFontGetBoundingRectsForGlyphs(
				glyph.mpFont, kCTFontOrientationHorizontal, &glyph.mGlyph, nullptr, 1);
			CGSize advance {};
			CTFontGetAdvancesForGlyphs(
				glyph.mpFont, kCTFontOrientationHorizontal, &glyph.mGlyph, &advance, 1);

			const int left = (int)floor(CGRectGetMinX(bounds));
			const int right = (int)ceil(CGRectGetMaxX(bounds));
			metrics.mWidth = std::max(0, right - left);
			metrics.mHeight = mMetrics.mAscent + mMetrics.mDescent;
			metrics.mX = left;
			metrics.mY = mMetrics.mAscent;
			metrics.mAdvance = (int)lround(advance.width);
		}

		void ShapeText(const wchar_t *s, uint32 n,
			vdfastvector<VDDisplayFontGlyphPlacement>& glyphPlacements,
			vdrect32 *cellBounds, vdrect32 *glyphBounds, vdpoint32 *nextPos) override {
			const size_t placementBase = glyphPlacements.size();
			glyphPlacements.resize(placementBase + n);

			int x = 0;
			int minCellX = 0;
			int maxCellX = 0;
			int minGlyphX = 0;
			int maxGlyphX = 0;

			for(uint32 i = 0; i < n; ++i) {
				const uint32 glyphIndex = (uint32)s[i];
				VDDisplayFontGlyphMetrics metrics {};
				GetGlyphMetrics(glyphIndex, metrics);

				VDDisplayFontGlyphPlacement& placement = glyphPlacements[placementBase + i];
				placement.mGlyphIndex = glyphIndex;
				placement.mCellX = x;
				placement.mX = x + metrics.mX;
				placement.mY = -metrics.mY;
				placement.mOriginalOffset = i;

				x += metrics.mAdvance;
				minCellX = std::min(minCellX, x);
				maxCellX = std::max(maxCellX, x);
				minGlyphX = std::min(minGlyphX, placement.mX);
				maxGlyphX = std::max(maxGlyphX, placement.mX + metrics.mWidth);
			}

			minGlyphX = std::min(minGlyphX, minCellX);
			maxGlyphX = std::max(maxGlyphX, maxCellX);

			if (cellBounds)
				cellBounds->set(minCellX, -mMetrics.mAscent, maxCellX, mMetrics.mDescent);
			if (glyphBounds)
				glyphBounds->set(minGlyphX, -mMetrics.mAscent, maxGlyphX, mMetrics.mDescent);
			if (nextPos)
				*nextPos = vdpoint32(x, 0);
		}

		bool GetGlyphImage(uint32 glyphIndex, bool inverted, const VDPixmap& dst) override {
			if (dst.format != nsVDPixmap::kPixFormat_XRGB8888 || !dst.data
				|| dst.w < 0 || dst.h < 0)
				return false;

			VDDisplayFontGlyphMetrics metrics {};
			GetGlyphMetrics(glyphIndex, metrics);
			if (!metrics.mWidth || !metrics.mHeight || !dst.w || !dst.h)
				return true;

			VDCoreTextGlyph glyph;
			if (!VDResolveCoreTextGlyph(mpFont, glyphIndex, glyph))
				return false;

			const size_t sourcePitch = (size_t)metrics.mWidth * 4;
			std::vector<uint32> pixels((size_t)metrics.mWidth * metrics.mHeight);
			CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
			if (!colorSpace)
				return false;

			CGContextRef context = CGBitmapContextCreate(
				pixels.data(), metrics.mWidth, metrics.mHeight, 8, sourcePitch,
				colorSpace, static_cast<CGBitmapInfo>(
					static_cast<uint32>(kCGImageAlphaNoneSkipFirst)
					| static_cast<uint32>(kCGBitmapByteOrder32Little)));
			CGColorSpaceRelease(colorSpace);
			if (!context)
				return false;

			const CGFloat background = inverted ? 1 : 0;
			const CGFloat foreground = inverted ? 0 : 1;
			CGContextSetRGBFillColor(context, background, background, background, 1);
			CGContextFillRect(context, CGRectMake(0, 0, metrics.mWidth, metrics.mHeight));
			CGContextSetShouldAntialias(context, true);
			CGContextSetShouldSmoothFonts(context, false);
			CGContextSetRGBFillColor(context, foreground, foreground, foreground, 1);
			const CGPoint position {
				(CGFloat)-metrics.mX,
				(CGFloat)mMetrics.mDescent
			};
			CTFontDrawGlyphs(glyph.mpFont, &glyph.mGlyph, &position, 1, context);
			CGContextRelease(context);

			const int width = std::min(dst.w, metrics.mWidth);
			const int height = std::min(dst.h, metrics.mHeight);
			for(int y = 0; y < height; ++y) {
				const uint32 *source = pixels.data() + (size_t)(metrics.mHeight - 1 - y) * metrics.mWidth;
				uint32 *destination = (uint32 *)((char *)dst.data + (ptrdiff_t)y * dst.pitch);

				for(int x = 0; x < width; ++x) {
					uint32 rgb = source[x] & 0x00FFFFFF;
					if (inverted)
						rgb = (~rgb) & 0x00FFFFFF;
					destination[x] = rgb | 0xFF000000;
				}
			}

			return true;
		}

		vdsize32 MeasureString(const wchar_t *s, uint32 n, bool includeOverhangs) override {
			vdfastvector<VDDisplayFontGlyphPlacement> placements;
			vdrect32 bounds;
			ShapeText(s, n, placements,
				includeOverhangs ? nullptr : &bounds,
				includeOverhangs ? &bounds : nullptr,
				nullptr);
			return bounds.size();
		}

		vdsize32 FitString(const wchar_t *s, uint32 n, uint32 maxWidth, uint32 *count) override {
			uint32 fitted = 0;
			int width = 0;
			for(; fitted < n; ++fitted) {
				VDDisplayFontGlyphMetrics metrics {};
				GetGlyphMetrics((uint32)s[fitted], metrics);
				const int nextWidth = width + metrics.mAdvance;
				if (nextWidth > 0 && (uint32)nextWidth > maxWidth)
					break;
				width = nextWidth;
			}

			if (count)
				*count = fitted;
			return vdsize32(width, mMetrics.mAscent + mMetrics.mDescent);
		}

	private:
		CTFontRef mpFont = nullptr;
		VDDisplayFontMetrics mMetrics {};
	};
}

bool VDCreateDisplaySystemFont(int height, bool bold, const char *fontName, IVDDisplayFont **font) {
	if (!font)
		return false;

	*font = nullptr;
	vdrefptr<VDDisplayFontCoreText> newFont(new VDDisplayFontCoreText);
	if (!newFont->Init(height, bold, fontName))
		return false;

	*font = newFont.release();
	return true;
}
