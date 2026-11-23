// Production trace processor, history storage, memory bus and CPU clocks.
#ifndef f_ATTEST_CPUTRACERPORTABLEFIXTURE_H
#define f_ATTEST_CPUTRACERPORTABLEFIXTURE_H
#include <algorithm>
#include <array>
#include <cstring>
#include <memory>
#include <string>
#include <vector>
#include <at/attest/portabletest.h>
#include <cputracer.h>
#include <trace.h>
#include <tracecpu.h>
#include "maincpuportablefixture.h"

struct CPUTraceRecord {
	double mStart, mStop;
	std::wstring mName;
	uint32 mColor;
};
inline std::vector<CPUTraceRecord> CPUTraceEvents(ATTraceContext& c, size_t channel) {
	auto *ch = c.mpCollection->GetGroup(0)->GetChannel(channel);
	ch->StartIteration(0, 1e10, 0); ATTraceEvent e {}; std::vector<CPUTraceRecord> result;
	while (ch->GetNextEvent(e)) result.push_back({e.mEventStart, e.mEventStop, e.mpName, e.mBgColor});
	return result;
}
inline ATTraceChannelCPUHistory *CPUTraceHistory(ATTraceContext& c) {
	auto *group = c.mpCollection->GetGroupByType(kATTraceGroupType_CPUHistory);
	return group ? static_cast<ATTraceChannelCPUHistory *>(group->GetChannel(0)->AsInterface(ATTraceChannelCPUHistory::kTypeID)) : nullptr;
}
inline ATCPUHistoryEntry CPUTraceEntry(uint64 tick, uint8 opcode = 0xEA, uint8 s = 0xFD) {
	ATCPUHistoryEntry e {}; e.mCycle = e.mUnhaltedCycle = static_cast<uint32>(tick);
	e.mPC = 0x2000; e.mOpcode[0] = opcode; e.mS = s; e.mP = 0x24; e.mbEmulation = true;
	return e;
}
struct CPUTraceProcessorFixture {
	CPUTraceProcessorFixture(bool insns, bool basic = false, uint64 base = 100, bool async = false)
		: mMachine(std::make_unique<MainCPUPortableFixture>()) {
		mContext.mBaseTime = base; mContext.mBaseTickScale = 1;
		mContext.mpCollection = new ATTraceCollection;
		mDecoder = {static_cast<uint32>(base), 0, 262 * 114};
		mProcessor.Init(0xFD, kATDebugDisasmMode_6502, 1, &mContext, insns, basic, &mMachine->mMemory, mDecoder, async);
	}
	void Process(const std::vector<ATCPUHistoryEntry>& entries, size_t first, size_t count) {
		std::vector<const ATCPUHistoryEntry *> pointers(count + 1);
		for (size_t i = 0; i <= count; ++i) pointers[i] = &entries[first + i];
		const uint64 end = mContext.mBaseTime + static_cast<uint32>(entries[first + count].mCycle - static_cast<uint32>(mContext.mBaseTime));
		mProcessor.ProcessInsns(end, pointers.data(), count, mDecoder);
	}
	void Line(uint16 address, uint16 line) {
		mMachine->mMemory.WriteByte(address, static_cast<uint8>(line));
		mMachine->mMemory.WriteByte(static_cast<uint16>(address + 1), static_cast<uint8>(line >> 8));
	}
	std::unique_ptr<MainCPUPortableFixture> mMachine;
	ATTraceContext mContext {};
	ATCPUTimestampDecoder mDecoder;
	ATCPUTraceProcessor mProcessor;
};
#endif
