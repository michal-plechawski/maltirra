// Native AppKit lifecycle/event counterpart of Windows uinativewindow.cpp.
#ifndef f_AT_UINATIVEWINDOW_H
#define f_AT_UINATIVEWINDOW_H

#include <memory>
#include <vd2/system/atomic.h>
#include <vd2/system/unknown.h>
#include <at/atnativeui/nativewindowproxy.h>
#include <at/atnativeui/nativeinput.h>
#include <at/atui/constants.h>

struct ATMacNativeWindowState;

// Main-thread API. Creation attaches a native lifetime reference. Dispatch
// temporarily pins the C++ object, so callbacks may Destroy/Release safely.
// OnCreate failure is followed by OnDestroy for partially-created resources.
// Exceptions in AppKit-dispatched callbacks are contained. OnCreate exceptions
// clean up partial resources and propagate through the C++ Create call.
// Allocate derived objects on the heap and manage external AddRef/Release refs.
class ATUINativeWindow : public IVDUnknown, public ATUINativeWindowProxy {
public:
	ATUINativeWindow();
	virtual ~ATUINativeWindow();
	int AddRef();
	int Release();
	void *AsInterface(uint32 id) override;
	bool CreateChild(VDGUIHandle parent, uint16 id, int x, int y, int w, int h,
		bool visible = false, const wchar_t *text = nullptr);
	// Outer window frame in AppKit points, top-left primary-screen coordinates.
	bool CreateWindow(const vdrect32& area, const wchar_t *title = nullptr, bool visible = false);
	VDGUIHandle GetClientViewHandle() const;
	void Destroy();
	void Close();
	void SetTouchMode(ATUITouchMode mode);
	static ATUINativeWindow *FromNativeHandle(VDGUIHandle handle);
	template<class T> static T *FromHandle(VDGUIHandle handle) {
		return static_cast<T *>(FromNativeHandle(handle));
	}

protected:
	virtual bool OnCreate() { return true; }
	virtual void OnDestroy() {}
	virtual void OnSize() {}
	virtual bool OnCloseRequest() { return true; }
	virtual void OnSetFocus() {}
	virtual void OnKillFocus() {}
	virtual bool OnInput(const ATNativeInputEvent&) { return false; }
	virtual void OnPaint(VDGUIHandle /* CGContextRef */, const vdrect32&) {}
	virtual ATUITouchMode GetTouchModeAtPoint(const vdpoint32&) const { return kATUITouchMode_Default; }

private:
	friend struct ATMacNativeWindowState;
	bool FinishCreate(const std::shared_ptr<ATMacNativeWindowState>& state, bool visible);
	VDAtomicInt mRefCount;
	std::shared_ptr<ATMacNativeWindowState> mpState;
	ATUITouchMode mTouchMode = kATUITouchMode_Default;
};

#endif
