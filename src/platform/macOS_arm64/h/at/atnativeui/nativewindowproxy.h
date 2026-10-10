// Altirra native AppKit counterpart of the borrowed Win32 window proxy.

#ifndef f_AT_UINATIVEWINDOWPROXY_H
#define f_AT_UINATIVEWINDOWPROXY_H

#include <vd2/system/vdtypes.h>
#include <vd2/system/vectors.h>
#include <vd2/system/VDString.h>

// Main-thread-only, non-owning wrapper around an NSView or NSWindow. The owner
// must outlive all proxies. Destroy detaches a view or forcibly closes a window
// and clears this proxy; other borrowed aliases must no longer be used.
// Geometry is in AppKit points with a top-left origin, not backing pixels.
// Native event hosts must consult view enabled/tab-stop metadata;
// NSControl enabled state and text are updated directly when available.
class ATUINativeWindowProxy {
public:
	ATUINativeWindowProxy() = default;
	explicit ATUINativeWindowProxy(VDGUIHandle handle) : mhwnd(handle) {}
	bool IsValid() const { return mhwnd != nullptr; }
	VDGUIHandle GetWindowHandle() const { return mhwnd; }
	uint16 GetWindowId() const;
	void SetWindowId(uint16 id);
	bool IsVisible() const;
	void SetVisible(bool visible);
	void Show();
	void Hide();
	void Activate();
	void Focus();
	// Windows receive a vetoable close request. Views can implement performClose:
	// to intercept it; otherwise Close detaches that view, not its entire window.
	void Close();
	void Destroy();
	bool IsEnabled() const;
	void SetEnabled(bool enabled);
	bool IsTabStop() const;
	void SetTabStop(bool enabled);
	vdsize32 GetSize() const;
	void SetSize(const vdsize32& size);
	vdpoint32 GetPosition() const;
	void SetPosition(const vdpoint32& position);
	vdrect32 GetArea() const;
	void SetArea(const vdrect32& area);
	vdrect32 GetClientArea() const;
	vdsize32 GetClientSize() const;
	// Detached views have no screen area; point transforms are identity.
	vdrect32 GetWindowArea() const;
	vdpoint32 TransformScreenToClient(const vdpoint32& point) const;
	vdrect32 TransformScreenToClient(const vdrect32& rect) const;
	vdpoint32 TransformClientToScreen(const vdpoint32& point) const;
	vdrect32 TransformClientToScreen(const vdrect32& rect) const;
	void BringToFront();
	void InsertBelow(VDGUIHandle reference);
	VDStringW GetCaption() const;
	void SetCaption(const wchar_t *caption);
	void Invalidate();
	void InvalidateArea(const vdrect32& area);

protected:
	union {
		VDGUIHandle mhwnd = nullptr;
		VDGUIHandle mhdlg;
	};
};

#endif
