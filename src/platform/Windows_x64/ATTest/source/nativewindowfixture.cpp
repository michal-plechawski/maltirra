// Hidden real Win32 windows for the shared native-window proxy contract.

#include <windows.h>
#include <at/attest/nativewindowfixture.h>

namespace {
	class Fixture final : public IATNativeWindowProxyFixture {
	public:
		Fixture() {
			mhParent = CreateWindowExW(0, L"STATIC", L"", WS_OVERLAPPED,
				40, 60, 500, 400, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
			mhChild = CreateWindowExW(0, L"STATIC", L"", WS_CHILD,
				0, 0, 0, 0, mhParent, nullptr, GetModuleHandleW(nullptr), nullptr);
			mhSibling = CreateWindowExW(0, L"STATIC", L"", WS_CHILD,
				0, 0, 10, 10, mhParent, nullptr, GetModuleHandleW(nullptr), nullptr);
			mProxy = ATUINativeWindowProxy(mhChild);
			mSibling = ATUINativeWindowProxy(mhSibling);
		}
		~Fixture() { if (mhParent) DestroyWindow(mhParent); }
		bool IsValid() const { return mhParent && mhChild && mhSibling; }
		ATUINativeWindowProxy& GetProxy() override { return mProxy; }
		ATUINativeWindowProxy& GetSibling() override { return mSibling; }
		bool IsAboveSibling() const override {
			return GetWindow(mhChild, GW_HWNDNEXT) == mhSibling;
		}
	private:
		HWND mhParent, mhChild, mhSibling;
		ATUINativeWindowProxy mProxy, mSibling;
	};
}
std::unique_ptr<IATNativeWindowProxyFixture> ATCreateNativeWindowProxyFixture() {
	auto fixture = std::make_unique<Fixture>();
	if (!fixture->IsValid()) return nullptr;
	return fixture;
}
