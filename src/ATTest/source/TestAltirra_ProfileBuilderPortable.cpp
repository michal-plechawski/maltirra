// Portable tests for real profile construction, counters, call graphs, and merges.

#include <algorithm>
#include <array>
#include <memory>

#include <at/attest/portabletest.h>
#include <at/atcpu/history.h>
#include <cpuflags.h>
#include <profiler.h>

namespace {
	ATCPUHistoryEntry History(uint32 cycle, uint32 unhalted, uint16 pc, uint8 sp, uint8 opcode = 0xEA) {
		ATCPUHistoryEntry h {};
		h.mCycle = cycle;
		h.mUnhaltedCycle = unhalted;
		h.mPC = pc;
		h.mS = sp;
		h.mOpcode[0] = opcode;
		h.mOpcode[1] = 0;
		return h;
	}

	const ATProfileRecord *FindRecord(const ATProfileFrame& frame, uint32 address, uint32 context = kATProfileContext_Main) {
		for(const auto& record : frame.mRecords)
			if (record.mAddress == address && record.mContext == context)
				return &record;
		return nullptr;
	}

	const ATProfileRecord *FindBlock(const ATProfileFrame& frame, uint32 address) {
		for(const auto& record : frame.mBlockRecords)
			if (record.mAddress == address)
				return &record;
		return nullptr;
	}

	void Feed(ATCPUProfileBuilder& builder, const ATCPUTimestampDecoder& decoder,
		const ATCPUHistoryEntry *const *entries, uint32 n, bool global = false) {
		builder.Update(decoder, entries, n, global);
	}

	bool TestBuilderModes(ATPortableTestContext& context) {
		ATCPUTimestampDecoder decoder {};
		decoder.mCyclesPerFrame = 262 * 114;
		std::array<ATCPUHistoryEntry, 3> insns {
			History(0, 0, 0x1000, 0xFF),
			History(3, 2, 0x1000, 0xFF),
			History(5, 5, 0x1002, 0xFF)
		};
		insns[0].mP = AT6502::kFlagI;
		const ATCPUHistoryEntry *insnPtrs[] { &insns[0], &insns[1], &insns[2] };
		ATCPUProfileBuilder instructionBuilder;
		instructionBuilder.Init(kATProfileMode_Insns, kATProfileCounterMode_None, kATProfileCounterMode_None);
		instructionBuilder.OpenFrame(0, 0, decoder);
		Feed(instructionBuilder, decoder, insnPtrs, 2);
		instructionBuilder.CloseFrame(5, 5, true);
		instructionBuilder.Finalize();
		ATProfileSession instructions;
		instructionBuilder.TakeSession(instructions);
		AT_PORTABLE_TEST_ASSERT(context, instructions.mpFrames.size() == 1);
		const auto& instructionFrame = *instructions.mpFrames[0];
		const auto *interruptInstruction = FindRecord(instructionFrame, 0x1000, kATProfileContext_Interrupt);
		const auto *mainInstruction = FindRecord(instructionFrame, 0x1000, kATProfileContext_Main);
		AT_PORTABLE_TEST_ASSERT(context, interruptInstruction && mainInstruction);
		AT_PORTABLE_TEST_ASSERT(context, interruptInstruction->mInsns == 1 && interruptInstruction->mCycles == 3 && interruptInstruction->mUnhaltedCycles == 2);
		AT_PORTABLE_TEST_ASSERT(context, mainInstruction->mInsns == 1 && mainInstruction->mCycles == 2 && mainInstruction->mUnhaltedCycles == 3);
		AT_PORTABLE_TEST_ASSERT(context, instructionFrame.mTotalInsns == 2 && instructionFrame.mTotalCycles == 5 && instructionFrame.mTotalUnhaltedCycles == 5);

		std::array<ATCPUHistoryEntry, 5> calls {
			History(0, 0, 0x1000, 0xFF),
			History(3, 3, 0x1001, 0xFF, 0x20),
			History(9, 8, 0x2000, 0xFD),
			History(15, 14, 0x2001, 0xFD, 0x60),
			History(21, 18, 0x1003, 0xFF)
		};
		const ATCPUHistoryEntry *callPtrs[] { &calls[0], &calls[1], &calls[2], &calls[3], &calls[4] };
		ATCPUProfileBuilder functionBuilder;
		functionBuilder.Init(kATProfileMode_Functions, kATProfileCounterMode_None, kATProfileCounterMode_None);
		functionBuilder.SetS(0xFF);
		functionBuilder.OpenFrame(0, 0, decoder);
		Feed(functionBuilder, decoder, callPtrs, 4);
		functionBuilder.CloseFrame(21, 18, true);
		functionBuilder.Finalize();
		ATProfileSession functions;
		functionBuilder.TakeSession(functions);
		const auto *subroutine = FindBlock(*functions.mpFrames[0], 0x2000);
		AT_PORTABLE_TEST_ASSERT(context, subroutine != nullptr);
		AT_PORTABLE_TEST_ASSERT(context, subroutine->mCalls == 1 && subroutine->mInsns == 2 && subroutine->mCycles == 12 && subroutine->mUnhaltedCycles == 10);
		AT_PORTABLE_TEST_ASSERT(context, FindRecord(*functions.mpFrames[0], 0x1001) != nullptr);

		ATCPUProfileBuilder blockBuilder;
		blockBuilder.Init(kATProfileMode_BasicBlock, kATProfileCounterMode_None, kATProfileCounterMode_None);
		blockBuilder.SetS(0xFF);
		blockBuilder.OpenFrame(0, 0, decoder);
		Feed(blockBuilder, decoder, callPtrs, 4);
		blockBuilder.CloseFrame(21, 18, true);
		blockBuilder.Finalize();
		ATProfileSession blocks;
		blockBuilder.TakeSession(blocks);
		const auto *callBlock = FindBlock(*blocks.mpFrames[0], 0x2000);
		AT_PORTABLE_TEST_ASSERT(context, callBlock && callBlock->mCalls == 1 && callBlock->mInsns == 2);
		AT_PORTABLE_TEST_ASSERT(context, FindRecord(*blocks.mpFrames[0], 0x2000) != nullptr);

		ATCPUProfileBuilder lineBuilder;
		lineBuilder.Init(kATProfileMode_BasicLines, kATProfileCounterMode_None, kATProfileCounterMode_None);
		lineBuilder.OpenFrame(0, 0, decoder);
		lineBuilder.UpdateBasicLines(decoder, 42, insnPtrs, 2);
		lineBuilder.CloseFrame(5, 5, true);
		lineBuilder.Finalize();
		ATProfileSession lines;
		lineBuilder.TakeSession(lines);
		const auto *line = FindRecord(*lines.mpFrames[0], 42);
		AT_PORTABLE_TEST_ASSERT(context, line && line->mInsns == 2 && line->mCycles == 5 && line->mUnhaltedCycles == 5);
		AT_PORTABLE_TEST_ASSERT(context, lines.mpFrames[0]->mBlockRecords.empty());
		return true;
	}

	bool TestCountersAndAddressing(ATPortableTestContext& context) {
		ATCPUTimestampDecoder decoder {};
		decoder.mCyclesPerFrame = 262 * 114;
		std::array<ATCPUHistoryEntry, 5> h {
			History(10, 10, 0x10FD, 0xFF, 0xD0), // BNE crosses $10FF->$1101 and is taken.
			History(13, 12, 0x2000, 0xFF, 0xD0), // Z set: BNE is not taken.
			History(17, 15, 0x3000, 0xFF, 0x18), // CLC while carry is clear.
			History(20, 19, 0x4000, 0xFF, 0xA9), // LDA #$44 already in A.
			History(22, 21, 0x4002, 0xFF)       // n+1 timestamp sentinel, not a sample.
		};
		h[0].mP = 0;
		h[0].mOpcode[1] = 0x02;
		h[1].mP = AT6502::kFlagZ;
		h[2].mP = 0;
		h[3].mA = 0x44;
		h[3].mOpcode[1] = 0x44;
		const ATCPUHistoryEntry *ptrs[] { &h[0], &h[1], &h[2], &h[3], &h[4] };
		ATCPUProfileBuilder branches;
		branches.Init(kATProfileMode_Insns, kATProfileCounterMode_BranchTaken, kATProfileCounterMode_PageCrossing);
		branches.OpenFrame(10, 10, decoder);
		Feed(branches, decoder, ptrs, 4);
		branches.CloseFrame(22, 21, true);
		branches.Finalize();
		ATProfileSession branchSession;
		branches.TakeSession(branchSession);
		const auto *taken = FindRecord(*branchSession.mpFrames[0], 0x10FD);
		const auto *notTaken = FindRecord(*branchSession.mpFrames[0], 0x2000);
		AT_PORTABLE_TEST_ASSERT(context, taken && taken->mCounters[0] == 1 && taken->mCounters[1] == 1);
		AT_PORTABLE_TEST_ASSERT(context, notTaken && notTaken->mCounters[0] == 0 && notTaken->mCounters[1] == 0);

		ATCPUProfileBuilder redundant;
		redundant.Init(kATProfileMode_Insns, kATProfileCounterMode_BranchNotTaken, kATProfileCounterMode_RedundantOp);
		redundant.OpenFrame(10, 10, decoder);
		Feed(redundant, decoder, ptrs, 4);
		redundant.CloseFrame(22, 21, true);
		redundant.Finalize();
		ATProfileSession redundantSession;
		redundant.TakeSession(redundantSession);
		const auto *untaken = FindRecord(*redundantSession.mpFrames[0], 0x2000);
		const auto *clc = FindRecord(*redundantSession.mpFrames[0], 0x3000);
		const auto *lda = FindRecord(*redundantSession.mpFrames[0], 0x4000);
		AT_PORTABLE_TEST_ASSERT(context, untaken && untaken->mCounters[0] == 1);
		AT_PORTABLE_TEST_ASSERT(context, clc && clc->mCounters[1] == 1);
		AT_PORTABLE_TEST_ASSERT(context, lda && lda->mCounters[1] == 1);

		// The history API requires an overlapping n+1 entry array. Batching at
		// the overlap must produce identical deltas, while global addresses add
		// the per-entry base only when both switches are enabled.
		std::array<ATCPUHistoryEntry, 3> batch {
			History(0, 0, 0x2345, 0xFF),
			History(4, 3, 0x2345, 0xFF),
			History(9, 8, 0x2345, 0xFF)
		};
		batch[0].mK = 2;
		batch[1].mK = 2;
		batch[2].mK = 2;
		batch[0].mGlobalPCBase = 0x100000;
		batch[1].mGlobalPCBase = 0x100000;
		batch[2].mGlobalPCBase = 0x100000;
		const ATCPUHistoryEntry *batchPtrs[] { &batch[0], &batch[1], &batch[2] };
		ATCPUProfileBuilder batched;
		batched.Init(kATProfileMode_Insns, kATProfileCounterMode_None, kATProfileCounterMode_None);
		batched.SetGlobalAddressesEnabled(true);
		batched.OpenFrame(0, 0, decoder);
		Feed(batched, decoder, batchPtrs, 1, true);
		Feed(batched, decoder, batchPtrs + 1, 1, true);
		batched.CloseFrame(9, 8, true);
		batched.Finalize();
		ATProfileSession batchSession;
		batched.TakeSession(batchSession);
		const auto *global = FindRecord(*batchSession.mpFrames[0], 0x122345);
		AT_PORTABLE_TEST_ASSERT(context, global && global->mInsns == 2 && global->mCycles == 9 && global->mUnhaltedCycles == 8);

		ATCPUProfileBuilder disabledGlobal;
		disabledGlobal.Init(kATProfileMode_Insns, kATProfileCounterMode_None, kATProfileCounterMode_None);
		disabledGlobal.SetGlobalAddressesEnabled(false);
		disabledGlobal.OpenFrame(0, 0, decoder);
		Feed(disabledGlobal, decoder, batchPtrs, 2, true);
		disabledGlobal.CloseFrame(9, 8, true);
		disabledGlobal.Finalize();
		ATProfileSession localSession;
		disabledGlobal.TakeSession(localSession);
		AT_PORTABLE_TEST_ASSERT(context, FindRecord(*localSession.mpFrames[0], 0x22345) != nullptr);
		return true;
	}

	bool TestFramesAndMerges(ATPortableTestContext& context) {
		ATCPUTimestampDecoder decoder {};
		decoder.mCyclesPerFrame = 262 * 114;
		std::array<ATCPUHistoryEntry, 5> h {
			History(0, 0, 0x1001, 0xFF),
			History(3, 2, 0x2000, 0xFF),
			History(5, 4, 0x1001, 0xFF),
			History(8, 7, 0x2FFF, 0xFF),
			History(10, 9, 0x7777, 0xFF)
		};
		const ATCPUHistoryEntry *ptrs[] { &h[0], &h[1], &h[2], &h[3], &h[4] };
		ATCPUProfileBuilder builder;
		builder.Init(kATProfileMode_Insns, kATProfileCounterMode_None, kATProfileCounterMode_None);
		builder.SetBoundaryRule(kATProfileBoundaryRule_PCAddress, 0x2000, 0x2FFF);
		builder.OpenFrame(0, 0, decoder);
		Feed(builder, decoder, ptrs, 4);
		builder.CloseFrame(10, 9, false); // Discard the final disabled interval.
		builder.Finalize();
		ATProfileSession session;
		builder.TakeSession(session);
		AT_PORTABLE_TEST_ASSERT(context, session.mpFrames.size() == 2);
		AT_PORTABLE_TEST_ASSERT(context, session.mpFrames[0]->mTotalCycles == 3 && session.mpFrames[0]->mTotalUnhaltedCycles == 2 && session.mpFrames[0]->mTotalInsns == 1);
		AT_PORTABLE_TEST_ASSERT(context, session.mpFrames[1]->mTotalCycles == 5 && session.mpFrames[1]->mTotalUnhaltedCycles == 5 && session.mpFrames[1]->mTotalInsns == 2);
		AT_PORTABLE_TEST_ASSERT(context, FindRecord(*session.mpFrames[0], 0x1001) != nullptr);
		AT_PORTABLE_TEST_ASSERT(context, FindRecord(*session.mpFrames[1], 0x1001) != nullptr);
		vdrefptr<ATProfileMergedFrame> merged;
		ATProfileMergeFrames(session, 0, 2, ~merged);
		AT_PORTABLE_TEST_ASSERT(context, !!merged);
		AT_PORTABLE_TEST_ASSERT(context, merged->mTotalCycles == 8 && merged->mTotalUnhaltedCycles == 7 && merged->mTotalInsns == 3);
		const auto *combined = FindRecord(*merged, 0x1001);
		AT_PORTABLE_TEST_ASSERT(context, combined && combined->mInsns == 2 && combined->mCycles == 6 && combined->mUnhaltedCycles == 5);

		// Main and interrupt samples at the same PC are distinct profile rows;
		// merging frames must preserve that context split.
		ATProfileSession contextSession;
		contextSession.mProfileMode = kATProfileMode_Insns;
		for(uint32 i = 0; i < 2; ++i) {
			auto *frame = new ATProfileFrame;
			frame->mRecords.push_back(ATProfileRecord {});
			frame->mRecords.back().mAddress = 0x4567;
			frame->mRecords.back().mContext = i ? kATProfileContext_Interrupt : kATProfileContext_Main;
			frame->mRecords.back().mInsns = 1;
			frame->mRecords.back().mCycles = 2;
			frame->mRecords.back().mUnhaltedCycles = 2;
			frame->mTotalCycles = 2;
			frame->mTotalUnhaltedCycles = 2;
			frame->mTotalInsns = 1;
			contextSession.mpFrames.push_back(frame);
		}
		vdrefptr<ATProfileMergedFrame> contextMerge;
		ATProfileMergeFrames(contextSession, 0, 2, ~contextMerge);
		AT_PORTABLE_TEST_ASSERT(context, contextMerge->mRecords.size() == 2);
		AT_PORTABLE_TEST_ASSERT(context, FindRecord(*contextMerge, 0x4567, kATProfileContext_Main) != nullptr);
		AT_PORTABLE_TEST_ASSERT(context, FindRecord(*contextMerge, 0x4567, kATProfileContext_Interrupt) != nullptr);

		ATProfileCallGraphContext contexts[] {
			{ 0, 0 }, { 0, 0x4000000 }, { 0, 0x6000000 }, { 0, 0x8000000 },
			{ 0, 0x1000 }, { 4, 0x2000 }, { 4, 0x3000 }
		};
		ATProfileCallGraphRecord exclusive[] {
			{ 1, 10, 8, 0 }, { 0, 0, 0, 0 }, { 0, 0, 0, 0 }, { 0, 0, 0, 0 },
			{ 2, 3, 2, 1 }, { 3, 5, 4, 1 }, { 4, 7, 6, 1 }
		};
		ATProfileCallGraphInclusiveRecord inclusive[7] {};
		ATProfileComputeInclusiveStats(inclusive, exclusive, contexts, 7);
		AT_PORTABLE_TEST_ASSERT(context, inclusive[6].mInclusiveCycles == 7 && inclusive[6].mInclusiveUnhaltedCycles == 6 && inclusive[6].mInclusiveInsns == 4);
		AT_PORTABLE_TEST_ASSERT(context, inclusive[5].mInclusiveCycles == 5 && inclusive[5].mInclusiveInsns == 3);
		AT_PORTABLE_TEST_ASSERT(context, inclusive[4].mInclusiveCycles == 15 && inclusive[4].mInclusiveUnhaltedCycles == 12 && inclusive[4].mInclusiveInsns == 9);
		AT_PORTABLE_TEST_ASSERT(context, inclusive[0].mInclusiveCycles == 25 && inclusive[0].mInclusiveUnhaltedCycles == 20 && inclusive[0].mInclusiveInsns == 10);

		// Init is a fresh profiling run: old frames and counter metadata must not
		// leak into a subsequent session when the prior one was not taken.
		ATCPUProfileBuilder reused;
		reused.Init(kATProfileMode_Insns, kATProfileCounterMode_BranchTaken, kATProfileCounterMode_None);
		reused.OpenFrame(0, 0, decoder);
		reused.CloseFrame(1, 1, true);
		reused.Init(kATProfileMode_Insns, kATProfileCounterMode_PageCrossing, kATProfileCounterMode_None);
		reused.OpenFrame(1, 1, decoder);
		reused.CloseFrame(2, 2, true);
		reused.Finalize();
		ATProfileSession fresh;
		reused.TakeSession(fresh);
		AT_PORTABLE_TEST_ASSERT(context, fresh.mpFrames.size() == 1);
		AT_PORTABLE_TEST_ASSERT(context, fresh.mCounterModes.size() == 1 && fresh.mCounterModes[0] == kATProfileCounterMode_PageCrossing);

		ATProfileFrame *const ownedFrame = fresh.mpFrames[0];
		ATProfileSession moved(std::move(fresh));
		AT_PORTABLE_TEST_ASSERT(context, fresh.mpFrames.empty() && moved.mpFrames.size() == 1);
		auto *self = &moved;
		moved = std::move(*self);
		AT_PORTABLE_TEST_ASSERT(context, moved.mpFrames.size() == 1 && moved.mpFrames[0] == ownedFrame);
		return true;
	}

	bool TestAutomaticBoundaries(ATPortableTestContext& context) {
		ATCPUTimestampDecoder decoder {};
		decoder.mCyclesPerFrame = 262 * 114;
		std::array<ATCPUHistoryEntry, 3> vblank {
			History(248 * 114 - 2, 10, 0x1000, 0xFF),
			History(248 * 114, 12, 0x1001, 0xFF),
			History(248 * 114 + 3, 15, 0x1002, 0xFF)
		};
		const ATCPUHistoryEntry *vptrs[] { &vblank[0], &vblank[1], &vblank[2] };
		ATCPUProfileBuilder builder;
		builder.Init(kATProfileMode_Insns, kATProfileCounterMode_None, kATProfileCounterMode_None);
		builder.SetBoundaryRule(kATProfileBoundaryRule_VBlank, 0, 0);
		builder.OpenFrame(vblank[0].mCycle, 10, decoder);
		builder.Update(decoder, vptrs, 2, false);
		builder.CloseFrame(vblank[2].mCycle, 15, true);
		builder.Finalize();
		ATProfileSession frames;
		builder.TakeSession(frames);
		AT_PORTABLE_TEST_ASSERT(context, frames.mpFrames.size() == 2);
		AT_PORTABLE_TEST_ASSERT(context, frames.mpFrames[0]->mTotalCycles == 2 && frames.mpFrames[0]->mTotalInsns == 1);
		AT_PORTABLE_TEST_ASSERT(context, frames.mpFrames[1]->mTotalCycles == 3 && frames.mpFrames[1]->mTotalInsns == 1);

		std::array<ATCPUHistoryEntry, 5> function {
			History(0, 0, 0x1000, 0xFF, 0x20),
			History(6, 6, 0x2000, 0xFD),
			History(8, 8, 0x2001, 0xFD, 0x60),
			History(14, 14, 0x1003, 0xFF),
			History(16, 16, 0x1004, 0xFF)
		};
		const ATCPUHistoryEntry *fptrs[] { &function[0], &function[1], &function[2], &function[3], &function[4] };
		builder.Init(kATProfileMode_Insns, kATProfileCounterMode_None, kATProfileCounterMode_None);
		builder.SetBoundaryRule(kATProfileBoundaryRule_PCAddressFunction, 0x2000, 0);
		builder.OpenFrame(0, 0, decoder);
		builder.Update(decoder, fptrs, 4, false);
		builder.CloseFrame(16, 16, false);
		builder.Finalize();
		builder.TakeSession(frames);
		AT_PORTABLE_TEST_ASSERT(context, frames.mpFrames.size() == 2);
		AT_PORTABLE_TEST_ASSERT(context, frames.mpFrames[0]->mTotalCycles == 6 && frames.mpFrames[0]->mTotalInsns == 1);
		AT_PORTABLE_TEST_ASSERT(context, frames.mpFrames[1]->mTotalCycles == 8 && frames.mpFrames[1]->mTotalInsns == 2);
		AT_PORTABLE_TEST_ASSERT(context, FindRecord(*frames.mpFrames[1], 0x2000) && FindRecord(*frames.mpFrames[1], 0x2001));
		return true;
	}

	bool TestReusedRecordContexts(ATPortableTestContext& context) {
		ATCPUTimestampDecoder decoder {};
		decoder.mCyclesPerFrame = 262 * 114;
		std::array<ATCPUHistoryEntry, 2> h { History(0, 0, 0x1234, 0xFF), History(3, 3, 0x1235, 0xFF) };
		h[0].mP = AT6502::kFlagI;
		const ATCPUHistoryEntry *ptrs[] { &h[0], &h[1] };
		for (const auto mode : { kATProfileMode_BasicLines, kATProfileMode_CallGraph }) {
			ATCPUProfileBuilder builder;
			builder.Init(kATProfileMode_Insns, kATProfileCounterMode_None, kATProfileCounterMode_None);
			builder.OpenFrame(0, 0, decoder);
			builder.Update(decoder, ptrs, 1, false);
			builder.CloseFrame(3, 3, true);
			builder.Finalize();
			ATProfileSession session;
			builder.TakeSession(session);
			AT_PORTABLE_TEST_ASSERT(context, FindRecord(*session.mpFrames[0], 0x1234, kATProfileContext_Interrupt));
			// Reuse the linear allocator's formerly interrupt-tagged record storage.
			builder.Init(mode, kATProfileCounterMode_None, kATProfileCounterMode_None);
			builder.SetS(0xFF);
			builder.OpenFrame(0, 0, decoder);
			if (mode == kATProfileMode_BasicLines)
				builder.UpdateBasicLines(decoder, 42, ptrs, 1);
			else
				builder.Update(decoder, ptrs, 1, false);
			builder.CloseFrame(3, 3, true);
			builder.Finalize();
			builder.TakeSession(session);
			AT_PORTABLE_TEST_ASSERT(context, session.mpFrames[0]->mRecords.size() == 1);
			AT_PORTABLE_TEST_ASSERT(context, session.mpFrames[0]->mRecords[0].mContext == kATProfileContext_Main);
		}
		return true;
	}

	bool TestCallGraph(ATPortableTestContext& context) {
		ATCPUTimestampDecoder decoder {};
		decoder.mFrameTimestampBase = 0;
		decoder.mCyclesPerFrame = 262 * 114;
		std::array<ATCPUHistoryEntry, 8> h {
			History(0, 0, 0x1000, 0xFF, 0x20),
			History(3, 3, 0x2000, 0xFD),
			History(9, 8, 0x2001, 0xFD, 0x60),
			History(15, 14, 0x1003, 0xFF),
			History(20, 18, 0x3000, 0xFD),
			History(26, 24, 0x3001, 0xFD, 0x40),
			History(30, 27, 0x1005, 0xFF),
			History(34, 31, 0x1006, 0xFF)
		};
		h[4].mbIRQ = true;
		const ATCPUHistoryEntry *ptrs[] { &h[0], &h[1], &h[2], &h[3], &h[4], &h[5], &h[6], &h[7] };
		ATCPUProfileBuilder builder;
		builder.Init(kATProfileMode_CallGraph, kATProfileCounterMode_None, kATProfileCounterMode_None);
		builder.SetS(0xFF);
		builder.OpenFrame(0, 0, decoder);
		Feed(builder, decoder, ptrs, 7);
		builder.CloseFrame(34, 31, true);
		builder.Finalize();
		ATProfileSession session;
		builder.TakeSession(session);
		AT_PORTABLE_TEST_ASSERT(context, session.mContexts.size() >= 6);
		uint32 callContext = ~UINT32_C(0);
		uint32 irqContext = ~UINT32_C(0);
		for(uint32 i = 4; i < session.mContexts.size(); ++i) {
			if (session.mContexts[i].mAddress == 0x2000)
				callContext = i;
			if (session.mContexts[i].mAddress == 0x3000)
				irqContext = i;
		}
		AT_PORTABLE_TEST_ASSERT(context, callContext != ~UINT32_C(0) && session.mContexts[callContext].mParent == 0);
		AT_PORTABLE_TEST_ASSERT(context, irqContext != ~UINT32_C(0) && session.mContexts[irqContext].mParent == 1);
		const auto& records = session.mpFrames[0]->mCallGraphRecords;
		AT_PORTABLE_TEST_ASSERT(context, records[callContext].mCalls == 1 && records[callContext].mInsns == 2 && records[callContext].mCycles == 12);
		AT_PORTABLE_TEST_ASSERT(context, records[irqContext].mCalls == 1 && records[irqContext].mInsns == 2 && records[irqContext].mCycles == 10);

		vdrefptr<ATProfileMergedFrame> merged;
		ATProfileMergeFrames(session, 0, 1, ~merged);
		AT_PORTABLE_TEST_ASSERT(context, merged->mCallGraphRecords.size() == records.size());
		AT_PORTABLE_TEST_ASSERT(context, merged->mTotalCycles == 34 && merged->mTotalInsns == 7);
		AT_PORTABLE_TEST_ASSERT(context, merged->mInclusiveRecords[0].mInclusiveCycles == 24);
		AT_PORTABLE_TEST_ASSERT(context, merged->mInclusiveRecords[1].mInclusiveCycles == 10);
		return true;
	}
}

bool ATTestAltirraProfileBuilder(ATPortableTestContext& context) { return TestBuilderModes(context) && TestReusedRecordContexts(context); }
bool ATTestAltirraProfileCounters(ATPortableTestContext& context) { return TestCountersAndAddressing(context); }
bool ATTestAltirraProfileFrames(ATPortableTestContext& context) { return TestFramesAndMerges(context) && TestAutomaticBoundaries(context); }
bool ATTestAltirraProfileCallGraph(ATPortableTestContext& context) { return TestCallGraph(context); }
