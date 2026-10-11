// Real native backend fixtures for the shared window-lifetime contract.
#ifndef f_AT_ATTEST_NATIVEWINDOWLIFECYCLEFIXTURE_H
#define f_AT_ATTEST_NATIVEWINDOWLIFECYCLEFIXTURE_H

#include <memory>
#include <vd2/system/vectors.h>

struct ATNativeWindowLifecycleCounts {
	int mCreate = 0, mDestroy = 0, mResize = 0, mClose = 0, mDeleted = 0;
	bool mbFailCreate = false, mbVetoClose = false;
	bool mbDestroyFromResize = false, mbReleaseFromResize = false;
	bool mbValidDuringCreate = false, mbValidDuringDestroy = false;
	vdsize32 mSize {0, 0};
};
class IATNativeWindowLifecycleFixture {
public:
	virtual ~IATNativeWindowLifecycleFixture() = default;
	virtual bool Create(bool topLevel = false) = 0;
	virtual bool IsValid() const = 0;
	virtual bool IsHandleBound() const = 0;
	virtual void Resize() = 0;
	virtual void Close() = 0;
	virtual void Destroy() = 0;
	virtual void DestroyParent() = 0;
	virtual void ReleaseExternal() = 0;
};
std::unique_ptr<IATNativeWindowLifecycleFixture> ATCreateNativeWindowLifecycleFixture(ATNativeWindowLifecycleCounts& counts);

#endif
