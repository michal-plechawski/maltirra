// Altirra portable PCLink host error translation tests

#if !defined(_WIN32)
	#include <cerrno>
#endif

#include <at/attest/portabletest.h>
#include <at/atcore/cio.h>
#include <pclinkerror.h>

bool ATTestAltirraPCLinkError(ATPortableTestContext& context) {
#if defined(_WIN32)
	AT_PORTABLE_TEST_ASSERT(context, ATTranslateHostErrorToSIOError(2) == kATCIOStat_FileNotFound);		// ERROR_FILE_NOT_FOUND
	AT_PORTABLE_TEST_ASSERT(context, ATTranslateHostErrorToSIOError(3) == kATCIOStat_PathNotFound);		// ERROR_PATH_NOT_FOUND
	AT_PORTABLE_TEST_ASSERT(context, ATTranslateHostErrorToSIOError(80) == kATCIOStat_FileExists);		// ERROR_FILE_EXISTS
	AT_PORTABLE_TEST_ASSERT(context, ATTranslateHostErrorToSIOError(183) == kATCIOStat_FileExists);		// ERROR_ALREADY_EXISTS
	AT_PORTABLE_TEST_ASSERT(context, ATTranslateHostErrorToSIOError(112) == kATCIOStat_DiskFull);		// ERROR_DISK_FULL
	AT_PORTABLE_TEST_ASSERT(context, ATTranslateHostErrorToSIOError(145) == kATCIOStat_DirNotEmpty);		// ERROR_DIR_NOT_EMPTY
	AT_PORTABLE_TEST_ASSERT(context, ATTranslateHostErrorToSIOError(5) == kATCIOStat_AccessDenied);		// ERROR_ACCESS_DENIED
	AT_PORTABLE_TEST_ASSERT(context, ATTranslateHostErrorToSIOError(32) == kATCIOStat_FileLocked);		// ERROR_SHARING_VIOLATION
	AT_PORTABLE_TEST_ASSERT(context, ATTranslateHostErrorToSIOError(87) == kATCIOStat_SystemError);		// ERROR_INVALID_PARAMETER
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
