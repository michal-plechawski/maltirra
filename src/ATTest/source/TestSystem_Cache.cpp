// Altirra portable cache and object-pool tests

#include <atomic>
#include <thread>
#include <vector>

#include <at/attest/portabletest.h>
#include <vd2/system/cache.h>

namespace {
	struct VDFixedMapTestNode : public vdfixedhashmap_node {
		explicit VDFixedMapTestNode(int key)
			: mHashKey(key) {
		}

		int mHashKey;
	};

	struct VDCacheTestStats {
		std::atomic<int> mAllocations { 0 };
		std::atomic<int> mDestructions { 0 };
		std::atomic<int> mEvictions { 0 };
		std::atomic<int> mPendingAborts { 0 };
		std::atomic<int> mStatusDumps { 0 };
		std::atomic<sint64> mBlockedEvictionKey { -1 };
		VDSignal mEvictionEntered;
		VDSignal mEvictionRelease;
	};

	class VDCacheTestObject final : public VDCachedObject {
	public:
		explicit VDCacheTestObject(VDCacheTestStats& stats)
			: mStats(stats) {
		}

		~VDCacheTestObject() override {
			++mStats.mDestructions;
		}

		void SetValid(bool valid) {
			mbValid = valid;
		}

		VDCacheState State() const {
			return GetState();
		}

		sint64 Key() const {
			return GetCacheKey();
		}

	protected:
		bool IsValid() const override {
			return mbValid;
		}

		void OnCacheEvict() override {
			mbValid = false;
			++mStats.mEvictions;

			if (mStats.mBlockedEvictionKey.load() == Key()) {
				mStats.mEvictionEntered.signal();
				mStats.mEvictionRelease.wait();
			}
		}

		void OnCacheAbortPending() override {
			++mStats.mPendingAborts;
		}

		void DumpStatus() override {
			++mStats.mStatusDumps;
		}

	private:
		VDCacheTestStats& mStats;
		bool mbValid = true;
	};

	class VDCacheTestAllocator final : public IVDCacheAllocator {
	public:
		explicit VDCacheTestAllocator(VDCacheTestStats& stats)
			: mStats(stats) {
		}

		VDCachedObject *OnCacheAllocate() override {
			++mStats.mAllocations;
			return new VDCacheTestObject(mStats);
		}

	private:
		VDCacheTestStats& mStats;
	};

	struct VDPoolTestStats {
		std::atomic<int> mAllocations { 0 };
		std::atomic<int> mDestructions { 0 };
	};

	class VDPoolTestObject final : public VDPooledObject {
	public:
		explicit VDPoolTestObject(VDPoolTestStats& stats)
			: mStats(stats) {
		}

		~VDPoolTestObject() override {
			++mStats.mDestructions;
		}

	private:
		VDPoolTestStats& mStats;
	};

	class VDPoolTestAllocator final : public IVDPoolAllocator {
	public:
		explicit VDPoolTestAllocator(VDPoolTestStats& stats)
			: mStats(stats) {
		}

		VDPooledObject *OnPoolAllocate() override {
			++mStats.mAllocations;
			return new VDPoolTestObject(mStats);
		}

	private:
		VDPoolTestStats& mStats;
	};
}

bool ATTestSystemCache(ATPortableTestContext& context) {
	{
		vdfixedhashmap<int, VDFixedMapTestNode, vdhash<int>, 4> map;
		AT_PORTABLE_TEST_ASSERT(context, map.begin() == map.end());

		VDFixedMapTestNode first(1);
		VDFixedMapTestNode collision(5);
		VDFixedMapTestNode other(2);
		map.insert(&first);
		map.insert(&collision);
		map.insert(&other);

		AT_PORTABLE_TEST_ASSERT(context, map[1] == &first);
		AT_PORTABLE_TEST_ASSERT(context, map[5] == &collision);
		AT_PORTABLE_TEST_ASSERT(context, map[2] == &other);
		AT_PORTABLE_TEST_ASSERT(context, map[9] == nullptr);
		AT_PORTABLE_TEST_ASSERT(context, map.find(5) != map.end());

		int count = 0;
		int keySum = 0;
		for(auto it = map.begin(); it != map.end(); ++it) {
			++count;
			keySum += it->mHashKey;
		}
		AT_PORTABLE_TEST_ASSERT(context, count == 3);
		AT_PORTABLE_TEST_ASSERT(context, keySum == 8);

		map.erase(&collision);
		AT_PORTABLE_TEST_ASSERT(context, map[5] == nullptr);
		map.erase(map.find(1));
		AT_PORTABLE_TEST_ASSERT(context, map[1] == nullptr);
		AT_PORTABLE_TEST_ASSERT(context, map.begin()->mHashKey == 2);
	}

	{
		VDCacheTestStats stats;
		VDCacheTestAllocator allocator(stats);
		VDCache cache(&allocator);
		bool isNew = false;
		auto *object = static_cast<VDCacheTestObject *>(cache.Create(42, isNew));
		AT_PORTABLE_TEST_ASSERT(context, isNew);
		AT_PORTABLE_TEST_ASSERT(context, object->Key() == 42);
		AT_PORTABLE_TEST_ASSERT(context, object->State() == kVDCacheStatePending);
		AT_PORTABLE_TEST_ASSERT(context, cache.GetStateCount(kVDCacheStatePending) == 1);

		auto *duplicate = static_cast<VDCacheTestObject *>(cache.Create(42, isNew));
		AT_PORTABLE_TEST_ASSERT(context, !isNew);
		AT_PORTABLE_TEST_ASSERT(context, duplicate == object);
		AT_PORTABLE_TEST_ASSERT(context, duplicate->Release() == 1);

		cache.Schedule(object);
		AT_PORTABLE_TEST_ASSERT(context, object->State() == kVDCacheStateReady);
		AT_PORTABLE_TEST_ASSERT(context, cache.GetStateCount(kVDCacheStateReady) == 1);
		auto *ready = static_cast<VDCacheTestObject *>(cache.GetNextReady());
		AT_PORTABLE_TEST_ASSERT(context, ready == object);
		AT_PORTABLE_TEST_ASSERT(context, ready->State() == kVDCacheStateActive);
		cache.MarkCompleted(ready);
		AT_PORTABLE_TEST_ASSERT(context, ready->State() == kVDCacheStateComplete);
		AT_PORTABLE_TEST_ASSERT(context, ready->Release() == 1);
		AT_PORTABLE_TEST_ASSERT(context, object->Release() == 0);
		AT_PORTABLE_TEST_ASSERT(context, object->State() == kVDCacheStateIdle);
		AT_PORTABLE_TEST_ASSERT(context, cache.GetStateCount(kVDCacheStateIdle) == 1);

		auto *reused = static_cast<VDCacheTestObject *>(cache.Create(42, isNew));
		AT_PORTABLE_TEST_ASSERT(context, !isNew);
		AT_PORTABLE_TEST_ASSERT(context, reused == object);
		AT_PORTABLE_TEST_ASSERT(context, reused->State() == kVDCacheStateComplete);
		AT_PORTABLE_TEST_ASSERT(context, reused->Release() == 0);
		cache.DumpListStatus(kVDCacheStateIdle);
		AT_PORTABLE_TEST_ASSERT(context, stats.mStatusDumps.load() == 1);

		cache.Shutdown();
		AT_PORTABLE_TEST_ASSERT(context, stats.mEvictions.load() == 1);
		AT_PORTABLE_TEST_ASSERT(context, stats.mDestructions.load() == 1);
	}

	{
		VDCacheTestStats stats;
		VDCacheTestAllocator allocator(stats);
		VDCache cache(&allocator);
		bool isNew = false;
		auto *aborted = static_cast<VDCacheTestObject *>(cache.Allocate(7));
		aborted->SetValid(false);
		AT_PORTABLE_TEST_ASSERT(context, aborted->Release() == 0);
		AT_PORTABLE_TEST_ASSERT(context, aborted->State() == kVDCacheStateFree);
		AT_PORTABLE_TEST_ASSERT(context, stats.mPendingAborts.load() == 1);
		AT_PORTABLE_TEST_ASSERT(context, cache.GetStateCount(kVDCacheStateFree) == 1);

		auto *reused = static_cast<VDCacheTestObject *>(cache.Create(7, isNew));
		AT_PORTABLE_TEST_ASSERT(context, isNew);
		AT_PORTABLE_TEST_ASSERT(context, reused == aborted);
		reused->SetValid(true);
		cache.MarkCompleted(reused);
		AT_PORTABLE_TEST_ASSERT(context, reused->Release() == 0);
		AT_PORTABLE_TEST_ASSERT(context, reused->State() == kVDCacheStateIdle);
		cache.Shutdown();
		AT_PORTABLE_TEST_ASSERT(context, stats.mAllocations.load() == 1);
		AT_PORTABLE_TEST_ASSERT(context, stats.mDestructions.load() == 1);
	}

	{
		VDCacheTestStats stats;
		VDCacheTestAllocator allocator(stats);
		VDCache cache(&allocator);
		std::vector<VDCacheTestObject *> objects;
		for(int index = 0; index < 16; ++index) {
			bool isNew = false;
			auto *object = static_cast<VDCacheTestObject *>(cache.Create(index, isNew));
			AT_PORTABLE_TEST_ASSERT(context, isNew);
			objects.push_back(object);
			object->Release();
		}

		bool isNew = false;
		auto *replacement = static_cast<VDCacheTestObject *>(cache.Create(1000, isNew));
		AT_PORTABLE_TEST_ASSERT(context, isNew);
		AT_PORTABLE_TEST_ASSERT(context, replacement == objects.front());
		AT_PORTABLE_TEST_ASSERT(context, replacement->Key() == 1000);
		AT_PORTABLE_TEST_ASSERT(context, stats.mAllocations.load() == 16);
		AT_PORTABLE_TEST_ASSERT(context, stats.mEvictions.load() == 1);
		replacement->SetValid(true);
		replacement->Release();
		cache.Shutdown();
		AT_PORTABLE_TEST_ASSERT(context, stats.mDestructions.load() == 16);
	}

	{
		VDCacheTestStats stats;
		VDCacheTestAllocator allocator(stats);
		VDCache cache(&allocator);
		std::vector<VDCacheTestObject *> objects;
		for(int index = 0; index < 16; ++index) {
			bool isNew = false;
			auto *object = static_cast<VDCacheTestObject *>(cache.Create(index, isNew));
			objects.push_back(object);
			object->Release();
		}

		stats.mBlockedEvictionKey = 0;
		VDCacheTestObject *evictingReplacement = nullptr;
		bool evictingReplacementIsNew = false;
		std::thread evictor([&] {
			evictingReplacement = static_cast<VDCacheTestObject *>(
				cache.Create(1000, evictingReplacementIsNew));
		});

		const bool evictionEntered = stats.mEvictionEntered.tryWait(5000);
		VDCacheTestObject *sameKeyReplacement = nullptr;
		bool sameKeyReplacementIsNew = false;
		if (evictionEntered) {
			sameKeyReplacement = static_cast<VDCacheTestObject *>(
				cache.Create(0, sameKeyReplacementIsNew));
			sameKeyReplacement->Release();
		}
		stats.mEvictionRelease.signal();
		evictor.join();

		AT_PORTABLE_TEST_ASSERT(context, evictionEntered);
		AT_PORTABLE_TEST_ASSERT(context, sameKeyReplacementIsNew);
		AT_PORTABLE_TEST_ASSERT(context, sameKeyReplacement != objects.front());
		AT_PORTABLE_TEST_ASSERT(context, evictingReplacementIsNew);
		AT_PORTABLE_TEST_ASSERT(context, evictingReplacement != nullptr);
		evictingReplacement->SetValid(true);
		evictingReplacement->Release();
		stats.mBlockedEvictionKey = -1;
		cache.Shutdown();
		AT_PORTABLE_TEST_ASSERT(context, stats.mDestructions.load() == 16);
	}

	{
		VDCacheTestStats stats;
		VDCacheTestAllocator allocator(stats);
		VDCache cache(&allocator);
		VDSignalPersistent start;
		std::vector<VDCacheTestObject *> objects(8, nullptr);
		std::vector<std::thread> threads;
		for(size_t index = 0; index < objects.size(); ++index) {
			threads.emplace_back([&cache, &start, &objects, index] {
				start.wait();
				bool isNew = false;
				auto *object = static_cast<VDCacheTestObject *>(cache.Create(777, isNew));
				objects[index] = object;
				object->Release();
			});
		}
		start.signal();
		for(auto& thread : threads)
			thread.join();

		for(auto *object : objects)
			AT_PORTABLE_TEST_ASSERT(context, object == objects.front());
		AT_PORTABLE_TEST_ASSERT(context, stats.mAllocations.load() == 1);
		AT_PORTABLE_TEST_ASSERT(context, cache.GetStateCount(kVDCacheStateIdle) == 1);
		cache.Shutdown();
		AT_PORTABLE_TEST_ASSERT(context, stats.mDestructions.load() == 1);
	}

	{
		VDPoolTestStats stats;
		VDPoolTestAllocator allocator(stats);
		VDPool pool(&allocator);
		auto *first = static_cast<VDPoolTestObject *>(pool.Allocate());
		AT_PORTABLE_TEST_ASSERT(context, first->Release() == 0);
		auto *reused = static_cast<VDPoolTestObject *>(pool.Allocate());
		AT_PORTABLE_TEST_ASSERT(context, reused == first);
		AT_PORTABLE_TEST_ASSERT(context, reused->AddRef() == 2);
		AT_PORTABLE_TEST_ASSERT(context, reused->Release() == 1);
		auto *second = static_cast<VDPoolTestObject *>(pool.Allocate());
		AT_PORTABLE_TEST_ASSERT(context, second != reused);
		second->Release();
		reused->Release();
		pool.Shutdown();
		AT_PORTABLE_TEST_ASSERT(context, stats.mAllocations.load() == 2);
		AT_PORTABLE_TEST_ASSERT(context, stats.mDestructions.load() == 2);
	}

	{
		VDPoolTestStats stats;
		VDPoolTestAllocator allocator(stats);
		VDPool pool(&allocator);
		std::vector<VDPooledObject *> objects;
		for(int index = 0; index < 17; ++index)
			objects.push_back(pool.Allocate());
		for(auto *object : objects)
			object->Release();
		AT_PORTABLE_TEST_ASSERT(context, stats.mAllocations.load() == 17);
		AT_PORTABLE_TEST_ASSERT(context, stats.mDestructions.load() == 1);
		pool.Shutdown();
		AT_PORTABLE_TEST_ASSERT(context, stats.mDestructions.load() == 17);
	}

	{
		VDPoolTestStats stats;
		VDPoolTestAllocator allocator(stats);
		VDPool pool(&allocator);
		auto *active = pool.Allocate();
		pool.Shutdown();
		AT_PORTABLE_TEST_ASSERT(context, stats.mDestructions.load() == 0);
		active->Release();
		AT_PORTABLE_TEST_ASSERT(context, stats.mDestructions.load() == 1);
	}

	return true;
}
