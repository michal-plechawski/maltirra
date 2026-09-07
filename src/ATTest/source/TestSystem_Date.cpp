// Altirra portable date tests

#include <ctime>

#include <at/attest/portabletest.h>
#include <vd2/system/date.h>
#include <vd2/system/VDString.h>

namespace {
	constexpr uint64 kUnixEpochTicks = 116444736000000000ULL;

	bool VDSameExpandedDate(const VDExpandedDate& left, const VDExpandedDate& right) {
		return left.mYear == right.mYear
			&& left.mMonth == right.mMonth
			&& left.mDay == right.mDay
			&& left.mHour == right.mHour
			&& left.mMinute == right.mMinute
			&& left.mSecond == right.mSecond
			&& left.mMilliseconds == right.mMilliseconds;
	}
}

bool ATTestSystemDate(ATPortableTestContext& context) {
	AT_PORTABLE_TEST_ASSERT(context,
		VDGetDateAsTimeT(VDDate { kUnixEpochTicks }) == 0);
	AT_PORTABLE_TEST_ASSERT(context,
		VDGetDateAsTimeT(VDDate { kUnixEpochTicks + 30000000 }) == 3);
	AT_PORTABLE_TEST_ASSERT(context,
		VDGetDateAsTimeT(VDDate { kUnixEpochTicks - 20000000 }) == -2);

	const VDDate baseDate { kUnixEpochTicks };
	const VDDateInterval interval = VDDateInterval::FromSeconds(1.25f);
	AT_PORTABLE_TEST_ASSERT(context, interval.mDeltaTicks == 12500000);
	AT_PORTABLE_TEST_ASSERT(context, interval.ToSeconds() == 1.25f);
	AT_PORTABLE_TEST_ASSERT(context,
		baseDate + interval - baseDate == interval);
	AT_PORTABLE_TEST_ASSERT(context,
		baseDate + interval - interval == baseDate);

	const std::time_t before = std::time(nullptr);
	const VDDate currentDate = VDGetCurrentDate();
	const std::time_t after = std::time(nullptr);
	const sint64 currentTime = VDGetDateAsTimeT(currentDate);
	AT_PORTABLE_TEST_ASSERT(context, currentTime >= (sint64)before - 1);
	AT_PORTABLE_TEST_ASSERT(context, currentTime <= (sint64)after + 1);

	const VDExpandedDate localDate {
		2020, 1, 0, 15, 12, 34, 56, 789
	};
	const VDDate encodedDate = VDDateFromLocalDate(localDate);
	AT_PORTABLE_TEST_ASSERT(context, encodedDate.mTicks != 0);
	const VDExpandedDate decodedDate = VDGetLocalDate(encodedDate);
	AT_PORTABLE_TEST_ASSERT(context, VDSameExpandedDate(localDate, decodedDate));
	AT_PORTABLE_TEST_ASSERT(context, decodedDate.mDayOfWeek <= 6);

	const VDExpandedDate invalidDate {
		2021, 2, 0, 30, 12, 0, 0, 0
	};
	AT_PORTABLE_TEST_ASSERT(context, VDDateFromLocalDate(invalidDate).mTicks == 0);

	VDStringW formattedDate(L"date=");
	VDAppendLocalDateString(formattedDate, localDate);
	AT_PORTABLE_TEST_ASSERT(context, formattedDate.size() > 5);
	AT_PORTABLE_TEST_ASSERT(context,
		formattedDate[0] == L'd' && formattedDate[1] == L'a'
		&& formattedDate[2] == L't' && formattedDate[3] == L'e'
		&& formattedDate[4] == L'=');

	VDStringW formattedTime(L"time=");
	VDAppendLocalTimeString(formattedTime, localDate);
	AT_PORTABLE_TEST_ASSERT(context, formattedTime.size() > 5);
	AT_PORTABLE_TEST_ASSERT(context,
		formattedTime[0] == L't' && formattedTime[1] == L'i'
		&& formattedTime[2] == L'm' && formattedTime[3] == L'e'
		&& formattedTime[4] == L'=');

	return true;
}
