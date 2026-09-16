// Altirra portable disk rotation trace tests

#include <array>
#include <cmath>
#include <string>
#include <vector>

#include <at/atcore/scheduler.h>
#include <at/atio/diskimage.h>
#include <at/attest/portabletest.h>
#include <disktrace.h>
#include <trace.h>

namespace {
	struct ATDiskTraceEventSnapshot {
		double mStart;
		double mStop;
		uint32 mColor;
		std::wstring mName;
	};

	bool Near(double actual, double expected) {
		return std::abs(actual - expected) < 1e-9;
	}

	void Advance(ATScheduler& scheduler, uint32 ticks) {
		scheduler.mNextEventCounter += ticks;
	}

	std::vector<ATDiskTraceEventSnapshot> ReadEvents(IATTraceChannel& channel) {
		std::vector<ATDiskTraceEventSnapshot> events;
		channel.StartIteration(0, 100, 0);

		ATTraceEvent event {};
		while(channel.GetNextEvent(event)) {
			events.push_back({
				event.mEventStart,
				event.mEventStop,
				event.mBgColor,
				event.mpName ? event.mpName : L""
			});
		}

		return events;
	}

	vdrefptr<IATDiskImage> CreateRotationalTestDisk() {
		ATDiskGeometryInfo geometry {};
		geometry.mSectorSize = 128;
		geometry.mBootSectorCount = 3;
		geometry.mTrackCount = 2;
		geometry.mSectorsPerTrack = 3;
		geometry.mSideCount = 1;

		vdrefptr<IATDiskImage> image;
		ATCreateDiskImage(geometry, ~image);

		const ATDiskVirtualSectorInfo virtualSectors[] {
			{ 0, 2 },
			{ 2, 1 },
			{ 3, 1 }
		};

		ATDiskPhysicalSectorInfo physicalSectors[4] {};
		const float rotationalPositions[] { 0.125f, 0.625f, 0.375f, 0.875f };
		for(uint32 i = 0; i < 4; ++i) {
			auto& sector = physicalSectors[i];
			sector.mOffset = i * 128;
			sector.mDiskOffset = -1;
			sector.mImageSize = 128;
			sector.mPhysicalSize = 128;
			sector.mRotPos = rotationalPositions[i];
			sector.mFDCStatus = 0xFF;
			sector.mWeakDataOffset = -1;
		}

		std::array<uint8, 4 * 128> data {};
		image->FormatTrack(
			0,
			3,
			virtualSectors,
			4,
			physicalSectors,
			data.data());
		return image;
	}

	vdrefptr<IATDiskImage> CreateSingleTrackDisk() {
		ATDiskGeometryInfo geometry {};
		geometry.mSectorSize = 128;
		geometry.mBootSectorCount = 3;
		geometry.mTrackCount = 1;
		geometry.mSectorsPerTrack = 3;
		geometry.mSideCount = 1;

		vdrefptr<IATDiskImage> image;
		ATCreateDiskImage(geometry, ~image);
		return image;
	}
}

bool ATTestAltirraDiskTrace(ATPortableTestContext& context) {
	ATScheduler scheduler;
	scheduler.SetRate(VDFraction(80, 1));
	const uint64 baseTick = scheduler.GetTick64();

	vdrefptr<ATTraceCollection> collection { new ATTraceCollection };
	ATTraceGroup *group = collection->AddGroup(L"Disk");
	ATDiskRotationTracer tracer;
	tracer.Init(scheduler, baseTick, 80, *group);
	AT_PORTABLE_TEST_ASSERT(context, group->GetChannelCount() == 1);
	IATTraceChannel *channel = group->GetChannel(0);
	AT_PORTABLE_TEST_ASSERT(context, channel != nullptr);
	AT_PORTABLE_TEST_ASSERT(context, channel->IsEmpty());

	vdrefptr<IATDiskImage> image = CreateRotationalTestDisk();
	tracer.SetDiskImage(image);
	tracer.SetTrack(0);
	tracer.SetMotorRunning(true);

	Advance(scheduler, 9);
	tracer.Flush();
	AT_PORTABLE_TEST_ASSERT(context, channel->GetEventCount() == 0);

	Advance(scheduler, 1);
	tracer.Flush();
	auto events = ReadEvents(*channel);
	AT_PORTABLE_TEST_ASSERT(context, events.size() == 1);
	AT_PORTABLE_TEST_ASSERT(context, events[0].mName == L"1/1");
	AT_PORTABLE_TEST_ASSERT(context, Near(events[0].mStart, 0.125));
	AT_PORTABLE_TEST_ASSERT(context, events[0].mColor == 0x4080FF);

	Advance(scheduler, 20);
	tracer.Flush();
	Advance(scheduler, 20);
	tracer.Flush();
	events = ReadEvents(*channel);
	AT_PORTABLE_TEST_ASSERT(context, events.size() == 3);
	AT_PORTABLE_TEST_ASSERT(context, events[1].mName == L"2");
	AT_PORTABLE_TEST_ASSERT(context, events[2].mName == L"1/2");
	AT_PORTABLE_TEST_ASSERT(context, Near(events[0].mStop, 0.375));
	AT_PORTABLE_TEST_ASSERT(context, Near(events[1].mStart, 0.375));
	AT_PORTABLE_TEST_ASSERT(context, Near(events[1].mStop, 0.625));
	AT_PORTABLE_TEST_ASSERT(context, Near(events[2].mStart, 0.625));
	AT_PORTABLE_TEST_ASSERT(context, events[1].mColor == kATTraceColor_Default);

	// Stopping the motor pauses rotational time without producing sectors.
	tracer.SetMotorRunning(false);
	Advance(scheduler, 80);
	tracer.Flush();
	AT_PORTABLE_TEST_ASSERT(context, channel->GetEventCount() == 3);
	tracer.SetMotorRunning(true);
	Advance(scheduler, 20);
	tracer.Flush();
	events = ReadEvents(*channel);
	AT_PORTABLE_TEST_ASSERT(context, events.size() == 4);
	AT_PORTABLE_TEST_ASSERT(context, events[3].mName == L"3");
	AT_PORTABLE_TEST_ASSERT(context, Near(events[3].mStart, 1.875));

	// Warp repositions the disk five ticks before the first sector.
	tracer.Warp(5);
	Advance(scheduler, 5);
	tracer.Flush();
	events = ReadEvents(*channel);
	AT_PORTABLE_TEST_ASSERT(context, events.size() == 5);
	AT_PORTABLE_TEST_ASSERT(context, events[4].mName == L"1/1");
	AT_PORTABLE_TEST_ASSERT(context, Near(events[4].mStart, 1.9375));
	AT_PORTABLE_TEST_ASSERT(context, events[4].mColor == 0x4080FF);

	// Single-track block devices are intentionally omitted from rotation traces.
	vdrefptr<IATDiskImage> singleTrackImage = CreateSingleTrackDisk();
	tracer.SetDiskImage(singleTrackImage);
	Advance(scheduler, 80);
	tracer.Flush();
	AT_PORTABLE_TEST_ASSERT(context, channel->GetEventCount() == 5);

	tracer.Shutdown();
	events = ReadEvents(*channel);
	AT_PORTABLE_TEST_ASSERT(context, events.size() == 5);
	AT_PORTABLE_TEST_ASSERT(context, Near(events.back().mStop, 2.9375));
	return true;
}
