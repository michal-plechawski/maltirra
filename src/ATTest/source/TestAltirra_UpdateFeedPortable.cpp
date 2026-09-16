// Portable tests for update feed decoding, parsing, and signature verification.

#include <array>
#include <cstring>
#include <string>
#include <string_view>

#include <at/attest/portabletest.h>
#include <updatefeed.h>

namespace {
	bool TextEquals(const ATUpdateFeedDoc& doc, ATUpdateFeedNodeRef node, std::string_view expected) {
		const VDStringSpanA text = doc.GetText(node);
		return text.size() == expected.size() && !memcmp(text.data(), expected.data(), expected.size());
	}

	static constexpr char kSignedReleaseFeed[] =
		"<?xml version=\"1.0\" encoding=\"utf-8\"?><!-- sig:"
		"WA70kPF9f5aLqQoALRG4DWayAm4bm5cSXIOpqSpejrsloGnTOgL+bpq5P9rcTc1NUXco5sx+QrlvkxSDQFpgWtWiXJw5rC78aAfPjZU"
		"b5pY60XAFebgT+eOhj8ZpFapyPcXu9SwWFGewZFB1Htcp8PEN8sOVUrveKRrHIGXYTw9wgbkTX+1jQtVbwyNmpBKWyCX3Sm8C7PKyB"
		"Nro1/HKo0/R9y3hg6CGoxXAYgW+8cixYrHjBgCbbAyEMjho0zx0mrI9oGg3kgwVH3xS3DPhDj8poxh3LvAA0sbfh8bKLTZt9TcrJCo"
		"7ajh+Bl1pdoNJJ4WcpV8wQ5YSaW/9FvWwGA== -->"
		"<rss version=\"2.0\"> <channel> <title>Altirra update feed (release channel)</title> "
		"<link>https://www.virtualdub.org/altirra.html</link> "
		"<description>Automatic update notifications for release versions of the Altirra emulator.</description> "
		"<item> <title>Altirra 4.40 (4.40.200.17000; win7; sse2)</title><description> &lt;ul&gt;\n"
		"&lt;li&gt;Accuracy: ANTIC line buffer and POKEY pot scan fixes.&lt;/li&gt;\n"
		"&lt;li&gt;Debugger: Improved UI docking, memory and history windows, performance analyzer.&lt;/li&gt;\n"
		"&lt;li&gt;Devices: Added The Pill, Black Box Floppy Board, Speedy XF, CSS Multiplexer, XM301; VBXE, 850, modem improvements.&lt;/li&gt;\n"
		"&lt;li&gt;Disk: Indus GT fixes; improved acceleration timing; Disk Explorer fixes.&lt;/li&gt;\n"
		"&lt;li&gt;Display: Screen masks, D3D11 custom effects, improved bloom.&lt;/li&gt;\n"
		"&lt;li&gt;Input: Paddle noise, XInput fixes, grounded internal POT lines.&lt;/li&gt;\n"
		"&lt;li&gt;Recording: Pause/resume, stereo SAP type R, VGM support.&lt;/li&gt; &lt;/ul&gt;  </description>"
		"<link>https://www.virtualdub.org/altirra.html</link> <pubDate>Thu, 01 Jan 2026 02:31:12 -0000</pubDate> "
		"<category>release</category> <guid isPermaLink=\"false\">4.40.200.17000</guid></item> </channel>\n"
		"</rss>";
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

	ATUpdateFeedInfo feedInfo;
	AT_PORTABLE_TEST_ASSERT(context, feedInfo.Parse(kSignedReleaseFeed, sizeof kSignedReleaseFeed - 1));
	AT_PORTABLE_TEST_ASSERT(context, feedInfo.mLatestVersion == (UINT64_C(4) << 48) + (UINT64_C(40) << 32) + (UINT64_C(200) << 16) + 17000);
	AT_PORTABLE_TEST_ASSERT(context, feedInfo.mLatestReleaseItem.mTitle == L"Altirra 4.40");
	AT_PORTABLE_TEST_ASSERT(context, feedInfo.mLatestReleaseItem.mLink == L"https://www.virtualdub.org/altirra.html");
	const auto listName = feedInfo.mLatestReleaseItem.mDoc.GetNameToken(ATXMLSubsetHashedStr("ul"));
	AT_PORTABLE_TEST_ASSERT(context, feedInfo.mLatestReleaseItem.mDoc.GetRoot().IsElement(listName));

	std::string tamperedFeed(kSignedReleaseFeed, sizeof kSignedReleaseFeed - 1);
	const size_t versionOffset = tamperedFeed.find("Altirra 4.40");
	AT_PORTABLE_TEST_ASSERT(context, versionOffset != std::string::npos);
	tamperedFeed[versionOffset + 11] = '1';
	ATUpdateFeedInfo tamperedFeedInfo;
	AT_PORTABLE_TEST_ASSERT(context, !tamperedFeedInfo.Parse(tamperedFeed.data(), tamperedFeed.size()));

	return true;
}
