// VirtualDub system library: macOS ARM64 thread initialization hooks

#include <vd2/system/tls.h>

VDThreadInitHook g_pInitHook;

void VDInitThreadData(const char *threadName) {
	if (g_pInitHook)
		g_pInitHook(true, threadName);
}

void VDDeinitThreadData() {
	if (g_pInitHook)
		g_pInitHook(false, nullptr);
}

void VDSetThreadInitHook(VDThreadInitHook hook) {
	g_pInitHook = hook;
}
