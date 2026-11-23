// Trace interpretation tested through public channels, including clock wraps.
#include "cputracerportablefixture.h"

bool ATTestAltirraCPUTraceHistory(ATPortableTestContext& context) {
	constexpr uint64 base = UINT64_C(0x1FFFFFF80);
	constexpr uint32 count = 20000;
	for (bool async : {false, true}) {
		CPUTraceProcessorFixture f(true, false, base, async);
		std::vector<ATCPUHistoryEntry> input(count + 1);
		for (uint32 i = 0; i <= count; ++i) {
			auto& e = input[i]; e = CPUTraceEntry(base + i * 3, 0xEE);
			e.mPC = static_cast<uint16>(0x3000 + i * 5); e.mA = static_cast<uint8>(i);
			e.mX = static_cast<uint8>(i >> 3); e.mY = static_cast<uint8>(i >> 5); e.mEA = 0x1000 + i;
			e.mOpcode[1] = static_cast<uint8>(i); e.mOpcode[2] = 0x30;
		}
		for (uint32 first = 0; first < count; first += 257) f.Process(input, first, std::min<uint32>(257, count - first));
		f.mProcessor.Shutdown(); auto *h = CPUTraceHistory(f.mContext);
		AT_PORTABLE_TEST_ASSERT(context, h && h->GetEventCount() == count && h->GetSubCycles() == 1);
		const auto cursor = h->StartHistoryIteration(0, 0);
		for (uint32 first = 0; first < count; first += 53) {
			std::array<const ATCPUHistoryEntry *, 53> entries {}; const uint32 n = std::min<uint32>(53, count - first);
			AT_PORTABLE_TEST_ASSERT(context, h->ReadHistoryEvents(cursor, entries.data(), first, n) == n);
			for (uint32 j = 0; j < n; ++j) {
				AT_PORTABLE_TEST_ASSERT(context, !std::memcmp(entries[j], &input[first + j], sizeof(ATCPUHistoryEntry)));
				AT_PORTABLE_TEST_ASSERT(context, h->GetEventTime(cursor, first + j) == (first + j) * 3);
			}
		}
		auto main = CPUTraceEvents(f.mContext, 1);
		AT_PORTABLE_TEST_ASSERT(context, main.size() == 1 && main[0].mStart == 0 && main[0].mStop == count * 3);
	}
	return true;
}

bool ATTestAltirraCPUTraceModes(ATPortableTestContext& context) {
	for (bool insns : {false, true}) for (bool basic : {false, true}) {
		CPUTraceProcessorFixture f(insns, basic);
		f.mProcessor.ProcessInsns(100, nullptr, 0, f.mDecoder);
		AT_PORTABLE_TEST_ASSERT(context, f.mContext.mpCollection->GetGroupCount() == (insns ? 2 : 1));
		AT_PORTABLE_TEST_ASSERT(context, f.mContext.mpCollection->GetGroup(0)->GetChannelCount() == (basic ? 8 : 7));
		std::vector<ATCPUHistoryEntry> e {CPUTraceEntry(100), CPUTraceEntry(120)};
		f.Process(e, 0, 1); f.mProcessor.Shutdown(); f.mProcessor.Shutdown();
		auto main = CPUTraceEvents(f.mContext, 1);
		AT_PORTABLE_TEST_ASSERT(context, main.size() == 1 && main[0].mStop == 20);
		AT_PORTABLE_TEST_ASSERT(context, bool(CPUTraceHistory(f.mContext)) == insns);
		// A new collection must not inherit BASIC, stack or history pointers.
		ATTraceContext second {}; second.mBaseTime = 100; second.mBaseTickScale = 1; second.mpCollection = new ATTraceCollection;
		f.mProcessor.Init(0xFD, kATDebugDisasmMode_6502, 1, &second, false, false, nullptr, f.mDecoder, false);
		f.Process(e, 0, 1); f.mProcessor.Shutdown();
		AT_PORTABLE_TEST_ASSERT(context, CPUTraceEvents(second, 1).size() == 1 && main.size() == 1);
	}
	return true;
}

bool ATTestAltirraCPUTraceContexts(ATPortableTestContext& context) {
	// IRQ, CIO and SIO return to the suspended main context via the stack.
	for (uint32 kind = 0; kind < 3; ++kind) {
		CPUTraceProcessorFixture f(false);
		std::vector<ATCPUHistoryEntry> e {CPUTraceEntry(100, 0x20), CPUTraceEntry(110, 0x4C, 0xFB),
			CPUTraceEntry(120, 0x60, 0xFB), CPUTraceEntry(130), CPUTraceEntry(140)};
		if (kind == 0) { e[0].mOpcode[0] = 0xEA; e[1].mbIRQ = true; e[1].mS = e[2].mS = 0xFA; e[2].mOpcode[0] = 0x40; }
		else e[1].mPC = kind == 1 ? 0xE456 : 0xE459;
		f.Process(e, 0, 2); f.Process(e, 2, 2); f.mProcessor.Shutdown();
		const auto events = CPUTraceEvents(f.mContext, kind == 0 ? 4 : kind == 1 ? 2 : 3);
		AT_PORTABLE_TEST_ASSERT(context, events.size() == 1 && events[0].mStart == 10 && events[0].mStop == 30);
		AT_PORTABLE_TEST_ASSERT(context, events[0].mName == (kind == 0 ? L"IRQ" : kind == 1 ? L"CIO" : L"SIO"));
		AT_PORTABLE_TEST_ASSERT(context, CPUTraceEvents(f.mContext, 1).size() == 2);
	}
	for (bool vbi : {false, true}) {
		CPUTraceProcessorFixture f(false);
		const uint32 tick = 100 + (vbi ? 248 : 10) * 114;
		std::vector<ATCPUHistoryEntry> e {CPUTraceEntry(100), CPUTraceEntry(tick, 0xEA, 0xFA),
			CPUTraceEntry(tick + 10, 0x40, 0xFA), CPUTraceEntry(tick + 20), CPUTraceEntry(tick + 30)};
		e[1].mbNMI = true; if (vbi) e[2].mP &= ~AT6502::kFlagI;
		f.Process(e, 0, 4); f.mProcessor.Shutdown(); const auto events = CPUTraceEvents(f.mContext, vbi ? 5 : 6);
		AT_PORTABLE_TEST_ASSERT(context, events.size() == (vbi ? 2 : 1));
		AT_PORTABLE_TEST_ASSERT(context, events[0].mName == (vbi ? L"Imm" : L"DLI"));
		if (vbi) AT_PORTABLE_TEST_ASSERT(context, events[1].mName == L"Def" && events[1].mStop == tick - 100 + 20);
	}
	return true;
}

bool ATTestAltirraCPUTraceIdle(ATPortableTestContext& context) {
	for (uint8 opcode : {uint8(0xA9), uint8(0x4C), uint8(0x8D)}) for (bool wsync : {false, true}) {
		CPUTraceProcessorFixture f(false); std::vector<ATCPUHistoryEntry> e(27);
		for (uint32 i = 0; i < e.size(); ++i) {
			e[i] = CPUTraceEntry(100 + i * 10, opcode);
			e[i].mOpcode[1] = 0x0A; e[i].mOpcode[2] = wsync ? 0xD4 : 0x30;
		}
		e[25].mOpcode[0] = 0xEE; f.Process(e, 0, 13); f.Process(e, 13, 13); f.mProcessor.Shutdown();
		const auto idle = CPUTraceEvents(f.mContext, 0);
		if (opcode != 0x8D || wsync) AT_PORTABLE_TEST_ASSERT(context, idle.size() == 1 && idle[0].mStart == 0 && idle[0].mStop == 250);
		else AT_PORTABLE_TEST_ASSERT(context, idle.empty());
	}
	return true;
}

bool ATTestAltirraCPUTraceBASIC(ATPortableTestContext& context) {
	CPUTraceProcessorFixture f(false, true, 0);
	std::vector<ATCPUHistoryEntry> e(4);
	for (uint32 i = 0; i < 4; ++i) { e[i] = CPUTraceEntry(i * 10, 0xB1); e[i].mOpcode[1] = 0x8A; e[i].mEA = 0x3000; }
	f.Line(0x3000, 10); f.Process(e, 0, 1);
	f.Line(0x3000, 20); f.Process(e, 1, 1); // same statement address, edited line number
	f.Line(0xFFFF, 30); e[2].mEA = 0xFFFF; f.Process(e, 2, 1);
	f.mProcessor.Shutdown(); const auto lines = CPUTraceEvents(f.mContext, 7);
	AT_PORTABLE_TEST_ASSERT(context, lines.size() == 3);
	for (uint32 i = 0; i < 3; ++i) {
		AT_PORTABLE_TEST_ASSERT(context, lines[i].mName == std::to_wstring((i + 1) * 10));
		AT_PORTABLE_TEST_ASSERT(context, lines[i].mStart == i * 10 && lines[i].mStop == (i + 1) * 10);
	}
	return true;
}
