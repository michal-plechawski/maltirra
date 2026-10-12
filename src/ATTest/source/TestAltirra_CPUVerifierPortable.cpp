// Portable tests for the real CPU verifier rules and interrupt bookkeeping.

#include <array>
#include <initializer_list>
#include <string>
#include <utility>

#include <vd2/system/vdtypes.h>
#include <at/atcore/ksyms.h>
#include <at/attest/portabletest.h>
#include <cpuflags.h>
#include <consoletext.h>
#include <simeventmanager.h>
#include <verifier.h>

namespace {
	class CapturingConsole final : public IATUIDebuggerConsoleWindow {
	public:
		void Write(const char *text) override { mText += text; }
		void ShowEnd() override {}
		std::string mText;
	};

	class ConsoleScope {
	public:
		explicit ConsoleScope(IATUIDebuggerConsoleWindow *window)
			: mpPrevious(ATConsoleSetTextOutput(window))
			, mPreviousBeam(ATConsoleSetBeamPositionProvider({})) {}

		~ConsoleScope() {
			ATConsoleSetBeamPositionProvider(std::move(mPreviousBeam));
			ATConsoleSetTextOutput(mpPrevious);
		}

	private:
		IATUIDebuggerConsoleWindow *mpPrevious;
		vdfunction<bool(ATConsoleBeamPosition&)> mPreviousBeam;
	};

	struct VerifierFixture {
		ATSimulatorEventManager mEvents;
		ATCPUVerifier mVerifier;
		ATCPUVerifierCPUState mCPU;
		std::array<uint8, 65536> mMemory {};
		uint32 mVerifierFailures = 0;
		bool mbKernelROM = false;
		uint16 mKernelROMAddress = 0;
		uint16 mDisplayListPtr = 0;
		bool mbDisplayListEnabled = false;

		VerifierFixture() {
			mCPU.mS16 = 0x0100;
			ATCPUVerifierConnections connections;
			connections.mpReadCPU = [this] { return mCPU; };
			connections.mpIsKernelROM = [this](uint16 address) {
				return mbKernelROM && address == mKernelROMAddress;
			};
			connections.mpDebugReadByte = [this](uint16 address) { return mMemory[address]; };
			connections.mpDebugReadWord = [this](uint16 address) {
				return static_cast<uint16>(mMemory[address] | (static_cast<uint16>(mMemory[static_cast<uint16>(address + 1)]) << 8));
			};
			connections.mpGetDisplayListPtr = [this] { return mDisplayListPtr; };
			connections.mpIsDisplayListEnabled = [this] { return mbDisplayListEnabled; };
			mVerifier.Init(std::move(connections), &mEvents);
			mEvents.AddEventCallback(kATSimEvent_VerifierFailure, [this] { ++mVerifierFailures; });
		}

		void SetS(uint8 s) {
			mCPU.mS = s;
			mCPU.mS16 = static_cast<uint16>((mCPU.mS16 & 0xFF00) | s);
		}

		void Verify(uint8 opcode, uint16 target = 0) {
			mVerifier.VerifyInsn(opcode, target);
		}
	};

	bool TestAddressRules(ATPortableTestContext& context) {
		CapturingConsole sink;
		ConsoleScope consoleScope(&sink);
		VerifierFixture f;
		f.mVerifier.SetFlags(kATVerifierFlag_AddressZero);
		for(uint8 opcode : { uint8(0xA5), uint8(0xA6), uint8(0xA4) }) {
			const uint32 before = f.mVerifierFailures;
			f.Verify(opcode, 0);
			AT_PORTABLE_TEST_ASSERT(context, f.mVerifierFailures == before + 1);
			f.Verify(opcode, 1);
			AT_PORTABLE_TEST_ASSERT(context, f.mVerifierFailures == before + 1);
		}
		f.mVerifier.SetFlags(0);
		const uint32 addressZeroDisabled = f.mVerifierFailures;
		f.Verify(0xA5, 0);
		AT_PORTABLE_TEST_ASSERT(context, f.mVerifierFailures == addressZeroDisabled);

		f.mVerifier.SetFlags(kATVerifierFlag_NonCanonicalHardwareAddress);
		const uint8 absHardwareOpcodes[] { 0x8C, 0x8D, 0x8E, 0xAC, 0xAD, 0xAE };
		const uint16 canonicalAddresses[] { 0xD000, 0xD01F, 0xD200, 0xD21F, 0xD300, 0xD303, 0xD400, 0xD40F, 0xD500, 0xD7FF, 0xD800 };
		const uint16 aliasedAddresses[] { 0xD020, 0xD0FF, 0xD220, 0xD2FF, 0xD304, 0xD3FF, 0xD410, 0xD4FF };
		for(uint8 opcode : absHardwareOpcodes) {
			for(uint16 address : canonicalAddresses) {
				const uint32 before = f.mVerifierFailures;
				f.Verify(opcode, address);
				AT_PORTABLE_TEST_ASSERT(context, f.mVerifierFailures == before);
			}
			for(uint16 address : aliasedAddresses) {
				const uint32 before = f.mVerifierFailures;
				f.Verify(opcode, address);
				AT_PORTABLE_TEST_ASSERT(context, f.mVerifierFailures == before + 1);
			}
		}
		f.mVerifier.SetFlags(0);
		const uint32 hardwareDisabled = f.mVerifierFailures;
		f.Verify(0x8D, 0xD020);
		AT_PORTABLE_TEST_ASSERT(context, f.mVerifierFailures == hardwareDisabled);
		return true;
	}

	bool TestIndexedWrapRules(ATPortableTestContext& context) {
		CapturingConsole sink;
		ConsoleScope consoleScope(&sink);
		VerifierFixture f;
		f.mVerifier.SetFlags(kATVerifierFlag_64KWrap);
		const uint8 absXOpcodes[] {
			0x1D, 0x1E, 0x3D, 0x3E, 0x5D, 0x5E, 0x7D, 0x7E,
			0x9D, 0xBC, 0xBD, 0xDD, 0xDE, 0xFD, 0xFE
		};
		const uint8 absYOpcodes[] { 0x19, 0x39, 0x59, 0x79, 0x99, 0xB9, 0xBE, 0xD9, 0xF9 };
		const uint8 indirectYOpcodes[] { 0x11, 0x31, 0x51, 0x71, 0x91, 0xB1, 0xD1, 0xF1 };
		for(uint8 opcode : absXOpcodes) {
			f.mCPU.mX = 1;
			uint32 before = f.mVerifierFailures;
			f.Verify(opcode, 0);
			AT_PORTABLE_TEST_ASSERT(context, f.mVerifierFailures == before + 1);
			f.Verify(opcode, 1);
			f.Verify(opcode, 2);
			AT_PORTABLE_TEST_ASSERT(context, f.mVerifierFailures == before + 1);
		}
		for(uint8 opcode : absYOpcodes) {
			f.mCPU.mY = 1;
			uint32 before = f.mVerifierFailures;
			f.Verify(opcode, 0);
			AT_PORTABLE_TEST_ASSERT(context, f.mVerifierFailures == before + 1);
			f.Verify(opcode, 1);
			f.Verify(opcode, 2);
			AT_PORTABLE_TEST_ASSERT(context, f.mVerifierFailures == before + 1);
		}
		for(uint8 opcode : indirectYOpcodes) {
			f.mCPU.mY = 1;
			uint32 before = f.mVerifierFailures;
			f.Verify(opcode, 0);
			AT_PORTABLE_TEST_ASSERT(context, f.mVerifierFailures == before + 1);
			f.Verify(opcode, 1);
			f.Verify(opcode, 2);
			AT_PORTABLE_TEST_ASSERT(context, f.mVerifierFailures == before + 1);
		}

		f.mVerifier.SetFlags(0);
		const uint32 disabled = f.mVerifierFailures;
		f.Verify(0x1D, 0);
		f.Verify(0x19, 0);
		f.Verify(0x11, 0);
		AT_PORTABLE_TEST_ASSERT(context, f.mVerifierFailures == disabled);
		return true;
	}

	bool TestStackRules(ATPortableTestContext& context) {
		CapturingConsole sink;
		ConsoleScope consoleScope(&sink);
		VerifierFixture f;
		f.mVerifier.SetFlags(kATVerifierFlag_StackWrap);

		// 6502/65C02 emulation-mode one-byte pushes and calls wrap at the page edge.
		f.mCPU.mMode = kATCPUMode_6502;
		f.mCPU.mbEmulation = true;
		for(uint8 opcode : { uint8(0x48), uint8(0x08), uint8(0xDA), uint8(0x5A) }) {
			const uint32 before = f.mVerifierFailures;
			f.SetS(0);
			f.Verify(opcode);
			if (opcode == 0xDA || opcode == 0x5A)
				AT_PORTABLE_TEST_ASSERT(context, f.mVerifierFailures == before);
			else
				AT_PORTABLE_TEST_ASSERT(context, f.mVerifierFailures == before + 1);
			f.SetS(1);
			f.Verify(opcode);
			AT_PORTABLE_TEST_ASSERT(context, f.mVerifierFailures == before + (opcode == 0xDA || opcode == 0x5A ? 0u : 1u));
		}
		f.mCPU.mMode = kATCPUMode_65C02;
		f.SetS(0);
		uint32 before = f.mVerifierFailures;
		f.Verify(0xDA);
		f.Verify(0x5A);
		AT_PORTABLE_TEST_ASSERT(context, f.mVerifierFailures == before + 2);

		// Retain the legacy JSR verifier boundary; RTS/RTI detect underflow.
		f.mCPU.mMode = kATCPUMode_6502;
		f.SetS(2);
		before = f.mVerifierFailures;
		f.Verify(0x20);
		AT_PORTABLE_TEST_ASSERT(context, f.mVerifierFailures == before + 1);
		f.SetS(3);
		f.Verify(0x20);
		AT_PORTABLE_TEST_ASSERT(context, f.mVerifierFailures == before + 1);
		f.SetS(0xFE);
		f.Verify(0x60);
		AT_PORTABLE_TEST_ASSERT(context, f.mVerifierFailures == before + 2);
		f.SetS(0xFD);
		f.Verify(0x60);
		AT_PORTABLE_TEST_ASSERT(context, f.mVerifierFailures == before + 2);
		f.SetS(0xFD);
		f.Verify(0x40);
		AT_PORTABLE_TEST_ASSERT(context, f.mVerifierFailures == before + 3);
		f.SetS(0xFC);
		f.Verify(0x40);
		AT_PORTABLE_TEST_ASSERT(context, f.mVerifierFailures == before + 3);

		// Native 65C816 mode has a 16-bit stack and must not trigger emulation wrap checks.
		f.mCPU.mMode = kATCPUMode_65C816;
		f.mCPU.mbEmulation = false;
		f.SetS(0);
		before = f.mVerifierFailures;
		for(uint8 opcode : { uint8(0x20), uint8(0x48), uint8(0x08), uint8(0x68), uint8(0x28), uint8(0x60), uint8(0x40), uint8(0x22), uint8(0xFC), uint8(0x0B), uint8(0x4B), uint8(0x8B) })
			f.Verify(opcode);
		AT_PORTABLE_TEST_ASSERT(context, f.mVerifierFailures == before);

		// 816 emulation-mode 1/2/3-byte push and pop boundaries.
		f.mCPU.mbEmulation = true;
		for(uint8 opcode : { uint8(0x4B), uint8(0x8B), uint8(0x48), uint8(0x08), uint8(0xDA), uint8(0x5A) }) {
			f.SetS(0);
			before = f.mVerifierFailures;
			f.Verify(opcode);
			AT_PORTABLE_TEST_ASSERT(context, f.mVerifierFailures == before + 1);
			f.SetS(1);
			f.Verify(opcode);
			AT_PORTABLE_TEST_ASSERT(context, f.mVerifierFailures == before + 1);
		}
		for(uint8 opcode : { uint8(0x0B), uint8(0xFC), uint8(0xF4), uint8(0xD4) }) {
			f.SetS(1);
			before = f.mVerifierFailures;
			f.Verify(opcode);
			AT_PORTABLE_TEST_ASSERT(context, f.mVerifierFailures == before + 1);
			f.SetS(2);
			f.Verify(opcode);
			AT_PORTABLE_TEST_ASSERT(context, f.mVerifierFailures == before + 1);
			f.SetS(0xFF);
			f.Verify(opcode);
			AT_PORTABLE_TEST_ASSERT(context, f.mVerifierFailures == before + 1);
		}
		f.SetS(2);
		before = f.mVerifierFailures;
		f.Verify(0x22);
		AT_PORTABLE_TEST_ASSERT(context, f.mVerifierFailures == before + 1);
		f.SetS(3);
		f.Verify(0x22);
		AT_PORTABLE_TEST_ASSERT(context, f.mVerifierFailures == before + 1);

		for(uint8 opcode : { uint8(0xAB), uint8(0xFA), uint8(0x7A), uint8(0x28), uint8(0x68) }) {
			f.SetS(0xFF);
			before = f.mVerifierFailures;
			f.Verify(opcode);
			AT_PORTABLE_TEST_ASSERT(context, f.mVerifierFailures == before + 1);
			f.SetS(0xFE);
			f.Verify(opcode);
			AT_PORTABLE_TEST_ASSERT(context, f.mVerifierFailures == before + 1);
		}

		f.SetS(0xFE);
		before = f.mVerifierFailures;
		f.Verify(0x2B); // PLD pops two bytes, unlike PEA/PEI.
		AT_PORTABLE_TEST_ASSERT(context, f.mVerifierFailures == before + 1);
		f.SetS(0xFD);
		f.Verify(0x2B);
		AT_PORTABLE_TEST_ASSERT(context, f.mVerifierFailures == before + 1);

		// Page-zero-stack checking is independent of the 8-bit wrap rule.
		f.mVerifier.SetFlags(kATVerifierFlag_StackInZP816);
		f.mCPU.mbEmulation = false;
		f.mCPU.mS16 = 0x00FF;
		f.SetS(0xFF);
		before = f.mVerifierFailures;
		f.Verify(0x1B); // TCS
		AT_PORTABLE_TEST_ASSERT(context, f.mVerifierFailures == before + 1);
		f.mCPU.mS16 = 0x0100;
		f.SetS(0);
		f.Verify(0x9A); // TXS
		AT_PORTABLE_TEST_ASSERT(context, f.mVerifierFailures == before + 1);
		return true;
	}

	bool TestInterruptAndLifecycle(ATPortableTestContext& context) {
		CapturingConsole sink;
		ConsoleScope consoleScope(&sink);
		VerifierFixture f;
		f.mVerifier.SetFlags(kATVerifierFlag_InterruptRegs);
		f.SetS(0xE0);
		f.mCPU.mA = 0x11;
		f.mCPU.mX = 0x22;
		f.mCPU.mY = 0x33;
		f.mCPU.mInsnPC = 0x4567;
		f.mVerifier.OnIRQEntry();
		f.mCPU.mA = 0x99;
		uint32 before = f.mVerifierFailures;
		f.Verify(0x40);
		AT_PORTABLE_TEST_ASSERT(context, f.mVerifierFailures == before + 1);
		AT_PORTABLE_TEST_ASSERT(context, sink.mText.find("Entry: PC=4567  A=11 X=22 Y=33") != std::string::npos);
		f.mCPU.mA = 0x11;
		f.mCPU.mX = 0x22;
		f.mCPU.mY = 0x33;
		f.Verify(0x40);
		AT_PORTABLE_TEST_ASSERT(context, f.mVerifierFailures == before + 1); // Mismatch entry was consumed.

		f.SetS(0xE1);
		f.mCPU.mA = 0x44;
		f.mCPU.mX = 0x55;
		f.mCPU.mY = 0x66;
		f.mCPU.mInsnPC = 0x5678;
		f.mVerifier.OnNMIEntry();
		f.mCPU.mY = 0;
		f.Verify(0x40);
		AT_PORTABLE_TEST_ASSERT(context, f.mVerifierFailures == before + 2);

		// Disabling interrupt-register verification clears saved state.
		f.mVerifier.OnIRQEntry();
		f.mVerifier.SetFlags(0);
		f.mVerifier.SetFlags(kATVerifierFlag_InterruptRegs);
		f.Verify(0x40);
		AT_PORTABLE_TEST_ASSERT(context, f.mVerifierFailures == before + 2);

		// Recursive NMI state is entered once, reported once, and cleared by RTI/reset.
		f.mVerifier.SetFlags(kATVerifierFlag_RecursiveNMI);
		f.SetS(0x80);
		f.mVerifier.OnReset();
		f.mVerifier.OnNMIEntry();
		f.mCPU.mPC = 0x9000;
		f.mVerifier.OnNMIEntry();
		AT_PORTABLE_TEST_ASSERT(context, f.mVerifierFailures == before + 3);
		f.SetS(0x84);
		f.Verify(0x40);
		f.mVerifier.OnNMIEntry();
		AT_PORTABLE_TEST_ASSERT(context, f.mVerifierFailures == before + 3);
		f.mVerifier.OnReset();
		f.mVerifier.OnNMIEntry();
		f.SetS(0x84);
		f.Verify(0x40);

		// A JMP may notify, and then continue to a second rule. The second rule
		// observes CPU state after the synchronous first-event callback.
		f.mVerifier.SetFlags(kATVerifierFlag_UndocumentedKernelEntry | kATVerifierFlag_CallingConventionViolations);
		f.mVerifier.RemoveAllowedTarget(ATKernelSymbols::SIOV);
		f.mbKernelROM = true;
		f.mKernelROMAddress = ATKernelSymbols::SIOV;
		f.mCPU.mInsnPC = 0x2000;
		f.mCPU.mP = 0;
		bool changedState = false;
		const uint32 stateMutationId = f.mEvents.AddEventCallback(kATSimEvent_VerifierFailure, [&] {
			if (!changedState) {
				changedState = true;
				f.mCPU.mP |= AT6502::kFlagI;
			}
		});
		before = f.mVerifierFailures;
		f.Verify(0x4C, ATKernelSymbols::SIOV);
		AT_PORTABLE_TEST_ASSERT(context, f.mVerifierFailures == before + 2);
		f.mEvents.RemoveEventCallback(stateMutationId);

		// Init re-entry replaces the old abnormal-DMA callback, and Shutdown
		// removes it from the real event manager.
		f.mVerifier.SetFlags(kATVerifierFlag_AbnormalDMA);
		before = f.mVerifierFailures;
		f.mEvents.NotifyEvent(kATSimEvent_AbnormalDMA);
		AT_PORTABLE_TEST_ASSERT(context, f.mVerifierFailures == before + 1);
		f.mVerifier.Init(ATCPUVerifierConnections {
			[&f] { return f.mCPU; },
			[&f](uint16 address) { return f.mbKernelROM && address == f.mKernelROMAddress; },
			[&f](uint16 address) { return f.mMemory[address]; },
			[&f](uint16 address) { return static_cast<uint16>(f.mMemory[address] | (static_cast<uint16>(f.mMemory[static_cast<uint16>(address + 1)]) << 8)); },
			[&f] { return f.mDisplayListPtr; },
			[&f] { return f.mbDisplayListEnabled; }
		}, &f.mEvents);
		f.mVerifier.SetFlags(kATVerifierFlag_AbnormalDMA);
		f.mEvents.NotifyEvent(kATSimEvent_AbnormalDMA);
		AT_PORTABLE_TEST_ASSERT(context, f.mVerifierFailures == before + 2);
		f.mVerifier.Shutdown();
		f.mEvents.NotifyEvent(kATSimEvent_AbnormalDMA);
		AT_PORTABLE_TEST_ASSERT(context, f.mVerifierFailures == before + 2);
		return true;
	}
}

bool ATTestAltirraCPUVerifierAddressing(ATPortableTestContext& context) {
	return TestAddressRules(context) && TestIndexedWrapRules(context);
}

bool ATTestAltirraCPUVerifierStack(ATPortableTestContext& context) {
	return TestStackRules(context);
}

bool ATTestAltirraCPUVerifierInterrupts(ATPortableTestContext& context) {
	return TestInterruptAndLifecycle(context);
}
