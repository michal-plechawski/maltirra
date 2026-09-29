// Portable VDDisplay text renderer tests.

#include <vector>

#include <at/attest/portabletest.h>
#include <vd2/Kasumi/pixmap.h>
#include <vd2/system/refcount.h>
#include <vd2/VDDisplay/font.h>
#include <vd2/VDDisplay/textrenderer.h>

namespace {
	class CountingFont final : public vdrefcounted<IVDDisplayFont> {
	public:
		explicit CountingFont(IVDDisplayFont *font) : mpFont(font) {}

		void *AsInterface(uint32) override { return nullptr; }
		void GetMetrics(VDDisplayFontMetrics& metrics) override { mpFont->GetMetrics(metrics); }
		void GetGlyphMetrics(uint32 glyphIndex, VDDisplayFontGlyphMetrics& metrics) override {
			++mMetricsCalls;
			mpFont->GetGlyphMetrics(glyphIndex, metrics);
		}
		void ShapeText(const wchar_t *s, uint32 n, vdfastvector<VDDisplayFontGlyphPlacement>& placements, vdrect32 *cellBounds, vdrect32 *glyphBounds, vdpoint32 *nextPos) override {
			mpFont->ShapeText(s, n, placements, cellBounds, glyphBounds, nextPos);
		}
		bool GetGlyphImage(uint32 glyphIndex, bool inverted, const VDPixmap& dst) override {
			++mImageCalls;
			if (inverted)
				++mInvertedImageCalls;
			return mpFont->GetGlyphImage(glyphIndex, inverted, dst);
		}
		vdsize32 MeasureString(const wchar_t *s, uint32 n, bool includeOverhangs) override { return mpFont->MeasureString(s, n, includeOverhangs); }
		vdsize32 FitString(const wchar_t *s, uint32 n, uint32 maxWidth, uint32 *count) override { return mpFont->FitString(s, n, maxWidth, count); }

		uint32 mMetricsCalls = 0;
		uint32 mImageCalls = 0;
		uint32 mInvertedImageCalls = 0;

	private:
		vdrefptr<IVDDisplayFont> mpFont;
	};

	struct BltBatch {
		std::vector<VDDisplayBlt> mBlts;
		std::vector<uint32> mSourcePixels;
		IVDDisplayRenderer::BltMode mMode = IVDDisplayRenderer::kBltMode_Normal;
		uint32 mImageUniqueness = 0;
	};

	class RecordingRenderer final : public IVDDisplayRenderer {
	public:
		const VDDisplayRendererCaps& GetCaps() override { return mCaps; }
		VDDisplayTextRenderer *GetTextRenderer() override { return nullptr; }
		void SetColorRGB(uint32 color) override { mColor = color; ++mSetColorCalls; }
		void FillRect(sint32, sint32, sint32, sint32) override {}
		void MultiFillRect(const vdrect32 *, uint32) override {}
		void AlphaFillRect(sint32, sint32, sint32, sint32, uint32) override {}
		void AlphaTriStrip(const vdfloat2 *, uint32, uint32) override {}
		void Blt(sint32, sint32, VDDisplayImageView&) override {}
		void Blt(sint32, sint32, VDDisplayImageView&, sint32, sint32, sint32, sint32) override {}
		void StretchBlt(sint32, sint32, sint32, sint32, VDDisplayImageView&, sint32, sint32, sint32, sint32, const VDDisplayBltOptions&) override {}
		void MultiBlt(const VDDisplayBlt *blts, uint32 n, VDDisplayImageView& imageView, BltMode mode) override {
			BltBatch batch;
			batch.mMode = mode;
			batch.mImageUniqueness = imageView.GetUniquenessCounter();
			if (n)
				batch.mBlts.assign(blts, blts + n);

			const VDPixmap& image = imageView.GetImage();
			for(const VDDisplayBlt& blt : batch.mBlts) {
				const char *row = (const char *)image.data + image.pitch * blt.mSrcY;
				batch.mSourcePixels.push_back(((const uint32 *)row)[blt.mSrcX]);
			}

			mBatches.push_back(batch);
		}
		void PolyLine(const vdpoint32 *, uint32) override {}
		void PolyLineF(const vdfloat2 *, uint32, bool) override {}
		bool PushViewport(const vdrect32&, sint32, sint32) override { return true; }
		void PopViewport() override {}
		IVDDisplayRenderer *BeginSubRender(const vdrect32&, VDDisplaySubRenderCache&) override { return this; }
		void EndSubRender() override {}

		VDDisplayRendererCaps mCaps {};
		uint32 mColor = 0;
		uint32 mSetColorCalls = 0;
		std::vector<BltBatch> mBatches;
	};

	class TestTextRenderer final : public VDDisplayTextRenderer {
	public:
		bool TryAllocate(uint32 w, uint32 h) { return Allocate(0, 0, w, h) != nullptr; }
	};
}

bool ATTestVDDisplayTextRenderer(ATPortableTestContext& context) {
	const uint32 atlasPixels[] = {
		0xFF010203, 0xFF040506, 0xFF111213, 0xFF141516,
		0xFF070809, 0xFF0A0B0C, 0xFF171819, 0xFF1A1B1C
	};
	VDPixmap atlas {};
	atlas.data = const_cast<uint32 *>(atlasPixels);
	atlas.pitch = 4 * sizeof(uint32);
	atlas.w = 4;
	atlas.h = 2;
	atlas.format = nsVDPixmap::kPixFormat_XRGB8888;

	const wchar_t glyphChars[] = { L'A', L'B' };
	const VDDisplayBitmapFontGlyphInfo glyphInfos[] = {
		{ 0, 0, -1, -3, 2, 2, 4 },
		{ 2, 0,  0, -2, 2, 2, 5 }
	};
	const VDDisplayFontMetrics fontMetrics { 5, 2 };

	vdrefptr<IVDDisplayFont> bitmapFont;
	VDCreateDisplayBitmapFont(fontMetrics, 2, glyphChars, glyphInfos, atlas, 0, nullptr, ~bitmapFont);
	vdrefptr<CountingFont> font(new CountingFont(bitmapFont));

	RecordingRenderer renderer;
	VDDisplayTextRenderer textRenderer;
	textRenderer.Init(&renderer, 4, 4);
	textRenderer.Begin();
	textRenderer.DrawTextLine(1, 2, L"A");
	AT_PORTABLE_TEST_ASSERT(context, renderer.mBatches.empty());

	textRenderer.SetFont(font);
	textRenderer.SetColorRGB(0xFFFFFF);
	textRenderer.SetAlignment(VDDisplayTextRenderer::kAlignCenter, VDDisplayTextRenderer::kVertAlignTop);
	textRenderer.SetPosition(20, 30);
	textRenderer.DrawTextSpan(L"AA", 2);

	AT_PORTABLE_TEST_ASSERT(context, renderer.mColor == 0xFFFFFF);
	AT_PORTABLE_TEST_ASSERT(context, renderer.mSetColorCalls == 1);
	AT_PORTABLE_TEST_ASSERT(context, renderer.mBatches.size() == 1);
	AT_PORTABLE_TEST_ASSERT(context, renderer.mBatches[0].mMode == IVDDisplayRenderer::kBltMode_Color);
	AT_PORTABLE_TEST_ASSERT(context, renderer.mBatches[0].mBlts.size() == 2);
	AT_PORTABLE_TEST_ASSERT(context, renderer.mBatches[0].mBlts[0].mDestX == 15);
	AT_PORTABLE_TEST_ASSERT(context, renderer.mBatches[0].mBlts[0].mDestY == 32);
	AT_PORTABLE_TEST_ASSERT(context, renderer.mBatches[0].mBlts[1].mDestX == 19);
	AT_PORTABLE_TEST_ASSERT(context, renderer.mBatches[0].mBlts[1].mDestY == 32);
	AT_PORTABLE_TEST_ASSERT(context, renderer.mBatches[0].mBlts[0].mSrcX == 4);
	AT_PORTABLE_TEST_ASSERT(context, renderer.mBatches[0].mBlts[0].mSrcY == 0);
	AT_PORTABLE_TEST_ASSERT(context, renderer.mBatches[0].mBlts[0].mWidth == 2);
	AT_PORTABLE_TEST_ASSERT(context, renderer.mBatches[0].mBlts[0].mHeight == 2);
	AT_PORTABLE_TEST_ASSERT(context, renderer.mBatches[0].mSourcePixels[0] == atlasPixels[0]);
	AT_PORTABLE_TEST_ASSERT(context, font->mMetricsCalls == 1);
	AT_PORTABLE_TEST_ASSERT(context, font->mImageCalls == 2);
	AT_PORTABLE_TEST_ASSERT(context, font->mInvertedImageCalls == 1);
	const uint32 imageUniqueness = renderer.mBatches[0].mImageUniqueness;

	renderer.mBatches.clear();
	textRenderer.SetColorRGB(0x000001);
	textRenderer.SetPosition(20, 30);
	textRenderer.DrawTextSpan(L"AA", 2);
	AT_PORTABLE_TEST_ASSERT(context, renderer.mBatches.size() == 1);
	AT_PORTABLE_TEST_ASSERT(context, renderer.mBatches[0].mBlts[0].mSrcX == 0);
	AT_PORTABLE_TEST_ASSERT(context, renderer.mBatches[0].mImageUniqueness == imageUniqueness);
	AT_PORTABLE_TEST_ASSERT(context, font->mMetricsCalls == 1);
	AT_PORTABLE_TEST_ASSERT(context, font->mImageCalls == 2);

	renderer.mBatches.clear();
	textRenderer.SetAlignment(VDDisplayTextRenderer::kAlignRight, VDDisplayTextRenderer::kVertAlignBottom);
	textRenderer.DrawTextLine(20, 30, L"A");
	AT_PORTABLE_TEST_ASSERT(context, renderer.mBatches.size() == 1);
	AT_PORTABLE_TEST_ASSERT(context, renderer.mBatches[0].mBlts[0].mDestX == 15);
	AT_PORTABLE_TEST_ASSERT(context, renderer.mBatches[0].mBlts[0].mDestY == 25);

	textRenderer.End();
	renderer.mBatches.clear();
	textRenderer.DrawTextLine(1, 2, L"A");
	AT_PORTABLE_TEST_ASSERT(context, renderer.mBatches.empty());

	RecordingRenderer color2Renderer;
	VDDisplayTextRenderer color2TextRenderer;
	color2TextRenderer.Init(&color2Renderer, 4, 4, true);
	color2TextRenderer.Begin();
	color2TextRenderer.SetFont(font);
	color2TextRenderer.SetColorRGB(0xFFFFFF);
	color2TextRenderer.DrawTextLine(10, 10, L"A");
	AT_PORTABLE_TEST_ASSERT(context, color2Renderer.mBatches.size() == 1);
	AT_PORTABLE_TEST_ASSERT(context, color2Renderer.mBatches[0].mMode == IVDDisplayRenderer::kBltMode_Color2);
	AT_PORTABLE_TEST_ASSERT(context, color2Renderer.mBatches[0].mBlts[0].mSrcX == 0);

	RecordingRenderer flushRenderer;
	VDDisplayTextRenderer flushTextRenderer;
	flushTextRenderer.Init(&flushRenderer, 2, 2);
	flushTextRenderer.Begin();
	flushTextRenderer.SetFont(font);
	flushTextRenderer.DrawTextLine(0, 0, L"AB");
	AT_PORTABLE_TEST_ASSERT(context, flushRenderer.mBatches.size() == 2);
	AT_PORTABLE_TEST_ASSERT(context, flushRenderer.mBatches[0].mBlts.size() == 1);
	AT_PORTABLE_TEST_ASSERT(context, flushRenderer.mBatches[1].mBlts.size() == 1);
	AT_PORTABLE_TEST_ASSERT(context, flushRenderer.mBatches[0].mBlts[0].mDestX == -1);
	AT_PORTABLE_TEST_ASSERT(context, flushRenderer.mBatches[1].mBlts[0].mDestX == 4);
	AT_PORTABLE_TEST_ASSERT(context, flushRenderer.mBatches[1].mBlts[0].mSrcX == 0);

	TestTextRenderer allocationRenderer;
	allocationRenderer.Init(nullptr, 2, 2);
	AT_PORTABLE_TEST_ASSERT(context, !allocationRenderer.TryAllocate(3, 1));
	AT_PORTABLE_TEST_ASSERT(context, !allocationRenderer.TryAllocate(1, 3));
	AT_PORTABLE_TEST_ASSERT(context, allocationRenderer.TryAllocate(2, 2));

	return true;
}
