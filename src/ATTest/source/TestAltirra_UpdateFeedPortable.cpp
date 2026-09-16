// Portable tests for the update feed Base64 decoder and XML subset parser.

#include <array>
#include <cstring>
#include <string>
#include <string_view>

#include <at/attest/portabletest.h>
#include <updatefeed.h>

#if !defined(_WIN32)
	namespace {
		bool gAcceptTestSignature = false;
		bool gSawValidTestSignature = false;
	}

	// The production verifier is platform-specific. This test double lets the
	// common feed parser be exercised through its signed-document entry point.
	bool ATUpdateVerifyFeedSignature(const void *signature, const void *data, size_t len) {
		const auto *signatureBytes = static_cast<const uint8 *>(signature);
		gSawValidTestSignature = len >= 5 && !memcmp(data, "<?xml", 5);

		for(size_t i = 0; i < 256; ++i)
			gSawValidTestSignature &= signatureBytes[i] == 0;

		return gAcceptTestSignature && gSawValidTestSignature;
	}
#endif

namespace {
	bool TextEquals(const ATUpdateFeedDoc& doc, ATUpdateFeedNodeRef node, std::string_view expected) {
		const VDStringSpanA text = doc.GetText(node);
		return text.size() == expected.size() && !memcmp(text.data(), expected.data(), expected.size());
	}
}

bool ATTestAltirraUpdateFeed(ATPortableTestContext& context) {
	std::array<uint8, 6> decoded {};
	AT_PORTABLE_TEST_ASSERT(context, ATDecodeBase64(decoded.data(), 6, "TWFsdGly", 8));
	AT_PORTABLE_TEST_ASSERT(context, !memcmp(decoded.data(), "Maltir", 6));
	AT_PORTABLE_TEST_ASSERT(context, ATDecodeBase64(decoded.data(), 1, "TQ==", 4));
	AT_PORTABLE_TEST_ASSERT(context, decoded[0] == 'M');
	AT_PORTABLE_TEST_ASSERT(context, ATDecodeBase64(decoded.data(), 2, "TWE=", 4));
	AT_PORTABLE_TEST_ASSERT(context, decoded[0] == 'M' && decoded[1] == 'a');

	AT_PORTABLE_TEST_ASSERT(context, !ATDecodeBase64(decoded.data(), 3, "TWE=", 4));
	AT_PORTABLE_TEST_ASSERT(context, !ATDecodeBase64(decoded.data(), 2, "TQ==", 4));
	AT_PORTABLE_TEST_ASSERT(context, !ATDecodeBase64(decoded.data(), 1, "TR==", 4));
	AT_PORTABLE_TEST_ASSERT(context, !ATDecodeBase64(decoded.data(), 2, "TWF=", 4));
	AT_PORTABLE_TEST_ASSERT(context, !ATDecodeBase64(decoded.data(), 1, "TQ=!", 4));
	AT_PORTABLE_TEST_ASSERT(context, !ATDecodeBase64(decoded.data(), 1, "TQ=", 3));

	static constexpr char kXML[] =
		"<feed version=\"1 &amp; 2\"><title>Altirra &lt;portable&gt;</title>"
		"<empty/><item id=\"first\">one</item><item id=\"second\">two</item></feed>";
	ATUpdateFeedDoc doc;
	AT_PORTABLE_TEST_ASSERT(context, ATParseUpdateFeedXML(kXML, sizeof kXML - 1, doc));

	const auto feedName = doc.GetNameToken(ATXMLSubsetHashedStr("feed"));
	const auto versionName = doc.GetNameToken(ATXMLSubsetHashedStr("version"));
	const auto titleName = doc.GetNameToken(ATXMLSubsetHashedStr("title"));
	const auto emptyName = doc.GetNameToken(ATXMLSubsetHashedStr("empty"));
	const auto itemName = doc.GetNameToken(ATXMLSubsetHashedStr("item"));
	const auto idName = doc.GetNameToken(ATXMLSubsetHashedStr("id"));
	const auto root = doc.GetRoot();
	AT_PORTABLE_TEST_ASSERT(context, root.IsElement(feedName));

	const VDStringSpanA version = doc.GetAttributeValue(root, versionName);
	AT_PORTABLE_TEST_ASSERT(context, version.size() == 5 && !memcmp(version.data(), "1 & 2", 5));
	AT_PORTABLE_TEST_ASSERT(context, TextEquals(doc, *(root / titleName), "Altirra <portable>"));
	AT_PORTABLE_TEST_ASSERT(context, (root / emptyName));

	auto firstItem = root / itemName;
	AT_PORTABLE_TEST_ASSERT(context, firstItem);
	AT_PORTABLE_TEST_ASSERT(context, TextEquals(doc, *firstItem, "one"));
	const VDStringSpanA firstId = doc.GetAttributeValue(firstItem, idName);
	AT_PORTABLE_TEST_ASSERT(context, firstId.size() == 5 && !memcmp(firstId.data(), "first", 5));
	++firstItem;
	AT_PORTABLE_TEST_ASSERT(context, firstItem.IsElement(itemName));
	AT_PORTABLE_TEST_ASSERT(context, TextEquals(doc, *firstItem, "two"));

	static constexpr char kReplacementXML[] = "<replacement>fresh</replacement>";
	AT_PORTABLE_TEST_ASSERT(context, ATParseUpdateFeedXML(kReplacementXML, sizeof kReplacementXML - 1, doc));
	const auto replacementName = doc.GetNameToken(ATXMLSubsetHashedStr("replacement"));
	AT_PORTABLE_TEST_ASSERT(context, doc.GetRoot().IsElement(replacementName));
	AT_PORTABLE_TEST_ASSERT(context, TextEquals(doc, *doc.GetRoot(), "fresh"));
	AT_PORTABLE_TEST_ASSERT(context, doc.GetNameToken(ATXMLSubsetHashedStr("feed")) == ATUpdateFeedName::Invalid);

	static constexpr char kUnclosed[] = "<feed><item>missing end tags";
	AT_PORTABLE_TEST_ASSERT(context, !ATParseUpdateFeedXML(kUnclosed, sizeof kUnclosed - 1, doc));
	static constexpr char kMismatched[] = "<feed></item>";
	AT_PORTABLE_TEST_ASSERT(context, !ATParseUpdateFeedXML(kMismatched, sizeof kMismatched - 1, doc));
	static constexpr char kBadAttribute[] = "<feed version=unquoted/>";
	AT_PORTABLE_TEST_ASSERT(context, !ATParseUpdateFeedXML(kBadAttribute, sizeof kBadAttribute - 1, doc));

#if !defined(_WIN32)
	std::string signedFeed = "<?xml version=\"1.0\" encoding=\"utf-8\"?><!-- sig:";
	signedFeed.append(342, 'A');
	signedFeed += "== -->"
		"<rss><channel><item>"
		"<title>Altirra 4.50 (4.50.1.2)</title>"
		"<description>&lt;p&gt;Portable release&lt;/p&gt;</description>"
		"<category>release</category>"
		"<link>https://example.test/altirra</link>"
		"</item></channel></rss>";

	ATUpdateFeedInfo feedInfo;
	gAcceptTestSignature = false;
	gSawValidTestSignature = false;
	AT_PORTABLE_TEST_ASSERT(context, !feedInfo.Parse(signedFeed.data(), signedFeed.size()));
	AT_PORTABLE_TEST_ASSERT(context, gSawValidTestSignature);

	gAcceptTestSignature = true;
	gSawValidTestSignature = false;
	AT_PORTABLE_TEST_ASSERT(context, feedInfo.Parse(signedFeed.data(), signedFeed.size()));
	AT_PORTABLE_TEST_ASSERT(context, gSawValidTestSignature);
	AT_PORTABLE_TEST_ASSERT(context, feedInfo.mLatestVersion == (UINT64_C(4) << 48) + (UINT64_C(50) << 32) + (UINT64_C(1) << 16) + 2);
	AT_PORTABLE_TEST_ASSERT(context, feedInfo.mLatestReleaseItem.mTitle == L"Altirra 4.50");
	AT_PORTABLE_TEST_ASSERT(context, feedInfo.mLatestReleaseItem.mLink == L"https://example.test/altirra");
	const auto paragraphName = feedInfo.mLatestReleaseItem.mDoc.GetNameToken(ATXMLSubsetHashedStr("p"));
	AT_PORTABLE_TEST_ASSERT(context, feedInfo.mLatestReleaseItem.mDoc.GetRoot().IsElement(paragraphName));
	AT_PORTABLE_TEST_ASSERT(context, TextEquals(feedInfo.mLatestReleaseItem.mDoc, *feedInfo.mLatestReleaseItem.mDoc.GetRoot(), "Portable release"));
#endif

	return true;
}
