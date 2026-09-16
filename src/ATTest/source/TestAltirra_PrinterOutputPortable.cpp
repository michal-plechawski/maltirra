// Altirra portable printer output model tests

#include <cmath>
#include <string>

#include <at/attest/portabletest.h>
#include <printeroutput.h>

namespace {
	bool Near(float a, float b) {
		return std::fabs(a - b) < 0.0001f;
	}
}

bool ATTestAltirraPrinterOutput(ATPortableTestContext& context) {
	ATPrinterOutputManager manager;
	int textAdded = 0;
	int textRemoved = 0;
	int graphicsAdded = 0;
	int graphicsRemoved = 0;

	vdfunction<void(ATPrinterOutput&)> onTextAdded = [&](ATPrinterOutput&) { ++textAdded; };
	vdfunction<void(ATPrinterOutput&)> onTextRemoved = [&](ATPrinterOutput&) { ++textRemoved; };
	vdfunction<void(ATPrinterGraphicalOutput&)> onGraphicsAdded = [&](ATPrinterGraphicalOutput&) { ++graphicsAdded; };
	vdfunction<void(ATPrinterGraphicalOutput&)> onGraphicsRemoved = [&](ATPrinterGraphicalOutput&) { ++graphicsRemoved; };
	manager.OnAddedOutput.Add(&onTextAdded);
	manager.OnRemovingOutput.Add(&onTextRemoved);
	manager.OnAddedGraphicalOutput.Add(&onGraphicsAdded);
	manager.OnRemovingGraphicalOutput.Add(&onGraphicsRemoved);

	vdrefptr<IATPrinterOutput> textHandle = manager.CreatePrinterOutput(L"Text printer");
	AT_PORTABLE_TEST_ASSERT(context, textAdded == 1);
	AT_PORTABLE_TEST_ASSERT(context, manager.GetOutputCount() == 1);
	ATPrinterOutput& textOutput = manager.GetOutput(0);
	AT_PORTABLE_TEST_ASSERT(context, std::wstring(textOutput.GetName()) == L"Text printer");
	AT_PORTABLE_TEST_ASSERT(context, textOutput.WantUnicode());
	AT_PORTABLE_TEST_ASSERT(context, textOutput.AsInterface(IATPrinterOutput::kTypeID) == textHandle.get());

	int textInvalidations = 0;
	textOutput.SetOnInvalidation([&] { ++textInvalidations; });
	const uint8 rawText[] { 'A', '\r', '\n', 'B', '\n', '\r', 'C', 1, 0xE9 };
	textOutput.WriteRaw(rawText, sizeof rawText);
	AT_PORTABLE_TEST_ASSERT(context, textInvalidations == 1);
	AT_PORTABLE_TEST_ASSERT(context,
		std::wstring(textOutput.GetTextPointer(0), textOutput.GetLength()) == L"A\nB\nC?i");

	const uint8 rawSuffix[] { 'D' };
	textOutput.WriteRaw(rawSuffix, sizeof rawSuffix);
	AT_PORTABLE_TEST_ASSERT(context, textInvalidations == 1);
	textOutput.Revalidate();
	const wchar_t unicodeSuffix[] { L'E', 0, L'F' };
	textOutput.WriteUnicode(unicodeSuffix, 3);
	AT_PORTABLE_TEST_ASSERT(context, textInvalidations == 2);
	AT_PORTABLE_TEST_ASSERT(context,
		std::wstring(textOutput.GetTextPointer(0), textOutput.GetLength()) == L"A\nB\nC?iDEF");

	textOutput.Clear();
	textOutput.Revalidate();
	const std::wstring longLine(132, L'x');
	textOutput.WriteUnicode(longLine.data(), longLine.size());
	AT_PORTABLE_TEST_ASSERT(context, textOutput.GetLength() == 133);
	AT_PORTABLE_TEST_ASSERT(context, textOutput.GetTextPointer(132)[0] == L'\n');

	ATPrinterGraphicsSpec spec {};
	spec.mPageWidthMM = 210.0f;
	spec.mPageVBorderMM = 5.0f;
	spec.mDotRadiusMM = 0.2f;
	spec.mVerticalDotPitchMM = 0.4f;
	spec.mbBit0Top = true;
	spec.mNumPins = 9;

	vdrefptr<IATPrinterGraphicalOutput> graphicsHandle =
		manager.CreatePrinterGraphicalOutput(L"Graphics printer", spec);
	AT_PORTABLE_TEST_ASSERT(context, graphicsAdded == 1);
	AT_PORTABLE_TEST_ASSERT(context, manager.GetGraphicalOutputCount() == 1);
	ATPrinterGraphicalOutput& graphicsOutput = manager.GetGraphicalOutput(0);
	AT_PORTABLE_TEST_ASSERT(context, std::wstring(graphicsOutput.GetName()) == L"Graphics printer");
	AT_PORTABLE_TEST_ASSERT(context,
		graphicsOutput.AsInterface(IATPrinterGraphicalOutput::kTypeID) == graphicsHandle.get());
	AT_PORTABLE_TEST_ASSERT(context, !graphicsOutput.HasVectors());

	bool invalidatedAll = false;
	vdrect32f invalidationRect;
	AT_PORTABLE_TEST_ASSERT(context, graphicsOutput.ExtractInvalidationRect(invalidatedAll, invalidationRect));
	AT_PORTABLE_TEST_ASSERT(context, invalidatedAll);

	int graphicsInvalidations = 0;
	int clearCalls = 0;
	graphicsOutput.SetOnInvalidation([&] { ++graphicsInvalidations; });
	graphicsOutput.SetOnClear([&] { ++clearCalls; });
	graphicsOutput.Print(10.0f, 0x5);
	AT_PORTABLE_TEST_ASSERT(context, graphicsInvalidations == 1);
	AT_PORTABLE_TEST_ASSERT(context, graphicsOutput.ExtractInvalidationRect(invalidatedAll, invalidationRect));
	AT_PORTABLE_TEST_ASSERT(context, !invalidatedAll);
	AT_PORTABLE_TEST_ASSERT(context, Near(invalidationRect.left, 9.8f));
	AT_PORTABLE_TEST_ASSERT(context, Near(invalidationRect.top, 5.0f));
	AT_PORTABLE_TEST_ASSERT(context, Near(invalidationRect.right, 10.2f));
	AT_PORTABLE_TEST_ASSERT(context, Near(invalidationRect.bottom, 8.6f));

	graphicsOutput.FeedPaper(2.0f);
	graphicsOutput.Print(12.0f, 0x2);
	const vdrect32f documentBounds = graphicsOutput.GetDocumentBounds();
	AT_PORTABLE_TEST_ASSERT(context, Near(documentBounds.left, 0));
	AT_PORTABLE_TEST_ASSERT(context, Near(documentBounds.right, 210.0f));
	AT_PORTABLE_TEST_ASSERT(context, Near(documentBounds.top, 0));
	AT_PORTABLE_TEST_ASSERT(context, Near(documentBounds.bottom, 15.6f));

	ATPrinterGraphicalOutput::CullInfo cullInfo {};
	const vdrect32f fullPage(0, 0, 210, 30);
	AT_PORTABLE_TEST_ASSERT(context, graphicsOutput.PreCull(cullInfo, fullPage));
	vdfastvector<ATPrinterGraphicalOutput::RenderColumn> columns;
	float renderY = 0;
	AT_PORTABLE_TEST_ASSERT(context, graphicsOutput.ExtractNextLine(columns, renderY, cullInfo, fullPage));
	AT_PORTABLE_TEST_ASSERT(context, Near(renderY, 5.0f));
	AT_PORTABLE_TEST_ASSERT(context, columns.size() == 1);
	AT_PORTABLE_TEST_ASSERT(context, Near(columns[0].mX, 10.0f));
	AT_PORTABLE_TEST_ASSERT(context, columns[0].mPins == 0x5);
	AT_PORTABLE_TEST_ASSERT(context, graphicsOutput.ExtractNextLine(columns, renderY, cullInfo, fullPage));
	AT_PORTABLE_TEST_ASSERT(context, Near(renderY, 7.0f));
	AT_PORTABLE_TEST_ASSERT(context, columns.size() == 1 && columns[0].mPins == 0x2);
	AT_PORTABLE_TEST_ASSERT(context, !graphicsOutput.ExtractNextLine(columns, renderY, cullInfo, fullPage));

	AT_PORTABLE_TEST_ASSERT(context, graphicsOutput.PreCull(cullInfo, fullPage));
	vdfastvector<ATPrinterGraphicalOutput::RenderDot> dots;
	graphicsOutput.ExtractNextLineDots(dots, cullInfo, fullPage);
	AT_PORTABLE_TEST_ASSERT(context, dots.size() == 3);
	AT_PORTABLE_TEST_ASSERT(context, Near(dots[0].mX, 10.0f) && Near(dots[0].mY, 5.2f));
	AT_PORTABLE_TEST_ASSERT(context, Near(dots[1].mX, 10.0f) && Near(dots[1].mY, 6.0f));
	AT_PORTABLE_TEST_ASSERT(context, Near(dots[2].mX, 12.0f) && Near(dots[2].mY, 7.6f));

	graphicsOutput.AddVector(vdfloat2(25, 25), vdfloat2(5, 5), 0x123456);
	AT_PORTABLE_TEST_ASSERT(context, graphicsOutput.HasVectors());
	vdfastvector<ATPrinterGraphicalOutput::RenderVector> vectors;
	graphicsOutput.ExtractVectors(vectors, vdrect32f(0, 0, 30, 30));
	AT_PORTABLE_TEST_ASSERT(context, vectors.size() == 1);
	AT_PORTABLE_TEST_ASSERT(context, Near(vectors[0].mX1, 5) && Near(vectors[0].mY1, 5));
	AT_PORTABLE_TEST_ASSERT(context, Near(vectors[0].mX2, 25) && Near(vectors[0].mY2, 25));
	AT_PORTABLE_TEST_ASSERT(context, vectors[0].mLinearColor == 0x123456);
	AT_PORTABLE_TEST_ASSERT(context, graphicsOutput.ConvertColor(0) == 0);

	graphicsOutput.Clear();
	AT_PORTABLE_TEST_ASSERT(context, clearCalls == 1);
	AT_PORTABLE_TEST_ASSERT(context, !graphicsOutput.HasVectors());
	AT_PORTABLE_TEST_ASSERT(context, graphicsOutput.ExtractInvalidationRect(invalidatedAll, invalidationRect));
	AT_PORTABLE_TEST_ASSERT(context, invalidatedAll);

	// Force several hash-table growths and wrapped probe sequences. Each point
	// occupies one 10x10 mm vector tile and must be returned exactly once.
	for(int y = 0; y < 10; ++y) {
		for(int x = 0; x < 20; ++x) {
			const vdfloat2 point(5.0f + x * 10.0f, 5.0f + y * 10.0f);
			graphicsOutput.AddVector(point, point, (uint32)(y * 20 + x));
		}
	}

	vectors.clear();
	graphicsOutput.ExtractVectors(vectors, vdrect32f(0, 0, 200, 100));
	AT_PORTABLE_TEST_ASSERT(context, vectors.size() == 200);

	graphicsHandle = nullptr;
	AT_PORTABLE_TEST_ASSERT(context, graphicsRemoved == 1);
	AT_PORTABLE_TEST_ASSERT(context, manager.GetGraphicalOutputCount() == 0);
	textHandle = nullptr;
	AT_PORTABLE_TEST_ASSERT(context, textRemoved == 1);
	AT_PORTABLE_TEST_ASSERT(context, manager.GetOutputCount() == 0);
	return true;
}
