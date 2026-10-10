// Native fixtures for the shared window-proxy contract test.
#ifndef f_AT_ATTEST_NATIVEWINDOWFIXTURE_H
#define f_AT_ATTEST_NATIVEWINDOWFIXTURE_H

#include <memory>
#include <at/atnativeui/nativewindowproxy.h>

class IATNativeWindowProxyFixture {
public:
	virtual ~IATNativeWindowProxyFixture() = default;
	virtual ATUINativeWindowProxy& GetProxy() = 0;
	virtual ATUINativeWindowProxy& GetSibling() = 0;
	virtual bool IsAboveSibling() const = 0;
};
std::unique_ptr<IATNativeWindowProxyFixture> ATCreateNativeWindowProxyFixture();

#endif
