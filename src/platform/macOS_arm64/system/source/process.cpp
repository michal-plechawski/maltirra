// Altirra asynchronous process launching for macOS ARM64

#include <cerrno>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <spawn.h>
#include <sys/wait.h>

#include <dispatch/dispatch.h>

#include <vector>

#include <vd2/system/Error.h>
#include <vd2/system/process.h>
#include <vd2/system/text.h>
#include <vd2/system/VDString.h>

extern char **environ;

namespace {
	[[noreturn]] void VDThrowProcessError(const wchar_t *path, int errorCode) {
		const VDStringA pathUTF8 = VDTextWToU8(path ? path : L"", -1);
		throw MyError(
			"Unable to launch process \"%s\": %s (error %d).",
			pathUTF8.c_str(),
			strerror(errorCode),
			errorCode);
	}

	void VDAppendBackslashes(VDStringW& destination, size_t count) {
		while(count--)
			destination.push_back(L'\\');
	}

	std::vector<VDStringA> VDSplitProcessArguments(const wchar_t *arguments) {
		std::vector<VDStringA> result;
		if (!arguments)
			return result;

		const wchar_t *source = arguments;
		while(*source) {
			while(*source == L' ' || *source == L'\t')
				++source;
			if (!*source)
				break;

			VDStringW argument;
			bool quoted = false;
			for(;;) {
				size_t backslashCount = 0;
				while(*source == L'\\') {
					++backslashCount;
					++source;
				}

				if (*source == L'"') {
					VDAppendBackslashes(argument, backslashCount / 2);
					if (backslashCount & 1) {
						argument.push_back(L'"');
						++source;
					} else {
						++source;
						if (quoted && *source == L'"') {
							argument.push_back(L'"');
							++source;
						} else {
							quoted = !quoted;
						}
					}
					continue;
				}

				VDAppendBackslashes(argument, backslashCount);
				if (!*source || (!quoted && (*source == L' ' || *source == L'\t')))
					break;

				argument.push_back(*source++);
			}

			result.push_back(VDTextWToU8(argument.c_str(), static_cast<int>(argument.size())));
		}

		return result;
	}

	void VDReapProcess(void *context) {
		const pid_t processId = static_cast<pid_t>(reinterpret_cast<intptr_t>(context));
		int status = 0;
		while(waitpid(processId, &status, 0) < 0 && errno == EINTR) {
		}
	}
}

void VDLaunchProgram(const wchar_t *path, const wchar_t *args) {
	if (!path || !*path)
		VDThrowProcessError(path, EINVAL);

	std::vector<VDStringA> argumentStorage;
	argumentStorage.push_back(VDTextWToU8(path, -1));
	std::vector<VDStringA> additionalArguments = VDSplitProcessArguments(args);
	argumentStorage.insert(
		argumentStorage.end(),
		std::make_move_iterator(additionalArguments.begin()),
		std::make_move_iterator(additionalArguments.end()));

	std::vector<char *> argumentPointers;
	argumentPointers.reserve(argumentStorage.size() + 1);
	for(VDStringA& argument : argumentStorage)
		argumentPointers.push_back(const_cast<char *>(argument.c_str()));
	argumentPointers.push_back(nullptr);

	posix_spawnattr_t attributes;
	int errorCode = posix_spawnattr_init(&attributes);
	if (errorCode)
		VDThrowProcessError(path, errorCode);

	const short flags = POSIX_SPAWN_SETPGROUP | POSIX_SPAWN_CLOEXEC_DEFAULT;
	errorCode = posix_spawnattr_setflags(&attributes, flags);
	if (!errorCode)
		errorCode = posix_spawnattr_setpgroup(&attributes, 0);

	pid_t processId = 0;
	if (!errorCode) {
		errorCode = posix_spawnp(
			&processId,
			argumentStorage.front().c_str(),
			nullptr,
			&attributes,
			argumentPointers.data(),
			environ);
	}

	posix_spawnattr_destroy(&attributes);
	if (errorCode)
		VDThrowProcessError(path, errorCode);

	dispatch_async_f(
		dispatch_get_global_queue(QOS_CLASS_UTILITY, 0),
		reinterpret_cast<void *>(static_cast<intptr_t>(processId)),
		VDReapProcess);
}
