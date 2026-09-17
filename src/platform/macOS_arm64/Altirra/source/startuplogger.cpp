// Altirra startup logger for macOS

#include <sys/utsname.h>

#include <crt_externs.h>
#include <os/log.h>

#include <vd2/system/time.h>
#include <vd2/system/text.h>
#include <at/atcore/logging.h>
#include "startuplogger.h"

namespace {
	class ATStartupLogger {
	public:
		void Init(const wchar_t *channels);
		void Log(VDStringSpanA msg);

	private:
		bool mbShowTimeDeltas = false;
		uint64 mStartTick = 0;
		uint64 mLastMsgTick = 0;
	};

	ATStartupLogger *g_ATStartupLogger;

	os_log_t ATGetStartupLog() {
		static os_log_t log = os_log_create("org.virtualdub.altirra", "startup");
		return log;
	}
}

void ATStartupLogger::Init(const wchar_t *channels) {
	mStartTick = VDGetPreciseTick();
	mLastMsgTick = mStartTick;

	struct utsname osInfo {};
	if (!uname(&osInfo)) {
		VDStringA message;
		message.sprintf("macOS (Darwin %s %s)", osInfo.release, osInfo.machine);
		Log(VDStringSpanA(message));
	}

	VDStringA commandLine("Command line:");
	const int argc = *_NSGetArgc();
	char **const argv = *_NSGetArgv();
	for(int i = 0; i < argc; ++i) {
		commandLine += ' ';
		commandLine += argv[i];
	}
	Log(VDStringSpanA(commandLine));

	if (!channels || !*channels)
		return;

	VDStringRefW remaining(channels);
	for(;;) {
		VDStringRefW token;
		const bool last = !remaining.split(L',', token);
		if (last)
			token = remaining;

		bool found = false;
		if (token.comparei(L"time") == 0) {
			mbShowTimeDeltas = true;
			found = true;
		} else {
			bool enable = true;
			if (!token.empty() && token[0] == L'-') {
				token = token.subspan(1);
				enable = false;
			}

			for(ATLogChannel *channel = ATLogGetFirstChannel(); channel; channel = ATLogGetNextChannel(channel)) {
				if (token.comparei(VDTextU8ToW(VDStringSpanA(channel->GetName()))) == 0) {
					channel->SetEnabled(enable);
					found = true;
					break;
				}
			}
		}

		if (!found)
			Log(VDStringSpanA("Warning: A log channel specified in /startuplog was not found."));

		if (last)
			break;
	}
}

void ATStartupLogger::Log(VDStringSpanA msg) {
	const uint64 tick = VDGetPreciseTick();
	const double scale = VDGetPreciseSecondsPerTick();
	VDStringA line;
	line.sprintf("[%6.3f] ", (double)(tick - mStartTick) * scale);

	if (mbShowTimeDeltas) {
		line.append_sprintf("[%+6.3f] ", (double)(tick - mLastMsgTick) * scale);
		mLastMsgTick = tick;
	}

	line.append(msg.data(), msg.size());
	os_log_with_type(ATGetStartupLog(), OS_LOG_TYPE_DEFAULT, "%{public}s", line.c_str());
}

void ATStartupLogInit(const wchar_t *args) {
	if (!g_ATStartupLogger) {
		g_ATStartupLogger = new ATStartupLogger;
		g_ATStartupLogger->Init(args);
	}
}

void ATStartupLogShutdown() {
	delete g_ATStartupLogger;
	g_ATStartupLogger = nullptr;
}

bool ATStartupLogIsInited() {
	return g_ATStartupLogger != nullptr;
}

void ATStartupLog(const char *msg) {
	if (g_ATStartupLogger)
		g_ATStartupLogger->Log(VDStringSpanA(msg));
}

void ATStartupLog(VDStringSpanA msg) {
	if (g_ATStartupLogger)
		g_ATStartupLogger->Log(msg);
}
