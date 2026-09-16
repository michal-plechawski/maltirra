// Portable tests for the compiled compatibility database reader.

#include <array>
#include <cstring>

#include <at/attest/portabletest.h>
#include <compatdb.h>

namespace {
	template<typename T>
	void SetVector(ATDBVector<T>& vector, T *data, uint32 size) {
		vector.retarget(data);
		vector.mSize = size;
	}

	struct TestDatabase {
		ATCompatDBHeader mHeader;
		ATCompatDBRuleSet mRuleSets[2];
		ATCompatDBRule mRules[3];
		ATCompatDBAlias mAliases[2];
		ATCompatDBTitle mTitles[2];
		ATCompatDBTag mTags[2];
		uint32 mTagIds[2];
		uint32 mLargeRuleData[16];
		char mChars[23];
	};

	void InitDatabase(TestDatabase& db, const std::array<uint8, 32>& blob0, const std::array<uint8, 32>& blob1) {
		memcpy(db.mHeader.mSignature, ATCompatDBHeader::kSignature, sizeof db.mHeader.mSignature);
		db.mHeader.mVersion = 0x0200;

		SetVector(db.mHeader.mRuleSetTable, db.mRuleSets, 2);
		SetVector(db.mHeader.mRuleTable, db.mRules, 3);
		SetVector(db.mHeader.mAliasTable, db.mAliases, 2);
		SetVector(db.mHeader.mTitleTable, db.mTitles, 2);
		SetVector(db.mHeader.mTagTable, db.mTags, 2);
		SetVector(db.mHeader.mTagIdTable, db.mTagIds, 2);
		SetVector(db.mHeader.mCharTable, db.mChars, sizeof db.mChars);
		SetVector(db.mHeader.mLargeRuleDataTable, db.mLargeRuleData, 16);

		db.mRuleSets[0].mRuleType = kATCompatRuleType_CartChecksum;
		SetVector(db.mRuleSets[0].mRules, db.mRules, 2);
		db.mRuleSets[1].mRuleType = kATCompatRuleType_DiskChecksum;
		SetVector(db.mRuleSets[1].mRules, db.mRules + 2, 1);

		db.mRules[0] = { 0x100, 0, 0, 2 };
		db.mRules[1] = { 0x200, 0, 1, 1 };
		db.mRules[2] = { 0x300, 0, 0, 2 };
		db.mAliases[0] = { 2, 0 };
		db.mAliases[1] = { 1, 1 };

		static constexpr char kChars[] = "Game\0Other\0basic\0ctia\0";
		static_assert(sizeof kChars == sizeof db.mChars);
		memcpy(db.mChars, kChars, sizeof kChars);
		db.mTitles[0].mName.retarget(db.mChars);
		SetVector(db.mTitles[0].mTagIds, db.mTagIds, 1);
		db.mTitles[1].mName.retarget(db.mChars + 5);
		SetVector(db.mTitles[1].mTagIds, db.mTagIds + 1, 1);
		db.mTagIds[0] = 0;
		db.mTagIds[1] = 1;
		db.mTags[0].mKey.retarget(db.mChars + 11);
		db.mTags[1].mKey.retarget(db.mChars + 17);

		memcpy(db.mLargeRuleData, blob0.data(), blob0.size());
		memcpy(db.mLargeRuleData + 8, blob1.data(), blob1.size());
	}
}

bool ATTestAltirraCompatDB(ATPortableTestContext& context) {
	std::array<uint8, 32> blob0 {};
	std::array<uint8, 32> blob1 {};
	for(size_t i = 0; i < blob0.size(); ++i) {
		blob0[i] = (uint8)(0x10 + i);
		blob1[i] = (uint8)(0x80 + i);
	}
	TestDatabase db {};
	InitDatabase(db, blob0, blob1);

	AT_PORTABLE_TEST_ASSERT(context, db.mHeader.Validate(sizeof db));
	db.mHeader.mSignature[0] ^= 1;
	AT_PORTABLE_TEST_ASSERT(context, !db.mHeader.Validate(sizeof db));
	db.mHeader.mSignature[0] ^= 1;
	db.mRules[0].mAliasId = 2;
	AT_PORTABLE_TEST_ASSERT(context, !db.mHeader.Validate(sizeof db));
	db.mRules[0].mAliasId = 0;
	db.mChars[sizeof db.mChars - 1] = 1;
	AT_PORTABLE_TEST_ASSERT(context, !db.mHeader.Validate(sizeof db));
	db.mChars[sizeof db.mChars - 1] = 0;
	AT_PORTABLE_TEST_ASSERT(context, db.mHeader.Validate(sizeof db));

	ATCompatDBView view(&db.mHeader);
	auto matching = view.FindMatchingRules(kATCompatRuleType_CartChecksum, 0x100);
	AT_PORTABLE_TEST_ASSERT(context, matching.first == db.mRules && matching.second == db.mRules + 1);
	matching = view.FindMatchingRules(kATCompatRuleType_CartChecksum, 0x999);
	AT_PORTABLE_TEST_ASSERT(context, !matching.first && !matching.second);
	AT_PORTABLE_TEST_ASSERT(context, view.HasRelatedRuleOfType(db.mRules, kATCompatRuleType_DiskChecksum));
	AT_PORTABLE_TEST_ASSERT(context, !view.HasRelatedRuleOfType(db.mRules + 1, kATCompatRuleType_DiskChecksum));

	const ATCompatDBRule *titleRules[] { db.mRules, db.mRules + 2 };
	AT_PORTABLE_TEST_ASSERT(context, view.FindMatchingTitle(titleRules, 1) == nullptr);
	AT_PORTABLE_TEST_ASSERT(context, view.FindMatchingTitle(titleRules, 2) == db.mTitles);
	vdfastvector<const ATCompatDBTitle *> titles;
	view.FindMatchingTitles(titles, titleRules, 2);
	AT_PORTABLE_TEST_ASSERT(context, titles.size() == 1 && titles[0] == db.mTitles);
	AT_PORTABLE_TEST_ASSERT(context, view.GetKnownTag(0) == kATCompatKnownTag_BASIC);

	AT_PORTABLE_TEST_ASSERT(context, view.FindLargeRuleBlob(blob0.data()) == 0);
	AT_PORTABLE_TEST_ASSERT(context, view.FindLargeRuleBlob(blob1.data()) == 32);
	blob1[0] = 0x70;
	AT_PORTABLE_TEST_ASSERT(context, view.FindLargeRuleBlob(blob1.data()) == ~(uint32)0);

	AT_PORTABLE_TEST_ASSERT(context, ATCompatIsLargeRuleType(kATCompatRuleType_CartFileSHA256));
	AT_PORTABLE_TEST_ASSERT(context, ATCompatIsLargeRuleType(kATCompatRuleType_TapeFileSHA256));
	AT_PORTABLE_TEST_ASSERT(context, !ATCompatIsLargeRuleType(kATCompatRuleType_CartChecksum));
	AT_PORTABLE_TEST_ASSERT(context,
		ATCompatGetKnownTagByKey("cart520016konechip") == kATCompatKnownTag_Cart520016KOneChip);
	AT_PORTABLE_TEST_ASSERT(context,
		!strcmp(ATCompatGetKeyForKnownTag(kATCompatKnownTag_Cart520016KTwoChip), "cart520016ktwochip"));
	AT_PORTABLE_TEST_ASSERT(context, ATCompatGetKeyForKnownTag(kATCompatKnownTag_None) == nullptr);
	AT_PORTABLE_TEST_ASSERT(context, ATCompatGetKeyForKnownTag(kATCompatKnownTagCount) == nullptr);

	return true;
}
