// Altirra native socket synchronization helpers

#ifndef f_AT_ATNETWORKSOCKETS_INTERNAL_SOCKETUTILS_H
#define f_AT_ATNETWORKSOCKETS_INTERNAL_SOCKETUTILS_H

#include <vd2/system/refcount.h>
#include <vd2/system/thread.h>

struct ATNetSyncContext final : public vdrefcount {
	VDCriticalSection mMutex;
};

#endif
