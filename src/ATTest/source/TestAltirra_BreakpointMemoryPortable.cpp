// Portable integration tests for memory access breakpoints and ranges.

#include <algorithm>
#include <array>
#include <memory>
#include <utility>
#include <vector>

#include <at/attest/portabletest.h>
#include <bkptmanager.h>
#include <memorymanager.h>

namespace {
	class BreakpointFixture {
	public:
		BreakpointFixture() {
			mMemory.Init();
			mpBaseLayer = mMemory.CreateLayer(kATMemoryPri_BaseRAM, mBaseRAM.data(), 0, 0x100, false);
			mMemory.SetLayerName(mpBaseLayer, "breakpoint test base RAM");
			mMemory.EnableLayer(mpBaseLayer, true);

			mMemory.SetHighMemoryEnabled(true);
			mpBank1Layer = mMemory.CreateLayer(kATMemoryPri_ExtRAM, mBank1RAM.data(), 0x100, 0x100, false);
			mpBank2Layer = mMemory.CreateLayer(kATMemoryPri_ExtRAM, mBank2RAM.data(), 0x200, 0x100, false);
			mpBankFFLayer = mMemory.CreateLayer(kATMemoryPri_ExtRAM, mBankFFRAM.data(), 0xFF00, 0x100, false);
			mMemory.EnableLayer(mpBank1Layer, kATMemoryAccessMode_RW, true);
			mMemory.EnableLayer(mpBank2Layer, kATMemoryAccessMode_RW, true);
			mMemory.EnableLayer(mpBankFFLayer, kATMemoryAccessMode_RW, true);

			ATBreakpointManagerConnections connections;
			connections.mpSetBreakpointManager = [this](ATBreakpointManager *manager) { mpCurrentBreakpointManager = manager; };
			connections.mpSetBreakpoint = [this](uint16 address) { mSetBreakpointAddresses.push_back(address); };
			connections.mpClearBreakpoint = [this](uint16 address) { mClearBreakpointAddresses.push_back(address); };
			connections.mpSetAllBreakpoints = [this] { ++mSetAllBreakpointCalls; };
			connections.mpClearAllBreakpoints = [this] { ++mClearAllBreakpointCalls; };
			connections.mpGetXPC = [this] { return mXPC; };
			connections.mpPostInterruptingEvent = [this](ATSimulatorEvent event) { mPostedEvents.push_back(event); };
			mBreakpoints.Init(std::move(connections), &mMemory);
		}

		~BreakpointFixture() {
			mBreakpoints.Shutdown();
			mMemory.DeleteLayer(mpBankFFLayer);
			mMemory.DeleteLayer(mpBank2Layer);
			mMemory.DeleteLayer(mpBank1Layer);
			mMemory.DeleteLayer(mpBaseLayer);
		}

		ATMemoryManager mMemory;
		ATBreakpointManager mBreakpoints;
		alignas(2) std::array<uint8, 65536> mBaseRAM {};
		alignas(2) std::array<uint8, 65536> mBank1RAM {};
		alignas(2) std::array<uint8, 65536> mBank2RAM {};
		alignas(2) std::array<uint8, 65536> mBankFFRAM {};
		ATBreakpointManager *mpCurrentBreakpointManager = nullptr;
		std::vector<uint16> mSetBreakpointAddresses;
		std::vector<uint16> mClearBreakpointAddresses;
		std::vector<ATSimulatorEvent> mPostedEvents;
		uint32 mSetAllBreakpointCalls = 0;
		uint32 mClearAllBreakpointCalls = 0;
		uint32 mXPC = 0;

	private:
		ATMemoryLayer *mpBaseLayer = nullptr;
		ATMemoryLayer *mpBank1Layer = nullptr;
		ATMemoryLayer *mpBank2Layer = nullptr;
		ATMemoryLayer *mpBankFFLayer = nullptr;
	};

	class BreakpointHitCapture {
	public:
		explicit BreakpointHitCapture(ATBreakpointManager& manager)
			: mpManager(&manager) {
			manager.OnBreakpointHit() += mDelegate.Bind(this, &BreakpointHitCapture::OnHit);
		}

		~BreakpointHitCapture() {
			if (mpManager)
				mpManager->OnBreakpointHit() -= mDelegate;
		}

		void OnHit(ATBreakpointManager *, ATBreakpointEvent *event) {
			auto policy = std::find_if(mPolicies.begin(), mPolicies.end(), [event](const Policy& policy) { return policy.mId == event->mIndex; });
			event->mbBreak = policy == mPolicies.end() || policy->mbBreak;
			auto it = std::find_if(mSilentPolicies.begin(), mSilentPolicies.end(), [event](const auto& policy) { return policy.first == event->mIndex; });
			event->mbSilentBreak = it != mSilentPolicies.end() && it->second;
			mEvents.push_back(*event);
			if (event->mIndex == mRemoveOnHit)
				mpManager->Clear(event->mIndex);
		}

		void SetSilent(uint32 id, bool silent) {
			auto it = std::find_if(mSilentPolicies.begin(), mSilentPolicies.end(), [id](const auto& policy) { return policy.first == id; });
			if (it == mSilentPolicies.end())
				mSilentPolicies.emplace_back(id, silent);
			else
				it->second = silent;
		}
		void SetBreak(uint32 id, bool shouldBreak) {
			auto it = std::find_if(mPolicies.begin(), mPolicies.end(), [id](const Policy& policy) { return policy.mId == id; });
			if (it == mPolicies.end())
				mPolicies.push_back({ id, shouldBreak });
			else
				it->mbBreak = shouldBreak;
		}
		void RemoveOnHit(uint32 id) { mRemoveOnHit = id; }
		void Clear() { mEvents.clear(); }

		VDDelegate mDelegate;
		std::vector<ATBreakpointEvent> mEvents;

	private:
		struct Policy { uint32 mId; bool mbBreak; };
		ATBreakpointManager *mpManager;
		std::vector<std::pair<uint32, bool>> mSilentPolicies;
		std::vector<Policy> mPolicies;
		uint32 mRemoveOnHit = 0;
	};

	void ClearPosted(BreakpointFixture& fixture) {
		fixture.mPostedEvents.clear();
	}

	bool IsSingleEvent(const std::vector<ATBreakpointEvent>& events, uint32 id, uint32 address, uint8 value) {
		return events.size() == 1
			&& events[0].mIndex == id
			&& events[0].mTargetIndex == 0
			&& events[0].mAddress == address
			&& events[0].mValue == value;
	}

	bool TestSingleAndSharedAddress(ATPortableTestContext& context, BreakpointFixture& fixture, BreakpointHitCapture& hits) {
		constexpr uint16 address = 0x1234;
		fixture.mBaseRAM[address] = 0xA5;
		const uint32 access = fixture.mBreakpoints.SetAccessBP(address, true, true);
		AT_PORTABLE_TEST_ASSERT(context, access != 0);
		AT_PORTABLE_TEST_ASSERT(context, fixture.mMemory.ReadByte(address) == 0xA5);
		AT_PORTABLE_TEST_ASSERT(context, IsSingleEvent(hits.mEvents, access, address, 0));
		AT_PORTABLE_TEST_ASSERT(context, fixture.mPostedEvents == std::vector<ATSimulatorEvent>({ kATSimEvent_ReadBreakpoint }));

		hits.Clear();
		ClearPosted(fixture);
		fixture.mMemory.WriteByte(address, 0x5A);
		AT_PORTABLE_TEST_ASSERT(context, fixture.mBaseRAM[address] == 0x5A);
		AT_PORTABLE_TEST_ASSERT(context, IsSingleEvent(hits.mEvents, access, address, 0x5A));
		AT_PORTABLE_TEST_ASSERT(context, fixture.mPostedEvents == std::vector<ATSimulatorEvent>({ kATSimEvent_WriteBreakpoint }));

		hits.Clear();
		ClearPosted(fixture);
		AT_PORTABLE_TEST_ASSERT(context, fixture.mMemory.DebugReadByte(address) == 0x5A);
		AT_PORTABLE_TEST_ASSERT(context, fixture.mMemory.AnticReadByte(address) == 0x5A);
		AT_PORTABLE_TEST_ASSERT(context, hits.mEvents.empty());
		AT_PORTABLE_TEST_ASSERT(context, fixture.mPostedEvents.empty());

		AT_PORTABLE_TEST_ASSERT(context, fixture.mBreakpoints.Clear(access));
		const uint32 readA = fixture.mBreakpoints.SetAccessBP(address, true, false);
		const uint32 readB = fixture.mBreakpoints.SetAccessBP(address, true, false);
		hits.SetSilent(readA, true);
		hits.SetSilent(readB, false);
		AT_PORTABLE_TEST_ASSERT(context, fixture.mMemory.ReadByte(address) == 0x5A);
		AT_PORTABLE_TEST_ASSERT(context, hits.mEvents.size() == 2);
		AT_PORTABLE_TEST_ASSERT(context, hits.mEvents[0].mIndex == readA && hits.mEvents[1].mIndex == readB);
		AT_PORTABLE_TEST_ASSERT(context, fixture.mPostedEvents == std::vector<ATSimulatorEvent>({ kATSimEvent_ReadBreakpoint }));

		hits.Clear();
		ClearPosted(fixture);
		AT_PORTABLE_TEST_ASSERT(context, fixture.mBreakpoints.Clear(readA));
		AT_PORTABLE_TEST_ASSERT(context, fixture.mMemory.ReadByte(address) == 0x5A);
		AT_PORTABLE_TEST_ASSERT(context, IsSingleEvent(hits.mEvents, readB, address, 0));
		AT_PORTABLE_TEST_ASSERT(context, fixture.mPostedEvents.size() == 1 && fixture.mPostedEvents[0] == kATSimEvent_ReadBreakpoint);

		hits.Clear();
		ClearPosted(fixture);
		hits.SetSilent(readB, true);
		AT_PORTABLE_TEST_ASSERT(context, fixture.mMemory.ReadByte(address) == 0x5A);
		AT_PORTABLE_TEST_ASSERT(context, fixture.mPostedEvents.size() == 1 && fixture.mPostedEvents[0] == kATSimEvent_AnonymousInterrupt);
		AT_PORTABLE_TEST_ASSERT(context, fixture.mBreakpoints.Clear(readB));
		hits.Clear();
		ClearPosted(fixture);
		AT_PORTABLE_TEST_ASSERT(context, fixture.mMemory.ReadByte(address) == 0x5A);
		AT_PORTABLE_TEST_ASSERT(context, hits.mEvents.empty() && fixture.mPostedEvents.empty());

		// A debugger listener can synchronously remove its one-shot breakpoint.
		// Dispatch must still visit the other breakpoint at the same address.
		const uint32 oneShot = fixture.mBreakpoints.SetAccessBP(address, true, false);
		const uint32 survivor = fixture.mBreakpoints.SetAccessBP(address, true, false);
		hits.RemoveOnHit(oneShot);
		hits.Clear();
		ClearPosted(fixture);
		fixture.mMemory.ReadByte(address);
		AT_PORTABLE_TEST_ASSERT(context, hits.mEvents.size() == 2);
		AT_PORTABLE_TEST_ASSERT(context, hits.mEvents[0].mIndex == oneShot && hits.mEvents[1].mIndex == survivor);
		ATBreakpointInfo removedInfo = {};
		AT_PORTABLE_TEST_ASSERT(context, !fixture.mBreakpoints.GetInfo(oneShot, removedInfo));
		hits.RemoveOnHit(0);
		AT_PORTABLE_TEST_ASSERT(context, fixture.mBreakpoints.Clear(survivor));

		// A reported event without a break policy must not post an interrupt.
		const uint32 nonStopping = fixture.mBreakpoints.SetAccessBP(address, true, false);
		hits.SetBreak(nonStopping, false);
		hits.Clear();
		ClearPosted(fixture);
		fixture.mMemory.ReadByte(address);
		AT_PORTABLE_TEST_ASSERT(context, IsSingleEvent(hits.mEvents, nonStopping, address, 0));
		AT_PORTABLE_TEST_ASSERT(context, fixture.mPostedEvents.empty());
		AT_PORTABLE_TEST_ASSERT(context, fixture.mBreakpoints.Clear(nonStopping));
		return true;
	}

	bool TestBankIsolation(ATPortableTestContext& context, BreakpointFixture& fixture, BreakpointHitCapture& hits) {
		hits.Clear();
		ClearPosted(fixture);
		constexpr uint16 offset = 0x2345;
		constexpr uint32 bank1Address = 0x012345;
		constexpr uint32 bank2Address = 0x022345;
		fixture.mBank1RAM[offset] = 0x31;
		fixture.mBank2RAM[offset] = 0x42;
		const uint32 bank1BP = fixture.mBreakpoints.SetAccessBP(bank1Address, true, false);
		const uint32 bank2BP = fixture.mBreakpoints.SetAccessBP(bank2Address, true, false);

		AT_PORTABLE_TEST_ASSERT(context, fixture.mMemory.ExtReadByte(offset, 1) == 0x31);
		AT_PORTABLE_TEST_ASSERT(context, hits.mEvents.size() == 1 && hits.mEvents[0].mIndex == bank1BP && hits.mEvents[0].mAddress == bank1Address);
		hits.Clear();
		ClearPosted(fixture);
		AT_PORTABLE_TEST_ASSERT(context, fixture.mMemory.ExtReadByte(offset, 2) == 0x42);
		AT_PORTABLE_TEST_ASSERT(context, hits.mEvents.size() == 1 && hits.mEvents[0].mIndex == bank2BP && hits.mEvents[0].mAddress == bank2Address);

		hits.Clear();
		ClearPosted(fixture);
		AT_PORTABLE_TEST_ASSERT(context, fixture.mBreakpoints.Clear(bank1BP));
		AT_PORTABLE_TEST_ASSERT(context, fixture.mMemory.ExtReadByte(offset, 1) == 0x31);
		AT_PORTABLE_TEST_ASSERT(context, hits.mEvents.empty());
		AT_PORTABLE_TEST_ASSERT(context, fixture.mMemory.ExtReadByte(offset, 2) == 0x42);
		AT_PORTABLE_TEST_ASSERT(context, hits.mEvents.size() == 1 && hits.mEvents[0].mIndex == bank2BP);
		AT_PORTABLE_TEST_ASSERT(context, fixture.mBreakpoints.Clear(bank2BP));
		hits.Clear();
		ClearPosted(fixture);
		return true;
	}

	bool TestAccessRanges(ATPortableTestContext& context, BreakpointFixture& fixture, BreakpointHitCapture& hits) {
		// [20FE, 2102) spans two pages; the upper endpoint is excluded.
		const uint32 crossPage = fixture.mBreakpoints.SetAccessRangeBP(0x20FE, 4, true, true);
		AT_PORTABLE_TEST_ASSERT(context, crossPage != 0);
		for(uint32 address = 0x20FE; address < 0x2102; ++address) {
			fixture.mBaseRAM[address] = static_cast<uint8>(address);
			hits.Clear();
			ClearPosted(fixture);
			AT_PORTABLE_TEST_ASSERT(context, fixture.mMemory.ReadByte(address) == static_cast<uint8>(address));
			AT_PORTABLE_TEST_ASSERT(context, hits.mEvents.size() == 1 && hits.mEvents[0].mIndex == crossPage && hits.mEvents[0].mAddress == address);
		}
		hits.Clear();
		ClearPosted(fixture);
		fixture.mMemory.ReadByte(0x2102);
		AT_PORTABLE_TEST_ASSERT(context, hits.mEvents.empty());
		AT_PORTABLE_TEST_ASSERT(context, fixture.mBreakpoints.Clear(crossPage));

		// Overlapping ranges must remain individually effective when either is removed.
		const uint32 overlapA = fixture.mBreakpoints.SetAccessRangeBP(0x3000, 4, true, false);
		const uint32 overlapB = fixture.mBreakpoints.SetAccessRangeBP(0x3002, 4, true, false);
		hits.RemoveOnHit(overlapB);
		hits.Clear();
		fixture.mMemory.ReadByte(0x3003);
		AT_PORTABLE_TEST_ASSERT(context, hits.mEvents.size() == 2);
		AT_PORTABLE_TEST_ASSERT(context, hits.mEvents[0].mIndex == overlapB && hits.mEvents[1].mIndex == overlapA);
		hits.RemoveOnHit(0);
		ATBreakpointInfo removedRangeInfo = {};
		AT_PORTABLE_TEST_ASSERT(context, !fixture.mBreakpoints.GetInfo(overlapB, removedRangeInfo));
		hits.Clear();
		fixture.mMemory.ReadByte(0x3003);
		AT_PORTABLE_TEST_ASSERT(context, hits.mEvents.size() == 1 && hits.mEvents[0].mIndex == overlapA);
		AT_PORTABLE_TEST_ASSERT(context, fixture.mBreakpoints.Clear(overlapA));

		// Same start with different lengths and access modes is not one merged range.
		const uint32 sameRead = fixture.mBreakpoints.SetAccessRangeBP(0x4000, 4, true, false);
		const uint32 sameWrite = fixture.mBreakpoints.SetAccessRangeBP(0x4000, 2, false, true);
		AT_PORTABLE_TEST_ASSERT(context, fixture.mBreakpoints.Clear(sameRead));
		hits.Clear();
		ClearPosted(fixture);
		fixture.mMemory.ReadByte(0x4000);
		AT_PORTABLE_TEST_ASSERT(context, hits.mEvents.empty());
		fixture.mMemory.WriteByte(0x4000, 0xD4);
		AT_PORTABLE_TEST_ASSERT(context, hits.mEvents.size() == 1 && hits.mEvents[0].mIndex == sameWrite && hits.mEvents[0].mValue == 0xD4);
		AT_PORTABLE_TEST_ASSERT(context, fixture.mBreakpoints.Clear(sameWrite));

		// Attribute bits alias across banks, but removing one range must not erase
		// the other bank's active range breakpoint.
		const uint32 bank1Range = fixture.mBreakpoints.SetAccessRangeBP(0x010100, 4, true, false);
		const uint32 bank2Range = fixture.mBreakpoints.SetAccessRangeBP(0x020100, 4, true, false);
		AT_PORTABLE_TEST_ASSERT(context, fixture.mBreakpoints.Clear(bank1Range));
		hits.Clear();
		ClearPosted(fixture);
		AT_PORTABLE_TEST_ASSERT(context, fixture.mMemory.ExtReadByte(0x0100, 2) == fixture.mBank2RAM[0x0100]);
		AT_PORTABLE_TEST_ASSERT(context, hits.mEvents.size() == 1 && hits.mEvents[0].mIndex == bank2Range);
		AT_PORTABLE_TEST_ASSERT(context, fixture.mBreakpoints.Clear(bank2Range));

		// A huge length near the 24-bit boundary clips without overflow to the
		// remaining 16 bytes; zero length is rejected without a registration.
		const uint32 zeroLength = fixture.mBreakpoints.SetAccessRangeBP(0xFFFFF0, 0, true, false);
		AT_PORTABLE_TEST_ASSERT(context, zeroLength == 0);
		const uint32 clipped = fixture.mBreakpoints.SetAccessRangeBP(0xFFFFF0, 0xFFFFFFFFu, true, false);
		AT_PORTABLE_TEST_ASSERT(context, clipped != 0);
		ATBreakpointInfo info = {};
		AT_PORTABLE_TEST_ASSERT(context, fixture.mBreakpoints.GetInfo(clipped, info));
		AT_PORTABLE_TEST_ASSERT(context, info.mAddress == 0xFFFFF0 && info.mLength == 16 && info.mbBreakOnRead);
		hits.Clear();
		ClearPosted(fixture);
		AT_PORTABLE_TEST_ASSERT(context, fixture.mMemory.ExtReadByte(0xFFF0, 0xFF) == fixture.mBankFFRAM[0xFFF0]);
		AT_PORTABLE_TEST_ASSERT(context, hits.mEvents.size() == 1 && hits.mEvents[0].mIndex == clipped && hits.mEvents[0].mAddress == 0xFFFFF0);
		hits.Clear();
		fixture.mMemory.ExtReadByte(0xFFFF, 0xFF);
		AT_PORTABLE_TEST_ASSERT(context, hits.mEvents.size() == 1 && hits.mEvents[0].mAddress == 0xFFFFFF);
		hits.Clear();
		fixture.mMemory.ExtReadByte(0xFFEF, 0xFF);
		AT_PORTABLE_TEST_ASSERT(context, hits.mEvents.empty());
		AT_PORTABLE_TEST_ASSERT(context, fixture.mBreakpoints.Clear(clipped));
		return true;
	}

	bool TestClearAndReinitialize(ATPortableTestContext& context, BreakpointFixture& fixture, BreakpointHitCapture& hits) {
		const uint32 access = fixture.mBreakpoints.SetAccessBP(0x5000, true, true);
		const uint32 range = fixture.mBreakpoints.SetAccessRangeBP(0x50F0, 0x30, true, false);
		AT_PORTABLE_TEST_ASSERT(context, access != 0 && range != 0);
		fixture.mBreakpoints.ClearAll();
		ATBreakpointInfo info = {};
		AT_PORTABLE_TEST_ASSERT(context, !fixture.mBreakpoints.GetInfo(access, info));
		AT_PORTABLE_TEST_ASSERT(context, !fixture.mBreakpoints.GetInfo(range, info));
		hits.Clear();
		ClearPosted(fixture);
		fixture.mMemory.ReadByte(0x5000);
		fixture.mMemory.WriteByte(0x5000, 0xAA);
		fixture.mMemory.ReadByte(0x50F0);
		AT_PORTABLE_TEST_ASSERT(context, hits.mEvents.empty() && fixture.mPostedEvents.empty());

		fixture.mBreakpoints.Shutdown();
		fixture.mBreakpoints.Shutdown();
		AT_PORTABLE_TEST_ASSERT(context, fixture.mpCurrentBreakpointManager == nullptr);
		AT_PORTABLE_TEST_ASSERT(context, fixture.mMemory.ReadByte(0x5000) == 0xAA);

		ATBreakpointManagerConnections connections;
		connections.mpSetBreakpointManager = [&fixture](ATBreakpointManager *manager) { fixture.mpCurrentBreakpointManager = manager; };
		connections.mpPostInterruptingEvent = [&fixture](ATSimulatorEvent event) { fixture.mPostedEvents.push_back(event); };
		fixture.mBreakpoints.Init(std::move(connections), &fixture.mMemory);
		AT_PORTABLE_TEST_ASSERT(context, fixture.mpCurrentBreakpointManager == &fixture.mBreakpoints);
		const uint32 reinitialized = fixture.mBreakpoints.SetAccessBP(0x5000, true, false);
		hits.Clear();
		ClearPosted(fixture);
		fixture.mMemory.ReadByte(0x5000);
		AT_PORTABLE_TEST_ASSERT(context, hits.mEvents.size() == 1 && hits.mEvents[0].mIndex == reinitialized);
		fixture.mBreakpoints.Shutdown();
		AT_PORTABLE_TEST_ASSERT(context, fixture.mpCurrentBreakpointManager == nullptr);
		fixture.mBreakpoints.Shutdown();
		return true;
	}
}

bool ATTestAltirraBreakpointMemory(ATPortableTestContext& context) {
	auto fixture = std::make_unique<BreakpointFixture>();
	BreakpointHitCapture hits(fixture->mBreakpoints);
	return TestSingleAndSharedAddress(context, *fixture, hits) && TestBankIsolation(context, *fixture, hits);
}

bool ATTestAltirraBreakpointRanges(ATPortableTestContext& context) {
	auto fixture = std::make_unique<BreakpointFixture>();
	BreakpointHitCapture hits(fixture->mBreakpoints);
	return TestAccessRanges(context, *fixture, hits) && TestClearAndReinitialize(context, *fixture, hits);
}
