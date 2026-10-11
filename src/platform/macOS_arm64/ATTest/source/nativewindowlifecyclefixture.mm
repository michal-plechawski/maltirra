#import <AppKit/AppKit.h>
#include <at/attest/nativewindowlifecyclefixture.h>
#include <at/atnativeui/uinativewindow.h>
#include <vd2/system/function.h>
#include <vd2/system/refcount.h>

namespace {
	class TestWindow final : public ATUINativeWindow {
	public:
		explicit TestWindow(ATNativeWindowLifecycleCounts& c) : mCounts(c) {}
		~TestWindow() override { ++mCounts.mDeleted; }
		vdfunction<void()> mReleaseExternal;
	protected:
		bool OnCreate() override {
			++mCounts.mCreate;
			mCounts.mbValidDuringCreate = IsValid();
			return !mCounts.mbFailCreate;
		}
		void OnDestroy() override { ++mCounts.mDestroy; mCounts.mbValidDuringDestroy = IsValid(); }
		void OnSize() override {
			++mCounts.mResize;
			mCounts.mSize = GetClientSize();
			if (mCounts.mbReleaseFromResize && mReleaseExternal) mReleaseExternal();
			if (mCounts.mbDestroyFromResize) Destroy();
		}
		bool OnCloseRequest() override { ++mCounts.mClose; return !mCounts.mbVetoClose; }
	private:
		ATNativeWindowLifecycleCounts& mCounts;
	};
	class Fixture final : public IATNativeWindowLifecycleFixture {
	public:
		explicit Fixture(ATNativeWindowLifecycleCounts& c) : mCounts(c), mParent(new TestWindow(mParentCounts)) {
			mpPool = [[NSAutoreleasePool alloc] init];
			mpWindow = new TestWindow(c);
			mpWindow->AddRef();
			mpWindow->mReleaseExternal = [this] { ReleaseExternal(); };
		}
		~Fixture() override {
			Destroy(); ReleaseExternal();
			if (mParent) mParent->Destroy();
			mParent = nullptr;
			[mpPool drain];
		}
		bool Create(bool topLevel) override {
			if (topLevel) return mpWindow->CreateWindow({100, 120, 420, 340}, L"test");
			if (!mParent->CreateWindow({100, 120, 600, 520}, L"parent")) return false;
			return mpWindow->CreateChild(mParent->GetClientViewHandle(), 7, 23, 37, 93, 61);
		}
		bool IsValid() const override { return !mCounts.mDeleted && mpWindow->IsValid(); }
		bool IsHandleBound() const override {
			return IsValid() && ATUINativeWindow::FromHandle<TestWindow>(mpWindow->GetWindowHandle()) == mpWindow;
		}
		void Resize() override { if (IsValid()) mpWindow->SetSize({71, 49}); }
		void Close() override { if (IsValid()) mpWindow->Close(); }
		void Destroy() override { if (!mCounts.mDeleted) mpWindow->Destroy(); }
		void DestroyParent() override { mParent->Destroy(); }
		void ReleaseExternal() override {
			if (mbExternal) { mbExternal = false; mpWindow->Release(); }
		}
	private:
		ATNativeWindowLifecycleCounts& mCounts;
		ATNativeWindowLifecycleCounts mParentCounts;
		vdrefptr<TestWindow> mParent;
		TestWindow *mpWindow;
		bool mbExternal = true;
		NSAutoreleasePool *mpPool;
	};
}
std::unique_ptr<IATNativeWindowLifecycleFixture> ATCreateNativeWindowLifecycleFixture(ATNativeWindowLifecycleCounts& c) {
	return std::make_unique<Fixture>(c);
}
