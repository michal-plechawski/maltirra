// Portable CPU heat-map trap and diagnostic tests.

#include <memory>
#include <string>

#include <at/attest/portabletest.h>
#include <consoletext.h>
#include <cpuheatmap.h>
#include <simeventmanager.h>

namespace {
	class CapturingConsole final : public IATUIDebuggerConsoleWindow {
	public:
		void Write(const char *s) override { mText += s; }
		void ShowEnd() override {}

		std::string mText;
	};

	class ConsoleScope {
	public:
		explicit ConsoleScope(IATUIDebuggerConsoleWindow *output)
			: mpPrevious(ATConsoleSetTextOutput(output)) {}

		~ConsoleScope() { ATConsoleSetTextOutput(mpPrevious); }

	private:
		IATUIDebuggerConsoleWindow *mpPrevious;
	};

	struct TrapHarness {
		TrapHarness() {
			mHeatMap.Init(&mEventManager);
			mCallbackId = mEventManager.AddEventCallback(kATSimEvent_VerifierFailure, [this] { ++mFailures; });
		}

		~TrapHarness() {
			if (mCallbackId)
				mEventManager.RemoveEventCallback(mCallbackId);
		}

		ATCPUHeatMap mHeatMap;
		ATSimulatorEventManager mEventManager;
		uint32 mCallbackId = 0;
		uint32 mFailures = 0;
	};

	bool CheckOneTrap(ATPortableTestContext& context, ATCPUHeatMapTrapFlags flags,
		uint8 opcode, uint16 addr, const auto& setup) {
		auto storage = std::make_unique<TrapHarness>();
		auto& harness = *storage;
		CapturingConsole console;
		ConsoleScope consoleScope(&console);
		setup(harness.mHeatMap);
		harness.mHeatMap.SetNormalTrapFlags(flags);
		harness.mHeatMap.ProcessInsn(opcode, addr, 0x4567, 0xFD);
		AT_PORTABLE_TEST_ASSERT(context, harness.mFailures == 1);
		return true;
	}
}

bool ATTestAltirraCPUHeatMapTraps(ATPortableTestContext& context) {
	const uint16 testAddress = 0x2345;
	AT_PORTABLE_TEST_ASSERT(context, CheckOneTrap(context, kATCPUHeatMapTrapFlags_Load, 0xAD, testAddress,
		[testAddress](ATCPUHeatMap& hm) { hm.ResetMemoryRange(testAddress, 1); }));
	AT_PORTABLE_TEST_ASSERT(context, CheckOneTrap(context, kATCPUHeatMapTrapFlags_Compute, 0xC0, 0,
		[](ATCPUHeatMap&) {}));
	AT_PORTABLE_TEST_ASSERT(context, CheckOneTrap(context, kATCPUHeatMapTrapFlags_Branch, 0x70, 0,
		[](ATCPUHeatMap&) {}));
	AT_PORTABLE_TEST_ASSERT(context, CheckOneTrap(context, kATCPUHeatMapTrapFlags_EffectiveAddress, 0xBD, testAddress,
		[](ATCPUHeatMap&) {}));
	AT_PORTABLE_TEST_ASSERT(context, CheckOneTrap(context, kATCPUHeatMapTrapFlags_HwStore, 0x8D, testAddress,
		[testAddress](ATCPUHeatMap& hm) { hm.MarkMemoryRangeHardware(testAddress, 1); }));

	// Compute-only trapping must not enable the hardware-store trap path.
	{
		auto storage = std::make_unique<TrapHarness>();
		auto& harness = *storage;
		CapturingConsole console;
		ConsoleScope consoleScope(&console);
		harness.mHeatMap.MarkMemoryRangeHardware(testAddress, 1);
		harness.mHeatMap.SetNormalTrapFlags(kATCPUHeatMapTrapFlags_Compute);
		harness.mHeatMap.ProcessInsn(0x8D, testAddress, 0x4567, 0xFD);
		AT_PORTABLE_TEST_ASSERT(context, harness.mFailures == 0);
	}

	// The active trap mask switches between early and normal state immediately.
	{
		auto storage = std::make_unique<TrapHarness>();
		auto& harness = *storage;
		CapturingConsole console;
		ConsoleScope consoleScope(&console);
		harness.mHeatMap.ResetMemoryRange(testAddress, 1);
		harness.mHeatMap.SetEarlyTrapFlags(kATCPUHeatMapTrapFlags_Load);
		harness.mHeatMap.SetNormalTrapFlags((ATCPUHeatMapTrapFlags)0);
		harness.mHeatMap.SetEarlyState(true);
		harness.mHeatMap.ProcessInsn(0xAD, testAddress, 0x4567, 0xFD);
		AT_PORTABLE_TEST_ASSERT(context, harness.mFailures == 1);
		harness.mHeatMap.ResetMemoryRange(testAddress, 1);
		harness.mHeatMap.SetEarlyState(false);
		harness.mHeatMap.ProcessInsn(0xAD, testAddress, 0x4567, 0xFD);
		AT_PORTABLE_TEST_ASSERT(context, harness.mFailures == 1);
	}

	// Known data may be read and written without traps; writes to hardware retain
	// the hardware identity instead of replacing it with the source register tag.
	{
		auto storage = std::make_unique<TrapHarness>();
		auto& harness = *storage;
		CapturingConsole console;
		ConsoleScope consoleScope(&console);
		harness.mHeatMap.MarkMemoryRangeHardware(testAddress, 1);
		harness.mHeatMap.SetNormalTrapFlags(kATCPUHeatMapTrapFlags_All);
		harness.mHeatMap.ProcessInsn(0xA9, 0, 0x4567, 0xFD);
		harness.mHeatMap.ProcessInsn(0x8D, testAddress, 0x4568, 0xFD);
		AT_PORTABLE_TEST_ASSERT(context, harness.mFailures == 0);
		AT_PORTABLE_TEST_ASSERT(context, harness.mHeatMap.GetMemoryStatus(testAddress) == (ATCPUHeatMap::kTypeHardware + testAddress));
		AT_PORTABLE_TEST_ASSERT(context, harness.mHeatMap.GetMemoryValidity(testAddress) == 0xFF);
		AT_PORTABLE_TEST_ASSERT(context, harness.mHeatMap.GetMemoryAccesses(testAddress) == ATCPUHeatMap::kAccessWrite);

		// Initial carry validity is unknown. ROL of a hardware byte therefore
		// derives an unknown written bit and must follow the HwStore trap setting.
		harness.mHeatMap.Reset();
		harness.mHeatMap.MarkMemoryRangeHardware(testAddress, 1);
		harness.mHeatMap.SetNormalTrapFlags(kATCPUHeatMapTrapFlags_HwStore);
		harness.mHeatMap.ProcessInsn(0x2E, testAddress, 0x4569, 0xFD);
		AT_PORTABLE_TEST_ASSERT(context, harness.mFailures == 1);
		AT_PORTABLE_TEST_ASSERT(context, harness.mHeatMap.GetMemoryStatus(testAddress) == (ATCPUHeatMap::kTypeHardware + testAddress));
		AT_PORTABLE_TEST_ASSERT(context, harness.mHeatMap.GetMemoryValidity(testAddress) == 0xFF);
	}

	// A read from initialized/preset data is not a load trap.
	{
		auto storage = std::make_unique<TrapHarness>();
		auto& harness = *storage;
		CapturingConsole console;
		ConsoleScope consoleScope(&console);
		harness.mHeatMap.SetNormalTrapFlags(kATCPUHeatMapTrapFlags_Load);
		harness.mHeatMap.ProcessInsn(0xAD, testAddress, 0x4567, 0xFD);
		AT_PORTABLE_TEST_ASSERT(context, harness.mFailures == 0);
	}

	return true;
}

bool ATTestAltirraCPUHeatMapDiagnostics(ATPortableTestContext& context) {
	auto storage = std::make_unique<TrapHarness>();
	auto& harness = *storage;
	CapturingConsole console;
	ConsoleScope consoleScope(&console);

	harness.mHeatMap.SetNormalTrapFlags(kATCPUHeatMapTrapFlags_Branch);
	harness.mHeatMap.ProcessInsn(0xA9, 0, 0x4567, 0xFD); // N becomes valid, V remains invalid.
	harness.mHeatMap.ProcessInsn(0x70, 0, 0x4568, 0xFD); // BVS uses the invalid V flag.

	AT_PORTABLE_TEST_ASSERT(context, harness.mFailures == 1);
	AT_PORTABLE_TEST_ASSERT(context, console.mText.find("P~(N--IZ-)") != std::string::npos);

	// Opposite case: CLV makes V valid without making N valid. BPL must report
	// V independently, not infer it from N or only suppress a false V marker.
	harness.mHeatMap.Reset();
	console.mText.clear();
	harness.mHeatMap.ProcessInsn(0xB8, 0, 0x4569, 0xFD);
	harness.mHeatMap.ProcessInsn(0x10, 0, 0x456A, 0xFD);
	AT_PORTABLE_TEST_ASSERT(context, harness.mFailures == 2);
	AT_PORTABLE_TEST_ASSERT(context, console.mText.find("P~(-V-I--)") != std::string::npos);
	return true;
}
