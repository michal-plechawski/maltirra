// Altirra filesystem and path services for macOS ARM64

#include <algorithm>
#include <cerrno>
#include <climits>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <cwctype>
#include <limits>

#include <dirent.h>
#include <dlfcn.h>
#include <mach-o/dyld.h>
#include <sys/attr.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <unistd.h>

#include <vd2/system/Error.h>
#include <vd2/system/filesys.h>
#include <vd2/system/strutil.h>
#include <vd2/system/text.h>

namespace {
	constexpr wchar_t kVDNativePathSeparator = L'/';
	constexpr sint64 kVDDateTicksPerSecond = 10000000;
	constexpr sint64 kVDDateUnixEpochTicks = 116444736000000000;

	template<class T, class U>
	T VDSplitLeft(const T& string, const U *split) {
		const U *start = string.c_str();
		return T(start, split - start);
	}

	template<class T>
	T VDSplitRight(const T&, const typename T::value_type *split) {
		return T(split);
	}

	template<typename T>
	bool VDIsSeparator(T c) {
		return c == static_cast<T>(':')
			|| c == static_cast<T>('/')
			|| c == static_cast<T>('\\');
	}

	template<typename T>
	const T *VDFileSplitFirstDirImpl(const T *text) {
		const T *start = text;

		while(*text++) {
			if (VDIsSeparator(text[-1]))
				return text;
		}

		return start;
	}

	template<typename T>
	const T *VDFileSplitPathImpl(const T *start, const T *end) {
		const T *lastSeparator = start;

		while(start != end) {
			++start;
			if (VDIsSeparator(start[-1]))
				lastSeparator = start;
		}

		return lastSeparator;
	}

	template<typename T>
	const T *VDFileSplitPathImpl(const T *text) {
		const T *end = text;
		while(*end)
			++end;

		return VDFileSplitPathImpl(text, end);
	}

	template<typename T>
	bool VDIsDriveCharacter(T c, bool first) {
		return (c >= static_cast<T>('A') && c <= static_cast<T>('Z'))
			|| (c >= static_cast<T>('a') && c <= static_cast<T>('z'))
			|| (!first && c >= static_cast<T>('0') && c <= static_cast<T>('9'));
	}

	template<typename T>
	const T *VDFileSplitRootImpl(const T *text) {
		if (text[0] == static_cast<T>('/')) {
			const T *end = text + 1;
			while(*end == static_cast<T>('/'))
				++end;
			return end;
		}

		if (text[0] == static_cast<T>('\\') && text[1] == static_cast<T>('\\')) {
			const T *end = text + 2;
			for(int component = 0; component < 2; ++component) {
				while(*end && *end != static_cast<T>('\\'))
					++end;
				if (*end == static_cast<T>('\\'))
					++end;
			}
			return end;
		}

		const T *end = text;
		for(;;) {
			const T c = *end;
			if (c == static_cast<T>(':')) {
				if (end[1] == static_cast<T>('/') || end[1] == static_cast<T>('\\'))
					return end + 2;

				return end + 1;
			}

			if (!VDIsDriveCharacter(c, end == text))
				return text;

			++end;
		}
	}

	template<typename T>
	const T *VDFileSplitExtImpl(const T *start, const T *end) {
		const T *const originalEnd = end;

		while(end > start) {
			--end;
			if (*end == static_cast<T>('.'))
				return end;
			if (VDIsSeparator(*end))
				break;
		}

		return originalEnd;
	}

	char VDFoldCase(char c) {
		return static_cast<char>(tolower(static_cast<unsigned char>(c)));
	}

	wchar_t VDFoldCase(wchar_t c) {
		return towlower(c);
	}

	template<typename T>
	bool VDFileWildMatchImpl(const T *pattern, const T *path) {
		bool star = false;
		int offset = 0;

		for(;;) {
			T patternCharacter = VDFoldCase(pattern[offset]);
			if (patternCharacter == static_cast<T>('*')) {
				star = true;
				pattern += offset + 1;
				if (!*pattern)
					return true;

				path += offset;
				offset = 0;
				continue;
			}

			const T pathCharacter = VDFoldCase(path[offset]);
			++offset;
			if (patternCharacter == static_cast<T>('?')) {
				if (!pathCharacter)
					return false;
			} else if (patternCharacter != pathCharacter) {
				if (!star || !pathCharacter || !offset)
					return false;

				++path;
				offset = 0;
				continue;
			}

			if (!patternCharacter)
				return true;
		}
	}

	VDStringA VDPathToUTF8(const wchar_t *path) {
		return VDTextWToU8(path ? path : L"", -1);
	}

	[[noreturn]] void VDThrowFileSystemError(
		const char *operation,
		const wchar_t *path,
		int errorCode) {
		const VDStringA pathUTF8 = VDPathToUTF8(path);
		throw MyError(
			"%s \"%s\": %s",
			operation,
			pathUTF8.c_str(),
			strerror(errorCode));
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

	VDStringW VDTrimTrailingSeparators(const wchar_t *path) {
		VDStringW trimmed(path ? path : L"");
		while(trimmed.size() > 1 && VDIsSeparator(trimmed.back()))
			trimmed.pop_back();
		return trimmed;
	}

	uint32 VDAttributesFromStat(const wchar_t *path, const struct stat& info) {
		uint32 attributes = 0;
		if (!(info.st_mode & S_IWUSR))
			attributes |= kVDFileAttr_ReadOnly;
		if (S_ISDIR(info.st_mode))
			attributes |= kVDFileAttr_Directory;
		if (S_ISLNK(info.st_mode))
			attributes |= kVDFileAttr_Link;
		if (info.st_flags & UF_HIDDEN)
			attributes |= kVDFileAttr_Hidden;

		const wchar_t *name = VDFileSplitPath(path ? path : L"");
		if (*name == L'.' && name[1] && wcscmp(name, L".") && wcscmp(name, L".."))
			attributes |= kVDFileAttr_Hidden;

		return attributes;
	}

	struct VDMacDirectoryIteratorState {
		DIR *mpDirectory = nullptr;
		VDStringW mPattern;
	};
}

const char *VDFileSplitFirstDir(const char *text) {
	return VDFileSplitFirstDirImpl(text);
}

const wchar_t *VDFileSplitFirstDir(const wchar_t *text) {
	return VDFileSplitFirstDirImpl(text);
}

const char *VDFileSplitPath(const char *text) {
	return VDFileSplitPathImpl(text);
}

const wchar_t *VDFileSplitPath(const wchar_t *text) {
	return VDFileSplitPathImpl(text);
}

VDString VDFileSplitPathLeft(const VDString& text) {
	return VDSplitLeft(text, VDFileSplitPath(text.c_str()));
}

VDStringW VDFileSplitPathLeft(const VDStringW& text) {
	return VDSplitLeft(text, VDFileSplitPath(text.c_str()));
}

VDString VDFileSplitPathRight(const VDString& text) {
	return VDSplitRight(text, VDFileSplitPath(text.c_str()));
}

VDStringW VDFileSplitPathRight(const VDStringW& text) {
	return VDSplitRight(text, VDFileSplitPath(text.c_str()));
}

const char *VDFileSplitPath(const char *start, const char *end) {
	return VDFileSplitPathImpl(start, end);
}

const wchar_t *VDFileSplitPath(const wchar_t *start, const wchar_t *end) {
	return VDFileSplitPathImpl(start, end);
}

VDStringSpanA VDFileSplitPathLeftSpan(const VDStringSpanA& text) {
	return VDStringSpanA(text.begin(), VDFileSplitPath(text.begin(), text.end()));
}

VDStringSpanA VDFileSplitPathRightSpan(const VDStringSpanA& text) {
	return VDStringSpanA(VDFileSplitPath(text.begin(), text.end()), text.end());
}

VDStringSpanW VDFileSplitPathLeftSpan(const VDStringSpanW& text) {
	return VDStringSpanW(text.begin(), VDFileSplitPath(text.begin(), text.end()));
}

VDStringSpanW VDFileSplitPathRightSpan(const VDStringSpanW& text) {
	return VDStringSpanW(VDFileSplitPath(text.begin(), text.end()), text.end());
}

const char *VDFileSplitRoot(const char *text) {
	return VDFileSplitRootImpl(text);
}

const wchar_t *VDFileSplitRoot(const wchar_t *text) {
	return VDFileSplitRootImpl(text);
}

VDString VDFileSplitRoot(const VDString& text) {
	return VDSplitLeft(text, VDFileSplitRoot(text.c_str()));
}

VDStringW VDFileSplitRoot(const VDStringW& text) {
	return VDSplitLeft(text, VDFileSplitRoot(text.c_str()));
}

const char *VDFileSplitExt(const char *start, const char *end) {
	return VDFileSplitExtImpl(start, end);
}

const wchar_t *VDFileSplitExt(const wchar_t *start, const wchar_t *end) {
	return VDFileSplitExtImpl(start, end);
}

const char *VDFileSplitExt(const char *text) {
	return VDFileSplitExtImpl(text, text + strlen(text));
}

const wchar_t *VDFileSplitExt(const wchar_t *text) {
	return VDFileSplitExtImpl(text, text + wcslen(text));
}

VDString VDFileSplitExtLeft(const VDString& text) {
	return VDSplitLeft(text, VDFileSplitExt(text.c_str()));
}

VDStringW VDFileSplitExtLeft(const VDStringW& text) {
	return VDSplitLeft(text, VDFileSplitExt(text.c_str()));
}

VDString VDFileSplitExtRight(const VDString& text) {
	return VDSplitRight(text, VDFileSplitExt(text.c_str()));
}

VDStringW VDFileSplitExtRight(const VDStringW& text) {
	return VDSplitRight(text, VDFileSplitExt(text.c_str()));
}

VDStringSpanA VDFileSplitExtLeftSpan(const VDStringSpanA& text) {
	return VDStringSpanA(text.begin(), VDFileSplitExt(text.begin(), text.end()));
}

VDStringSpanW VDFileSplitExtLeftSpan(const VDStringSpanW& text) {
	return VDStringSpanW(text.begin(), VDFileSplitExt(text.begin(), text.end()));
}

VDStringSpanA VDFileSplitExtRightSpan(const VDStringSpanA& text) {
	return VDStringSpanA(VDFileSplitExt(text.begin(), text.end()), text.end());
}

VDStringSpanW VDFileSplitExtRightSpan(const VDStringSpanW& text) {
	return VDStringSpanW(VDFileSplitExt(text.begin(), text.end()), text.end());
}

bool VDFileWildMatch(const char *pattern, const char *path) {
	return VDFileWildMatchImpl(pattern, path);
}

bool VDFileWildMatch(const wchar_t *pattern, const wchar_t *path) {
	return VDFileWildMatchImpl(pattern, path);
}

VDParsedPath::VDParsedPath()
	: mbIsRelative(true) {
}

VDParsedPath::VDParsedPath(const wchar_t *path)
	: mbIsRelative(true) {
	const wchar_t *rootSplit = VDFileSplitRoot(path);
	if (rootSplit != path) {
		mRoot.assign(path, rootSplit);
		for(wchar_t& character : mRoot) {
			if (character == L'\\' || character == L'/')
				character = kVDNativePathSeparator;
		}

		mbIsRelative = mRoot.back() == L':';
		if (mRoot.size() >= 3
			&& mRoot[0] == kVDNativePathSeparator
			&& mRoot[1] == kVDNativePathSeparator
			&& mRoot.back() == kVDNativePathSeparator)
			mRoot.pop_back();

		path = rootSplit;
	}

	for(;;) {
		wchar_t character = *path++;
		while(character == L'\\' || character == L'/')
			character = *path++;
		if (!character)
			break;

		if (character == L';') {
			mStream = path;
			break;
		}

		const wchar_t *componentStart = path - 1;
		while(character && character != L'\\' && character != L'/' && character != L';')
			character = *path++;
		--path;
		const wchar_t *componentEnd = path;
		const size_t componentLength = componentEnd - componentStart;

		if (*componentStart == L'.') {
			if (componentLength == 1)
				continue;
			if (componentLength == 2 && componentStart[1] == L'.') {
				if (!mComponents.empty()
					&& (!mbIsRelative || mComponents.back() != L".."))
					mComponents.pop_back();
				else if (mbIsRelative)
					mComponents.push_back() = L"..";
				continue;
			}
		}

		mComponents.push_back().assign(componentStart, componentEnd);
	}
}

VDStringW VDParsedPath::ToString() const {
	VDStringW result(mRoot);
	if (!mbIsRelative && !result.empty()
		&& !VDIsPathSeparator(result.back())
		&& !mComponents.empty())
		result += kVDNativePathSeparator;

	bool first = true;
	for(const VDStringW& component : mComponents) {
		if (!first)
			result += kVDNativePathSeparator;
		first = false;
		result.append(component);
	}

	if (result.empty())
		result = L".";
	if (!mStream.empty()) {
		result += L';';
		result.append(mStream);
	}
	return result;
}

VDStringW VDFileGetCanonicalPath(const wchar_t *path) {
	return VDParsedPath(path).ToString();
}

VDStringW VDFileGetRelativePath(
	const wchar_t *basePath,
	const wchar_t *pathToConvert,
	bool allowAscent) {
	VDParsedPath base(basePath);
	VDParsedPath path(pathToConvert);
	if (base.IsRelative() || path.IsRelative())
		return VDStringW();
	if (wcscmp(base.GetRoot(), path.GetRoot()))
		return VDStringW();

	size_t baseCount = base.GetComponentCount();
	const size_t pathCount = path.GetComponentCount();
	size_t commonCount = 0;
	while(commonCount < baseCount && commonCount < pathCount
		&& !wcscmp(base.GetComponent(commonCount), path.GetComponent(commonCount)))
		++commonCount;

	VDParsedPath relativePath;
	if (baseCount > commonCount) {
		if (!allowAscent)
			return VDStringW();
		while(baseCount-- > commonCount)
			relativePath.AddComponent(L"..");
	}
	while(commonCount < pathCount)
		relativePath.AddComponent(path.GetComponent(commonCount++));
	relativePath.SetStream(path.GetStream());
	return relativePath.ToString();
}

bool VDFileIsRelativePath(const wchar_t *path) {
	return VDParsedPath(path).IsRelative();
}

VDStringW VDFileResolvePath(const wchar_t *basePath, const wchar_t *pathToResolve) {
	if (VDFileIsRelativePath(pathToResolve))
		return VDFileGetCanonicalPath(VDMakePath(basePath, pathToResolve).c_str());
	return VDFileGetCanonicalPath(pathToResolve);
}

sint64 VDGetDiskFreeSpace(const wchar_t *path) {
	const VDStringA pathUTF8 = VDPathToUTF8(path);
	struct statvfs info {};
	if (statvfs(pathUTF8.c_str(), &info))
		return -1;

	const unsigned __int128 freeBytes =
		static_cast<unsigned __int128>(info.f_bavail) * info.f_frsize;
	return freeBytes > static_cast<unsigned __int128>((std::numeric_limits<sint64>::max)())
		? (std::numeric_limits<sint64>::max)()
		: static_cast<sint64>(freeBytes);
}

bool VDDoesPathExist(const wchar_t *fileName) {
	const VDStringA pathUTF8 = VDPathToUTF8(fileName);
	struct stat info {};
	return !lstat(pathUTF8.c_str(), &info);
}

void VDCreateDirectory(const wchar_t *path) {
	const VDStringW trimmed = VDTrimTrailingSeparators(path);
	const VDStringA pathUTF8 = VDPathToUTF8(trimmed.c_str());
	if (mkdir(pathUTF8.c_str(), 0777))
		VDThrowFileSystemError("Cannot create directory", trimmed.c_str(), errno);
}

void VDRemoveDirectory(const wchar_t *path) {
	const VDStringW trimmed = VDTrimTrailingSeparators(path);
	const VDStringA pathUTF8 = VDPathToUTF8(trimmed.c_str());
	if (rmdir(pathUTF8.c_str()))
		VDThrowFileSystemError("Cannot remove directory", trimmed.c_str(), errno);
}

void VDSetDirectoryCreationTime(const wchar_t *path, const VDDate& date) {
	const VDStringA pathUTF8 = VDPathToUTF8(path);
	struct attrlist attributes {};
	attributes.bitmapcount = ATTR_BIT_MAP_COUNT;
	attributes.commonattr = ATTR_CMN_CRTIME;
	struct {
		struct timespec creationTime;
	} values { VDTimespecFromDate(date) };
	setattrlist(pathUTF8.c_str(), &attributes, &values, sizeof values, 0);
}

bool VDRemoveFile(const wchar_t *path) {
	const VDStringA pathUTF8 = VDPathToUTF8(path);
	return !unlink(pathUTF8.c_str());
}

void VDRemoveFileEx(const wchar_t *path) {
	if (!VDRemoveFile(path))
		VDThrowFileSystemError("Cannot delete file", path, errno);
}

void VDMoveFile(const wchar_t *sourcePath, const wchar_t *destinationPath) {
	const VDStringA sourceUTF8 = VDPathToUTF8(sourcePath);
	const VDStringA destinationUTF8 = VDPathToUTF8(destinationPath);
	if (renamex_np(sourceUTF8.c_str(), destinationUTF8.c_str(), RENAME_EXCL))
		VDThrowFileSystemError("Cannot rename file", sourcePath, errno);
}

uint64 VDFileGetLastWriteTime(const wchar_t *path) {
	const VDStringA pathUTF8 = VDPathToUTF8(path);
	struct stat info {};
	if (stat(pathUTF8.c_str(), &info))
		return 0;
	return VDDateFromTimespec(info.st_mtimespec).mTicks;
}

VDStringW VDFileGetRootPath(const wchar_t *path) {
	VDStringW candidate = VDGetFullPath(path);
	for(;;) {
		const VDStringA candidateUTF8 = VDPathToUTF8(candidate.c_str());
		struct statfs info {};
		if (!statfs(candidateUTF8.c_str(), &info)) {
			VDStringW root = VDTextU8ToW(info.f_mntonname, -1);
			VDFileFixDirPath(root);
			return root;
		}

		const wchar_t *split = VDFileSplitPath(candidate.c_str());
		if (split == candidate.c_str() || split <= candidate.c_str() + 1)
			return VDStringW(L"/");
		candidate.assign(candidate.c_str(), split - candidate.c_str() - 1);
	}
}

VDStringW VDGetFullPath(const wchar_t *partialPath) {
	if (!partialPath || !*partialPath)
		partialPath = L".";

	if (!VDFileIsRelativePath(partialPath))
		return VDFileGetCanonicalPath(partialPath);

	size_t capacity = PATH_MAX;
	for(;;) {
		VDStringA currentDirectory(capacity);
		if (getcwd(currentDirectory.begin(), capacity)) {
			currentDirectory.resize(strlen(currentDirectory.c_str()));
			const VDStringW currentDirectoryW = VDTextU8ToW(currentDirectory.c_str(), -1);
			return VDFileGetCanonicalPath(
				VDMakePath(currentDirectoryW.c_str(), partialPath).c_str());
		}
		if (errno != ERANGE)
			VDThrowFileSystemError("Cannot resolve current directory", partialPath, errno);
		capacity *= 2;
	}
}

VDStringW VDGetLongPath(const wchar_t *path) {
	return VDStringW(path ? path : L"");
}

VDStringW VDMakePath(const wchar_t *base, const wchar_t *file) {
	return VDMakePath(VDStringSpanW(base), VDStringSpanW(file));
}

VDStringW VDMakePath(const VDStringSpanW& base, const VDStringSpanW& file) {
	if (base.empty())
		return VDStringW(file);

	VDStringW result(base);
	const wchar_t lastCharacter = result.back();
	if (lastCharacter != L'/' && lastCharacter != L'\\' && lastCharacter != L':')
		result += kVDNativePathSeparator;
	result.append(file);
	return result;
}

bool VDFileIsPathEqual(const wchar_t *firstPath, const wchar_t *secondPath) {
	for(;;) {
		wchar_t first = *firstPath++;
		wchar_t second = *secondPath++;
		if (first == L'/' || first == L'\\') {
			first = kVDNativePathSeparator;
			while(*firstPath == L'/' || *firstPath == L'\\')
				++firstPath;
			if (!*firstPath)
				first = 0;
		}
		if (second == L'/' || second == L'\\') {
			second = kVDNativePathSeparator;
			while(*secondPath == L'/' || *secondPath == L'\\')
				++secondPath;
			if (!*secondPath)
				second = 0;
		}
		if (first != second)
			return false;
		if (!first)
			return true;
	}
}

void VDFileFixDirPath(VDStringW& path) {
	if (!path.empty() && !VDIsPathSeparator(path.back()))
		path += kVDNativePathSeparator;
}

VDStringW VDGetLocalModulePath() {
	Dl_info info {};
	if (!dladdr(reinterpret_cast<const void *>(&VDGetLocalModulePath), &info) || !info.dli_fname)
		return VDGetProgramPath();
	return VDFileSplitPathLeft(VDGetFullPath(VDTextU8ToW(info.dli_fname, -1).c_str()));
}

VDStringW VDGetProgramPath() {
	return VDFileSplitPathLeft(VDGetProgramFilePath());
}

VDStringW VDGetProgramFilePath() {
	uint32 size = 0;
	_NSGetExecutablePath(nullptr, &size);
	if (!size)
		throw MyError("Unable to determine executable path.");

	VDStringA path(size);
	if (_NSGetExecutablePath(path.begin(), &size))
		throw MyError("Unable to determine executable path.");
	path.resize(strlen(path.c_str()));
	return VDGetFullPath(VDTextU8ToW(path.c_str(), -1).c_str());
}

VDStringW VDGetSystemPath() {
	return VDStringW(L"/System/Library/");
}

void VDGetRootPaths(vdvector<VDStringW>& paths) {
	struct statfs *mounts = nullptr;
	const int mountCount = getmntinfo(&mounts, MNT_NOWAIT);
	for(int index = 0; index < mountCount; ++index) {
		if (!(mounts[index].f_flags & MNT_LOCAL))
			continue;
		const VDStringW path = VDTextU8ToW(mounts[index].f_mntonname, -1);
		bool duplicate = false;
		for(const VDStringW& existing : paths) {
			if (existing == path) {
				duplicate = true;
				break;
			}
		}
		if (!duplicate)
			paths.push_back() = path;
	}
	if (paths.empty())
		paths.push_back() = L"/";
}

VDStringW VDGetRootVolumeLabel(const wchar_t *rootPath) {
	VDStringW trimmed = VDTrimTrailingSeparators(rootPath);
	const wchar_t *name = VDFileSplitPath(trimmed.c_str());
	if (*name)
		return VDStringW(name);
	return trimmed.empty() ? VDStringW(L"/") : trimmed;
}

uint32 VDFileGetAttributes(const wchar_t *path) {
	const VDStringA pathUTF8 = VDPathToUTF8(path);
	struct stat info {};
	if (lstat(pathUTF8.c_str(), &info))
		return kVDFileAttr_Invalid;

	uint32 attributes = VDAttributesFromStat(path, info);
	if (S_ISLNK(info.st_mode)) {
		struct stat targetInfo {};
		if (!stat(pathUTF8.c_str(), &targetInfo) && S_ISDIR(targetInfo.st_mode))
			attributes |= kVDFileAttr_Directory;
	}
	return attributes;
}

void VDFileSetAttributes(const wchar_t *path, uint32 attributesToChange, uint32 newAttributes) {
	const VDStringA pathUTF8 = VDPathToUTF8(path);
	struct stat info {};
	if (lstat(pathUTF8.c_str(), &info))
		VDThrowFileSystemError("Cannot read file attributes", path, errno);

	if (attributesToChange & kVDFileAttr_ReadOnly) {
		mode_t mode = info.st_mode;
		if (newAttributes & kVDFileAttr_ReadOnly)
			mode &= ~(S_IWUSR | S_IWGRP | S_IWOTH);
		else
			mode |= S_IWUSR;
		if (chmod(pathUTF8.c_str(), mode))
			VDThrowFileSystemError("Cannot change file permissions", path, errno);
	}

	if (attributesToChange & kVDFileAttr_Hidden) {
		const u_int flags = (newAttributes & kVDFileAttr_Hidden)
			? info.st_flags | UF_HIDDEN
			: info.st_flags & ~UF_HIDDEN;
		if (chflags(pathUTF8.c_str(), flags))
			VDThrowFileSystemError("Cannot change hidden file attribute", path, errno);
	}
}

VDDirectoryIterator::VDDirectoryIterator(const wchar_t *path)
	: mpHandle(nullptr)
	, mbSearchComplete(false)
	, mSearchPath(path ? path : L"")
	, mFileSize(0)
	, mbDirectory(false)
	, mAttributes(kVDFileAttr_Invalid)
	, mCreationDate { 0 }
	, mLastWriteDate { 0 } {
	mBasePath = VDFileSplitPathLeft(mSearchPath);
	VDFileFixDirPath(mBasePath);
}

VDDirectoryIterator::~VDDirectoryIterator() {
	if (mpHandle) {
		auto *state = static_cast<VDMacDirectoryIteratorState *>(mpHandle);
		if (state->mpDirectory)
			closedir(state->mpDirectory);
		delete state;
	}
}

bool VDDirectoryIterator::Next() {
	if (mbSearchComplete)
		return false;

	auto *state = static_cast<VDMacDirectoryIteratorState *>(mpHandle);
	if (!state) {
		state = new VDMacDirectoryIteratorState;
		state->mPattern = VDFileSplitPathRight(mSearchPath);
		const VDStringW directoryPath = mBasePath.empty()
			? VDStringW(L".")
			: VDTrimTrailingSeparators(mBasePath.c_str());
		const VDStringA directoryUTF8 = VDPathToUTF8(directoryPath.c_str());
		state->mpDirectory = opendir(directoryUTF8.c_str());
		mpHandle = state;
		if (!state->mpDirectory) {
			mbSearchComplete = true;
			return false;
		}
	}

	for(;;) {
		errno = 0;
		const struct dirent *entry = readdir(state->mpDirectory);
		if (!entry) {
			mbSearchComplete = true;
			return false;
		}
		if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, ".."))
			continue;

		const VDStringW name = VDTextU8ToW(entry->d_name, -1);
		if (!VDFileWildMatch(state->mPattern.c_str(), name.c_str()))
			continue;

		mFilename = name;
		const VDStringW fullPath = GetFullPath();
		const VDStringA fullPathUTF8 = VDPathToUTF8(fullPath.c_str());
		struct stat info {};
		if (lstat(fullPathUTF8.c_str(), &info))
			continue;

		mAttributes = VDAttributesFromStat(fullPath.c_str(), info);
		mbDirectory = S_ISDIR(info.st_mode);
		if (S_ISLNK(info.st_mode)) {
			struct stat targetInfo {};
			if (!stat(fullPathUTF8.c_str(), &targetInfo) && S_ISDIR(targetInfo.st_mode))
				mbDirectory = true;
		}
		if (mbDirectory)
			mAttributes |= kVDFileAttr_Directory;
		mFileSize = info.st_size;
		mCreationDate = VDDateFromTimespec(info.st_birthtimespec);
		mLastWriteDate = VDDateFromTimespec(info.st_mtimespec);
		return true;
	}
}

bool VDDirectoryIterator::ResolveLinkSize() {
	if (IsDirectory() || !IsLink())
		return true;

	const VDStringA pathUTF8 = VDPathToUTF8(GetFullPath().c_str());
	struct stat info {};
	if (stat(pathUTF8.c_str(), &info))
		return false;
	mFileSize = info.st_size;
	return true;
}
