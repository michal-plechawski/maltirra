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

#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <limits>
#include <mutex>
#include <utility>
#include <vector>

#include <fcntl.h>
#include <sys/attr.h>
#include <sys/stat.h>
#include <unistd.h>

#include <vd2/system/Error.h>
#include <vd2/system/date.h>
#include <vd2/system/file.h>
#include <vd2/system/filesys.h>
#include <vd2/system/text.h>
#include <vd2/system/VDString.h>

namespace {
	constexpr sint64 kVDDateTicksPerSecond = 10000000;
	constexpr sint64 kVDDateUnixEpochTicks = 116444736000000000;

	struct VDOpenFileRecord {
		int mFileDescriptor;
		dev_t mDevice;
		ino_t mNode;
		bool mbRead;
		bool mbWrite;
		bool mbDenyRead;
		bool mbDenyWrite;
	};

	std::mutex gVDOpenFileMutex;
	std::vector<VDOpenFileRecord> gVDOpenFiles;

	int VDFileHandleToDescriptor(VDFileHandle handle) {
		return handle
			? static_cast<int>(reinterpret_cast<sintptr>(handle) - 1)
			: -1;
	}

	VDFileHandle VDFileDescriptorToHandle(int fileDescriptor) {
		return reinterpret_cast<VDFileHandle>(static_cast<sintptr>(fileDescriptor) + 1);
	}

	const wchar_t *VDGetErrorFilename(const vdautoptr2<wchar_t>& filename) {
		return filename.get() ? filename.get() : L"";
	}

	[[noreturn]] void VDThrowFileError(
		const char *operation,
		const wchar_t *filename,
		int errorCode) {
		const VDStringA filenameUTF8 = VDTextWToU8(filename ? filename : L"", -1);
		throw MyError(
			"Cannot %s file \"%s\": %s",
			operation,
			filenameUTF8.c_str(),
			strerror(errorCode));
	}

	wchar_t *VDDuplicateFilename(const wchar_t *filename) {
		const size_t length = wcslen(filename);
		auto *copy = static_cast<wchar_t *>(malloc((length + 1) * sizeof(wchar_t)));
		if (copy)
			memcpy(copy, filename, (length + 1) * sizeof(wchar_t));
		return copy;
	}

	bool VDRegisterOpenFile(int fileDescriptor, const struct stat& info, uint32 flags) {
		const bool read = (flags & nsVDFile::kRead) != 0;
		const bool write = (flags & nsVDFile::kWrite) != 0;
		const bool denyRead = (flags & nsVDFile::kDenyRead) != 0;
		const bool denyWrite = (flags & nsVDFile::kDenyWrite) != 0;

		std::lock_guard lock(gVDOpenFileMutex);
		for(const VDOpenFileRecord& record : gVDOpenFiles) {
			if (record.mDevice != info.st_dev || record.mNode != info.st_ino)
				continue;

			if ((read && record.mbDenyRead)
				|| (write && record.mbDenyWrite)
				|| (record.mbRead && denyRead)
				|| (record.mbWrite && denyWrite))
				return false;
		}

		gVDOpenFiles.push_back(VDOpenFileRecord {
			fileDescriptor,
			info.st_dev,
			info.st_ino,
			read,
			write,
			denyRead,
			denyWrite
		});
		return true;
	}

	void VDUnregisterOpenFile(int fileDescriptor) {
		std::lock_guard lock(gVDOpenFileMutex);
		const auto record = std::find_if(
			gVDOpenFiles.begin(),
			gVDOpenFiles.end(),
			[fileDescriptor](const VDOpenFileRecord& candidate) {
				return candidate.mFileDescriptor == fileDescriptor;
			});
		if (record != gVDOpenFiles.end())
			gVDOpenFiles.erase(record);
	}

	VDDate VDDateFromTimespec(const struct timespec& value) {
		const __int128 ticks = static_cast<__int128>(kVDDateUnixEpochTicks)
			+ static_cast<__int128>(value.tv_sec) * kVDDateTicksPerSecond
			+ value.tv_nsec / 100;
		if (ticks <= 0)
			return VDDate { 0 };
		if (ticks >= static_cast<__int128>((std::numeric_limits<uint64>::max)()))
			return VDDate { (std::numeric_limits<uint64>::max)() };

		return VDDate { static_cast<uint64>(ticks) };
	}

	struct timespec VDTimespecFromDate(const VDDate& date) {
		const sint64 ticks = static_cast<sint64>(date.mTicks) - kVDDateUnixEpochTicks;
		struct timespec value {
			ticks / kVDDateTicksPerSecond,
			(ticks % kVDDateTicksPerSecond) * 100
		};
		if (value.tv_nsec < 0) {
			value.tv_nsec += 1000000000;
			--value.tv_sec;
		}
		return value;
	}

	uint32 VDAttributesFromFile(const wchar_t *filename, const struct stat& info) {
		uint32 attributes = 0;
		if (!(info.st_mode & S_IWUSR))
			attributes |= kVDFileAttr_ReadOnly;
		if (S_ISDIR(info.st_mode))
			attributes |= kVDFileAttr_Directory;
		if (info.st_flags & UF_HIDDEN)
			attributes |= kVDFileAttr_Hidden;

		if (filename && *filename == L'.' && filename[1]
			&& wcscmp(filename, L".") && wcscmp(filename, L".."))
			attributes |= kVDFileAttr_Hidden;

		return attributes;
	}
}

using namespace nsVDFile;

VDFile::VDFile(const char *filename, uint32 flags) {
	open(filename, flags);
}

VDFile::VDFile(const wchar_t *filename, uint32 flags) {
	open(filename, flags);
}

VDFile::VDFile(VDFileHandle handle)
	: mhFile(handle) {
	const off_t position = lseek(VDFileHandleToDescriptor(handle), 0, SEEK_CUR);
	mFilePosition = position >= 0 ? position : 0;
}

vdnothrow VDFile::VDFile(VDFile&& other) vdnoexcept
	: mhFile(other.mhFile)
	, mpFilename(std::move(other.mpFilename))
	, mFilePosition(other.mFilePosition) {
	other.mhFile = nullptr;
	other.mFilePosition = 0;
}

VDFile::~VDFile() {
	closeNT();
}

vdnothrow VDFile& VDFile::operator=(VDFile&& other) vdnoexcept {
	std::swap(mhFile, other.mhFile);
	std::swap(mpFilename, other.mpFilename);
	std::swap(mFilePosition, other.mFilePosition);
	return *this;
}

void VDFile::open(const char *filename, uint32 flags) {
	const uint32 error = open_internal(filename, nullptr, flags);
	if (error)
		VDThrowFileError("open", VDGetErrorFilename(mpFilename), error);
}

void VDFile::open(const wchar_t *filename, uint32 flags) {
	const uint32 error = open_internal(nullptr, filename, flags);
	if (error)
		VDThrowFileError("open", VDGetErrorFilename(mpFilename), error);
}

bool VDFile::openNT(const wchar_t *filename, uint32 flags) {
	return !open_internal(nullptr, filename, flags);
}

bool VDFile::tryOpen(const wchar_t *filename, uint32 flags) {
	const uint32 error = open_internal(nullptr, filename, flags);
	if (error == ENOENT || error == ENOTDIR)
		return false;
	if (error)
		VDThrowFileError("open", VDGetErrorFilename(mpFilename), error);
	return true;
}

bool VDFile::openAlways(const wchar_t *filename, uint32 flags) {
	const uint32 createFlags = (flags & ~kCreationMask) | kCreateNew;
	uint32 error = open_internal(nullptr, filename, createFlags);
	if (!error)
		return false;

	if (error != EEXIST)
		VDThrowFileError("open", VDGetErrorFilename(mpFilename), error);

	const uint32 existingFlags = (flags & ~kCreationMask) | kOpenExisting;
	error = open_internal(nullptr, filename, existingFlags);
	if (error)
		VDThrowFileError("open", VDGetErrorFilename(mpFilename), error);
	return true;
}

uint32 VDFile::open_internal(
	const char *narrowFilename,
	const wchar_t *wideFilename,
	uint32 flags) {
	close();

	VDStringW convertedFilename;
	if (narrowFilename) {
		convertedFilename = VDTextAToW(narrowFilename);
		wideFilename = convertedFilename.c_str();
	}
	if (!wideFilename)
		wideFilename = L"";

	mpFilename = VDDuplicateFilename(VDFileSplitPath(wideFilename));
	if (!mpFilename)
		return ENOMEM;

	const uint32 access = flags & kReadWrite;
	const uint32 creation = flags & kCreationMask;
	if (!access
		|| (flags & (kSequential | kRandomAccess)) == (kSequential | kRandomAccess))
		return EINVAL;

	int nativeFlags = O_CLOEXEC;
	if (access == kRead)
		nativeFlags |= O_RDONLY;
	else if (access == kWrite)
		nativeFlags |= O_WRONLY;
	else if (access == kReadWrite)
		nativeFlags |= O_RDWR;
	else
		return EINVAL;

	bool truncate = false;
	switch(creation) {
		case kOpenExisting:
			break;
		case kOpenAlways:
			nativeFlags |= O_CREAT;
			break;
		case kCreateAlways:
			nativeFlags |= O_CREAT;
			truncate = true;
			break;
		case kCreateNew:
			nativeFlags |= O_CREAT | O_EXCL;
			break;
		case kTruncateExisting:
			truncate = true;
			break;
		default:
			return EINVAL;
	}

	if (flags & kWriteThrough)
		nativeFlags |= O_SYNC;

	const VDStringA filenameUTF8 = VDTextWToU8(wideFilename, -1);
	int fileDescriptor;
	do {
		fileDescriptor = ::open(filenameUTF8.c_str(), nativeFlags, 0666);
	} while(fileDescriptor < 0 && errno == EINTR);
	if (fileDescriptor < 0)
		return errno;

	struct stat info {};
	if (fstat(fileDescriptor, &info)) {
		const int error = errno;
		::close(fileDescriptor);
		return error;
	}
	if (S_ISDIR(info.st_mode)) {
		::close(fileDescriptor);
		return EISDIR;
	}
	if (!VDRegisterOpenFile(fileDescriptor, info, flags)) {
		::close(fileDescriptor);
		return EACCES;
	}

	if (truncate && ftruncate(fileDescriptor, 0)) {
		const int error = errno;
		VDUnregisterOpenFile(fileDescriptor);
		::close(fileDescriptor);
		return error;
	}

	if (flags & kSequential)
		(void)fcntl(fileDescriptor, F_RDAHEAD, 1);
	if (flags & kRandomAccess)
		(void)fcntl(fileDescriptor, F_RDAHEAD, 0);
	if (flags & kUnbuffered)
		(void)fcntl(fileDescriptor, F_NOCACHE, 1);

	mhFile = VDFileDescriptorToHandle(fileDescriptor);
	mFilePosition = 0;
	return 0;
}

bool VDFile::closeNT() {
	if (!mhFile)
		return true;

	const int fileDescriptor = VDFileHandleToDescriptor(mhFile);
	mhFile = nullptr;
	VDUnregisterOpenFile(fileDescriptor);
	return !::close(fileDescriptor);
}

void VDFile::close() {
	if (!closeNT())
		VDThrowFileError("close", VDGetErrorFilename(mpFilename), errno);
}

bool VDFile::truncateNT() {
	if (!mhFile) {
		errno = EBADF;
		return false;
	}
	return !ftruncate(VDFileHandleToDescriptor(mhFile), mFilePosition);
}

void VDFile::truncate() {
	if (!truncateNT())
		VDThrowFileError("truncate", VDGetErrorFilename(mpFilename), errno);
}

bool VDFile::extendValidNT(sint64 position) {
	if (!mhFile) {
		errno = EBADF;
		return false;
	}

	struct stat info {};
	if (position < 0 || fstat(VDFileHandleToDescriptor(mhFile), &info)) {
		if (position < 0)
			errno = EINVAL;
		return false;
	}
	if (position > info.st_size) {
		errno = EINVAL;
		return false;
	}

	// POSIX filesystems do not expose an unsafe valid-data threshold: bytes
	// inside the logical file size are already valid (and zero-filled where
	// sparse). There is therefore nothing further to enable on macOS.
	return true;
}

void VDFile::extendValid(sint64 position) {
	if (!extendValidNT(position))
		VDThrowFileError("extend", VDGetErrorFilename(mpFilename), errno);
}

bool VDFile::enableExtendValid() {
	return true;
}

long VDFile::readData(void *buffer, long length) {
	if (!mhFile || length < 0) {
		errno = EINVAL;
		VDThrowFileError("read from", VDGetErrorFilename(mpFilename), errno);
	}

	ssize_t actual;
	do {
		actual = ::read(
			VDFileHandleToDescriptor(mhFile),
			buffer,
			static_cast<size_t>(length));
	} while(actual < 0 && errno == EINTR);
	if (actual < 0)
		VDThrowFileError("read from", VDGetErrorFilename(mpFilename), errno);

	mFilePosition += actual;
	return static_cast<long>(actual);
}

void VDFile::read(void *buffer, long length) {
	if (readData(buffer, length) != length)
		throw MyError(
			L"Cannot read from file \"%ls\": Premature end of file.",
			VDGetErrorFilename(mpFilename));
}

long VDFile::writeData(const void *buffer, long length) {
	if (!mhFile || length < 0) {
		errno = EINVAL;
		VDThrowFileError("write to", VDGetErrorFilename(mpFilename), errno);
	}

	const char *source = static_cast<const char *>(buffer);
	long total = 0;
	while(total < length) {
		ssize_t actual;
		do {
			actual = ::write(
				VDFileHandleToDescriptor(mhFile),
				source + total,
				static_cast<size_t>(length - total));
		} while(actual < 0 && errno == EINTR);
		if (actual <= 0) {
			if (!actual)
				errno = EIO;
			VDThrowFileError("write to", VDGetErrorFilename(mpFilename), errno);
		}

		total += static_cast<long>(actual);
		mFilePosition += actual;
	}
	return total;
}

void VDFile::write(const void *buffer, long length) {
	if (writeData(buffer, length) != length)
		throw MyError(
			L"Cannot write to file \"%ls\": Unable to write all data.",
			VDGetErrorFilename(mpFilename));
}

bool VDFile::seekNT(sint64 newPosition, eSeekMode mode) {
	int origin;
	switch(mode) {
		case kSeekStart:
			origin = SEEK_SET;
			break;
		case kSeekCur:
			origin = SEEK_CUR;
			break;
		case kSeekEnd:
			origin = SEEK_END;
			break;
		default:
			errno = EINVAL;
			return false;
	}

	if (!mhFile) {
		errno = EBADF;
		return false;
	}
	const off_t position = lseek(
		VDFileHandleToDescriptor(mhFile),
		static_cast<off_t>(newPosition),
		origin);
	if (position < 0)
		return false;

	mFilePosition = position;
	return true;
}

void VDFile::seek(sint64 newPosition, eSeekMode mode) {
	if (!seekNT(newPosition, mode))
		VDThrowFileError("seek within", VDGetErrorFilename(mpFilename), errno);
}

bool VDFile::skipNT(sint64 delta) {
	if (!delta)
		return true;

	char buffer[1024];
	if (delta > 0 && delta <= static_cast<sint64>(sizeof buffer))
		return readData(buffer, static_cast<long>(delta)) == delta;
	return seekNT(delta, kSeekCur);
}

void VDFile::skip(sint64 delta) {
	if (!delta)
		return;

	char buffer[1024];
	if (delta > 0 && delta <= static_cast<sint64>(sizeof buffer)) {
		if (readData(buffer, static_cast<long>(delta)) != delta)
			throw MyError(
				L"Cannot seek within file \"%ls\": Premature end of file.",
				VDGetErrorFilename(mpFilename));
	} else {
		seek(delta, kSeekCur);
	}
}

sint64 VDFile::size() const {
	struct stat info {};
	if (!mhFile) {
		errno = EBADF;
		VDThrowFileError("retrieve size of", VDGetErrorFilename(mpFilename), errno);
	}
	if (fstat(VDFileHandleToDescriptor(mhFile), &info))
		VDThrowFileError("retrieve size of", VDGetErrorFilename(mpFilename), errno);
	return info.st_size;
}

sint64 VDFile::tell() const {
	return mFilePosition;
}

bool VDFile::flushNT() {
	if (!mhFile) {
		errno = EBADF;
		return false;
	}
	return !fsync(VDFileHandleToDescriptor(mhFile));
}

void VDFile::flush() {
	if (!flushNT())
		VDThrowFileError("flush", VDGetErrorFilename(mpFilename), errno);
}

bool VDFile::isOpen() const {
	return mhFile != nullptr;
}

VDFileHandle VDFile::getRawHandle() const {
	return mhFile;
}

uint32 VDFile::getAttributes() const {
	struct stat info {};
	if (!mhFile || fstat(VDFileHandleToDescriptor(mhFile), &info))
		return 0;
	return VDAttributesFromFile(VDGetErrorFilename(mpFilename), info);
}

VDDate VDFile::getCreationTime() const {
	struct stat info {};
	if (!mhFile || fstat(VDFileHandleToDescriptor(mhFile), &info))
		return VDDate {};
	return VDDateFromTimespec(info.st_birthtimespec);
}

void VDFile::setCreationTime(const VDDate& date) {
	if (!mhFile)
		return;

	struct attrlist attributes {};
	attributes.bitmapcount = ATTR_BIT_MAP_COUNT;
	attributes.commonattr = ATTR_CMN_CRTIME;
	struct {
		struct timespec creationTime;
	} values { VDTimespecFromDate(date) };
	(void)fsetattrlist(
		VDFileHandleToDescriptor(mhFile),
		&attributes,
		&values,
		sizeof values,
		0);
}

VDDate VDFile::getLastWriteTime() const {
	struct stat info {};
	if (!mhFile || fstat(VDFileHandleToDescriptor(mhFile), &info))
		return VDDate {};
	return VDDateFromTimespec(info.st_mtimespec);
}

void VDFile::setLastWriteTime(const VDDate& date) {
	if (!mhFile)
		return;

	struct timespec times[2] {};
	times[0].tv_nsec = UTIME_OMIT;
	times[1] = VDTimespecFromDate(date);
	(void)futimens(VDFileHandleToDescriptor(mhFile), times);
}

void *VDFile::AllocUnbuffer(size_t byteCount) {
	if (!byteCount)
		return nullptr;

	const long pageSize = sysconf(_SC_PAGESIZE);
	if (pageSize <= 0)
		return nullptr;

	void *buffer = nullptr;
	if (posix_memalign(&buffer, static_cast<size_t>(pageSize), byteCount))
		return nullptr;
	return buffer;
}

void VDFile::FreeUnbuffer(void *buffer) {
	free(buffer);
}
