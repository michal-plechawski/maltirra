// Altirra portable PCLink host error translation tests

#if defined(_WIN32)
	#define WIN32_LEAN_AND_MEAN
	#include <windows.h>
#else
	#include <cerrno>
#endif

#include <at/attest/portabletest.h>
#include <at/atcore/cio.h>
#include <pclinkerror.h>

bool ATTestAltirraPCLinkError(ATPortableTestContext& context) {
#if defined(_WIN32)
	AT_PORTABLE_TEST_ASSERT(context, ATTranslateHostErrorToSIOError(ERROR_FILE_NOT_FOUND) == kATCIOStat_FileNotFound);
	AT_PORTABLE_TEST_ASSERT(context, ATTranslateHostErrorToSIOError(ERROR_PATH_NOT_FOUND) == kATCIOStat_PathNotFound);
	AT_PORTABLE_TEST_ASSERT(context, ATTranslateHostErrorToSIOError(ERROR_FILE_EXISTS) == kATCIOStat_FileExists);
	AT_PORTABLE_TEST_ASSERT(context, ATTranslateHostErrorToSIOError(ERROR_ALREADY_EXISTS) == kATCIOStat_FileExists);
	AT_PORTABLE_TEST_ASSERT(context, ATTranslateHostErrorToSIOError(ERROR_DISK_FULL) == kATCIOStat_DiskFull);
	AT_PORTABLE_TEST_ASSERT(context, ATTranslateHostErrorToSIOError(ERROR_DIR_NOT_EMPTY) == kATCIOStat_DirNotEmpty);
	AT_PORTABLE_TEST_ASSERT(context, ATTranslateHostErrorToSIOError(ERROR_ACCESS_DENIED) == kATCIOStat_AccessDenied);
	AT_PORTABLE_TEST_ASSERT(context, ATTranslateHostErrorToSIOError(ERROR_SHARING_VIOLATION) == kATCIOStat_FileLocked);
	AT_PORTABLE_TEST_ASSERT(context, ATTranslateHostErrorToSIOError(ERROR_INVALID_PARAMETER) == kATCIOStat_SystemError);
#else
	AT_PORTABLE_TEST_ASSERT(context, ATTranslateHostErrorToSIOError(ENOENT) == kATCIOStat_FileNotFound);
	AT_PORTABLE_TEST_ASSERT(context, ATTranslateHostErrorToSIOError(ENOTDIR) == kATCIOStat_PathNotFound);
	AT_PORTABLE_TEST_ASSERT(context, ATTranslateHostErrorToSIOError(EEXIST) == kATCIOStat_FileExists);
	AT_PORTABLE_TEST_ASSERT(context, ATTranslateHostErrorToSIOError(ENOSPC) == kATCIOStat_DiskFull);
	AT_PORTABLE_TEST_ASSERT(context, ATTranslateHostErrorToSIOError(EDQUOT) == kATCIOStat_DiskFull);
	AT_PORTABLE_TEST_ASSERT(context, ATTranslateHostErrorToSIOError(ENOTEMPTY) == kATCIOStat_DirNotEmpty);
	AT_PORTABLE_TEST_ASSERT(context, ATTranslateHostErrorToSIOError(EACCES) == kATCIOStat_AccessDenied);
	AT_PORTABLE_TEST_ASSERT(context, ATTranslateHostErrorToSIOError(EPERM) == kATCIOStat_AccessDenied);
	AT_PORTABLE_TEST_ASSERT(context, ATTranslateHostErrorToSIOError(EROFS) == kATCIOStat_AccessDenied);
	AT_PORTABLE_TEST_ASSERT(context, ATTranslateHostErrorToSIOError(EAGAIN) == kATCIOStat_FileLocked);
	AT_PORTABLE_TEST_ASSERT(context, ATTranslateHostErrorToSIOError(EBUSY) == kATCIOStat_FileLocked);
	AT_PORTABLE_TEST_ASSERT(context, ATTranslateHostErrorToSIOError(ETXTBSY) == kATCIOStat_FileLocked);
	AT_PORTABLE_TEST_ASSERT(context, ATTranslateHostErrorToSIOError(EINVAL) == kATCIOStat_SystemError);
#endif

	return true;
}
