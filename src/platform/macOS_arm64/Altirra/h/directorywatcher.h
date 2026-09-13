#ifndef f_AT_DIRECTORYWATCHER_H
#define f_AT_DIRECTORYWATCHER_H

#include <vd2/system/filewatcher.h>
#include <vd2/system/vdstl.h>

class ATDirectoryWatcher {
	ATDirectoryWatcher(const ATDirectoryWatcher&) = delete;
	ATDirectoryWatcher& operator=(const ATDirectoryWatcher&) = delete;

public:
	static void SetShouldUsePolling(bool enabled);

	ATDirectoryWatcher();
	~ATDirectoryWatcher();

	void Init(const wchar_t *basePath, bool recursive = true);
	void Shutdown();

	bool CheckForChanges();
	bool CheckForChanges(vdfastvector<wchar_t>& strheap);

private:
	VDFileWatcher mWatcher;
};

#endif
