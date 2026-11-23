// The scheduled tracer consumes the production CPU's real history ring.
#include "cputracerportablefixture.h"

namespace {
	void Prepare(MainCPUPortableFixture& h, ATTraceContext& c) {
		c.mBaseTime = h.mFastScheduler.GetTick64(); c.mBaseTickScale = 1; c.mpCollection = new ATTraceCollection;
		h.Load(0x2000, {0xA9,3,0xA2,5,0x20,0x20,0x20,0xEE,0,0x30,0x4C,7,0x20});
		h.Load(0x2020, {0x69,7,0x60}); h.Prepare();
	}
}

bool ATTestAltirraCPUTraceLive(ATPortableTestContext& context) {
	for (bool native : {false, true}) {
		auto host = std::make_unique<MainCPUPortableFixture>(); auto& h = *host;
		ATTraceContext c {}; Prepare(h, c); ATCPUTracer tracer;
		if (native) h.Prepare(kATCPUMode_65C816, 0x2000, 4, true, 0x34);
		const uint32 initial = h.mCPU.GetHistoryCounter();
		tracer.Init(&h.mCPU, &h.mFastScheduler, &h.mSlowScheduler, &h, &c, true, false);
		AT_PORTABLE_TEST_ASSERT(context, h.RunCycles(24000));
		const uint32 complete = h.mCPU.GetHistoryCounter() - initial - 1;
		tracer.Shutdown(); auto *history = CPUTraceHistory(c);
		AT_PORTABLE_TEST_ASSERT(context, history && history->GetEventCount() == complete && complete > 4000);
		AT_PORTABLE_TEST_ASSERT(context, h.mCPU.GetA() == 10 && h.mCPU.GetX() == 5 && h.mCPU.GetS() == 0xFD);
		AT_PORTABLE_TEST_ASSERT(context, history->GetDisasmMode() == h.mCPU.GetDisasmMode() && history->GetSubCycles() == h.mCPU.GetSubCycles());
		const auto cursor = history->StartHistoryIteration(0, 0);
		for (uint32 i = 0; i < complete; i += 53) {
			std::array<const ATCPUHistoryEntry *, 53> entries {}; const uint32 n = std::min<uint32>(53, complete - i);
			AT_PORTABLE_TEST_ASSERT(context, history->ReadHistoryEvents(cursor, entries.data(), i, n) == n);
			for (uint32 j = 0; j < n; ++j) {
				const auto& expected = h.mCPU.GetHistory(complete - i - j);
				AT_PORTABLE_TEST_ASSERT(context, !std::memcmp(entries[j], &expected, sizeof expected));
			}
		}
		const uint32 after = h.mCPU.GetHistoryCounter();
		AT_PORTABLE_TEST_ASSERT(context, h.RunCycles(8000) && h.mCPU.GetHistoryCounter() == after);
		AT_PORTABLE_TEST_ASSERT(context, history->GetEventCount() == complete);
	}
	return true;
}

bool ATTestAltirraCPUTraceOverflow(ATPortableTestContext& context) {
	auto host = std::make_unique<MainCPUPortableFixture>(); auto& h = *host;
	ATTraceContext c {}; Prepare(h, c); ATScheduler paused;
	ATCPUTracer tracer; tracer.Init(&h.mCPU, &h.mFastScheduler, &paused, &h, &c, true, false);
	const uint32 initial = h.mCPU.GetHistoryCounter(); const uint32 length = h.mCPU.GetHistoryLength();
	// No scheduled updates: exactly one full ring of completed instructions.
	while (h.mCPU.GetHistoryCounter() - initial < length + 1)
		AT_PORTABLE_TEST_ASSERT(context, h.RunCycles(1));
	tracer.Shutdown(); auto *history = CPUTraceHistory(c);
	AT_PORTABLE_TEST_ASSERT(context, history && history->GetEventCount() == length - 1);
	const auto cursor = history->StartHistoryIteration(0, 0);
	for (uint32 i = 0; i < length - 1; i += 53) {
		std::array<const ATCPUHistoryEntry *, 53> entries {}; const uint32 n = std::min<uint32>(53, length - 1 - i);
		AT_PORTABLE_TEST_ASSERT(context, history->ReadHistoryEvents(cursor, entries.data(), i, n) == n);
		for (uint32 j = 0; j < n; ++j) {
			const auto& expected = h.mCPU.GetHistory(length - 1 - i - j);
			AT_PORTABLE_TEST_ASSERT(context, !std::memcmp(entries[j], &expected, sizeof expected));
		}
	}
	return true;
}

bool ATTestAltirraCPUTraceLifecycle(ATPortableTestContext& context) {
	auto first = std::make_unique<MainCPUPortableFixture>(); auto second = std::make_unique<MainCPUPortableFixture>();
	ATTraceContext c1 {}, c2 {}; Prepare(*first, c1); Prepare(*second, c2);
	{
		ATCPUTracer tracer;
		tracer.Init(&first->mCPU, &first->mFastScheduler, &first->mSlowScheduler, first.get(), &c1, false, true);
		AT_PORTABLE_TEST_ASSERT(context, first->RunCycles(1000));
		// Reinitialization must detach the old CPU and cancel its event.
		tracer.Init(&second->mCPU, &second->mFastScheduler, &second->mSlowScheduler, second.get(), &c2, true, false);
		const uint32 frozen = first->mCPU.GetHistoryCounter();
		AT_PORTABLE_TEST_ASSERT(context, first->RunCycles(10000) && first->mCPU.GetHistoryCounter() == frozen);
		AT_PORTABLE_TEST_ASSERT(context, second->RunCycles(5000));
	} // destruction flushes trace data and removes the pending scheduler event
	auto *history = CPUTraceHistory(c2); AT_PORTABLE_TEST_ASSERT(context, history && history->GetEventCount() > 1000);
	const uint32 events = history->GetEventCount(), frozen = second->mCPU.GetHistoryCounter();
	AT_PORTABLE_TEST_ASSERT(context, second->RunCycles(10000) && second->mCPU.GetHistoryCounter() == frozen && history->GetEventCount() == events);
	// The tracer must preserve a separately enabled debugger history recorder.
	second->mCPU.SetHistoryEnabled(true); ATTraceContext c3 {}; Prepare(*second, c3);
	ATCPUTracer tracer; tracer.Init(&second->mCPU, &second->mFastScheduler, &second->mSlowScheduler, second.get(), &c3, false, false);
	AT_PORTABLE_TEST_ASSERT(context, second->RunCycles(200)); tracer.Shutdown(); tracer.Shutdown();
	const uint32 active = second->mCPU.GetHistoryCounter();
	AT_PORTABLE_TEST_ASSERT(context, second->RunCycles(200) && second->mCPU.GetHistoryCounter() > active);
	second->mCPU.SetHistoryEnabled(false);
	// A gap must not fabricate a continuous BASIC/context span over lost data.
	CPUTraceProcessorFixture f(false, true, 0); f.Line(0x3000, 10);
	std::vector<ATCPUHistoryEntry> e {CPUTraceEntry(0, 0xB1), CPUTraceEntry(10), CPUTraceEntry(100, 0xB1), CPUTraceEntry(110)};
	e[0].mOpcode[1] = e[2].mOpcode[1] = 0x8A; e[0].mEA = e[2].mEA = 0x3000;
	f.Process(e, 0, 1); f.mProcessor.OnHistoryGap(); f.Process(e, 2, 1); f.mProcessor.Shutdown();
	const auto lines = CPUTraceEvents(f.mContext, 7);
	AT_PORTABLE_TEST_ASSERT(context, lines.size() == 2 && lines[0].mStart == 0 && lines[0].mStop == 10 && lines[1].mStart == 100 && lines[1].mStop == 110);
	return true;
}
