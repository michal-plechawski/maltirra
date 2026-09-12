// Altirra portable POKEY trace tests

#include <cmath>
#include <cwchar>

#include <at/attest/portabletest.h>
#include <pokeytrace.h>
#include <trace.h>

bool ATTestAltirraPokeyTrace(ATPortableTestContext& context) {
	vdrefptr<ATTraceCollection> collection { new ATTraceCollection };
	ATTraceContext traceContext {};
	traceContext.mBaseTime = 500;
	traceContext.mBaseTickScale = 0.01;
	traceContext.mpCollection = collection;

	ATPokeyTracer tracer(traceContext);
	AT_PORTABLE_TEST_ASSERT(context, collection->GetGroupCount() == 1);
	ATTraceGroup *group = collection->GetGroup(0);
	AT_PORTABLE_TEST_ASSERT(context, !wcscmp(group->GetName(), L"POKEY"));
	AT_PORTABLE_TEST_ASSERT(context, group->GetChannelCount() == 1);

	IATTraceChannel *rawChannel = group->GetChannel(0);
	AT_PORTABLE_TEST_ASSERT(context, !wcscmp(rawChannel->GetName(), L"IRQ"));
	auto *channel = static_cast<ATTraceChannelTickBased *>(
		rawChannel->AsInterface(ATTraceChannelTickBased::kTypeID));
	AT_PORTABLE_TEST_ASSERT(context, channel != nullptr);
	AT_PORTABLE_TEST_ASSERT(context, channel->GetBaseTick() == 500);
	AT_PORTABLE_TEST_ASSERT(context, std::abs(channel->GetSecondsPerTick() - 0.01) < 1e-9);

	IATPokeyTraceOutput& output = tracer;
	output.AddIRQ(510, 520);
	output.AddIRQ(530, 540);
	AT_PORTABLE_TEST_ASSERT(context, channel->GetEventCount() == 2);
	channel->StartIteration(0, 1, 0);
	ATTraceEvent event {};
	AT_PORTABLE_TEST_ASSERT(context, channel->GetNextEvent(event));
	AT_PORTABLE_TEST_ASSERT(context, std::abs(event.mEventStart - 0.1) < 1e-9);
	AT_PORTABLE_TEST_ASSERT(context, std::abs(event.mEventStop - 0.2) < 1e-9);
	AT_PORTABLE_TEST_ASSERT(context, !wcscmp(event.mpName, L"IRQ"));
	AT_PORTABLE_TEST_ASSERT(context, event.mBgColor == kATTraceColor_Default);
	AT_PORTABLE_TEST_ASSERT(context, channel->GetNextEvent(event));
	AT_PORTABLE_TEST_ASSERT(context, std::abs(event.mEventStart - 0.3) < 1e-9);
	AT_PORTABLE_TEST_ASSERT(context, std::abs(event.mEventStop - 0.4) < 1e-9);
	AT_PORTABLE_TEST_ASSERT(context, !channel->GetNextEvent(event));
	return true;
}
