// Portable live-profiler integration tests using the production main CPU.

#include <algorithm>
#include <memory>
#include <vector>

#include <at/attest/portabletest.h>
#include <at/atcpu/history.h>
#include <profiler.h>
#include "maincpuportablefixture.h"

namespace {
	struct LoggingTimestampProvider final : IATCPUTimestampDecoderProvider {
		MainCPUPortableFixture& mFixture;
		std::vector<uint32> mSlowTicks;

		explicit LoggingTimestampProvider(MainCPUPortableFixture& fixture) : mFixture(fixture) {}
		ATCPUTimestampDecoder GetTimestampDecoder() const override {
			const_cast<LoggingTimestampProvider *>(this)->mSlowTicks.push_back(mFixture.mSlowScheduler.GetTick());
			return mFixture.GetTimestampDecoder();
		}
	};

	struct LiveProfilerScope {
		MainCPUPortableFixture& mFixture;
		ATCPUProfiler mProfiler;
		LoggingTimestampProvider mTimestamps;

		explicit LiveProfilerScope(MainCPUPortableFixture& fixture)
			: mFixture(fixture), mTimestamps(fixture) {
			mProfiler.Init(&mFixture.mCPU, &mFixture.mMemory, &mFixture,
				&mFixture.mFastScheduler, &mFixture.mSlowScheduler, &mTimestamps);
			mFixture.mCPU.SetProfiler(&mProfiler);
		}
		~LiveProfilerScope() {
			if (mProfiler.IsRunning())
				mProfiler.End();
			mFixture.mCPU.SetProfiler(nullptr);
		}
	};

	void LoadProfilerLoop(MainCPUPortableFixture& f) {
		// Main loop calls a real subroutine; the subroutine changes A and returns.
		f.Load(0x2000, { 0xE8, 0x20, 0x10, 0x20, 0x4C, 0x00, 0x20 }); // INX; JSR $2010; JMP $2000
		f.Load(0x2010, { 0xA9, 0x2A, 0x60 });                    // LDA #$2A; RTS
	}

	bool TestAllModesAndFrames(ATPortableTestContext& context) {
		static constexpr ATProfileMode kModes[] = {
			kATProfileMode_Insns,
			kATProfileMode_Functions,
			kATProfileMode_CallGraph,
			kATProfileMode_BasicBlock,
			kATProfileMode_BasicLines,
		};

		for (ATProfileMode mode : kModes) {
			auto fixture = std::make_unique<MainCPUPortableFixture>();
			fixture->Prepare(kATCPUMode_6502, 0x2000);
			LoadProfilerLoop(*fixture);
			fixture->mCPU.SetHistoryEnabled(true);

			if (mode == kATProfileMode_BasicLines) {
				fixture->mRAM[0x008A] = 0x00;
				fixture->mRAM[0x008B] = 0x30;
				fixture->mRAM[0x3000] = 0x34;
				fixture->mRAM[0x3001] = 0x12;
			}

			LiveProfilerScope scope(*fixture);
			scope.mProfiler.Start(mode, kATProfileCounterMode_None, kATProfileCounterMode_None);
			AT_PORTABLE_TEST_ASSERT(context, fixture->RunCycles(4800));
			AT_PORTABLE_TEST_ASSERT(context, scope.mProfiler.IsRunning());
			scope.mProfiler.BeginFrame();
			AT_PORTABLE_TEST_ASSERT(context, fixture->RunCycles(4800));
			scope.mProfiler.EndFrame();
			AT_PORTABLE_TEST_ASSERT(context, fixture->RunCycles(2400));
			scope.mProfiler.End();
			AT_PORTABLE_TEST_ASSERT(context, !scope.mProfiler.IsRunning());

			ATProfileSession session;
			scope.mProfiler.GetSession(session);
			AT_PORTABLE_TEST_ASSERT(context, session.mProfileMode == mode);
			// BeginFrame/EndFrame each close a frame; End retains the final partial frame.
			AT_PORTABLE_TEST_ASSERT(context, session.mpFrames.size() == 3);
			AT_PORTABLE_TEST_ASSERT(context, session.mpFrames[0]->mTotalCycles == 4800);
			AT_PORTABLE_TEST_ASSERT(context, session.mpFrames[1]->mTotalCycles == 4800);
			AT_PORTABLE_TEST_ASSERT(context, session.mpFrames[2]->mTotalCycles == 2400);
			AT_PORTABLE_TEST_ASSERT(context, session.mpFrames[0] != nullptr);
			AT_PORTABLE_TEST_ASSERT(context, session.mpFrames[0]->mTotalInsns > 0);
			AT_PORTABLE_TEST_ASSERT(context, session.mpFrames[0]->mTotalCycles > 0);
			if (mode == kATProfileMode_CallGraph)
				AT_PORTABLE_TEST_ASSERT(context, !session.mpFrames[0]->mCallGraphRecords.empty());
			else
				AT_PORTABLE_TEST_ASSERT(context, !session.mpFrames[0]->mRecords.empty());
			if (mode == kATProfileMode_BasicLines) {
				const auto& records = session.mpFrames[0]->mRecords;
				AT_PORTABLE_TEST_ASSERT(context, std::any_of(records.begin(), records.end(), [](const ATProfileRecord& r) {
					return r.mAddress == 0x1234 && r.mInsns > 0;
				}));
			}
		}
		return true;
	}

	bool TestBasicLineCadence(ATPortableTestContext& context) {
		auto fixture = std::make_unique<MainCPUPortableFixture>();
		fixture->Prepare(kATCPUMode_6502, 0x2000);
		LoadProfilerLoop(*fixture);
		fixture->mCPU.SetHistoryEnabled(true);
		fixture->mRAM[0x008A] = 0x00;
		fixture->mRAM[0x008B] = 0x30;
		fixture->mRAM[0x3000] = 0x34;
		fixture->mRAM[0x3001] = 0x12;
		LiveProfilerScope scope(*fixture);
		const uint32 startSlow = fixture->mSlowScheduler.GetTick();
		scope.mProfiler.Start(kATProfileMode_BasicLines, kATProfileCounterMode_None, kATProfileCounterMode_None);

		// 800 real CPU ticks advance the real slow scheduler by seven ticks.
		AT_PORTABLE_TEST_ASSERT(context, fixture->RunCycles(800));
		AT_PORTABLE_TEST_ASSERT(context, fixture->mSlowScheduler.GetTick() - startSlow == 7);
		AT_PORTABLE_TEST_ASSERT(context, scope.mTimestamps.mSlowTicks.size() >= 4);
		// The first entry is Start/OpenFrame at tick 0. The next entries come from
		// scheduled samples at ticks 2, 4 and 6.
		AT_PORTABLE_TEST_ASSERT(context, scope.mTimestamps.mSlowTicks[0] - startSlow == 0);
		AT_PORTABLE_TEST_ASSERT(context, scope.mTimestamps.mSlowTicks[1] - startSlow == 2);
		AT_PORTABLE_TEST_ASSERT(context, scope.mTimestamps.mSlowTicks[2] - startSlow == 4);
		AT_PORTABLE_TEST_ASSERT(context, scope.mTimestamps.mSlowTicks[3] - startSlow == 6);
		scope.mProfiler.End();
		return true;
	}

	bool TestRepeatedStartAndEnd(ATPortableTestContext& context) {
		auto fixture = std::make_unique<MainCPUPortableFixture>();
		fixture->Prepare(kATCPUMode_6502, 0x2000);
		LoadProfilerLoop(*fixture);
		fixture->mCPU.SetHistoryEnabled(true);
		LiveProfilerScope scope(*fixture);
		scope.mProfiler.Start(kATProfileMode_Insns, kATProfileCounterMode_None, kATProfileCounterMode_None);
		AT_PORTABLE_TEST_ASSERT(context, fixture->RunCycles(240));
		scope.mProfiler.Start(kATProfileMode_Insns, kATProfileCounterMode_None, kATProfileCounterMode_None);
		AT_PORTABLE_TEST_ASSERT(context, fixture->RunCycles(240));
		scope.mProfiler.End();
		AT_PORTABLE_TEST_ASSERT(context, !scope.mProfiler.IsRunning());
		scope.mProfiler.End();
		const size_t updatesAfterEnd = scope.mTimestamps.mSlowTicks.size();
		AT_PORTABLE_TEST_ASSERT(context, fixture->RunCycles(8000));
		AT_PORTABLE_TEST_ASSERT(context, scope.mTimestamps.mSlowTicks.size() == updatesAfterEnd);
		return true;
	}

	bool TestDestroyWhileScheduled(ATPortableTestContext& context) {
		auto fixture = std::make_unique<MainCPUPortableFixture>();
		fixture->Prepare(kATCPUMode_6502, 0x2000);
		LoadProfilerLoop(*fixture);
		fixture->mCPU.SetHistoryEnabled(true);
		{
			ATCPUProfiler profiler;
			profiler.Init(&fixture->mCPU, &fixture->mMemory, fixture.get(),
				&fixture->mFastScheduler, &fixture->mSlowScheduler, fixture.get());
			fixture->mCPU.SetProfiler(&profiler);
			profiler.Start(kATProfileMode_Insns, kATProfileCounterMode_None, kATProfileCounterMode_None);
			AT_PORTABLE_TEST_ASSERT(context, fixture->RunCycles(120));
		}
		fixture->mCPU.SetProfiler(nullptr);
		// Cross several old callback deadlines after the profiler has been destroyed.
		AT_PORTABLE_TEST_ASSERT(context, fixture->RunCycles(6000));
		return true;
	}
}

bool ATTestAltirraCPULiveProfiler(ATPortableTestContext& context) {
	return TestAllModesAndFrames(context)
		&& TestBasicLineCadence(context)
		&& TestRepeatedStartAndEnd(context)
		&& TestDestroyWhileScheduled(context);
}
