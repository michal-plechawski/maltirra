// Altirra portable text document model tests

#include <algorithm>
#include <string>

#include <at/attest/portabletest.h>
#include <textdom.h>

namespace {
	using namespace nsVDTextDOM;

	bool IteratorAt(const Iterator& it, int para, int line, int offset) {
		return it.mPara == para && it.mLine == line && it.mOffset == offset;
	}

	bool TextEquals(const vdfastvector<wchar_t>& text, const wchar_t *expected) {
		return std::wstring(text.begin(), text.end()) == expected;
	}

	class HardWrapCallback final : public IDocumentCallback {
	public:
		void InvalidateRows(int, int) override {}
		void VerticalShiftRows(int, int) override {}

		void ReflowPara(int paraIdx, const Paragraph& para) override {
			size_t len = para.mText.size();

			if (len && para.mText.back() == '\n')
				--len;

			const int lines = len ? ((int)len + 3) / 4 : 1;
			mLines.resize(lines);

			for(int i = 0; i < lines; ++i)
				mLines[i] = Line { i * 4, std::min<int>(4, (int)len - 4 * i), 1 };

			mpDocument->ReflowPara(paraIdx, mLines.data(), lines);
		}

		void RecolorParagraph(int, Paragraph&) override {}
		void ChangeTotalHeight(int) override {}

		Document *mpDocument = nullptr;
		vdfastvector<Line> mLines;
	};
}

bool ATTestAltirraTextDOM(ATPortableTestContext& context) {
	using namespace nsVDTextDOM;

	{
		Document doc;
		Iterator it(doc);
		Iterator after;

		doc.Insert(it, L"Hello, world!", 13, &after);
		AT_PORTABLE_TEST_ASSERT(context, IteratorAt(it, 0, 0, 0));
		AT_PORTABLE_TEST_ASSERT(context, IteratorAt(after, 0, 0, 13));

		Iterator after2;
		doc.Insert(it, L"foo\n", 4, &after2);
		AT_PORTABLE_TEST_ASSERT(context, IteratorAt(it, 0, 0, 0));
		AT_PORTABLE_TEST_ASSERT(context, IteratorAt(after, 1, 0, 13));
		AT_PORTABLE_TEST_ASSERT(context, IteratorAt(after2, 1, 0, 0));

		Iterator it2(doc, 0, 0, 1);
		doc.Insert(it, L"\nfoo", 4, &after);
		AT_PORTABLE_TEST_ASSERT(context, IteratorAt(it, 0, 0, 0));
		AT_PORTABLE_TEST_ASSERT(context, IteratorAt(it2, 1, 0, 4));
		AT_PORTABLE_TEST_ASSERT(context, IteratorAt(after2, 2, 0, 0));
		AT_PORTABLE_TEST_ASSERT(context, IteratorAt(after, 1, 0, 3));

		Iterator end(doc, 2, 0, 13);
		doc.Delete(after, after2);
		AT_PORTABLE_TEST_ASSERT(context, IteratorAt(it, 0, 0, 0));
		AT_PORTABLE_TEST_ASSERT(context, IteratorAt(after, 1, 0, 3));
		AT_PORTABLE_TEST_ASSERT(context, IteratorAt(it2, 1, 0, 3));
		AT_PORTABLE_TEST_ASSERT(context, IteratorAt(after2, 1, 0, 3));
		AT_PORTABLE_TEST_ASSERT(context, IteratorAt(end, 1, 0, 16));

		doc.Insert(it2, L"test1\ntest2\ntest3", 17, &after);
		AT_PORTABLE_TEST_ASSERT(context, IteratorAt(it, 0, 0, 0));
		AT_PORTABLE_TEST_ASSERT(context, IteratorAt(it2, 1, 0, 3));
		AT_PORTABLE_TEST_ASSERT(context, IteratorAt(after2, 1, 0, 3));
		AT_PORTABLE_TEST_ASSERT(context, IteratorAt(after, 3, 0, 5));
		AT_PORTABLE_TEST_ASSERT(context, IteratorAt(end, 3, 0, 18));
	}

	{
		Document doc;
		HardWrapCallback callback;
		callback.mpDocument = &doc;
		doc.SetCallback(&callback);

		Iterator it(doc);
		Iterator after;
		doc.Insert(it, L"Hello, world!", 13, &after);
		AT_PORTABLE_TEST_ASSERT(context, IteratorAt(it, 0, 0, 0));
		AT_PORTABLE_TEST_ASSERT(context, IteratorAt(after, 0, 3, 1));

		Iterator after2;
		doc.Insert(it, L"foo\n", 4, &after2);
		AT_PORTABLE_TEST_ASSERT(context, IteratorAt(it, 0, 0, 0));
		AT_PORTABLE_TEST_ASSERT(context, IteratorAt(after, 1, 3, 1));
		AT_PORTABLE_TEST_ASSERT(context, IteratorAt(after2, 1, 0, 0));

		Iterator it2(doc, 0, 0, 1);
		doc.Insert(it, L"\nfoo", 4, &after);
		AT_PORTABLE_TEST_ASSERT(context, IteratorAt(it, 0, 0, 0));
		AT_PORTABLE_TEST_ASSERT(context, IteratorAt(it2, 1, 1, 0));
		AT_PORTABLE_TEST_ASSERT(context, IteratorAt(after2, 2, 0, 0));
		AT_PORTABLE_TEST_ASSERT(context, IteratorAt(after, 1, 0, 3));

		Iterator end(doc, 2, 3, 1);
		doc.Delete(after, after2);
		AT_PORTABLE_TEST_ASSERT(context, IteratorAt(it, 0, 0, 0));
		AT_PORTABLE_TEST_ASSERT(context, IteratorAt(after, 1, 0, 3));
		AT_PORTABLE_TEST_ASSERT(context, IteratorAt(it2, 1, 0, 3));
		AT_PORTABLE_TEST_ASSERT(context, IteratorAt(after2, 1, 0, 3));
		AT_PORTABLE_TEST_ASSERT(context, IteratorAt(end, 1, 3, 4));

		doc.Insert(it2, L"test1\ntest2\ntest3", 17, &after);
		AT_PORTABLE_TEST_ASSERT(context, IteratorAt(it, 0, 0, 0));
		AT_PORTABLE_TEST_ASSERT(context, IteratorAt(it2, 1, 0, 3));
		AT_PORTABLE_TEST_ASSERT(context, IteratorAt(after2, 1, 0, 3));
		AT_PORTABLE_TEST_ASSERT(context, IteratorAt(after, 3, 1, 1));
		AT_PORTABLE_TEST_ASSERT(context, IteratorAt(end, 3, 4, 2));
	}

	{
		Document doc;
		Iterator start(doc);
		Iterator end;
		doc.Insert(start, L"ab\ncd", 5, &end);

		vdfastvector<wchar_t> text;
		doc.GetText(start, end, false, text);
		AT_PORTABLE_TEST_ASSERT(context, TextEquals(text, L"ab\ncd"));
		doc.GetText(start, end, true, text);
		AT_PORTABLE_TEST_ASSERT(context, TextEquals(text, L"ab\r\ncd"));

		end.MoveToPrevChar();
		AT_PORTABLE_TEST_ASSERT(context, IteratorAt(end, 1, 0, 1));
		end.MoveToPrevLine();
		AT_PORTABLE_TEST_ASSERT(context, IteratorAt(end, 0, 0, 1));
		end.MoveToEnd();
		AT_PORTABLE_TEST_ASSERT(context, IteratorAt(end, 1, 0, 2));

		doc.DeleteAll();
		AT_PORTABLE_TEST_ASSERT(context, IteratorAt(start, 0, 0, 0));
		AT_PORTABLE_TEST_ASSERT(context, IteratorAt(end, 0, 0, 0));
	}

	return true;
}
