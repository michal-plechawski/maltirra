// Portable tests for compressed CPU history trace storage and lookup.

#include <algorithm>
#include <cstring>
#include <vector>

#include <at/attest/portabletest.h>

#include <tracecpu.h>

namespace {
	ATCPUHistoryEntry MakeHistoryEntry(uint64 tick, uint32 index) {
		ATCPUHistoryEntry entry {};
		entry.mCycle = static_cast<uint32>(tick);
		entry.mUnhaltedCycle = static_cast<uint32>(UINT64_C(0xFFFFFF80) + index);
		entry.mEA = 0x00100000U + index * 3;
		entry.mA = static_cast<uint8>(index);
		entry.mX = static_cast<uint8>(index >> 1);
		entry.mY = static_cast<uint8>(index >> 2);
		entry.mS = static_cast<uint8>(0xFF - index);
		entry.mPC = static_cast<uint16>(0x2000 + index * 3);
		entry.mP = static_cast<uint8>(0x30 | (index & 0xCF));
		entry.mbIRQ = (index % 11) == 0;
		entry.mbNMI = (index % 37) == 0;
		entry.mbEmulation = (index & 1) != 0;
		entry.mSubCycle = static_cast<uint8>(index & 0x1F);
		entry.mOpcode[0] = static_cast<uint8>(index * 3);
		entry.mOpcode[1] = static_cast<uint8>(index * 5);
		entry.mOpcode[2] = static_cast<uint8>(index * 7);
		entry.mOpcode[3] = static_cast<uint8>(index * 11);
		entry.mGlobalPCBase = 0x00400000U + (index & 0xFFFF00U);
		entry.mB = static_cast<uint8>(index * 13);
		entry.mK = static_cast<uint8>(index * 17);
		entry.mD = static_cast<uint16>(index * 19);
		return entry;
	}

	bool EntriesEqual(const ATCPUHistoryEntry& left, const ATCPUHistoryEntry& right) {
		return !std::memcmp(&left, &right, sizeof left);
	}

	bool VerifyCPUTrace(ATPortableTestContext& context, bool async, uint32 eventCount) {
		static constexpr uint64 kTickBase = UINT64_C(0xFFFFFF00);
		static constexpr double kTickScale = 0.25;

		vdrefptr channel { new ATTraceChannelCPUHistory(
			kTickBase,
			kTickScale,
			async ? L"Async CPU" : L"CPU",
			kATDebugDisasmMode_6502,
			2,
			nullptr,
			async) };

		AT_PORTABLE_TEST_ASSERT(context, channel->IsEmpty());
		AT_PORTABLE_TEST_ASSERT(context, channel->GetDuration() == 0.0);
		AT_PORTABLE_TEST_ASSERT(context, channel->GetDisasmMode() == kATDebugDisasmMode_6502);
		AT_PORTABLE_TEST_ASSERT(context, channel->GetSubCycles() == 2);
		AT_PORTABLE_TEST_ASSERT(context, channel->GetHistoryBaseCycle() == static_cast<uint32>(kTickBase));

		ATCPUTimestampDecoder timestampDecoder;
		timestampDecoder.mFrameTimestampBase = 123;
		timestampDecoder.mFrameCountBase = 456;
		timestampDecoder.mCyclesPerFrame = 789;
		channel->SetTimestampDecoder(timestampDecoder);
		AT_PORTABLE_TEST_ASSERT(context, channel->GetTimestampDecoder().mFrameCountBase == 456);

		std::vector<ATCPUHistoryEntry> source(eventCount);
		channel->BeginEvents();
		for(uint32 i = 0; i < eventCount; ++i) {
			const uint64 tick = kTickBase + i * 2;
			source[i] = MakeHistoryEntry(tick, i);
			channel->AddEvent(tick, source[i]);

			if (i == 31 || i == 127 || i == 4095) {
				const uint32 probe = i - 17;
				const auto cursor = channel->StartHistoryIteration(probe * 2 * kTickScale, 0);
				const ATCPUHistoryEntry *entry = nullptr;
				AT_PORTABLE_TEST_ASSERT(context, channel->ReadHistoryEvents(cursor, &entry, 0, 1) == 1);
				AT_PORTABLE_TEST_ASSERT(context, entry != nullptr);
				AT_PORTABLE_TEST_ASSERT(context, EntriesEqual(*entry, source[probe]));
			}
		}
		channel->EndEvents();

		AT_PORTABLE_TEST_ASSERT(context, !channel->IsEmpty());
		AT_PORTABLE_TEST_ASSERT(context, channel->GetEventCount() == eventCount);
		AT_PORTABLE_TEST_ASSERT(context, channel->GetTraceSize() > 0 || eventCount < 16384);
		AT_PORTABLE_TEST_ASSERT(context, channel->GetDuration() == (eventCount - 1) * 2 * kTickScale);

		const auto start = channel->StartHistoryIteration(0, 0);
		std::vector<const ATCPUHistoryEntry *> entries(113);
		for(uint32 offset = 0; offset < eventCount; offset += static_cast<uint32>(entries.size())) {
			const uint32 expected = std::min<uint32>(static_cast<uint32>(entries.size()), eventCount - offset);
			AT_PORTABLE_TEST_ASSERT(context, channel->ReadHistoryEvents(start, entries.data(), offset, static_cast<uint32>(entries.size())) == expected);
			for(uint32 i = 0; i < expected; ++i) {
				AT_PORTABLE_TEST_ASSERT(context, entries[i] != nullptr);
				AT_PORTABLE_TEST_ASSERT(context, EntriesEqual(*entries[i], source[offset + i]));
			}
		}

		for(uint32 i = 0; i < 257; ++i) {
			const uint32 index = static_cast<uint32>((static_cast<uint64>(i) * 7919) % eventCount);
			const ATCPUHistoryEntry *entry = nullptr;
			AT_PORTABLE_TEST_ASSERT(context, channel->ReadHistoryEvents(start, &entry, index, 1) == 1);
			AT_PORTABLE_TEST_ASSERT(context, EntriesEqual(*entry, source[index]));
		}

		const uint32 selectedIndex = std::min<uint32>(1000, eventCount - 1);
		const double selectedTime = selectedIndex * 2 * kTickScale;
		const auto selected = channel->StartHistoryIteration(selectedTime, -3);
		AT_PORTABLE_TEST_ASSERT(context, selected.mIterPos == selectedIndex - 3);
		AT_PORTABLE_TEST_ASSERT(context, channel->FindEvent(selected, selectedTime) == 3);
		AT_PORTABLE_TEST_ASSERT(context, channel->GetEventTime(selected, 3) == selectedTime);

		const auto afterEnd = channel->StartHistoryIteration(channel->GetDuration() + 100.0, 0);
		AT_PORTABLE_TEST_ASSERT(context, afterEnd.mIterPos == eventCount);
		AT_PORTABLE_TEST_ASSERT(context, channel->ReadHistoryEvents(afterEnd, nullptr, 0, 1) == 0);
		return true;
	}
}

bool ATTestAltirraTraceCPU(ATPortableTestContext& context) {
	if (!VerifyCPUTrace(context, false, 4096))
		return false;

	// More than 256 64-entry blocks exercises the compressed asynchronous path.
	if (!VerifyCPUTrace(context, true, 20000))
		return false;

	return true;
}
