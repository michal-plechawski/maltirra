// Altirra portable execution trace tests

#include <cmath>
#include <cwchar>
#include <memory>

#include <at/attest/portabletest.h>
#include <trace.h>

namespace {
	bool Near(double actual, double expected) {
		return std::abs(actual - expected) < 1e-9;
	}
}

bool ATTestAltirraTrace(ATPortableTestContext& context) {
	vdrefptr<ATTraceCollection> collection { new ATTraceCollection };
	AT_PORTABLE_TEST_ASSERT(context, collection->GetGroupCount() == 0);
	AT_PORTABLE_TEST_ASSERT(
		context,
		collection->GetGroupByType(kATTraceGroupType_CPUHistory) == nullptr);

	ATTraceGroup *group = collection->AddGroup(L"CPU", kATTraceGroupType_CPUHistory);
	AT_PORTABLE_TEST_ASSERT(context, collection->GetGroupCount() == 1);
	AT_PORTABLE_TEST_ASSERT(context, collection->GetGroup(0) == group);
	AT_PORTABLE_TEST_ASSERT(
		context,
		collection->GetGroupByType(kATTraceGroupType_CPUHistory) == group);
	AT_PORTABLE_TEST_ASSERT(context, group->GetType() == kATTraceGroupType_CPUHistory);
	AT_PORTABLE_TEST_ASSERT(context, !wcscmp(group->GetName(), L"CPU"));
	AT_PORTABLE_TEST_ASSERT(context, Near(group->GetDuration(), 0));

	ATTraceChannelSimple *simple = group->AddSimpleChannel(1000, 0.01, L"IRQ");
	AT_PORTABLE_TEST_ASSERT(context, group->GetChannelCount() == 1);
	AT_PORTABLE_TEST_ASSERT(context, group->GetChannel(0) == simple);
	AT_PORTABLE_TEST_ASSERT(context, !wcscmp(simple->GetName(), L"IRQ"));
	AT_PORTABLE_TEST_ASSERT(context, simple->IsEmpty());
	AT_PORTABLE_TEST_ASSERT(context, simple->AsInterface(0) == nullptr);
	AT_PORTABLE_TEST_ASSERT(
		context,
		simple->AsInterface(ATTraceChannelTickBased::kTypeID) == simple);

	simple->AddTickEvent(1000, 1050, L"first", kATTraceColor_CPUThread_Main);
	simple->AddTickEvent(1100, 1110, L"second", kATTraceColor_CPUThread_IRQ);
	AT_PORTABLE_TEST_ASSERT(context, simple->GetEventCount() == 2);
	AT_PORTABLE_TEST_ASSERT(context, !simple->IsEmpty());
	AT_PORTABLE_TEST_ASSERT(context, Near(simple->GetDuration(), 1.1));
	AT_PORTABLE_TEST_ASSERT(context, Near(group->GetDuration(), 1.1));
	AT_PORTABLE_TEST_ASSERT(context, simple->GetTraceSize() > 0);

	// The new threshold, not the threshold from a previous iteration, decides
	// whether an older event may overlap the requested start time.
	simple->StartIteration(0, 2, 2.0);
	simple->StartIteration(1.0, 2.0, 0.1);
	ATTraceEvent event {};
	AT_PORTABLE_TEST_ASSERT(context, simple->GetNextEvent(event));
	AT_PORTABLE_TEST_ASSERT(context, Near(event.mEventStart, 1.0));
	AT_PORTABLE_TEST_ASSERT(context, Near(event.mEventStop, 1.1));
	AT_PORTABLE_TEST_ASSERT(context, !wcscmp(event.mpName, L"second"));
	AT_PORTABLE_TEST_ASSERT(context, event.mBgColor == kATTraceColor_CPUThread_IRQ);
	AT_PORTABLE_TEST_ASSERT(context, !simple->GetNextEvent(event));

	simple->StartIteration(0.2, 0.9, 0.0);
	AT_PORTABLE_TEST_ASSERT(context, simple->GetNextEvent(event));
	AT_PORTABLE_TEST_ASSERT(context, !wcscmp(event.mpName, L"first"));
	AT_PORTABLE_TEST_ASSERT(context, !simple->GetNextEvent(event));

	simple->AddOpenTickEvent(1200, L"open", kATTraceColor_CPUThread_SIO);
	AT_PORTABLE_TEST_ASSERT(context, simple->GetEventCount() == 3);
	AT_PORTABLE_TEST_ASSERT(context, Near(simple->GetDuration(), kATTraceTime_Infinity));
	simple->TruncateLastEvent(1250);
	AT_PORTABLE_TEST_ASSERT(context, Near(simple->GetDuration(), 2.5));
	simple->TruncateLastEvent(1150);
	AT_PORTABLE_TEST_ASSERT(context, simple->GetEventCount() == 2);
	AT_PORTABLE_TEST_ASSERT(context, Near(simple->GetDuration(), 1.1));

	vdrefptr<ATTraceChannelStringTable> strings {
		new ATTraceChannelStringTable(0, 0.5, L"Strings")
	};
	strings->AddString(L"known");
	strings->AddTickEvent(2, 4, 0, kATTraceColor_Default);
	strings->AddTickEvent(4, 6, 5, kATTraceColor_Default);
	strings->StartIteration(0, 4, 0);
	AT_PORTABLE_TEST_ASSERT(context, strings->GetNextEvent(event));
	AT_PORTABLE_TEST_ASSERT(context, !wcscmp(event.mpName, L"known"));
	AT_PORTABLE_TEST_ASSERT(context, strings->GetNextEvent(event));
	AT_PORTABLE_TEST_ASSERT(context, !wcscmp(event.mpName, L""));
	AT_PORTABLE_TEST_ASSERT(context, !strings->GetNextEvent(event));

	std::weak_ptr<int> heldValue;
	{
		vdrefptr<ATTraceChannelFormatted> formatted {
			new ATTraceChannelFormatted(0, 1.0, L"Formatted")
		};
		auto value = std::make_shared<int>(42);
		heldValue = value;
		formatted->AddTickEvent(1, 2,
			[hold = value](VDStringW& name) { name = L"formatted"; },
			kATTraceColor_Default);
		value.reset();
		AT_PORTABLE_TEST_ASSERT(context, !heldValue.expired());
		formatted->StartIteration(0, 3, 0);
		AT_PORTABLE_TEST_ASSERT(context, formatted->GetNextEvent(event));
		AT_PORTABLE_TEST_ASSERT(context, !wcscmp(event.mpName, L"formatted"));
		AT_PORTABLE_TEST_ASSERT(context, formatted->GetTraceSize() > 0);
	}
	AT_PORTABLE_TEST_ASSERT(context, heldValue.expired());

	return true;
}
