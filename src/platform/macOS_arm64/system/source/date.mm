//	VirtualDub - Video processing and capture application
//	System library component
//	Copyright (C) 1998-2011 Avery Lee, All Rights Reserved.
//
//	Beginning with 1.6.0, the VirtualDub system library is licensed
//	differently than the remainder of VirtualDub.  This particular file is
//	thus licensed as follows (the "zlib" license):
//
//	This software is provided 'as-is', without any express or implied
//	warranty.  In no event will the authors be held liable for any
//	damages arising from the use of this software.
//
//	Permission is granted to anyone to use this software for any purpose,
//	including commercial applications, and to alter it and redistribute it
//	freely, subject to the following restrictions:
//
//	1.	The origin of this software must not be misrepresented; you must
//		not claim that you wrote the original software. If you use this
//		software in a product, an acknowledgment in the product
//		documentation would be appreciated but is not required.
//	2.	Altered source versions must be plainly marked as such, and must
//		not be misrepresented as being the original software.
//	3.	This notice may not be removed or altered from any source
//		distribution.

#import <Foundation/Foundation.h>

#include <limits>
#include <time.h>

#include <vd2/system/date.h>
#include <vd2/system/VDString.h>

namespace {
	constexpr sint64 kVDDateTicksPerSecond = 10000000;
	constexpr sint64 kVDDateTicksPerMillisecond = 10000;
	constexpr sint64 kVDDateUnixEpochTicks = 116444736000000000;

	bool VDIsValidExpandedDateFields(const VDExpandedDate& date) {
		return date.mYear >= 1601
			&& date.mYear <= (uint32)std::numeric_limits<int>::max() + 1900U
			&& date.mMonth >= 1 && date.mMonth <= 12
			&& date.mDay >= 1 && date.mDay <= 31
			&& date.mHour <= 23
			&& date.mMinute <= 59
			&& date.mSecond <= 59
			&& date.mMilliseconds <= 999;
	}

	NSDate *VDCreateNSDate(const VDExpandedDate& date) {
		if (!VDIsValidExpandedDateFields(date))
			return nil;

		NSCalendar *calendar = [[[NSCalendar alloc]
			initWithCalendarIdentifier:NSCalendarIdentifierGregorian] autorelease];
		calendar.timeZone = [NSTimeZone localTimeZone];

		NSDateComponents *components = [[[NSDateComponents alloc] init] autorelease];
		components.year = date.mYear;
		components.month = date.mMonth;
		components.day = date.mDay;
		components.hour = date.mHour;
		components.minute = date.mMinute;
		components.second = date.mSecond;
		components.nanosecond = date.mMilliseconds * 1000000;

		return [calendar dateFromComponents:components];
	}

	void VDAppendNSString(VDStringW& destination, NSString *source) {
		const NSUInteger length = source.length;

		for(NSUInteger index = 0; index < length; ++index) {
			uint32 codePoint = [source characterAtIndex:index];

			if (codePoint >= 0xD800 && codePoint <= 0xDBFF && index + 1 < length) {
				const uint32 lowSurrogate = [source characterAtIndex:index + 1];

				if (lowSurrogate >= 0xDC00 && lowSurrogate <= 0xDFFF) {
					codePoint = 0x10000
						+ ((codePoint - 0xD800) << 10)
						+ (lowSurrogate - 0xDC00);
					++index;
				}
			}

			destination.push_back((wchar_t)codePoint);
		}
	}

	void VDAppendLocalizedDate(VDStringW& destination, const VDExpandedDate& date,
		NSDateFormatterStyle dateStyle, NSDateFormatterStyle timeStyle) {
		@autoreleasepool {
			NSDate *nativeDate = VDCreateNSDate(date);
			if (!nativeDate)
				return;

			NSDateFormatter *formatter = [[[NSDateFormatter alloc] init] autorelease];
			formatter.locale = [NSLocale currentLocale];
			formatter.timeZone = [NSTimeZone localTimeZone];
			formatter.dateStyle = dateStyle;
			formatter.timeStyle = timeStyle;

			NSString *formattedDate = [formatter stringFromDate:nativeDate];
			if (formattedDate)
				VDAppendNSString(destination, formattedDate);
		}
	}
}

static_assert(+VDDateInterval{0} == VDDateInterval{0});
static_assert(+VDDateInterval{1} == VDDateInterval{1});
static_assert(-VDDateInterval{0} == VDDateInterval{0});
static_assert(-VDDateInterval{1} == VDDateInterval{-1});

static_assert(VDDateInterval{0}.Abs().mDeltaTicks == 0);
static_assert(VDDateInterval{1}.Abs().mDeltaTicks == 1);
static_assert(VDDateInterval{-1}.Abs().mDeltaTicks == 1);

static_assert(VDDateInterval{ 1} != VDDateInterval{0});
static_assert(VDDateInterval{ 0} == VDDateInterval{0});
static_assert(VDDateInterval{ 0} >= VDDateInterval{0});
static_assert(VDDateInterval{ 1} >= VDDateInterval{0});
static_assert(VDDateInterval{ 1} >  VDDateInterval{0});
static_assert(VDDateInterval{ 0} <= VDDateInterval{0});
static_assert(VDDateInterval{-1} <= VDDateInterval{0});
static_assert(VDDateInterval{-1} <  VDDateInterval{0});

static_assert(!(VDDateInterval{ 0} != VDDateInterval{0}));
static_assert(!(VDDateInterval{ 1} == VDDateInterval{0}));
static_assert(!(VDDateInterval{-1} >= VDDateInterval{0}));
static_assert(!(VDDateInterval{ 0} >  VDDateInterval{0}));
static_assert(!(VDDateInterval{ 1} <= VDDateInterval{0}));
static_assert(!(VDDateInterval{ 0} <  VDDateInterval{0}));

static_assert(VDDateInterval{0}.ToSeconds() == 0.0f);
static_assert(VDDateInterval{10000000}.ToSeconds() == 1.0f);
static_assert(VDDateInterval{-10000000}.ToSeconds() == -1.0f);
static_assert(VDDateInterval::FromSeconds(0).mDeltaTicks == 0);
static_assert(VDDateInterval::FromSeconds(1.0f).mDeltaTicks == 10000000);
static_assert(VDDateInterval::FromSeconds(-1.0f).mDeltaTicks == -10000000);

static_assert(VDDate{0} - VDDate{0} == VDDateInterval{0});
static_assert(VDDate{0} - VDDate{1} == VDDateInterval{-1});
static_assert(VDDate{1} - VDDate{0} == VDDateInterval{1});
static_assert(VDDate{1000} + VDDateInterval{1} == VDDate{1001});
static_assert(VDDateInterval{1} + VDDate{1000} == VDDate{1001});
static_assert(VDDate{1000} - VDDateInterval{1} == VDDate{999});

VDDate VDGetCurrentDate() {
	struct timespec currentTime {};
	if (clock_gettime(CLOCK_REALTIME, &currentTime))
		return {};

	const sint64 ticks = kVDDateUnixEpochTicks
		+ (sint64)currentTime.tv_sec * kVDDateTicksPerSecond
		+ currentTime.tv_nsec / 100;

	return VDDate { (uint64)ticks };
}

sint64 VDGetDateAsTimeT(const VDDate& date) {
	return ((sint64)date.mTicks - kVDDateUnixEpochTicks) / kVDDateTicksPerSecond;
}

VDExpandedDate VDGetLocalDate(const VDDate& date) {
	VDExpandedDate result {};
	sint64 unixTicks = (sint64)date.mTicks - kVDDateUnixEpochTicks;
	sint64 seconds = unixTicks / kVDDateTicksPerSecond;
	sint64 subsecondTicks = unixTicks % kVDDateTicksPerSecond;

	if (subsecondTicks < 0) {
		subsecondTicks += kVDDateTicksPerSecond;
		--seconds;
	}

	const time_t nativeTime = (time_t)seconds;
	struct tm localTime {};
	if (!localtime_r(&nativeTime, &localTime))
		return result;

	result.mYear = (uint32)(localTime.tm_year + 1900);
	result.mMonth = (uint8)(localTime.tm_mon + 1);
	result.mDayOfWeek = (uint8)localTime.tm_wday;
	result.mDay = (uint8)localTime.tm_mday;
	result.mHour = (uint8)localTime.tm_hour;
	result.mMinute = (uint8)localTime.tm_min;
	result.mSecond = (uint8)localTime.tm_sec;
	result.mMilliseconds = (uint16)(subsecondTicks / kVDDateTicksPerMillisecond);

	return result;
}

VDDate VDDateFromLocalDate(const VDExpandedDate& date) {
	if (!VDIsValidExpandedDateFields(date))
		return {};

	struct tm localTime {};
	localTime.tm_year = (int)date.mYear - 1900;
	localTime.tm_mon = date.mMonth - 1;
	localTime.tm_mday = date.mDay;
	localTime.tm_hour = date.mHour;
	localTime.tm_min = date.mMinute;
	localTime.tm_sec = date.mSecond;
	localTime.tm_isdst = -1;

	const time_t nativeTime = mktime(&localTime);
	struct tm verifiedTime {};
	if (!localtime_r(&nativeTime, &verifiedTime)
		|| verifiedTime.tm_year != (int)date.mYear - 1900
		|| verifiedTime.tm_mon != date.mMonth - 1
		|| verifiedTime.tm_mday != date.mDay
		|| verifiedTime.tm_hour != date.mHour
		|| verifiedTime.tm_min != date.mMinute
		|| verifiedTime.tm_sec != date.mSecond)
	{
		return {};
	}

	const __int128 ticks = (__int128)kVDDateUnixEpochTicks
		+ (__int128)nativeTime * kVDDateTicksPerSecond
		+ (__int128)date.mMilliseconds * kVDDateTicksPerMillisecond;

	if (ticks < 0 || ticks > std::numeric_limits<uint64>::max())
		return {};

	return VDDate { (uint64)ticks };
}

void VDAppendLocalDateString(VDStringW& destination, const VDExpandedDate& date) {
	VDAppendLocalizedDate(destination, date,
		NSDateFormatterShortStyle, NSDateFormatterNoStyle);
}

void VDAppendLocalTimeString(VDStringW& destination, const VDExpandedDate& date) {
	VDAppendLocalizedDate(destination, date,
		NSDateFormatterNoStyle, NSDateFormatterShortStyle);
}
