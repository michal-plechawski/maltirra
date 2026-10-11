// Identical lifetime assertions against real Win32 and AppKit windows.

#include <at/attest/portabletest.h>
#include <at/attest/nativewindowlifecyclefixture.h>

bool ATTestNativeUIWindowLifecycle(ATPortableTestContext& context) {
	{
		ATNativeWindowLifecycleCounts c;
		auto f = ATCreateNativeWindowLifecycleFixture(c);
		AT_PORTABLE_TEST_ASSERT(context, f && f->Create());
		AT_PORTABLE_TEST_ASSERT(context, c.mCreate == 1 && c.mbValidDuringCreate);
		AT_PORTABLE_TEST_ASSERT(context, c.mResize >= 1 && c.mSize.w == 93 && c.mSize.h == 61);
		AT_PORTABLE_TEST_ASSERT(context, f->IsValid() && f->IsHandleBound());
		const int resize = c.mResize;
		f->Resize();
		AT_PORTABLE_TEST_ASSERT(context, c.mResize > resize && c.mSize.w == 71 && c.mSize.h == 49);
		f->DestroyParent();
		AT_PORTABLE_TEST_ASSERT(context, !f->IsValid() && c.mDestroy == 1 && c.mDeleted == 0);
		f->Destroy();
		AT_PORTABLE_TEST_ASSERT(context, c.mDestroy == 1);
		f->ReleaseExternal();
		AT_PORTABLE_TEST_ASSERT(context, c.mDeleted == 1);
	}
	{
		ATNativeWindowLifecycleCounts c;
		c.mbFailCreate = true;
		auto f = ATCreateNativeWindowLifecycleFixture(c);
		AT_PORTABLE_TEST_ASSERT(context, f && !f->Create());
		AT_PORTABLE_TEST_ASSERT(context, c.mCreate == 1 && c.mDestroy == 1 && !f->IsValid());
		f->ReleaseExternal();
		AT_PORTABLE_TEST_ASSERT(context, c.mDeleted == 1);
	}
	{
		ATNativeWindowLifecycleCounts c;
		c.mbVetoClose = true;
		auto f = ATCreateNativeWindowLifecycleFixture(c);
		AT_PORTABLE_TEST_ASSERT(context, f && f->Create(true));
		AT_PORTABLE_TEST_ASSERT(context, f->IsHandleBound());
		f->Close();
		AT_PORTABLE_TEST_ASSERT(context, c.mClose == 1 && c.mDestroy == 0 && f->IsValid());
		c.mbVetoClose = false;
		f->Close();
		AT_PORTABLE_TEST_ASSERT(context, c.mClose == 2 && c.mDestroy == 1 && !f->IsValid());
	}
	{
		ATNativeWindowLifecycleCounts c;
		c.mbVetoClose = true;
		auto f = ATCreateNativeWindowLifecycleFixture(c);
		AT_PORTABLE_TEST_ASSERT(context, f && f->Create());
		f->Destroy();
		AT_PORTABLE_TEST_ASSERT(context, c.mClose == 0 && c.mDestroy == 1 && !f->IsValid());
	}
	{
		ATNativeWindowLifecycleCounts c;
		auto f = ATCreateNativeWindowLifecycleFixture(c);
		AT_PORTABLE_TEST_ASSERT(context, f && f->Create());
		c.mbDestroyFromResize = true;
		c.mbReleaseFromResize = true;
		f->Resize();
		AT_PORTABLE_TEST_ASSERT(context, c.mDestroy == 1 && c.mDeleted == 1 && !f->IsValid());
	}
	{
		ATNativeWindowLifecycleCounts c;
		auto f = ATCreateNativeWindowLifecycleFixture(c);
		AT_PORTABLE_TEST_ASSERT(context, f && f->Create());
		c.mbReleaseFromResize = true;
		f->Resize();
		AT_PORTABLE_TEST_ASSERT(context, c.mDeleted == 0 && f->IsValid());
		f->Destroy();
		AT_PORTABLE_TEST_ASSERT(context, c.mDeleted == 1 && c.mDestroy == 1);
	}
	return true;
}
