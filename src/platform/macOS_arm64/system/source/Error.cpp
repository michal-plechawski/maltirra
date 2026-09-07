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

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <new>
#include <utility>

#include <vd2/system/atomic.h>
#include <vd2/system/Error.h>

struct VDException::StringHeader {
	VDAtomicInt mRefCount;
	bool mbHidden = false;
};

VDException::VDException(const VDException& error) noexcept {
	operator=(error);
}

VDException::VDException(VDException&& error) noexcept {
	operator=(std::move(error));
}

VDException::VDException(const char *message) {
	assign(message);
}

VDException::VDException(const wchar_t *message) {
	assign(message);
}

VDException::~VDException() {
	clear();
}

VDException& VDException::operator=(const VDException& error) noexcept {
	if (mpBuffer != error.mpBuffer) {
		clear();
		mpBuffer = error.mpBuffer;

		if (mpBuffer)
			++mpBuffer->mRefCount;
	}

	mpMessage = error.mpMessage;
	mpMessageW = error.mpMessageW;
	return *this;
}

VDException& VDException::operator=(VDException&& error) noexcept {
	if (&error != this) {
		clear();

		mpBuffer = error.mpBuffer;
		mpMessage = error.mpMessage;
		mpMessageW = error.mpMessageW;

		error.mpBuffer = nullptr;
		error.mpMessage = nullptr;
		error.mpMessageW = nullptr;
	}

	return *this;
}

void VDException::clear() noexcept {
	if (mpBuffer) {
		if (!--mpBuffer->mRefCount)
			free(mpBuffer);

		mpBuffer = nullptr;
	}

	mpMessage = nullptr;
	mpMessageW = nullptr;
}

void VDException::assign(const char *message) {
	clear();

	if (!message || !*message)
		return;

	const size_t length = strlen(message);
	char *buffer = Alloc(length);

	if (buffer) {
		memcpy(buffer, message, length + 1);
		MakeWide();
	}
}

void VDException::assign(const wchar_t *message) {
	clear();

	if (!message || !*message)
		return;

	const size_t length = wcslen(message);
	wchar_t *buffer = AllocWide(length);

	if (buffer) {
		memcpy(buffer, message, sizeof(wchar_t) * (length + 1));
		MakeNarrow();
	}
}

void VDException::setf(const char *format, ...) {
	va_list arguments;

	va_start(arguments, format);
	vsetf(format, arguments);
	va_end(arguments);
}

void VDException::wsetf(const wchar_t *format, ...) {
	va_list arguments;

	va_start(arguments, format);
	vwsetf(format, arguments);
	va_end(arguments);
}

void VDException::vsetf(const char *format, va_list arguments) {
	size_t capacity = 256;

	while(capacity <= 1024 * 1024) {
		char *buffer = Alloc(capacity - 1);
		if (!buffer)
			return;

		va_list copy;
		va_copy(copy, arguments);
		const int length = vsnprintf(buffer, capacity, format, copy);
		va_end(copy);

		if (!length) {
			clear();
			mpMessage = "";
			mpMessageW = L"";
			return;
		}

		if (length > 0 && (size_t)length < capacity) {
			MakeWide();
			return;
		}

		capacity = length > 0 ? (size_t)length + 1 : capacity * 2;
	}

	const size_t length = strlen(format);
	char *buffer = Alloc(length + 2);
	if (buffer) {
		buffer[0] = '<';
		memcpy(buffer + 1, format, length);
		buffer[length + 1] = '>';
		buffer[length + 2] = 0;
		MakeWide();
	}
}

void VDException::vwsetf(const wchar_t *format, va_list arguments) {
	size_t capacity = 256;

	while(capacity <= 1024 * 1024) {
		wchar_t *buffer = AllocWide(capacity - 1);
		if (!buffer)
			return;

		va_list copy;
		va_copy(copy, arguments);
		const int length = vswprintf(buffer, capacity, format, copy);
		va_end(copy);

		if (!length) {
			clear();
			mpMessage = "";
			mpMessageW = L"";
			return;
		}

		if (length > 0 && (size_t)length < capacity) {
			MakeNarrow();
			return;
		}

		capacity = length > 0 ? (size_t)length + 1 : capacity * 2;
	}

	const size_t length = wcslen(format);
	wchar_t *buffer = AllocWide(length + 2);
	if (buffer) {
		buffer[0] = L'<';
		memcpy(buffer + 1, format, sizeof(wchar_t) * length);
		buffer[length + 1] = L'>';
		buffer[length + 2] = 0;
		MakeNarrow();
	}
}

void VDException::post(VDExceptionPostContext context, const char *title) const noexcept {
	if (visible())
		VDPostException(context, c_str(), title);
}

const char *VDException::c_str() const noexcept {
	return mpMessage ? mpMessage : "";
}

const wchar_t *VDException::wc_str() const noexcept {
	return mpMessageW ? mpMessageW : L"";
}

void VDException::set_hidden() {
	if (mpBuffer)
		mpBuffer->mbHidden = true;
}

bool VDException::visible() const noexcept {
	return mpBuffer && !mpBuffer->mbHidden;
}

const char *VDException::what() const noexcept {
	return c_str();
}

char *VDException::Alloc(size_t length) {
	Alloc(length, length);
	return const_cast<char *>(mpMessage);
}

wchar_t *VDException::AllocWide(size_t length) {
	Alloc(length, length);
	return const_cast<wchar_t *>(mpMessageW);
}

void VDException::Alloc(size_t narrowLength, size_t wideLength) {
	clear();

	const size_t messageBytes = sizeof(wchar_t) * (wideLength + 1) + narrowLength + 1;
	void *storage = malloc(sizeof(StringHeader) + messageBytes);
	if (!storage)
		return;

	mpBuffer = new(storage) StringHeader;
	mpBuffer->mRefCount = 1;
	mpMessageW = reinterpret_cast<const wchar_t *>(mpBuffer + 1);
	mpMessage = reinterpret_cast<const char *>(mpMessageW + wideLength + 1);
	memset(mpBuffer + 1, 0, messageBytes);
}

void VDException::MakeNarrow() {
	char *narrow = const_cast<char *>(mpMessage);
	const wchar_t *wide = mpMessageW;

	for(;;) {
		const wchar_t value = *wide++;
		*narrow++ = value < 0x80 ? (char)value : '?';

		if (!value)
			break;
	}
}

void VDException::MakeWide() {
	const unsigned char *narrow = reinterpret_cast<const unsigned char *>(mpMessage);
	wchar_t *wide = const_cast<wchar_t *>(mpMessageW);

	for(;;) {
		const unsigned char value = *narrow++;
		*wide++ = (wchar_t)value;

		if (!value)
			break;
	}
}

VDAllocationFailedException::VDAllocationFailedException() {
	assign("Out of memory");
}

VDAllocationFailedException::VDAllocationFailedException(size_t requestedSize) {
	setf("Out of memory (unable to allocate %llu bytes)", (unsigned long long)requestedSize);
}

VDUserCancelException::VDUserCancelException() {
	assign("Operation cancelled by user");
	set_hidden();
}

void VDPostCurrentException(VDExceptionPostContext context, const char *title) {
	try {
		throw;
	} catch(const VDException& error) {
		const VDException titleString(title);
		VDPostException(context, error.wc_str(), titleString.wc_str());
	} catch(const std::exception& error) {
		VDPostException(context, error.what(), title);
	} catch(...) {
		VDPostException(context, "An unknown exception occurred.", title);
	}
}

VDNOINLINE void VDRaiseInternalFailure(const char *context) {
	[[maybe_unused]] const char *volatile contextPointer = context;
	std::terminate();
}
