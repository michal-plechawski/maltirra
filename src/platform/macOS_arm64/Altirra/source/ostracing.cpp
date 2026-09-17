// Altirra - Atari 800/800XL/5200 emulator
// Copyright (C) 2026
//
// macOS native tracing backend.

#include <os/signpost.h>

#include "ostracing.h"

namespace {
	bool g_ATOSTracingEnabled;
	bool g_ATOSSimulationIntervalActive;

	os_log_t ATGetOSSignpostLog() {
		static os_log_t log = os_log_create("org.virtualdub.altirra", "simulation");
		return log;
	}
}

void ATInitOSTracing() {
	g_ATOSTracingEnabled = true;
}

void ATShutdownOSTracing() {
	ATOSTraceSimulateEnd();
	g_ATOSTracingEnabled = false;
}

bool ATIsOSTracingEnabled() {
	return g_ATOSTracingEnabled;
}

void ATOSTraceSimulateBegin() {
	if (!g_ATOSTracingEnabled || g_ATOSSimulationIntervalActive)
		return;

	os_signpost_interval_begin(
		ATGetOSSignpostLog(),
		OS_SIGNPOST_ID_EXCLUSIVE,
		"Simulate");
	g_ATOSSimulationIntervalActive = true;
}

void ATOSTraceSimulateEnd() {
	if (!g_ATOSSimulationIntervalActive)
		return;

	os_signpost_interval_end(
		ATGetOSSignpostLog(),
		OS_SIGNPOST_ID_EXCLUSIVE,
		"Simulate");
	g_ATOSSimulationIntervalActive = false;
}
