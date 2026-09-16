// Portable tests for video trace frame storage and sampling.

#include <cstring>
#include <cwchar>

#include <at/attest/portabletest.h>
#include <vd2/Kasumi/pixmaputils.h>

#include <trace.h>
#include <tracevideo.h>

namespace {
	void FillBytes(VDPixmapBuffer& pixmap, uint8 value) {
		std::memset(pixmap.base(), value, pixmap.size());
	}

	void FillXRGB(VDPixmapBuffer& pixmap, uint32 color) {
		FillBytes(pixmap, 0);

		for(sint32 y = 0; y < pixmap.h; ++y) {
			uint32 *row = pixmap.GetPixelRow<uint32>(y);
			for(sint32 x = 0; x < pixmap.w; ++x)
				row[x] = color;
		}
	}

	bool TestRawFrameStorage(ATPortableTestContext& context) {
		ATTraceMemoryTracker memoryTracker;
		vdrefptr channel = ATCreateTraceChannelVideo(L"Raw video", &memoryTracker);
		IATTraceChannel *traceChannel = channel->AsTraceChannel();

		AT_PORTABLE_TEST_ASSERT(context, traceChannel != nullptr);
		AT_PORTABLE_TEST_ASSERT(context, !std::wcscmp(traceChannel->GetName(), L"Raw video"));
		AT_PORTABLE_TEST_ASSERT(context, traceChannel->IsEmpty());
		AT_PORTABLE_TEST_ASSERT(context, traceChannel->GetDuration() == 0.0);
		AT_PORTABLE_TEST_ASSERT(context, traceChannel->GetEventCount() == 0);
		AT_PORTABLE_TEST_ASSERT(context, traceChannel->GetTraceSize() == 0);
		AT_PORTABLE_TEST_ASSERT(context, traceChannel->AsInterface(IATTraceChannelVideo::kTypeID) == channel.get());

		VDPixmapBuffer first(4, 4, nsVDPixmap::kPixFormat_YUV420_Planar_Centered);
		VDPixmapBuffer second(6, 2, nsVDPixmap::kPixFormat_YUV420_Planar_Centered);
		FillBytes(first, 0x11);
		FillBytes(second, 0x22);
		const uint64 expectedTraceSize = first.size() + second.size();

		channel->AddRawFrameBuffer(first);
		FillBytes(first, 0xEE);
		channel->AddRawFrameBuffer(second);

		AT_PORTABLE_TEST_ASSERT(context, channel->GetFrameBufferCount() == 2);
		AT_PORTABLE_TEST_ASSERT(context, channel->GetTraceSize() == expectedTraceSize);
		AT_PORTABLE_TEST_ASSERT(context, memoryTracker.GetSize() == expectedTraceSize);
		AT_PORTABLE_TEST_ASSERT(context, *static_cast<const uint8 *>(channel->GetFrameBufferByIndex(0).data) == 0x11);
		AT_PORTABLE_TEST_ASSERT(context, channel->GetFrameSizeByIndex(0) == vdsize32(4, 4));
		AT_PORTABLE_TEST_ASSERT(context, channel->GetFrameSizeByIndex(1) == vdsize32(6, 2));

		channel->AddFrame(10.0, 0);
		channel->AddFrame(4.0, 1);
		channel->AddFrame(7.0, 0);

		AT_PORTABLE_TEST_ASSERT(context, !traceChannel->IsEmpty());
		AT_PORTABLE_TEST_ASSERT(context, traceChannel->GetEventCount() == 3);
		AT_PORTABLE_TEST_ASSERT(context, traceChannel->GetDuration() == 10.0);
		AT_PORTABLE_TEST_ASSERT(context, channel->GetTimeForFrame(0) == 4.0);
		AT_PORTABLE_TEST_ASSERT(context, channel->GetTimeForFrame(1) == 7.0);
		AT_PORTABLE_TEST_ASSERT(context, channel->GetTimeForFrame(2) == 10.0);
		AT_PORTABLE_TEST_ASSERT(context, channel->GetFrameBufferIndexForFrame(-1) == -1);
		AT_PORTABLE_TEST_ASSERT(context, channel->GetFrameBufferIndexForFrame(0) == 1);
		AT_PORTABLE_TEST_ASSERT(context, channel->GetFrameBufferIndexForFrame(1) == 0);
		AT_PORTABLE_TEST_ASSERT(context, channel->GetFrameBufferIndexForFrame(3) == -1);

		double frameTime = -1;
		AT_PORTABLE_TEST_ASSERT(context, channel->GetNearestFrameIndex(3.0, 6.0, frameTime) == 1);
		AT_PORTABLE_TEST_ASSERT(context, frameTime == 4.0);
		AT_PORTABLE_TEST_ASSERT(context, channel->GetNearestFrameIndex(6.0, 9.0, frameTime) == 0);
		AT_PORTABLE_TEST_ASSERT(context, frameTime == 7.0);
		AT_PORTABLE_TEST_ASSERT(context, channel->GetNearestFrameIndex(10.1, 20.0, frameTime) == -1);

		ATTraceEvent event {};
		traceChannel->StartIteration(0.0, 20.0, 0.0);
		AT_PORTABLE_TEST_ASSERT(context, !traceChannel->GetNextEvent(event));
		return true;
	}

	bool TestVideoTracer(ATPortableTestContext& context) {
		ATTraceMemoryTracker memoryTracker;
		vdrefptr channel = ATCreateTraceChannelVideo(L"Sampled video", &memoryTracker);
		vdrefptr tracer = ATCreateVideoTracer();
		IATGTIAVideoTap *tap = tracer->AsVideoTap();
		AT_PORTABLE_TEST_ASSERT(context, tap != nullptr);

		tracer->Init(channel, 100, 0.5, 2, 17);
		VDPixmapBuffer input(8, 8, nsVDPixmap::kPixFormat_XRGB8888);
		FillXRGB(input, 0x00102030);

		tap->WriteFrame(input, 90, 94, 1.0f);
		AT_PORTABLE_TEST_ASSERT(context, channel->AsTraceChannel()->IsEmpty());

		tap->WriteFrame(input, 100, 104, 1.0f);
		tap->WriteFrame(input, 104, 108, 1.0f);
		tap->WriteFrame(input, 108, 112, 1.0f);

		AT_PORTABLE_TEST_ASSERT(context, channel->AsTraceChannel()->GetEventCount() == 2);
		AT_PORTABLE_TEST_ASSERT(context, channel->GetFrameBufferCount() == 1);
		AT_PORTABLE_TEST_ASSERT(context, channel->GetFrameBufferIndexForFrame(0) == 0);
		AT_PORTABLE_TEST_ASSERT(context, channel->GetFrameBufferIndexForFrame(1) == 0);
		AT_PORTABLE_TEST_ASSERT(context, channel->GetTimeForFrame(0) == 1.0);
		AT_PORTABLE_TEST_ASSERT(context, channel->GetTimeForFrame(1) == 5.0);

		FillXRGB(input, 0x00E0D0C0);
		tap->WriteFrame(input, 112, 116, 1.0f);
		tap->WriteFrame(input, 116, 120, 1.0f);

		AT_PORTABLE_TEST_ASSERT(context, channel->AsTraceChannel()->GetEventCount() == 3);
		AT_PORTABLE_TEST_ASSERT(context, channel->GetFrameBufferCount() == 2);
		AT_PORTABLE_TEST_ASSERT(context, channel->GetFrameBufferIndexForFrame(2) == 1);
		AT_PORTABLE_TEST_ASSERT(context, channel->GetTimeForFrame(2) == 9.0);
		AT_PORTABLE_TEST_ASSERT(context, channel->AsTraceChannel()->GetDuration() == 9.0);

		const VDPixmap& output = channel->GetFrameBufferByIndex(0);
		AT_PORTABLE_TEST_ASSERT(context, output.format == nsVDPixmap::kPixFormat_YUV420_Planar_Centered);
		AT_PORTABLE_TEST_ASSERT(context, channel->GetFrameSizeByIndex(0) == vdsize32(16, 16));
		AT_PORTABLE_TEST_ASSERT(context, channel->GetFrameSizeByIndex(1) == vdsize32(16, 16));
		AT_PORTABLE_TEST_ASSERT(context, channel->GetTraceSize() > 0);
		AT_PORTABLE_TEST_ASSERT(context, memoryTracker.GetSize() == channel->GetTraceSize());

		double frameTime = -1;
		AT_PORTABLE_TEST_ASSERT(context, channel->GetNearestFrameIndex(4.0, 6.0, frameTime) == 0);
		AT_PORTABLE_TEST_ASSERT(context, frameTime == 5.0);

		tracer->Shutdown();
		return true;
	}
}

bool ATTestAltirraTraceVideo(ATPortableTestContext& context) {
	return TestRawFrameStorage(context) && TestVideoTracer(context);
}
