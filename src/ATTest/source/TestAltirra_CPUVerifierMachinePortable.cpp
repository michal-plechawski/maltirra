// Portable tests for the real verifier kernel-target and machine checks.

#include <algorithm>
#include <array>
#include <memory>
#include <string>
#include <utility>

#include <vd2/system/vdtypes.h>
#include <at/atcore/ksyms.h>
#include <at/attest/portabletest.h>
#include <consoletext.h>
#include <verifier.h>
#include "anticportablefixture.h"

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

	class VerifierHarness {
	public:
		explicit VerifierHarness(ANTICPortableFixture& antic)
			: mpAntic(&antic) {
			ATCPUVerifierConnections connections;
			connections.mpReadCPU = [this] { return mCPU; };
			connections.mpIsKernelROM = [this](uint16 address) {
				if (address == ATKernelSymbols::SIOV)
					++mSIOVROMQueryCount;
				if (address == mCPU.mInsnPC)
					++mPCROMQueryCount;
				return mbKernelROMEnabled && address >= 0xC000;
			};
			connections.mpDebugReadByte = [this](uint16 address) {
				++mDebugReadCount;
				return mRAM[address];
			};
			connections.mpDebugReadWord = [this](uint16 address) {
				++mDebugReadCount;
				const uint16 next = static_cast<uint16>(address + 1);
				return static_cast<uint16>(mRAM[address] | (static_cast<uint16>(mRAM[next]) << 8));
			};
			connections.mpGetDisplayListPtr = [this] { return mpAntic->mAntic.GetDisplayListPtr(); };
			connections.mpIsDisplayListEnabled = [this] { return mpAntic->mAntic.IsDisplayListEnabled(); };
			mVerifier.Init(std::move(connections), &mEventManager);
			mVerifierFailureCallback = mEventManager.AddEventCallback(kATSimEvent_VerifierFailure, [this] { ++mVerifierFailureCount; });
		}

		~VerifierHarness() {
			if (mVerifierFailureCallback)
				mEventManager.RemoveEventCallback(mVerifierFailureCallback);
		}

		void InitializeDisplayListRead(uint16 displayListAddress, uint16 bufferAddress, uint16 length) {
			mpAntic->WriteANTIC(0x00, 0x20);
			mpAntic->SetDisplayList(displayListAddress);
			mRAM[ATKernelSymbols::DSTATS] = 0x40;
			mRAM[ATKernelSymbols::DBUFLO] = static_cast<uint8>(bufferAddress);
			mRAM[ATKernelSymbols::DBUFLO + 1] = static_cast<uint8>(bufferAddress >> 8);
			mRAM[ATKernelSymbols::DBYTLO] = static_cast<uint8>(length);
			mRAM[ATKernelSymbols::DBYTLO + 1] = static_cast<uint8>(length >> 8);
		}

		ANTICPortableFixture *mpAntic;
		ATSimulatorEventManager mEventManager;
		ATCPUVerifier mVerifier;
		ATCPUVerifierCPUState mCPU;
		std::array<uint8, 65536> mRAM {};
		uint32 mVerifierFailureCount = 0;
		uint32 mSIOVROMQueryCount = 0;
		uint32 mPCROMQueryCount = 0;
		uint32 mDebugReadCount = 0;
		bool mbKernelROMEnabled = true;

	private:
		uint32 mVerifierFailureCallback = 0;
	};

	bool TestKernelTargets(ATPortableTestContext& context, ATCPUVerifier& verifier) {
		vdfastvector<uint16> defaults;
		verifier.ResetAllowedTargets();
		verifier.GetAllowedTargets(defaults);
		AT_PORTABLE_TEST_ASSERT(context, !defaults.empty());
		AT_PORTABLE_TEST_ASSERT(context, std::is_sorted(defaults.begin(), defaults.end()));
		AT_PORTABLE_TEST_ASSERT(context, std::adjacent_find(defaults.begin(), defaults.end()) == defaults.end());
		AT_PORTABLE_TEST_ASSERT(context, std::binary_search(defaults.begin(), defaults.end(), static_cast<uint16>(ATKernelSymbols::SIOV)));

		verifier.AddAllowedTarget(0x0000);
		verifier.AddAllowedTarget(0xFFFF);
		verifier.AddAllowedTarget(0x1234);
		verifier.AddAllowedTarget(0x1234);
		vdfastvector<uint16> modified;
		verifier.GetAllowedTargets(modified);
		AT_PORTABLE_TEST_ASSERT(context, std::is_sorted(modified.begin(), modified.end()));
		AT_PORTABLE_TEST_ASSERT(context, std::adjacent_find(modified.begin(), modified.end()) == modified.end());
		AT_PORTABLE_TEST_ASSERT(context, std::binary_search(modified.begin(), modified.end(), 0x0000));
		AT_PORTABLE_TEST_ASSERT(context, std::binary_search(modified.begin(), modified.end(), 0xFFFF));
		AT_PORTABLE_TEST_ASSERT(context, std::binary_search(modified.begin(), modified.end(), 0x1234));
		AT_PORTABLE_TEST_ASSERT(context, modified.size() == defaults.size() + 3);

		verifier.RemoveAllowedTarget(0x1234);
		verifier.RemoveAllowedTarget(0x1234);
		verifier.GetAllowedTargets(modified);
		AT_PORTABLE_TEST_ASSERT(context, !std::binary_search(modified.begin(), modified.end(), 0x1234));
		verifier.RemoveAllowedTargets();
		verifier.GetAllowedTargets(modified);
		AT_PORTABLE_TEST_ASSERT(context, modified.empty());
		verifier.ResetAllowedTargets();
		verifier.GetAllowedTargets(modified);
		AT_PORTABLE_TEST_ASSERT(context, modified == defaults);
		verifier.AddAllowedTarget(0x10000);
		verifier.AddAllowedTarget(0x12345);
		verifier.AddAllowedTarget(0xFFFFFFFF);
		verifier.RemoveAllowedTarget(0x1E459);
		verifier.GetAllowedTargets(modified);
		AT_PORTABLE_TEST_ASSERT(context, modified == defaults);
		return true;
	}

	bool TestKernelEntryFiltering(ATPortableTestContext& context, VerifierHarness& harness) {
		CapturingConsole console;
		ConsoleScope consoleScope(&console);
		auto& verifier = harness.mVerifier;
		verifier.SetFlags(kATVerifierFlag_UndocumentedKernelEntry);

		harness.mCPU.mInsnPC = 0x4000;
		verifier.VerifyInsn(0x4C, 0xD820);
		AT_PORTABLE_TEST_ASSERT(context, harness.mVerifierFailureCount == 1);

		verifier.VerifyInsn(0x4C, 0xBFFF); // Not in kernel ROM.
		AT_PORTABLE_TEST_ASSERT(context, harness.mVerifierFailureCount == 1);

		harness.mCPU.mInsnPC = 0xD900;
		verifier.VerifyInsn(0x4C, 0xD820); // Jump originates in kernel ROM.
		AT_PORTABLE_TEST_ASSERT(context, harness.mVerifierFailureCount == 1);

		harness.mCPU.mInsnPC = 0x4000;
		verifier.AddAllowedTarget(0xD820);
		verifier.VerifyInsn(0x4C, 0xD820); // Explicit exception.
		AT_PORTABLE_TEST_ASSERT(context, harness.mVerifierFailureCount == 1);
		AT_PORTABLE_TEST_ASSERT(context, console.mText.find("Invalid jump into kernel ROM") != std::string::npos);
		return true;
	}
}

bool ATTestAltirraCPUVerifierKernel(ATPortableTestContext& context) {
	auto anticOwner = std::make_unique<ANTICPortableFixture>();
	auto& antic = *anticOwner;
	VerifierHarness harness(antic);
	return TestKernelTargets(context, harness.mVerifier) && TestKernelEntryFiltering(context, harness);
}

bool ATTestAltirraCPUVerifierMachine(ATPortableTestContext& context) {
	auto anticOwner = std::make_unique<ANTICPortableFixture>();
	auto& antic = *anticOwner;
	VerifierHarness harness(antic);
	CapturingConsole console;
	ConsoleScope consoleScope(&console);
	harness.mCPU.mInsnPC = 0x4000;
	harness.InitializeDisplayListRead(0x0010, 0xFFF0, 0x0030);

	// SIOV is in the default allowed-target table, but that exception must only
	// suppress the undocumented-entry warning; independent SIO checks still run.
	harness.mCPU.mP = 0x04;
	harness.mVerifier.SetFlags(kATVerifierFlag_UndocumentedKernelEntry
		| kATVerifierFlag_CallingConventionViolations
		| kATVerifierFlag_LoadingOverDisplayList);
	harness.mVerifier.VerifyInsn(0x4C, ATKernelSymbols::SIOV);
	AT_PORTABLE_TEST_ASSERT(context, harness.mVerifierFailureCount == 1);
	AT_PORTABLE_TEST_ASSERT(context, console.mText.find("calling into SIO with I flag set") != std::string::npos);
	AT_PORTABLE_TEST_ASSERT(context, console.mText.find("Loading over active display list") != std::string::npos);
	AT_PORTABLE_TEST_ASSERT(context, antic.mAntic.IsDisplayListEnabled());
	AT_PORTABLE_TEST_ASSERT(context, antic.mAntic.GetDisplayListPtr() == 0x0010);

	// Retain the original display-list warning's console-only behavior. Check
	// half-open, modular read ranges against the real ANTIC DMA enable bit.
	struct ReadCase {
		uint16 mDList, mBuffer, mLength;
		bool mbRead, mbEnabled, mbROM, mbWarning;
	};
	const ReadCase cases[] {
		{ 0x0010, 0xFFF0, 0x0020, true, true, true, false }, // Exclusive end.
		{ 0x0010, 0xFFF0, 0x0021, true, true, true, true },
		{ 0xFFF0, 0xFFF0, 0x0001, true, true, true, true }, // Inclusive start.
		{ 0xFFEF, 0xFFF0, 0x0030, true, true, true, false },
		{ 0x0010, 0xFFF0, 0x0000, true, true, true, false },
		{ 0x0010, 0xFFF0, 0x0030, false, true, true, false },
		{ 0x0010, 0xFFF0, 0x0030, true, false, true, false },
		{ 0x0010, 0xFFF0, 0x0030, true, true, false, false }
	};
	harness.mVerifier.SetFlags(kATVerifierFlag_LoadingOverDisplayList);
	for (const auto& c : cases) {
		harness.InitializeDisplayListRead(c.mDList, c.mBuffer, c.mLength);
		harness.mRAM[ATKernelSymbols::DSTATS] = c.mbRead ? 0x40 : 0x80;
		antic.WriteANTIC(0x00, c.mbEnabled ? 0x20 : 0);
		harness.mbKernelROMEnabled = c.mbROM;
		for (uint8 opcode : { uint8(0x20), uint8(0x4C) }) {
			console.mText.clear();
			harness.mVerifier.VerifyInsn(opcode, ATKernelSymbols::SIOV);
			AT_PORTABLE_TEST_ASSERT(context, (console.mText.find("Loading over active display list") != std::string::npos) == c.mbWarning);
			AT_PORTABLE_TEST_ASSERT(context, harness.mVerifierFailureCount == 1);
		}
	}
	harness.InitializeDisplayListRead(0x0010, 0xFFF0, 0x0030);
	harness.mbKernelROMEnabled = true;
	harness.mVerifier.SetFlags(0);
	console.mText.clear();
	harness.mVerifier.VerifyInsn(0x4C, ATKernelSymbols::SIOV);
	AT_PORTABLE_TEST_ASSERT(context, console.mText.empty());

	// A synchronous verifier-failure callback mutates CPU flags and RAM between
	// checks. Later SIO checks must observe those new values in this same call.
	{
		VerifierHarness callbackHarness(antic);
		CapturingConsole callbackConsole;
		ConsoleScope callbackConsoleScope(&callbackConsole);
		callbackHarness.mCPU.mInsnPC = 0x4000;
		callbackHarness.mbKernelROMEnabled = true;
		callbackHarness.mVerifier.RemoveAllowedTargets();
		callbackHarness.mVerifier.SetFlags(kATVerifierFlag_UndocumentedKernelEntry);
		bool mutated = false;
		const uint32 mutationId = callbackHarness.mEventManager.AddEventCallback(kATSimEvent_VerifierFailure, [&] {
			if (mutated)
				return;
			mutated = true;
			callbackHarness.mCPU.mP = 0x04;
			callbackHarness.mRAM[ATKernelSymbols::DSTATS] = 0x40;
			callbackHarness.mRAM[ATKernelSymbols::DBUFLO] = 0xF0;
			callbackHarness.mRAM[ATKernelSymbols::DBUFLO + 1] = 0xFF;
			callbackHarness.mRAM[ATKernelSymbols::DBYTLO] = 0x30;
			callbackHarness.mRAM[ATKernelSymbols::DBYTLO + 1] = 0;
			callbackHarness.mVerifier.SetFlags(kATVerifierFlag_CallingConventionViolations | kATVerifierFlag_LoadingOverDisplayList);
		});
		callbackHarness.mVerifier.VerifyInsn(0x4C, ATKernelSymbols::SIOV);
		callbackHarness.mEventManager.RemoveEventCallback(mutationId);
		AT_PORTABLE_TEST_ASSERT(context, callbackHarness.mVerifierFailureCount == 2);
		AT_PORTABLE_TEST_ASSERT(context, callbackConsole.mText.find("Invalid jump into kernel ROM") != std::string::npos);
		AT_PORTABLE_TEST_ASSERT(context, callbackConsole.mText.find("calling into SIO with I flag set") != std::string::npos);
		AT_PORTABLE_TEST_ASSERT(context, callbackConsole.mText.find("Loading over active display list") != std::string::npos);
	}

	// ROM status is queried independently after notifications. If a callback
	// changes the target's mapping, the later SIO checks must not use stale ROM
	// classification or read the SIO parameter block.
	{
		VerifierHarness callbackHarness(antic);
		CapturingConsole callbackConsole;
		ConsoleScope callbackConsoleScope(&callbackConsole);
		callbackHarness.mCPU.mInsnPC = 0x4000;
		callbackHarness.mbKernelROMEnabled = true;
		callbackHarness.mVerifier.RemoveAllowedTargets();
		callbackHarness.mVerifier.SetFlags(kATVerifierFlag_UndocumentedKernelEntry
			| kATVerifierFlag_CallingConventionViolations
			| kATVerifierFlag_LoadingOverDisplayList);
		const uint32 mutationId = callbackHarness.mEventManager.AddEventCallback(kATSimEvent_VerifierFailure, [&] {
			callbackHarness.mbKernelROMEnabled = false;
		});
		callbackHarness.mCPU.mP = 0x04;
		callbackHarness.mVerifier.VerifyInsn(0x4C, ATKernelSymbols::SIOV);
		callbackHarness.mEventManager.RemoveEventCallback(mutationId);
		AT_PORTABLE_TEST_ASSERT(context, callbackHarness.mVerifierFailureCount == 1);
		AT_PORTABLE_TEST_ASSERT(context, callbackHarness.mSIOVROMQueryCount >= 3);
		AT_PORTABLE_TEST_ASSERT(context, callbackHarness.mDebugReadCount == 0);
		AT_PORTABLE_TEST_ASSERT(context, callbackConsole.mText.find("Invalid jump into kernel ROM") != std::string::npos);
		AT_PORTABLE_TEST_ASSERT(context, callbackConsole.mText.find("calling into SIO with I flag set") == std::string::npos);
		AT_PORTABLE_TEST_ASSERT(context, callbackConsole.mText.find("Loading over active display list") == std::string::npos);
	}

	return true;
}
