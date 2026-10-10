// Shared Win32/AppKit native-window proxy contract; no simulated backend.

#include <at/attest/portabletest.h>
#include <at/attest/nativewindowfixture.h>

namespace {
	bool Equal(vdrect32 a, vdrect32 b) {
		return a.left == b.left && a.top == b.top && a.right == b.right && a.bottom == b.bottom;
	}
}

bool ATTestNativeUIWindowProxy(ATPortableTestContext& context) {
	ATUINativeWindowProxy empty;
	AT_PORTABLE_TEST_ASSERT(context, !empty.IsValid() && !empty.IsVisible() && !empty.IsEnabled());
	AT_PORTABLE_TEST_ASSERT(context, !empty.IsTabStop() && empty.GetWindowId() == 0);
	AT_PORTABLE_TEST_ASSERT(context, empty.GetCaption().empty());
	AT_PORTABLE_TEST_ASSERT(context, Equal(empty.GetArea(), {0, 0, 0, 0}));
	AT_PORTABLE_TEST_ASSERT(context, Equal(empty.GetClientArea(), {0, 0, 0, 0}));
	AT_PORTABLE_TEST_ASSERT(context, Equal(empty.GetWindowArea(), {0, 0, 0, 0}));
	const vdrect32 testRect(-4, 7, 13, 21);
	AT_PORTABLE_TEST_ASSERT(context, Equal(empty.TransformClientToScreen(testRect), testRect));
	AT_PORTABLE_TEST_ASSERT(context, Equal(empty.TransformScreenToClient(testRect), testRect));
	empty.SetVisible(true);
	empty.Focus();
	empty.Activate();
	empty.Hide();
	empty.SetEnabled(false);
	empty.SetWindowId(5);
	empty.SetCaption(L"ignored");
	empty.SetArea({2, 3, 20, 30});
	empty.Invalidate();
	empty.InvalidateArea({1, 2, 3, 4});
	empty.Close();
	empty.Destroy();
	AT_PORTABLE_TEST_ASSERT(context, !empty.IsValid());

	auto fixture = ATCreateNativeWindowProxyFixture();
	AT_PORTABLE_TEST_ASSERT(context, fixture != nullptr);
	auto& proxy = fixture->GetProxy();
	AT_PORTABLE_TEST_ASSERT(context, proxy.IsValid());
	ATUINativeWindowProxy alias(proxy.GetWindowHandle());
	proxy.SetArea({23, 37, 116, 98});
	AT_PORTABLE_TEST_ASSERT(context, Equal(proxy.GetArea(), {23, 37, 116, 98}));
	AT_PORTABLE_TEST_ASSERT(context, proxy.GetSize().w == 93 && proxy.GetSize().h == 61);
	proxy.SetSize({71, 49});
	AT_PORTABLE_TEST_ASSERT(context, Equal(alias.GetArea(), {23, 37, 94, 86}));
	proxy.SetPosition({-11, 19});
	AT_PORTABLE_TEST_ASSERT(context, Equal(proxy.GetArea(), {-11, 19, 60, 68}));
	AT_PORTABLE_TEST_ASSERT(context, proxy.GetPosition().x == -11 && proxy.GetPosition().y == 19);
	AT_PORTABLE_TEST_ASSERT(context, proxy.GetClientSize().w == 71 && proxy.GetClientSize().h == 49);
	AT_PORTABLE_TEST_ASSERT(context, Equal(proxy.GetClientArea(), {0, 0, 71, 49}));
	AT_PORTABLE_TEST_ASSERT(context,
		Equal(proxy.TransformScreenToClient(proxy.TransformClientToScreen(testRect)), testRect));

	proxy.SetVisible(false);
	AT_PORTABLE_TEST_ASSERT(context, !proxy.IsVisible());
	proxy.Show();
	AT_PORTABLE_TEST_ASSERT(context, alias.IsVisible());
	proxy.Hide();
	AT_PORTABLE_TEST_ASSERT(context, !alias.IsVisible());
	proxy.SetVisible(true);
	AT_PORTABLE_TEST_ASSERT(context, proxy.IsVisible());
	proxy.SetEnabled(false);
	AT_PORTABLE_TEST_ASSERT(context, !alias.IsEnabled());
	proxy.SetEnabled(true);
	AT_PORTABLE_TEST_ASSERT(context, alias.IsEnabled());
	proxy.SetTabStop(true);
	AT_PORTABLE_TEST_ASSERT(context, alias.IsTabStop());
	proxy.SetTabStop(false);
	AT_PORTABLE_TEST_ASSERT(context, !alias.IsTabStop());
	proxy.SetWindowId(0xFEDC);
	AT_PORTABLE_TEST_ASSERT(context, alias.GetWindowId() == 0xFEDC);
	proxy.SetCaption(L"Altirra \u0105\u03A9 \U0001F680");
	AT_PORTABLE_TEST_ASSERT(context, proxy.GetCaption() == L"Altirra \u0105\u03A9 \U0001F680");
	proxy.SetCaption(L"");
	AT_PORTABLE_TEST_ASSERT(context, alias.GetCaption().empty());
	proxy.InvalidateArea({0, 0, 5, 7});
	proxy.InvalidateArea({5, 7, 0, 0});
	proxy.Invalidate();
	const auto area = proxy.GetArea();
	proxy.BringToFront();
	AT_PORTABLE_TEST_ASSERT(context, fixture->IsAboveSibling());
	proxy.InsertBelow(fixture->GetSibling().GetWindowHandle());
	AT_PORTABLE_TEST_ASSERT(context, !fixture->IsAboveSibling());
	AT_PORTABLE_TEST_ASSERT(context, Equal(proxy.GetArea(), area));
	proxy.SetArea({7, 9, 2, 3});
	AT_PORTABLE_TEST_ASSERT(context, proxy.GetSize().w == 0 && proxy.GetSize().h == 0);
	proxy.Destroy();
	return true;
}
