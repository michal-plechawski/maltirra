// Altirra portable simulator event manager tests

#include <functional>
#include <vector>

#include <at/attest/portabletest.h>
#include <simeventmanager.h>

namespace {
	class RecordingCallback final : public IATSimulatorCallback {
	public:
		RecordingCallback(std::vector<int>& calls, int value)
			: mCalls(calls), mValue(value) {}

		void OnSimulatorEvent(ATSimulatorEvent) override {
			mCalls.push_back(mValue);
			if (mAction)
				mAction();
		}

		std::function<void()> mAction;

	private:
		std::vector<int>& mCalls;
		int mValue;
	};
}

bool ATTestAltirraSimEventManager(ATPortableTestContext& context) {
	ATSimulatorEventManager manager;
	std::vector<int> calls;
	RecordingCallback first(calls, 1);
	RecordingCallback second(calls, 2);
	RecordingCallback late(calls, 3);

	manager.AddCallback(&first);
	manager.AddCallback(&second);
	manager.AddCallback(&first);
	manager.NotifyEvent(kATSimEvent_None);
	manager.NotifyEvent(kATSimEvent_AnonymousPause);
	manager.NotifyEvent(kATSimEvent_AnonymousInterrupt);
	AT_PORTABLE_TEST_ASSERT(context, calls.empty());

	manager.NotifyEvent(kATSimEvent_FrameTick);
	AT_PORTABLE_TEST_ASSERT(context, calls == std::vector<int>({1, 2}));
	calls.clear();

	first.mAction = [&] {
		manager.RemoveCallback(&second);
		manager.AddCallback(&late);
	};
	manager.NotifyEvent(kATSimEvent_FrameTick);
	AT_PORTABLE_TEST_ASSERT(context, calls == std::vector<int>({1}));
	calls.clear();
	manager.NotifyEvent(kATSimEvent_FrameTick);
	AT_PORTABLE_TEST_ASSERT(context, calls == std::vector<int>({1, 3}));
	calls.clear();

	manager.RemoveCallback(&first);
	manager.NotifyEvent(kATSimEvent_WarmReset);
	AT_PORTABLE_TEST_ASSERT(context, calls == std::vector<int>({3}));
	manager.RemoveCallback(&late);
	manager.RemoveCallback(&second);
	calls.clear();

	const uint32 oldId = manager.AddEventCallback(kATSimEvent_FrameTick,
		[&] { calls.push_back(10); });
	std::vector<uint32> newIds;
	const uint32 mutatingId = manager.AddEventCallback(kATSimEvent_FrameTick,
		[&] {
			calls.push_back(20);
			// Growing the table during a callback must not destroy the
			// callable that is currently executing.
			for(int i = 0; i < 32; ++i) {
				newIds.push_back(manager.AddEventCallback(kATSimEvent_FrameTick,
					[&, i] { calls.push_back(30 + i); }));
			}
			manager.RemoveEventCallback(oldId);
		});
	AT_PORTABLE_TEST_ASSERT(context, oldId != 0 && mutatingId != 0);
	AT_PORTABLE_TEST_ASSERT(context, oldId != mutatingId);
	manager.NotifyEvent(kATSimEvent_FrameTick);
	AT_PORTABLE_TEST_ASSERT(context, calls == std::vector<int>({20}));
	AT_PORTABLE_TEST_ASSERT(context, newIds.size() == 32);
	AT_PORTABLE_TEST_ASSERT(context, newIds.front() != 0);
	calls.clear();

	manager.RemoveEventCallback(mutatingId);
	manager.NotifyEvent(kATSimEvent_FrameTick);
	AT_PORTABLE_TEST_ASSERT(context, calls.size() == 32);
	for(int i = 0; i < 32; ++i)
		AT_PORTABLE_TEST_ASSERT(context, calls[i] == 61 - i);
	for(uint32 id : newIds)
		manager.RemoveEventCallback(id);
	calls.clear();
	manager.NotifyEvent(kATSimEvent_FrameTick);
	AT_PORTABLE_TEST_ASSERT(context, calls.empty());

	ATSimulatorEventManager nestedManager;
	RecordingCallback outer(calls, 4);
	RecordingCallback inner(calls, 5);
	bool nested = false;
	outer.mAction = [&] {
		if (!nested) {
			nested = true;
			nestedManager.NotifyEvent(kATSimEvent_WarmReset);
		}
	};
	nestedManager.AddCallback(&outer);
	nestedManager.AddCallback(&inner);
	nestedManager.NotifyEvent(kATSimEvent_ColdReset);
	AT_PORTABLE_TEST_ASSERT(context, calls == std::vector<int>({4, 4, 5, 5}));
	nestedManager.Shutdown();

	return true;
}
