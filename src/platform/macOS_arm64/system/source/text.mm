//	VirtualDub - Video processing and capture application
//	System library component
//	Copyright (C) 1998-2004 Avery Lee, All Rights Reserved.
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

#include <algorithm>
#include <cstring>

#include <vd2/system/text.h>
#include <vd2/system/VDString.h>

int VDTextWToA(char *destination, int destinationCapacity,
	const wchar_t *source, int sourceLength) {
	VDASSERTPTR(destination);
	VDASSERTPTR(source);
	VDASSERT(destinationCapacity > 0);

	if (!destination || !source || destinationCapacity <= 0)
		return 0;

	destination[0] = 0;
	const VDStringA converted = VDTextWToU8(source, sourceLength);
	const int convertedLength = (int)converted.size();

	if (convertedLength >= destinationCapacity)
		return 0;

	if (convertedLength)
		memcpy(destination, converted.data(), convertedLength);

	destination[convertedLength] = 0;
	return convertedLength;
}

int VDTextAToW(wchar_t *destination, int destinationCapacity,
	const char *source, int sourceLength) {
	VDASSERTPTR(destination);
	VDASSERTPTR(source);
	VDASSERT(destinationCapacity > 0);

	if (!destination || !source || destinationCapacity <= 0)
		return 0;

	destination[0] = 0;
	const VDStringW converted = VDTextU8ToW(source, sourceLength);
	const int convertedLength = (int)converted.size();

	if (convertedLength >= destinationCapacity)
		return 0;

	if (convertedLength)
		memcpy(destination, converted.data(), convertedLength * sizeof(wchar_t));

	destination[convertedLength] = 0;
	return convertedLength;
}

VDStringA VDTextWToA(const VDStringW& source) {
	return VDTextWToA(source.data(), source.length());
}

VDStringA VDTextWToA(const wchar_t *source, int sourceLength) {
	if (!source)
		return {};

	return VDTextWToU8(source, sourceLength);
}

VDStringW VDTextAToW(const VDStringA& source) {
	return VDTextAToW(source.data(), source.length());
}

VDStringW VDTextAToW(const char *source, int sourceLength) {
	if (!source)
		return {};

	return VDTextU8ToW(source, sourceLength);
}

int VDTextWToALength(const wchar_t *source, int sourceLength) {
	if (!source)
		return 0;

	return (int)VDTextWToU8(source, sourceLength).size();
}

int VDTextAToWLength(const char *source, int sourceLength) {
	if (!source)
		return 0;

	return (int)VDTextU8ToW(source, sourceLength).size();
}

bool VDTextContainsSubstringMatchByLocale(VDStringSpanW sourceString,
	VDStringSpanW searchString) {
	@autoreleasepool {
		const VDStringA sourceUTF8 = VDTextWToU8(sourceString);
		const VDStringA searchUTF8 = VDTextWToU8(searchString);

		NSString *source = [[[NSString alloc]
			initWithBytes:sourceUTF8.data()
			length:sourceUTF8.size()
			encoding:NSUTF8StringEncoding] autorelease];
		NSString *search = [[[NSString alloc]
			initWithBytes:searchUTF8.data()
			length:searchUTF8.size()
			encoding:NSUTF8StringEncoding] autorelease];

		if (!source || !search)
			return false;

		const NSStringCompareOptions options = NSCaseInsensitiveSearch
			| NSDiacriticInsensitiveSearch
			| NSWidthInsensitiveSearch;
		const NSRange match = [source
			rangeOfString:search
			options:options
			range:NSMakeRange(0, source.length)
			locale:[NSLocale currentLocale]];

		return match.location != NSNotFound;
	}
}

#include "../../../../system/source/textimpl.inl"
