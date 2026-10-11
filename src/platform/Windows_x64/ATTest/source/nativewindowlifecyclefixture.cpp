#include <windows.h>
#include <at/attest/nativewindowlifecyclefixture.h>
#include <at/atnativeui/uinativewindow.h>
#include <vd2/system/function.h>

namespace {
	class TestWindow final : public ATUINativeWindow {
	public:
		explicit TestWindow(ATNativeWindowLifecycleCounts& c) : mCounts(c) {}
		~TestWindow() override { ++mCounts.mDeleted; }
		vdfunction<void()> mReleaseExternal;
		LRESULT WndProc(UINT msg, WPARAM w, LPARAM l) override {
			switch(msg) {
				case WM_CREATE:
					++mCounts.mCreate;
					mCounts.mbValidDuringCreate = IsValid();
					if (mCounts.mbFailCreate) return -1;
					break;
				case WM_DESTROY:
					++mCounts.mDestroy;
					mCounts.mbValidDuringDestroy = IsValid();
					break;
				case WM_SIZE:
					++mCounts.mResize;
					mCounts.mSize = GetClientSize();
					if (mCounts.mbReleaseFromResize && mReleaseExternal) mReleaseExternal();
					if (mCounts.mbDestroyFromResize) { Destroy(); return 0; }
					break;
				case WM_CLOSE:
					++mCounts.mClose;
					if (mCounts.mbVetoClose) return 0;
					break;
			}
			return ATUINativeWindow::WndProc(msg, w, l);
		}
	private:
		ATNativeWindowLifecycleCounts& mCounts;
	};
	class Fixture final : public IATNativeWindowLifecycleFixture {
	public:
		explicit Fixture(ATNativeWindowLifecycleCounts& c) : mCounts(c) {
			mpWindow = new TestWindow(c);
			mpWindow->AddRef();
			mpWindow->mReleaseExternal = [this] { ReleaseExternal(); };
			mhParent = CreateWindowExW(0, L"STATIC", L"", WS_OVERLAPPED,
				100, 120, 500, 400, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
		}
		~Fixture() override { Destroy(); ReleaseExternal(); DestroyParent(); }
		bool Create(bool topLevel) override {
			if (!ATUINativeWindow::Register() || !mhParent) return false;
			if (topLevel) return CreateWindowExW(0, MAKEINTATOM(ATUINativeWindow::Register()),
				L"test", WS_OVERLAPPEDWINDOW, 100, 120, 320, 220, nullptr, nullptr,
				GetModuleHandleW(nullptr), mpWindow) != nullptr;
			return mpWindow->CreateChild(mhParent, 7, 23, 37, 93, 61, WS_CHILD);
		}
		bool IsValid() const override { return !mCounts.mDeleted && mpWindow->IsValid(); }
		bool IsHandleBound() const override {
			return IsValid() && ATUINativeWindow::FromHandle<TestWindow>(mpWindow->GetWindowHandle()) == mpWindow;
		}
		void Resize() override { if (IsValid()) mpWindow->SetSize({71, 49}); }
		void Close() override { if (IsValid()) mpWindow->Close(); }
		void Destroy() override { if (!mCounts.mDeleted && mpWindow->IsValid()) mpWindow->Destroy(); }
		void DestroyParent() override { if (mhParent) { DestroyWindow(mhParent); mhParent = nullptr; } }
		void ReleaseExternal() override { if (mbExternal) { mbExternal = false; mpWindow->Release(); } }
	private:
		ATNativeWindowLifecycleCounts& mCounts;
		TestWindow *mpWindow;
		HWND mhParent = nullptr;
		bool mbExternal = true;
	};
}
std::unique_ptr<IATNativeWindowLifecycleFixture> ATCreateNativeWindowLifecycleFixture(ATNativeWindowLifecycleCounts& c) {
	return std::make_unique<Fixture>(c);
}
