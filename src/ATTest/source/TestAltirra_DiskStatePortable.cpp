// Disk state, real audio/trace output, and ownership of scheduled callbacks.
#include <cctype>
#include <at/attest/portabletest.h>
#include "diskemulatorportablefixture.h"

bool ATTestAltirraDiskSnapshot(ATPortableTestContext& context) {
	auto f = std::make_unique<DiskEmulatorPortableFixture>(); f->Fill(0, f->Pattern(128));
	AT_PORTABLE_TEST_ASSERT(context, f->Accel(0x52, 0x40, 128));
	vdrefptr<IATObjectState> state; f->mDrive.SaveState(~state); auto json = DiskStateJSON(*state);
	const uint32 remaining = DiskStateNumber(json, "idle_timer");
	AT_PORTABLE_TEST_ASSERT(context, remaining > 0 && remaining <= ATGetDiskProfile(kATDiskEmulationMode_Generic).mCyclesToMotorOff * 2);
	auto decoded = DiskStateDecode(json); auto restored = std::make_unique<DiskEmulatorPortableFixture>();
	restored->mDrive.LoadState(*decoded); restored->mDrive.SaveState(~state);
	AT_PORTABLE_TEST_ASSERT(context, DiskStateNumber(DiskStateJSON(*state), "idle_timer") == remaining);
	// A slot with no disk also has a serializable valid head position.
	auto empty = std::make_unique<DiskEmulatorPortableFixture>(false); empty->mDrive.SaveState(~state);
	AT_PORTABLE_TEST_ASSERT(context, DiskStateNumber(DiskStateJSON(*state), "current_track") == 0);
	decoded = DiskStateDecode(DiskStateJSON(*state)); empty->mDrive.LoadState(*decoded);
	for (const char *key : {"current_track", "idle_timer"}) {
		auto invalid = DiskStateJSON(*state); const size_t pos = invalid.find(std::string("\"") + key + "\"");
		AT_PORTABLE_TEST_ASSERT(context, pos != std::string::npos);
		const size_t start = invalid.find(':', pos) + 1, end = invalid.find_first_of(",}", start);
		invalid.replace(start, end - start, std::string(key) == "current_track" ? "65535" : "1073741824");
		bool rejected = false; try { (void)DiskStateDecode(invalid); } catch (const MyError&) { rejected = true; }
		AT_PORTABLE_TEST_ASSERT(context, rejected);
	}
	const double stoppedPosition = DiskStateFloat(DiskStateJSON(*state), "rotational_pos");
	empty->mBus.Tick(1000); empty->mDrive.SaveState(~state);
	AT_PORTABLE_TEST_ASSERT(context, DiskStateFloat(DiskStateJSON(*state), "rotational_pos") == stoppedPosition);
	// Restore a pending buffered read into a fresh drive, including its SIO queue.
	f = std::make_unique<DiskEmulatorPortableFixture>(); f->Fill(0, f->Pattern(128));
	f->mDrive.SetEmulationMode(kATDiskEmulationMode_Happy1050); f->mBus.ReceiveClock(); f->mBus.Command(0x31, 0x52, 1, 0);
	f->mDrive.SaveState(~state); json = DiskStateJSON(*state);
	const size_t key = json.find("\"active_command_buffered\"");
	AT_PORTABLE_TEST_ASSERT(context, key != std::string::npos);
	size_t value = json.find(':', key) + 1; while (value < json.size() && std::isspace(static_cast<unsigned char>(json[value]))) ++value;
	AT_PORTABLE_TEST_ASSERT(context, json.compare(value, 4, "true") == 0);
	decoded = DiskStateDecode(json); const auto response = f->mBus.Collect(250000);
	const auto data = f->Pattern(128); std::vector<uint8> expected {'A', 'C'};
	expected.insert(expected.end(), data.begin(), data.end()); expected.push_back(ATComputeSIOChecksum(data.data(), data.size()));
	AT_PORTABLE_TEST_ASSERT(context, response == expected);
	restored = std::make_unique<DiskEmulatorPortableFixture>(); restored->Fill(0, f->Pattern(128));
	restored->mDrive.SetEmulationMode(kATDiskEmulationMode_Happy1050); restored->mBus.ReceiveClock(); restored->mDrive.LoadState(*decoded);
	AT_PORTABLE_TEST_ASSERT(context, restored->mBus.Collect(250000) == expected);
	return true;
}

bool ATTestAltirraDiskAudioTrace(ATPortableTestContext& context) {
	ATTraceContext trace {}; trace.mpCollection = new ATTraceCollection;
	auto f = std::make_unique<DiskEmulatorPortableFixture>(); auto& clock = f->mBus.mMachine.mChips.mVideo.mScheduler;
	trace.mBaseTime = clock.GetTick64(); trace.mBaseTickScale = clock.GetRate().AsInverseDouble();
	f->mSlot.mDisk.SetDriveSoundsEnabled(true); f->mSlot.mDisk.SetSectorBreakpoint(4); f->mDrive.SetTraceContext(&trace);
	AT_PORTABLE_TEST_ASSERT(context, f->Accel(0x52, 0x40, 128, 4) && f->Status() == 1 && f->mSlot.mBreaks == 1);
	AT_PORTABLE_TEST_ASSERT(context, f->mSlot.mbMotor && trace.mpCollection->GetGroupCount() == 1);
	auto *commands = trace.mpCollection->GetGroup(0)->GetChannel(0);
	AT_PORTABLE_TEST_ASSERT(context, commands->GetEventCount() > 0);
	std::array<float, 512> audio {}; std::array<float, kATAudioMixCount> levels; levels.fill(1);
	ATAudioAsyncMixInfo info {}; info.mStartTime = clock.GetTick64(); info.mCount = audio.size(); info.mNumCycles = audio.size() * 28;
	info.mMixingRate = 63920.8f; info.mpLeft = audio.data(); info.mpMixLevels = levels.data();
	AT_PORTABLE_TEST_ASSERT(context, f->mPlayer.WriteAsyncAudio(info) && std::any_of(audio.begin(), audio.end(), [](float v) { return std::abs(v) > 1e-8f; }));
	f->mSlot.mDisk.SetDriveSoundsEnabled(false); f->mDrive.SetTraceContext(nullptr);
	f->mDrive.SetEnabled(false); AT_PORTABLE_TEST_ASSERT(context, !f->mSlot.mbMotor);
	return true;
}

bool ATTestAltirraDiskLifecycle(ATPortableTestContext& context) {
	struct Sentinel final : IATSchedulerCallback { void OnScheduledEvent(uint32) override { ++mCalls; } uint32 mCalls = 0; } sentinel;
	auto f = std::make_unique<DiskEmulatorPortableFixture>();
	AT_PORTABLE_TEST_ASSERT(context, f->Accel(0x52, 0x40, 128) && f->mSlot.mbMotor);
	auto& slow = f->mBus.mMachine.mSlowScheduler; slow.AddEvent(1000000, &sentinel, 1);
	f->mDrive.Shutdown(); f->mDrive.Shutdown();
	for (uint32 i = 0; i < 1000001; ++i) ATSCHEDULER_ADVANCE(&slow);
	AT_PORTABLE_TEST_ASSERT(context, sentinel.mCalls == 1 && !f->mSlot.mbMotor && f->mSlot.mDisk.GetClientCount() == 0);
	f->mBus.Tick(2000);
	return true;
}
