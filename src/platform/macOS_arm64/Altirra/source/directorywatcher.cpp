// Altirra virtual disk directory monitoring for macOS ARM64

#include "directorywatcher.h"

void ATDirectoryWatcher::SetShouldUsePolling(bool) {
	// This option selects the Windows polling fallback. macOS uses FSEvents.
}

ATDirectoryWatcher::ATDirectoryWatcher() = default;

ATDirectoryWatcher::~ATDirectoryWatcher() {
	Shutdown();
}

void ATDirectoryWatcher::Init(const wchar_t *basePath, bool recursive) {
	mWatcher.InitDir(basePath, recursive, nullptr);
}

void ATDirectoryWatcher::Shutdown() {
	mWatcher.Shutdown();
}

bool ATDirectoryWatcher::CheckForChanges() {
	return mWatcher.Wait(0);
}

bool ATDirectoryWatcher::CheckForChanges(vdfastvector<wchar_t>& strheap) {
	strheap.clear();
	// FSEvents does not promise per-directory changes. Request a full rescan.
	return mWatcher.Wait(0);
}
