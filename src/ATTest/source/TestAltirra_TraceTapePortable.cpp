// Altirra portable tape trace tests

#include <cmath>
#include <cwchar>

#include <at/attest/portabletest.h>
#include <tracetape.h>

namespace {
	bool NearTapeTime(double actual, double expected) {
		return std::abs(actual - expected) < 1e-9;
	}
}

bool ATTestAltirraTraceTape(ATPortableTestContext& context) {
	vdrefptr<ATTraceChannelTape> tape {
		new ATTraceChannelTape(1000, 0.01, L"Tape", true, 44100.0)
	};
	AT_PORTABLE_TEST_ASSERT(context, !wcscmp(tape->GetName(), L"Tape"));
	AT_PORTABLE_TEST_ASSERT(context, tape->IsTurbo());
	AT_PORTABLE_TEST_ASSERT(context, tape->GetSamplesPerSec() == 44100.0);
	AT_PORTABLE_TEST_ASSERT(context, tape->AsInterface(0) == nullptr);
	AT_PORTABLE_TEST_ASSERT(context, tape->IsEmpty());
	AT_PORTABLE_TEST_ASSERT(context, tape->GetEventCount() == 0);
	AT_PORTABLE_TEST_ASSERT(context, NearTapeTime(tape->GetDuration(), 0));

	tape->AddEvent(1000, ATTraceChannelTape::kEventType_Play, 10);
	AT_PORTABLE_TEST_ASSERT(context, NearTapeTime(tape->GetDuration(), kATTraceTime_Infinity));
	tape->TruncateLastEvent(1050);
	AT_PORTABLE_TEST_ASSERT(context, NearTapeTime(tape->GetDuration(), 0.5));
	tape->AddEvent(1100, ATTraceChannelTape::kEventType_Record, 100);
	tape->TruncateLastEvent(1150);
	AT_PORTABLE_TEST_ASSERT(context, tape->GetEventCount() == 2);
	AT_PORTABLE_TEST_ASSERT(context, tape->GetTraceSize() > 0);
	AT_PORTABLE_TEST_ASSERT(context, NearTapeTime(tape->GetDuration(), 1.5));

	ATTraceEvent event {};
	tape->StartIteration(0, 2, 2.0);
	AT_PORTABLE_TEST_ASSERT(context, tape->GetNextEvent(event));
	AT_PORTABLE_TEST_ASSERT(context, event.mpName == nullptr);
	AT_PORTABLE_TEST_ASSERT(context, NearTapeTime(event.mEventStop, 1.5));
	AT_PORTABLE_TEST_ASSERT(context, !tape->GetNextEvent(event));

	// The current threshold must control whether the previous event overlaps.
	tape->StartIteration(1.0, 2.0, 0.1);
	AT_PORTABLE_TEST_ASSERT(context, tape->GetNextEvent(event));
	AT_PORTABLE_TEST_ASSERT(context, NearTapeTime(event.mEventStart, 1.0));
	AT_PORTABLE_TEST_ASSERT(context, NearTapeTime(event.mEventStop, 1.5));
	AT_PORTABLE_TEST_ASSERT(context, !wcscmp(event.mpName, L"Record"));
	AT_PORTABLE_TEST_ASSERT(context, event.mBgColor == kATTraceColor_Tape_Record);
	AT_PORTABLE_TEST_ASSERT(context, event.mFgColor == 0);
	AT_PORTABLE_TEST_ASSERT(context, tape->GetLastEvent().mEventType == ATTraceChannelTape::kEventType_Record);
	AT_PORTABLE_TEST_ASSERT(context, tape->GetLastEvent().mPosition == 100);
	AT_PORTABLE_TEST_ASSERT(context, !tape->GetNextEvent(event));

	tape->StartIteration(0.2, 0.9, 0);
	AT_PORTABLE_TEST_ASSERT(context, tape->GetNextEvent(event));
	AT_PORTABLE_TEST_ASSERT(context, !wcscmp(event.mpName, L"Play"));
	AT_PORTABLE_TEST_ASSERT(context, event.mBgColor == kATTraceColor_Tape_Play);
	AT_PORTABLE_TEST_ASSERT(context, tape->GetLastEvent().mPosition == 10);
	AT_PORTABLE_TEST_ASSERT(context, !tape->GetNextEvent(event));

	tape->StartIteration(1.6, 2.0, 0);
	AT_PORTABLE_TEST_ASSERT(context, !tape->GetNextEvent(event));

	// Tape events may start before the trace's base tick.
	vdrefptr<ATTraceChannelTape> early {
		new ATTraceChannelTape(1000, 0.01, L"Early", false, 600.0)
	};
	AT_PORTABLE_TEST_ASSERT(context, !early->IsTurbo());
	early->AddEvent(900, ATTraceChannelTape::kEventType_Play, 3);
	early->TruncateLastEvent(950);
	early->StartIteration(-2.0, 0, 0);
	AT_PORTABLE_TEST_ASSERT(context, early->GetNextEvent(event));
	AT_PORTABLE_TEST_ASSERT(context, NearTapeTime(event.mEventStart, -1.0));
	AT_PORTABLE_TEST_ASSERT(context, NearTapeTime(event.mEventStop, -0.5));
	AT_PORTABLE_TEST_ASSERT(context, early->GetLastEvent().mPosition == 3);
	AT_PORTABLE_TEST_ASSERT(context, !early->GetNextEvent(event));

	return true;
}
