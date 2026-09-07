// Altirra portable event and delegate tests

#include <at/attest/portabletest.h>
#include <vd2/system/event.h>

namespace {
	struct EventSource {
	};

	struct EventLog {
		void Add(int value) {
			if (mCount < 8)
				mValues[mCount] = value;

			++mCount;
		}

		int mValues[8] {};
		int mCount = 0;
	};

	class ValueListener {
	public:
		ValueListener(EventLog& log, EventSource& expectedSource, int id)
			: mLog(log), mExpectedSource(expectedSource), mId(id) {
		}

		void OnValue(EventSource *source, const int& value) {
			mSourceMatched = source == &mExpectedSource;
			mLog.Add(mId * 100 + value);
		}

		bool mSourceMatched = false;

	private:
		EventLog& mLog;
		EventSource& mExpectedSource;
		int mId;
	};

	class SelfRemovingListener {
	public:
		SelfRemovingListener(VDEvent<EventSource, int>& event, EventLog& log)
			: mEvent(event), mLog(log) {
		}

		void OnValue(EventSource *, const int& value) {
			mLog.Add(value);
			mEvent -= mDelegate;
		}

		VDEvent<EventSource, int>& mEvent;
		EventLog& mLog;
		VDDelegate mDelegate;
	};

	class ReferenceListener {
	public:
		void OnValue(EventSource *, int& value) {
			value += 7;
		}
	};

	struct FirstBase {
		int mPadding = 0;
	};

	struct SecondBase {
		int mPadding = 0;
	};

	class MultipleInheritanceListener : public FirstBase, public SecondBase {
	public:
		void OnValue(EventSource *, const int& value) {
			mValue = value;
		}

		int mValue = 0;
	};
}

bool ATTestSystemEvent(ATPortableTestContext& context) {
	EventSource source;
	EventLog log;
	VDEvent<EventSource, int> event;
	VDDelegate firstDelegate;
	VDDelegate secondDelegate;
	ValueListener first(log, source, 1);
	ValueListener second(log, source, 2);

	AT_PORTABLE_TEST_ASSERT(context, event.IsEmpty());
	event += firstDelegate(&first, &ValueListener::OnValue);
	event += secondDelegate(&second, &ValueListener::OnValue);
	AT_PORTABLE_TEST_ASSERT(context, !event.IsEmpty());

	event.Raise(&source, 3);
	AT_PORTABLE_TEST_ASSERT(context, log.mCount == 2);
	AT_PORTABLE_TEST_ASSERT(context, log.mValues[0] == 203);
	AT_PORTABLE_TEST_ASSERT(context, log.mValues[1] == 103);
	AT_PORTABLE_TEST_ASSERT(context, first.mSourceMatched && second.mSourceMatched);

	event -= secondDelegate;
	log.mCount = 0;
	event.Raise(&source, 4);
	AT_PORTABLE_TEST_ASSERT(context, log.mCount == 1 && log.mValues[0] == 104);
	event -= firstDelegate;
	AT_PORTABLE_TEST_ASSERT(context, event.IsEmpty());

	VDEvent<EventSource, int> selfRemovingEvent;
	EventLog selfRemovingLog;
	SelfRemovingListener selfRemoving(selfRemovingEvent, selfRemovingLog);
	selfRemovingEvent += selfRemoving.mDelegate(
		&selfRemoving, &SelfRemovingListener::OnValue);
	selfRemovingEvent.Raise(&source, 11);
	selfRemovingEvent.Raise(&source, 12);
	AT_PORTABLE_TEST_ASSERT(context,
		selfRemovingLog.mCount == 1 && selfRemovingLog.mValues[0] == 11);
	AT_PORTABLE_TEST_ASSERT(context, selfRemovingEvent.IsEmpty());

	VDEvent<EventSource, int&> referenceEvent;
	VDDelegate referenceDelegate;
	ReferenceListener referenceListener;
	referenceEvent += referenceDelegate.Bind(
		&referenceListener, &ReferenceListener::OnValue);
	int mutableValue = 5;
	referenceEvent.Raise(&source, mutableValue);
	AT_PORTABLE_TEST_ASSERT(context, mutableValue == 12);

	MultipleInheritanceListener multipleListener;
	VDDelegate multipleDelegate;
	VDEvent<EventSource, int> multipleEvent;
	multipleEvent += multipleDelegate(
		&multipleListener, &MultipleInheritanceListener::OnValue);
	multipleEvent.Raise(&source, 27);
	AT_PORTABLE_TEST_ASSERT(context, multipleListener.mValue == 27);

	VDDelegate reusableDelegate;
	ValueListener reusableListener(log, source, 3);
	{
		VDEvent<EventSource, int> temporaryEvent;
		temporaryEvent += reusableDelegate(
			&reusableListener, &ValueListener::OnValue);
	}
	AT_PORTABLE_TEST_ASSERT(context, reusableDelegate.mpPrev == &reusableDelegate);
	AT_PORTABLE_TEST_ASSERT(context, reusableDelegate.mpNext == &reusableDelegate);

	VDEvent<EventSource, int> detachOnDelegateDestruction;
	{
		VDDelegate temporaryDelegate;
		detachOnDelegateDestruction += temporaryDelegate(
			&reusableListener, &ValueListener::OnValue);
		AT_PORTABLE_TEST_ASSERT(context, !detachOnDelegateDestruction.IsEmpty());
	}
	AT_PORTABLE_TEST_ASSERT(context, detachOnDelegateDestruction.IsEmpty());

	return true;
}
