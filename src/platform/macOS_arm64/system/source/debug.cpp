// Altirra low-level diagnostics for macOS ARM64

#include <cerrno>
#include <cstdarg>
#include <cstdio>

#include <sys/sysctl.h>
#include <sys/types.h>
#include <unistd.h>

#include <vd2/system/debug.h>

namespace {
	IVDExternalCallTrap *gVDExternalCallTrap;

	bool VDIsDebuggerAttached() {
		int query[] = {
			CTL_KERN,
			KERN_PROC,
			KERN_PROC_PID,
			static_cast<int>(getpid())
		};
		struct kinfo_proc processInfo {};
		size_t processInfoSize = sizeof processInfo;
		if (sysctl(query, 4, &processInfo, &processInfoSize, nullptr, 0))
			return false;

		return (processInfo.kp_proc.p_flag & P_TRACED) != 0;
	}

	VDAssertResult VDReportAssertion(
		const char *expression,
		const char *file,
		int line,
		bool pointerAssertion) {
		const int savedError = errno;
		fprintf(
			stderr,
			pointerAssertion
				? "%s(%d): Assertion failed: %s is not a valid pointer\n"
				: "%s(%d): Assertion failed: %s\n",
			file ? file : "<unknown>",
			line,
			expression ? expression : "<unknown>");
		fflush(stderr);
		errno = savedError;

		return VDIsDebuggerAttached() ? kVDAssertBreak : kVDAssertIgnore;
	}
}

VDAssertResult VDAssert(const char *expression, const char *file, int line) {
	return VDReportAssertion(expression, file, line, false);
}

VDAssertResult VDAssertPtr(const char *expression, const char *file, int line) {
	return VDReportAssertion(expression, file, line, true);
}

void VDProtectedAutoScopeICLWorkaround() {
}

void VDDebugPrint(const char *format, ...) {
	if (!format)
		return;

	const int savedError = errno;
	char buffer[4096] = {};
	va_list arguments;
	va_start(arguments, format);
	vsnprintf(buffer, sizeof buffer, format, arguments);
	va_end(arguments);
	buffer[sizeof buffer - 1] = 0;
	fputs(buffer, stderr);
	fflush(stderr);
	errno = savedError;
}

void VDSetExternalCallTrap(IVDExternalCallTrap *trap) {
	gVDExternalCallTrap = trap;
}

bool IsMMXState() {
	return false;
}

void ClearMMXState() {
}

void VDClearEvilCPUStates() {
}

void VDPreCheckExternalCodeCall(const char *file, int line) {
	(void)gVDExternalCallTrap;
	(void)file;
	(void)line;
}

void VDPostCheckExternalCodeCall(const wchar_t *context, const char *file, int line) {
	(void)gVDExternalCallTrap;
	(void)context;
	(void)file;
	(void)line;
}
