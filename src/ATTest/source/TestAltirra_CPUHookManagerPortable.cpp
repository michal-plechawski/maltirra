// Altirra portable CPU hook manager tests

#include <memory>
#include <vector>

#include <at/attest/portabletest.h>
#include <cpuhookmanager.h>

namespace {
	struct HookChange {
		uint16 mPC;
		bool mbEnable;

		bool operator==(const HookChange& other) const {
			return mPC == other.mPC && mbEnable == other.mbEnable;
		}
	};

	class CPUStub {
	public:
		void SetHook(uint16 pc, bool enable) {
			mChanges.push_back({ pc, enable });
		}

		std::vector<HookChange> mChanges;
	};

	class MMUStub {
	public:
		bool IsKernelROMEnabled() const { return mbKernelROMEnabled; }

		bool mbKernelROMEnabled = false;
	};

	class PBIStub {
	public:
		bool IsROMOverlayActive() const { return mbROMOverlayActive; }

		bool mbROMOverlayActive = false;
	};
}

bool ATTestAltirraCPUHookManager(ATPortableTestContext& context) {
	CPUStub cpu;
	MMUStub mmu;
	PBIStub pbi;
	ATCPUHookManager hooks;
	hooks.Init(&cpu, &mmu, &pbi);

	std::vector<int> callOrder;
	uint16 lowPriorityPC = 0;
	ATCPUHookNode *lowPriority = hooks.AddHook(
		kATCPUHookMode_Always, 0x1234, 1,
		[&](uint16 pc) {
			lowPriorityPC = pc;
			callOrder.push_back(1);
			return uint8(0x60);
		});
	ATCPUHookNode *highPriority = hooks.AddHook(
		kATCPUHookMode_Always, 0x1234, 5,
		[&](uint16) {
			callOrder.push_back(2);
			return uint8(0);
		});
	hooks.AddHook(
		kATCPUHookMode_Always, 0x1334, 0,
		[&](uint16) {
			callOrder.push_back(3);
			return uint8(0x4C);
		});

	AT_PORTABLE_TEST_ASSERT(context, cpu.mChanges == std::vector<HookChange>({
		{ 0x1234, true }, { 0x1234, true }, { 0x1334, true }
	}));
	AT_PORTABLE_TEST_ASSERT(context, hooks.OnHookHit(0x1234) == 0x60);
	AT_PORTABLE_TEST_ASSERT(context, lowPriorityPC == 0x1234);
	AT_PORTABLE_TEST_ASSERT(context, callOrder == std::vector<int>({ 2, 1 }));
	callOrder.clear();
	AT_PORTABLE_TEST_ASSERT(context, hooks.OnHookHit(0x1334) == 0x4C);
	AT_PORTABLE_TEST_ASSERT(context, callOrder == std::vector<int>({ 3 }));

	hooks.RemoveHook(highPriority);
	AT_PORTABLE_TEST_ASSERT(context, cpu.mChanges.size() == 3);
	hooks.RemoveHook(lowPriority);
	AT_PORTABLE_TEST_ASSERT(context, (cpu.mChanges.back() == HookChange { 0x1234, false }));

	uint32 kernelCalls = 0;
	ATCPUHookNode *kernelHook = hooks.AddHook(
		kATCPUHookMode_KernelROMOnly, 0x2000, 0,
		[&](uint16) {
			++kernelCalls;
			return uint8(0x60);
		});
	AT_PORTABLE_TEST_ASSERT(context, hooks.OnHookHit(0x2000) == 0);
	hooks.EnableOSHooks(true);
	AT_PORTABLE_TEST_ASSERT(context, hooks.OnHookHit(0x2000) == 0);
	mmu.mbKernelROMEnabled = true;
	AT_PORTABLE_TEST_ASSERT(context, hooks.OnHookHit(0x2000) == 0x60);
	AT_PORTABLE_TEST_ASSERT(context, kernelCalls == 1);

	uint32 mathCalls = 0;
	ATCPUHookNode *mathHook = hooks.AddHook(
		kATCPUHookMode_MathPackROMOnly, 0x2001, 0,
		[&](uint16) {
			++mathCalls;
			return uint8(0x60);
		});
	pbi.mbROMOverlayActive = true;
	AT_PORTABLE_TEST_ASSERT(context, hooks.OnHookHit(0x2001) == 0);
	pbi.mbROMOverlayActive = false;
	AT_PORTABLE_TEST_ASSERT(context, hooks.OnHookHit(0x2001) == 0x60);
	AT_PORTABLE_TEST_ASSERT(context, mathCalls == 1);

	std::vector<int> lifecycleOrder;
	bool initROMsMatched = false;
	ATCPUHookInitNode *init1 = hooks.AddInitHook(
		[&](const uint8 *lower, const uint8 *upper) {
			initROMsMatched = lower[0] == 0x11 && upper[0] == 0x22;
			lifecycleOrder.push_back(1);
		});
	ATCPUHookInitNode *init2 = hooks.AddInitHook(
		[&](const uint8 *, const uint8 *) { lifecycleOrder.push_back(2); });
	const uint8 lowerROM[] = { 0x11 };
	const uint8 upperROM[] = { 0x22 };
	hooks.CallInitHooks(lowerROM, upperROM);
	AT_PORTABLE_TEST_ASSERT(context, initROMsMatched);
	AT_PORTABLE_TEST_ASSERT(context, lifecycleOrder == std::vector<int>({ 2, 1 }));
	hooks.RemoveInitHook(init2);
	hooks.RemoveInitHook(init1);

	// Keeping an init node on its free list must not suppress allocation of
	// the first reset node. This regresses a mix-up between the two lists.
	uint32 resetCalls = 0;
	ATCPUHookResetNode *resetHook = nullptr;
	resetHook = hooks.AddResetHook([&] {
		++resetCalls;
		hooks.RemoveResetHook(resetHook);
	});
	hooks.CallResetHooks();
	hooks.CallResetHooks();
	AT_PORTABLE_TEST_ASSERT(context, resetCalls == 1);

	ATCPUHookResetNode *reusedResetHook = hooks.AddResetHook([&] { ++resetCalls; });
	hooks.CallResetHooks();
	AT_PORTABLE_TEST_ASSERT(context, resetCalls == 2);
	hooks.RemoveResetHook(reusedResetHook);

	hooks.RemoveHook(kernelHook);
	hooks.RemoveHook(mathHook);
	auto callbackResource = std::make_shared<int>(42);
	std::weak_ptr<int> callbackResourceObserver = callbackResource;
	hooks.AddInitHook(
		[callbackResource](const uint8 *, const uint8 *) {});
	hooks.AddResetHook([callbackResource] {});
	callbackResource.reset();
	const size_t changesBeforeShutdown = cpu.mChanges.size();
	hooks.Shutdown();
	AT_PORTABLE_TEST_ASSERT(context, cpu.mChanges.size() == changesBeforeShutdown + 1);
	AT_PORTABLE_TEST_ASSERT(context, (cpu.mChanges.back() == HookChange { 0x1334, false }));
	AT_PORTABLE_TEST_ASSERT(context, callbackResourceObserver.expired());

	// Shutdown must clear the reset chains before the linear allocator is
	// reused; otherwise this can follow stale nodes from the old arena.
	hooks.Init(&cpu, &mmu, &pbi);
	ATCPUHookResetNode *postShutdownReset = hooks.AddResetHook([&] { ++resetCalls; });
	hooks.CallResetHooks();
	AT_PORTABLE_TEST_ASSERT(context, resetCalls == 3);
	hooks.RemoveResetHook(postShutdownReset);
	hooks.Shutdown();

	return true;
}
