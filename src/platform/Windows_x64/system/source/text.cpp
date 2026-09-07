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

#include <stdafx.h>
#include <vector>
#include <algorithm>

#include <stdarg.h>
#include <stdio.h>

#include <windows.h>
#include <winnls.h>

#include <vd2/system/vdtypes.h>
#include <vd2/system/vdstdc.h>
#include <vd2/system/vdstl.h>
#include <vd2/system/text.h>
#include <vd2/system/tls.h>
#include <vd2/system/VDString.h>

int VDTextWToA(char *dst, int max_dst, const wchar_t *src, int max_src) {
	VDASSERTPTR(dst);
	VDASSERTPTR(src);
	VDASSERT(max_dst>0);

	*dst = 0;

	int len = WideCharToMultiByte(CP_ACP, 0, src, max_src, dst, max_dst, NULL, NULL);
	if (!len)
		*dst = 0;

	// remove null terminator if source was null-terminated (source
	// length was provided)
	return max_src<0 && len>0 ? len-1 : len;
}

int VDTextAToW(wchar_t *dst, int max_dst, const char *src, int max_src) {
	VDASSERTPTR(dst);
	VDASSERTPTR(src);
	VDASSERT(max_dst>0);

	*dst = 0;

	int len = MultiByteToWideChar(CP_ACP, 0, src, max_src, dst, max_dst);
	if (!len)
		*dst = 0;

	// remove null terminator if source was null-terminated (source
	// length was provided)
	return max_src<0 && len>0 ? len-1 : len;
}

VDStringA VDTextWToA(const VDStringW& sw) {
	return VDTextWToA(sw.data(), sw.length());
}

VDStringA VDTextWToA(const wchar_t *src, int srclen) {
	VDStringA s;

	if (src) {
		int l = VDTextWToALength(src, srclen);

		if (l) {
			s.resize(l);
			VDTextWToA((char *)s.data(), l+1, src, srclen);
		}
	}

	return s;
}

VDStringW VDTextAToW(const VDStringA& s) {
	return VDTextAToW(s.data(), s.length());
}

VDStringW VDTextAToW(const char *src, int srclen) {
	VDStringW sw;

	if (src) {
		int l = VDTextAToWLength(src, srclen);

		if (l) {
			sw.resize(l);
			VDTextAToW(&sw[0], sw.length()+1, src, srclen);
		}
	}

	return sw;
}

int VDTextWToALength(const wchar_t *s, int length) {
	SetLastError(0);
	int rv = WideCharToMultiByte(CP_ACP, 0, s, length, NULL, 0, NULL, 0);

	if (length < 0 && rv>0)
		--rv;

	return rv;
}

int VDTextAToWLength(const char *s, int length) {
	SetLastError(0);
	int rv = MultiByteToWideChar(CP_ACP, 0, s, length, NULL, 0);

	if (length < 0 && rv > 0)
		--rv;

	return rv;
}

///////////////////////////////////////////////////////////////////////////

bool VDTextContainsSubstringMatchByLocale(VDStringSpanW sourceString, VDStringSpanW searchString) {
	const int pos = FindNLSStringEx(
		LOCALE_NAME_USER_DEFAULT,
		FIND_FROMSTART
			| NORM_IGNORECASE
			| NORM_IGNOREKANATYPE
			| NORM_IGNOREWIDTH
			| NORM_LINGUISTIC_CASING,
		sourceString.data(),
		(int)sourceString.size(),
		searchString.data(),
		(int)searchString.size(),
		nullptr,
		nullptr,
		nullptr,
		0);

	return pos >= 0;
}

#include "../../../../system/source/textimpl.inl"
