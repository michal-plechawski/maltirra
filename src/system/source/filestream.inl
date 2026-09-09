//	VirtualDub - Video processing and capture application
//	System library component
//	Copyright (C) 1998-2012 Avery Lee, All Rights Reserved.
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
//
// This file is included by the platform stream implementation translation
// units. It intentionally has no include guard.

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstring>

#include <vd2/system/Error.h>
#include <vd2/system/file.h>

namespace {
	class VDIOReadPastEOFException final : public VDException {
	public:
		VDIOReadPastEOFException()
			: VDException("Attempt to read beyond end of stream.") {
		}
	};
}

VDFileStream::~VDFileStream() {
}

const wchar_t *VDFileStream::GetNameForError() {
	return getFilenameForError();
}

sint64 VDFileStream::Pos() {
	return tell();
}

void VDFileStream::Read(void *buffer, sint32 bytes) {
	read(buffer, bytes);
}

sint32 VDFileStream::ReadData(void *buffer, sint32 bytes) {
	return readData(buffer, bytes);
}

void VDFileStream::Write(const void *buffer, sint32 bytes) {
	write(buffer, bytes);
}

sint64 VDFileStream::Length() {
	return size();
}

void VDFileStream::Seek(sint64 offset) {
	seek(offset);
}

VDMemoryStream::VDMemoryStream(const void *source, uint32 length)
	: mpSrc(static_cast<const char *>(source))
	, mLength(length)
	, mPos(0) {
}

const wchar_t *VDMemoryStream::GetNameForError() {
	return L"memory stream";
}

sint64 VDMemoryStream::Pos() {
	return mPos;
}

void VDMemoryStream::Read(void *buffer, sint32 bytes) {
	if (bytes != ReadData(buffer, bytes))
		throw VDIOReadPastEOFException();
}

sint32 VDMemoryStream::ReadData(void *buffer, sint32 bytes) {
	if (bytes <= 0)
		return 0;

	const uint32 available = mLength - mPos;
	const uint32 actual = std::min<uint32>(static_cast<uint32>(bytes), available);
	if (actual) {
		memcpy(buffer, mpSrc + mPos, actual);
		mPos += actual;
	}
	return static_cast<sint32>(actual);
}

void VDMemoryStream::Write(const void *, sint32) {
	throw MyError("Memory streams are read-only.");
}

sint64 VDMemoryStream::Length() {
	return mLength;
}

void VDMemoryStream::Seek(sint64 offset) {
	if (offset < 0 || static_cast<uint64>(offset) > mLength)
		throw MyError("Invalid seek position");
	mPos = static_cast<uint32>(offset);
}

VDMemoryBufferStream::VDMemoryBufferStream() {
}

VDMemoryBufferStream::~VDMemoryBufferStream() {
}

void VDMemoryBufferStream::Clear() {
	mPos = 0;
	mBuffer.clear();
}

const wchar_t *VDMemoryBufferStream::GetNameForError() {
	return L"memory buffer stream";
}

sint64 VDMemoryBufferStream::Pos() {
	return mPos;
}

void VDMemoryBufferStream::Read(void *buffer, sint32 bytes) {
	if (bytes > 0 && bytes != ReadData(buffer, bytes))
		throw VDIOReadPastEOFException();
}

sint32 VDMemoryBufferStream::ReadData(void *buffer, sint32 bytes) {
	if (bytes <= 0)
		return 0;

	const size_t available = mBuffer.size() - mPos;
	const size_t actual = std::min<size_t>(static_cast<size_t>(bytes), available);
	if (actual) {
		memcpy(buffer, &mBuffer[mPos], actual);
		mPos += actual;
	}
	return static_cast<sint32>(actual);
}

void VDMemoryBufferStream::Write(const void *buffer, sint32 bytes) {
	if (bytes <= 0)
		return;

	const size_t length = static_cast<size_t>(bytes);
	if (static_cast<size_t>(PTRDIFF_MAX) - mPos < length)
		throw VDException("Memory buffer full");

	const size_t newLength = mPos + length;
	if (mBuffer.size() < newLength)
		mBuffer.resize(newLength);
	memcpy(&mBuffer[mPos], buffer, length);
	mPos += length;
}

sint64 VDMemoryBufferStream::Length() {
	return static_cast<sint64>(mBuffer.size());
}

void VDMemoryBufferStream::Seek(sint64 offset) {
	if (offset < 0 || static_cast<uint64>(offset) > mBuffer.size())
		throw MyError("Invalid seek position");
	mPos = static_cast<size_t>(offset);
}

VDBufferedStream::VDBufferedStream(IVDStream *source, uint32 bufferSize)
	: mpSrc(source)
	, mBuffer(std::max<uint32>(bufferSize, 1))
	, mBasePosition(0)
	, mBufferOffset(0)
	, mBufferValidSize(0) {
}

VDBufferedStream::~VDBufferedStream() {
}

const wchar_t *VDBufferedStream::GetNameForError() {
	return mpSrc->GetNameForError();
}

sint64 VDBufferedStream::Pos() {
	return mBasePosition + mBufferOffset;
}

void VDBufferedStream::Read(void *buffer, sint32 bytes) {
	if (bytes != ReadData(buffer, bytes))
		throw VDException(
			L"Cannot read %d bytes at location %08llx from %ls",
			bytes,
			mBasePosition + mBufferOffset,
			mpSrc->GetNameForError());
}

sint32 VDBufferedStream::ReadData(void *buffer, sint32 bytes) {
	if (bytes <= 0)
		return 0;

	uint32 actual = 0;
	for(;;) {
		uint32 count = mBufferValidSize - mBufferOffset;
		if (count > static_cast<uint32>(bytes))
			count = static_cast<uint32>(bytes);

		if (count) {
			if (buffer) {
				memcpy(buffer, mBuffer.data() + mBufferOffset, count);
				buffer = static_cast<char *>(buffer) + count;
			}
			mBufferOffset += count;
			bytes -= count;
			actual += count;
			if (!bytes)
				break;
		}

		if (mBufferValidSize) {
			VDASSERT(mBufferOffset >= mBufferValidSize);
			mBasePosition += mBufferValidSize;
			mBufferOffset = 0;
			mBufferValidSize = 0;
		}

		if (buffer && static_cast<uint32>(bytes) >= mBuffer.size() * 2) {
			const sint32 localActual = mpSrc->ReadData(buffer, bytes);
			if (localActual > 0) {
				mBasePosition += localActual;
				actual += static_cast<uint32>(localActual);
			}
			break;
		}

		const sint32 refill = mpSrc->ReadData(mBuffer.data(), static_cast<sint32>(mBuffer.size()));
		mBufferValidSize = refill > 0 ? static_cast<uint32>(refill) : 0;
		mBufferOffset = 0;
		if (!mBufferValidSize)
			break;
	}
	return static_cast<sint32>(actual);
}

void VDBufferedStream::Write(const void *, sint32) {
	throw MyError("Buffered streams are read-only.");
}

void VDBufferedStream::Skip(sint64 size) {
	while(size > 0x7FFFFFFF) {
		size -= 0x7FFFFFFF;
		Read(nullptr, 0x7FFFFFFF);
	}
	if (size)
		Read(nullptr, static_cast<sint32>(size));
}

VDBufferedRandomAccessStream::VDBufferedRandomAccessStream(
	IVDRandomAccessStream *source,
	uint32 bufferSize)
	: mpSrc(source)
	, mBuffer(std::max<uint32>(bufferSize, 1))
	, mBasePosition(0)
	, mBufferOffset(0)
	, mBufferValidSize(0) {
}

VDBufferedRandomAccessStream::~VDBufferedRandomAccessStream() {
}

const wchar_t *VDBufferedRandomAccessStream::GetNameForError() {
	return mpSrc->GetNameForError();
}

sint64 VDBufferedRandomAccessStream::Pos() {
	return mBasePosition + mBufferOffset;
}

void VDBufferedRandomAccessStream::Read(void *buffer, sint32 bytes) {
	if (bytes != ReadData(buffer, bytes))
		throw VDException(
			L"Cannot read %d bytes at location %08llx from %ls",
			bytes,
			mBasePosition + mBufferOffset,
			mpSrc->GetNameForError());
}

sint32 VDBufferedRandomAccessStream::ReadData(void *buffer, sint32 bytes) {
	if (bytes <= 0)
		return 0;

	uint32 actual = 0;
	for(;;) {
		uint32 count = mBufferValidSize - mBufferOffset;
		if (count > static_cast<uint32>(bytes))
			count = static_cast<uint32>(bytes);

		if (count) {
			if (buffer) {
				memcpy(buffer, mBuffer.data() + mBufferOffset, count);
				buffer = static_cast<char *>(buffer) + count;
			}
			mBufferOffset += count;
			bytes -= count;
			actual += count;
			if (!bytes)
				break;
		}

		if (mBufferValidSize) {
			VDASSERT(mBufferOffset >= mBufferValidSize);
			mBasePosition += mBufferValidSize;
			mBufferOffset = 0;
			mBufferValidSize = 0;
		}

		if (buffer && static_cast<uint32>(bytes) >= mBuffer.size() * 2) {
			const sint32 localActual = mpSrc->ReadData(buffer, bytes);
			if (localActual > 0) {
				mBasePosition += localActual;
				actual += static_cast<uint32>(localActual);
			}
			break;
		}

		const sint32 refill = mpSrc->ReadData(mBuffer.data(), static_cast<sint32>(mBuffer.size()));
		mBufferValidSize = refill > 0 ? static_cast<uint32>(refill) : 0;
		mBufferOffset = 0;
		if (!mBufferValidSize)
			break;
	}
	return static_cast<sint32>(actual);
}

void VDBufferedRandomAccessStream::Write(const void *, sint32) {
	throw MyError("Buffered streams are read-only.");
}

sint64 VDBufferedRandomAccessStream::Length() {
	return mpSrc->Length();
}

void VDBufferedRandomAccessStream::Seek(sint64 offset) {
	const sint64 relativeOffset = offset - mBasePosition;
	if (relativeOffset >= 0 && relativeOffset <= static_cast<sint64>(mBufferValidSize)) {
		mBufferOffset = static_cast<uint32>(relativeOffset);
		return;
	}

	mBufferOffset = 0;
	mBufferValidSize = 0;
	mpSrc->Seek(offset);
	mBasePosition = offset;
}

void VDBufferedRandomAccessStream::Skip(sint64 size) {
	const sint64 targetPosition = mBasePosition + mBufferOffset + size;
	const sint64 bufferEnd = mBasePosition + mBufferValidSize;
	if (size >= 0
		&& targetPosition >= bufferEnd
		&& targetPosition < bufferEnd + static_cast<sint64>(mBuffer.size())) {
		Read(nullptr, static_cast<sint32>(size));
		return;
	}
	Seek(targetPosition);
}

VDBufferedWriteStream::VDBufferedWriteStream(
	IVDRandomAccessStream *source,
	uint32 bufferSize)
	: mpSrc(source)
	, mBuffer(bufferSize)
	, mBasePosition(source ? source->Pos() : 0)
	, mBufferOffset(0)
	, mBufferSize(bufferSize) {
}

VDBufferedWriteStream::~VDBufferedWriteStream() {
	try {
		Flush();
	} catch(const VDException&) {
	}
}

const wchar_t *VDBufferedWriteStream::GetNameForError() {
	return mpSrc->GetNameForError();
}

sint64 VDBufferedWriteStream::Pos() {
	return mBasePosition + mBufferOffset;
}

void VDBufferedWriteStream::Read(void *, sint32) {
	throw MyError("Buffered write streams are write-only.");
}

sint32 VDBufferedWriteStream::ReadData(void *, sint32) {
	return -1;
}

void VDBufferedWriteStream::Write(const void *buffer, sint32 bytes) {
	if (bytes <= 0)
		return;

	const uint32 byteCount = static_cast<uint32>(bytes);
	if (byteCount <= mBufferSize) {
		const uint32 space = mBufferSize - mBufferOffset;
		if (space < byteCount) {
			memcpy(mBuffer.data() + mBufferOffset, buffer, space);
			mBufferOffset += space;
			Flush();
			mBufferOffset = byteCount - space;
			memcpy(mBuffer.data(), static_cast<const char *>(buffer) + space, mBufferOffset);
		} else {
			memcpy(mBuffer.data() + mBufferOffset, buffer, byteCount);
			mBufferOffset += byteCount;
		}
	} else {
		Flush();
		mpSrc->Write(buffer, bytes);
		mBasePosition += bytes;
	}
}

sint64 VDBufferedWriteStream::Length() {
	return std::max(mpSrc->Length(), Pos());
}

void VDBufferedWriteStream::Seek(sint64 offset) {
	if (Pos() == offset)
		return;
	Flush();
	mpSrc->Seek(offset);
	mBasePosition = offset;
}

void VDBufferedWriteStream::Skip(sint64 size) {
	if (size)
		Seek(size + Pos());
}

void VDBufferedWriteStream::Flush() {
	if (!mBufferOffset)
		return;
	mpSrc->Write(mBuffer.data(), static_cast<sint32>(mBufferOffset));
	mBasePosition += mBufferOffset;
	mBufferOffset = 0;
}

VDTextStream::VDTextStream(IVDStream *source)
	: mpSrc(source)
	, mBufferPos(0)
	, mBufferLimit(0)
	, mState(kFetchLine)
	, mFileBuffer(kFileBufferSize) {
}

VDTextStream::~VDTextStream() {
}

const char *VDTextStream::GetNextLine() {
	if (!mpSrc)
		return nullptr;

	mLineBuffer.clear();
	for(;;) {
		if (mBufferPos >= mBufferLimit) {
			mBufferPos = 0;
			const sint32 actual = mpSrc->ReadData(mFileBuffer.data(), mFileBuffer.size());
			mBufferLimit = actual > 0 ? static_cast<uint32>(actual) : 0;
			if (!mBufferLimit) {
				mpSrc = nullptr;
				if (mLineBuffer.empty())
					return nullptr;
				mLineBuffer.push_back(0);
				return mLineBuffer.data();
			}
		}

		switch(mState) {
			case kEatNextIfCR:
				mState = kFetchLine;
				if (mFileBuffer[mBufferPos] == '\r')
					++mBufferPos;
				continue;

			case kEatNextIfLF:
				mState = kFetchLine;
				if (mFileBuffer[mBufferPos] == '\n')
					++mBufferPos;
				continue;

			case kFetchLine: {
				const uint32 base = mBufferPos;
				do {
					const char character = mFileBuffer[mBufferPos++];
					if (character == '\r' || character == '\n') {
						mState = character == '\r' ? kEatNextIfLF : kEatNextIfCR;
						mLineBuffer.insert(
							mLineBuffer.end(),
							mFileBuffer.begin() + base,
							mFileBuffer.begin() + mBufferPos - 1);
						mLineBuffer.push_back(0);
						return mLineBuffer.data();
					}
				} while(mBufferPos < mBufferLimit);
				mLineBuffer.insert(
					mLineBuffer.end(),
					mFileBuffer.begin() + base,
					mFileBuffer.begin() + mBufferLimit);
				break;
			}
		}
	}
}

VDTextInputFile::VDTextInputFile(const wchar_t *filename, uint32 flags)
	: mFileStream(filename, flags | nsVDFile::kRead)
	, mTextStream(&mFileStream) {
}

VDTextInputFile::~VDTextInputFile() {
}

VDTextOutputStream::VDTextOutputStream(IVDStream *stream)
	: mLevel(0)
	, mpDst(stream) {
}

VDTextOutputStream::~VDTextOutputStream() {
	try {
		Flush();
	} catch(const MyError&) {
	}
}

void VDTextOutputStream::Flush() {
	if (mLevel) {
		mpDst->Write(mBuf, mLevel);
		mLevel = 0;
	}
}

sint64 VDTextOutputStream::Pos() const {
	return mpDst->Pos() + mLevel;
}

void VDTextOutputStream::Write(const char *text) {
	PutData(text, static_cast<int>(strlen(text)));
}

void VDTextOutputStream::Write(const char *text, int length) {
	PutData(text, length);
}

void VDTextOutputStream::PutLine() {
	PutData("\r\n", 2);
}

void VDTextOutputStream::PutLine(const char *text) {
	PutLine(text, static_cast<int>(strlen(text)));
}

void VDTextOutputStream::PutLine(const char *text, int length) {
	PutData(text, length);
	PutData("\r\n", 2);
}

void VDTextOutputStream::Format(const char *format, ...) {
	va_list arguments;
	va_start(arguments, format);
	const int available = kBufSize - mLevel;
	int written = -1;
	if (available > 1) {
		va_list attempt;
		va_copy(attempt, arguments);
		written = vsnprintf(mBuf + mLevel, available, format, attempt);
		va_end(attempt);
	}
	if (written >= 0 && written < available)
		mLevel += written;
	else
		Format2(format, arguments);
	va_end(arguments);
}

void VDTextOutputStream::FormatLine(const char *format, ...) {
	va_list arguments;
	va_start(arguments, format);
	const int available = kBufSize - mLevel;
	int written = -1;
	if (available > 1) {
		va_list attempt;
		va_copy(attempt, arguments);
		written = vsnprintf(mBuf + mLevel, available, format, attempt);
		va_end(attempt);
	}
	if (written >= 0 && written < available)
		mLevel += written;
	else
		Format2(format, arguments);
	PutData("\r\n", 2);
	va_end(arguments);
}

void VDTextOutputStream::Format2(const char *format, va_list arguments) {
	va_list sizingArguments;
	va_copy(sizingArguments, arguments);
	const int required = vsnprintf(nullptr, 0, format, sizingArguments);
	va_end(sizingArguments);
	if (required <= 0)
		return;

	vdfastvector<char> buffer;
	buffer.resize(static_cast<size_t>(required) + 1);
	va_list renderingArguments;
	va_copy(renderingArguments, arguments);
	const int written = vsnprintf(buffer.data(), buffer.size(), format, renderingArguments);
	va_end(renderingArguments);
	if (written == required)
		PutData(buffer.data(), written);
}

void VDTextOutputStream::PutData(const char *text, int length) {
	while(length > 0) {
		int available = kBufSize - mLevel;
		if (!available) {
			mpDst->Write(mBuf, kBufSize);
			mLevel = 0;
			available = kBufSize;
		}
		const int count = std::min(length, available);
		memcpy(mBuf + mLevel, text, count);
		text += count;
		length -= count;
		mLevel += count;
	}
}
