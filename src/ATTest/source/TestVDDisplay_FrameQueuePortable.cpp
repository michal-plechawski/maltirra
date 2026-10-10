// Portable display frame ownership, recycling, and cancellation tests.

#include <atomic>
#include <thread>
#include <vd2/VDDisplay/internal/framequeue.h>
#include <at/attest/portabletest.h>

namespace {
	class TestFrame final : public VDVideoDisplayFrame {
	public:
		TestFrame(std::atomic<int>& destroyed, VDDisplayFrameQueue *queue = nullptr)
			: mDestroyed(destroyed), mpQueue(queue) {}
		~TestFrame() {
			// Exercise destructor reentrancy: queue must release outside its lock.
			if (mpQueue)
				(void)mpQueue->GetQueuedFrames();
			++mDestroyed;
		}
	private:
		std::atomic<int>& mDestroyed;
		VDDisplayFrameQueue *mpQueue;
	};
}

bool ATTestVDDisplayFrameQueue(ATPortableTestContext& context) {
	std::atomic<int> destroyed { 0 };
	VDDisplayFrameQueue queue;
	AT_PORTABLE_TEST_ASSERT(context, !queue.Post(nullptr));
	AT_PORTABLE_TEST_ASSERT(context, !queue.Begin().mpFrame);
	AT_PORTABLE_TEST_ASSERT(context, !queue.Revoke(true, nullptr));

	vdrefptr<VDVideoDisplayFrame> first(new TestFrame(destroyed, &queue));
	vdrefptr<VDVideoDisplayFrame> second(new TestFrame(destroyed, &queue));
	vdrefptr<VDVideoDisplayFrame> third(new TestFrame(destroyed, &queue));
	AT_PORTABLE_TEST_ASSERT(context, queue.Post(first));
	AT_PORTABLE_TEST_ASSERT(context, !queue.Post(first));
	first = nullptr; // Queue must retain the producer's released reference.
	AT_PORTABLE_TEST_ASSERT(context, destroyed == 0);
	AT_PORTABLE_TEST_ASSERT(context, queue.Post(second));
	AT_PORTABLE_TEST_ASSERT(context, queue.Post(third));
	AT_PORTABLE_TEST_ASSERT(context, queue.GetQueuedFrames() == 3);

	vdrefptr<VDVideoDisplayFrame> revoked;
	AT_PORTABLE_TEST_ASSERT(context, queue.Revoke(true, ~revoked));
	AT_PORTABLE_TEST_ASSERT(context, revoked == third);
	AT_PORTABLE_TEST_ASSERT(context, queue.GetQueuedFrames() == 2);
	auto firstLease = queue.Begin();
	AT_PORTABLE_TEST_ASSERT(context, firstLease.mpFrame != second);
	AT_PORTABLE_TEST_ASSERT(context, queue.IsFramePending());
	AT_PORTABLE_TEST_ASSERT(context, !queue.Begin().mpFrame);
	AT_PORTABLE_TEST_ASSERT(context, !queue.Revoke(true, ~revoked));
	AT_PORTABLE_TEST_ASSERT(context, queue.Complete(firstLease));
	AT_PORTABLE_TEST_ASSERT(context, !queue.Complete(firstLease));
	AT_PORTABLE_TEST_ASSERT(context, queue.GetLastFrame() == firstLease.mpFrame);
	AT_PORTABLE_TEST_ASSERT(context, !queue.IsFramePending());
	AT_PORTABLE_TEST_ASSERT(context, queue.GetQueuedFrames() == 1);

	auto secondLease = queue.Begin();
	AT_PORTABLE_TEST_ASSERT(context, secondLease.mpFrame == second);
	AT_PORTABLE_TEST_ASSERT(context, queue.Complete(secondLease));
	AT_PORTABLE_TEST_ASSERT(context, queue.Revoke(false, ~revoked));
	AT_PORTABLE_TEST_ASSERT(context, revoked == firstLease.mpFrame);
	AT_PORTABLE_TEST_ASSERT(context, !queue.Revoke(false, ~revoked));
	firstLease = {};
	revoked = nullptr;
	AT_PORTABLE_TEST_ASSERT(context, destroyed == 1);

	// A failed presentation must preserve the last successfully cached frame.
	AT_PORTABLE_TEST_ASSERT(context, queue.Post(third));
	auto failedLease = queue.Begin();
	AT_PORTABLE_TEST_ASSERT(context, queue.Complete(failedLease, false));
	AT_PORTABLE_TEST_ASSERT(context, queue.GetLastFrame() == second);
	AT_PORTABLE_TEST_ASSERT(context, queue.Revoke(false, ~revoked));
	AT_PORTABLE_TEST_ASSERT(context, revoked == third);
	failedLease = {};
	revoked = nullptr;

	// kDoNotCache releases a frame instead of holding it for redraw/recycling.
	third->mFlags = IVDVideoDisplay::kDoNotCache;
	AT_PORTABLE_TEST_ASSERT(context, queue.Post(third));
	third = nullptr;
	auto uncachedLease = queue.Begin();
	AT_PORTABLE_TEST_ASSERT(context, queue.Complete(uncachedLease));
	AT_PORTABLE_TEST_ASSERT(context, !queue.GetLastFrame());
	uncachedLease = {};
	AT_PORTABLE_TEST_ASSERT(context, destroyed == 2);

	// Flush cancels an in-flight lease, but must not free its borrowed pixels.
	AT_PORTABLE_TEST_ASSERT(context, queue.Revoke(false, ~revoked));
	AT_PORTABLE_TEST_ASSERT(context, revoked == second);
	AT_PORTABLE_TEST_ASSERT(context, queue.Post(second));
	auto staleLease = queue.Begin();
	queue.Flush();
	AT_PORTABLE_TEST_ASSERT(context, !queue.IsFramePending());
	AT_PORTABLE_TEST_ASSERT(context, queue.GetQueuedFrames() == 0);
	AT_PORTABLE_TEST_ASSERT(context, !queue.Complete(staleLease));
	AT_PORTABLE_TEST_ASSERT(context, queue.Post(second));
	auto newLease = queue.Begin();
	AT_PORTABLE_TEST_ASSERT(context, newLease.mSequence != staleLease.mSequence);
	AT_PORTABLE_TEST_ASSERT(context, !queue.Complete(staleLease));
	AT_PORTABLE_TEST_ASSERT(context, queue.Complete(newLease));
	staleLease = {};
	newLease = {};
	secondLease = {};
	revoked = nullptr;
	second = nullptr;
	queue.Flush();
	AT_PORTABLE_TEST_ASSERT(context, destroyed == 3);

	// Producer and consumer operate concurrently; FIFO order and references
	// must survive without leaking or exposing a frame that is still active.
	constexpr int count = 128;
	std::atomic<bool> orderValid { true };
	std::thread producer([&] {
		for(int i = 0; i < count; ++i) {
			vdrefptr<VDVideoDisplayFrame> frame(new TestFrame(destroyed, &queue));
			frame->mFrameNumber = (uint32)i;
			frame->mFlags = IVDVideoDisplay::kDoNotCache;
			if (!queue.Post(frame))
				orderValid = false;
		}
	});
	std::thread consumer([&] {
		for(int i = 0; i < count;) {
			auto lease = queue.Begin();
			if (!lease.mpFrame) {
				std::this_thread::yield();
				continue;
			}
			if (lease.mpFrame->mFrameNumber != (uint32)i || !queue.Complete(lease))
				orderValid = false;
			++i;
		}
	});
	producer.join();
	consumer.join();
	AT_PORTABLE_TEST_ASSERT(context, orderValid);
	AT_PORTABLE_TEST_ASSERT(context, queue.GetQueuedFrames() == 0);
	AT_PORTABLE_TEST_ASSERT(context, destroyed == 3 + count);
	return true;
}
