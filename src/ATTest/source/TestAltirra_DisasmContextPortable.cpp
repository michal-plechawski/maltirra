// Explicit register observations, instruction capture and width prediction.

#include <array>
#include <memory>
#include <vector>
#include <at/attest/portabletest.h>
#include <at/atcpu/execstate.h>
#include <at/atcpu/history.h>
#include <at/atdebugger/target.h>
#include <disasm.h>
#include <memorymanager.h>

namespace {
	class ContextTarget final : public IATDebugTarget {
	public:
		ContextTarget() {
			mMemory.Init();
			mMemory.SetHighMemoryEnabled(true);
			mpRAM = mMemory.CreateLayer(kATMemoryPri_BaseRAM, mRAM.data(), 0, 256, false);
			mpBank = mMemory.CreateLayer(kATMemoryPri_ExtRAM, mBank.data(), 0x2300, 256, false);
			mMemory.EnableLayer(mpRAM, true);
			mMemory.EnableLayer(mpBank, kATMemoryAccessMode_RW, true);
		}
		~ContextTarget() { mMemory.DeleteLayer(mpBank); mMemory.DeleteLayer(mpRAM); }
		void *AsInterface(uint32) override { return nullptr; }
		const char *GetName() override { return "disassembler register/memory observations"; }
		ATDebugDisasmMode GetDisasmMode() override { return mMode; }
		float GetDisplayCPUClock() const override { return 1.0f; }
		void GetExecState(ATCPUExecState& state) override { ++mContextReads; state = mState; }
		void SetExecState(const ATCPUExecState& state) override { mState = state; }
		sint32 GetTimeSkew() override { return 0; }
		uint8 ReadByte(uint32 address) override { return mMemory.ExtReadByte(static_cast<uint16>(address), static_cast<uint8>(address >> 16)); }
		uint8 DebugReadByte(uint32 address) override {
			mReads.push_back(address);
			return mMemory.DebugExtReadByte(static_cast<uint16>(address), static_cast<uint8>(address >> 16));
		}
		void WriteByte(uint32 address, uint8 value) override { mMemory.ExtWriteByte(static_cast<uint16>(address), static_cast<uint8>(address >> 16), value); }
		void ReadMemory(uint32 address, void *dst, uint32 n) override {
			for(uint32 i = 0; i < n; ++i) static_cast<uint8 *>(dst)[i] = ReadByte(address + i);
		}
		void DebugReadMemory(uint32 address, void *dst, uint32 n) override {
			for(uint32 i = 0; i < n; ++i) static_cast<uint8 *>(dst)[i] = DebugReadByte(address + i);
		}
		void WriteMemory(uint32 address, const void *src, uint32 n) override {
			for(uint32 i = 0; i < n; ++i) WriteByte(address + i, static_cast<const uint8 *>(src)[i]);
		}
		ATMemoryManager mMemory;
		alignas(2) std::array<uint8, 65536> mRAM {}, mBank {};
		ATCPUExecState mState {};
		ATDebugDisasmMode mMode = kATDebugDisasmMode_6502;
		std::vector<uint32> mReads;
		uint32 mContextReads = 0;
	private:
		ATMemoryLayer *mpRAM = nullptr, *mpBank = nullptr;
	};
}

bool ATTestAltirraDisasmContext(ATPortableTestContext& context) {
	ATCPUExecState state {};
	state.m6502.mP = 0x42; state.m6502.mX = 0x12; state.m6502.mXH = 0x34;
	state.m6502.mY = 0x56; state.m6502.mYH = 0x78;
	state.m6502.mS = 0x9A; state.m6502.mSH = 0xBC;
	state.m6502.mDP = 0xDEF0; state.m6502.mB = 0x23; state.m6502.mK = 0x45;
	state.m6502.mbEmulationFlag = false;
	ATCPUHistoryEntry history {};
	history.mCycle = 0xDEADBEEF; history.mPC = 0x4321; history.mOpcode[0] = 0xEA;
	for(auto mode : { kATDebugDisasmMode_6502, kATDebugDisasmMode_65C02, kATDebugDisasmMode_65C816 }) {
		ATDisassembleCaptureRegisterContext(history, state, mode);
		AT_PORTABLE_TEST_ASSERT(context, history.mP == 0x42 && history.mX == 0x12 && history.mExt.mXH == 0x34);
		AT_PORTABLE_TEST_ASSERT(context, history.mY == 0x56 && history.mExt.mYH == 0x78);
		AT_PORTABLE_TEST_ASSERT(context, history.mS == 0x9A && history.mExt.mSH == 0xBC);
		AT_PORTABLE_TEST_ASSERT(context, history.mD == 0xDEF0 && history.mB == 0x23 && history.mK == 0x45 && !history.mbEmulation);
		AT_PORTABLE_TEST_ASSERT(context, history.mCycle == 0xDEADBEEF && history.mPC == 0x4321 && history.mOpcode[0] == 0xEA);
	}
	state = {};
	state.mZ80.mA = 1; state.mZ80.mF = 2; state.mZ80.mB = 3; state.mZ80.mC = 4;
	state.mZ80.mD = 5; state.mZ80.mE = 6; state.mZ80.mH = 7; state.mZ80.mL = 8; state.mZ80.mSP = 0xFEDC;
	ATDisassembleCaptureRegisterContext(history, state, kATDebugDisasmMode_Z80);
	AT_PORTABLE_TEST_ASSERT(context, history.mZ80_A == 1 && history.mZ80_F == 2 && history.mZ80_B == 3 && history.mZ80_C == 4);
	AT_PORTABLE_TEST_ASSERT(context, history.mZ80_D == 5 && history.mExt.mZ80_E == 6 && history.mExt.mZ80_H == 7 && history.mExt.mZ80_L == 8);
	AT_PORTABLE_TEST_ASSERT(context, history.mZ80_SP == 0xFEDC && history.mbEmulation);
	state = {};
	state.m6809.mCC = 0x12; state.m6809.mA = 0x34; state.m6809.mB = 0x56;
	state.m6809.mX = 0x789A; state.m6809.mY = 0xBCDE; state.m6809.mS = 0xF012; state.m6809.mDP = 0x34;
	ATDisassembleCaptureRegisterContext(history, state, kATDebugDisasmMode_6809);
	AT_PORTABLE_TEST_ASSERT(context, history.mP == 0x12 && history.mA == 0x34 && history.mExt.mAH == 0x56);
	AT_PORTABLE_TEST_ASSERT(context, history.mX == 0x9A && history.mExt.mXH == 0x78 && history.mY == 0xDE && history.mExt.mYH == 0xBC);
	AT_PORTABLE_TEST_ASSERT(context, history.mS == 0x12 && history.mExt.mSH == 0xF0 && history.mD == 0x34 && history.mbEmulation);
	state = {}; state.m8048.mA = 0xAB; state.m8048.mPSW = 0xCD;
	ATDisassembleCaptureRegisterContext(history, state, kATDebugDisasmMode_8048);
	AT_PORTABLE_TEST_ASSERT(context, history.mA == 0xAB && history.mP == 0xCD);
	state = {}; state.m8051.mA = 0xEF; state.m8051.mPSW = 0x19;
	ATDisassembleCaptureRegisterContext(history, state, kATDebugDisasmMode_8051);
	AT_PORTABLE_TEST_ASSERT(context, history.mA == 0xEF && history.mP == 0x19);

	auto target = std::make_unique<ContextTarget>();
	target->mState = state; target->mMode = kATDebugDisasmMode_8051;
	ATDisassembleCaptureRegisterContext(target.get(), history);
	AT_PORTABLE_TEST_ASSERT(context, target->mContextReads == 1 && history.mA == 0xEF && history.mP == 0x19);
	target->mBank[0xFFFF] = 0xA9; target->mBank[0] = 0x42; target->mBank[1] = 0xEA; target->mBank[2] = 0x60;
	history = {};
	ATDisassembleCaptureInsnContext(target.get(), 0xFFFF, 0x23, history);
	AT_PORTABLE_TEST_ASSERT(context, history.mPC == 0xFFFF && history.mK == 0x23);
	AT_PORTABLE_TEST_ASSERT(context, history.mOpcode[0] == 0xA9 && history.mOpcode[1] == 0x42 && history.mOpcode[2] == 0xEA && history.mOpcode[3] == 0x60);
	AT_PORTABLE_TEST_ASSERT(context, target->mReads == std::vector<uint32>({ 0x23FFFF, 0x230000, 0x230001, 0x230002 }));
	target->mReads.clear();
	ATDisassembleCaptureInsnContext(target.get(), 0x23FFFF, history);
	AT_PORTABLE_TEST_ASSERT(context, history.mGlobalPCBase == 0x230000 && history.mK == 0 && history.mOpcode[1] == 0x42);
	AT_PORTABLE_TEST_ASSERT(context, target->mReads == std::vector<uint32>({ 0x23FFFF, 0x230000, 0x230001, 0x230002 }));

	target->mMode = kATDebugDisasmMode_6502;
	target->mRAM[0x2000] = 0xA9; target->mRAM[0x2001] = 0x42;
	target->mRAM[0x2002] = 0xEA;
	AT_PORTABLE_TEST_ASSERT(context, ATDisassembleGetFirstAnchor(target.get(), 0x2000, 0x2003, 0) == 0x2000);
	target->mMode = kATDebugDisasmMode_65C816;
	target->mState = {}; target->mState.m6502.mbEmulationFlag = false;
	target->mBank[0xFFFE] = 0xA9; target->mBank[0xFFFF] = 0x42; target->mBank[0] = 0x43;
	AT_PORTABLE_TEST_ASSERT(context, ATDisassembleGetFirstAnchor(target.get(), 0xFFFE, 1, 0x230000) == 0xFFFE);
	return true;
}

bool ATTestAltirraDisasmPrediction(ATPortableTestContext& context) {
	for(uint32 p = 0; p < 256; ++p) {
		for(bool emulation : { false, true }) {
			if (emulation && (p & 0x30) != 0x30) continue;
			for(uint8 opcode : { uint8(0x18), uint8(0x38), uint8(0xC2), uint8(0xE2), uint8(0xFB), uint8(0xEA) }) {
				for(uint8 operand : { uint8(0), uint8(0x31), uint8(0xFF) }) {
					ATCPUHistoryEntry h {};
					h.mP = static_cast<uint8>(p); h.mbEmulation = emulation; h.mOpcode[0] = opcode; h.mOpcode[1] = operand;
					uint8 expected = static_cast<uint8>(p);
					bool expectedEmulation = emulation;
					switch(opcode) {
						case 0x18: expected &= ~1; break;
						case 0x38: expected |= 1; break;
						case 0xC2: expected &= ~operand; if (emulation) expected |= 0x30; break;
						case 0xE2: expected |= operand; break;
						case 0xFB: expectedEmulation = (p & 1) != 0; expected = static_cast<uint8>((p & ~1) | (emulation ? 1 : 0)); if (expectedEmulation) expected |= 0x30; break;
					}
					ATDisassemblePredictContext(h, kATDebugDisasmMode_65C816);
					AT_PORTABLE_TEST_ASSERT(context, h.mP == expected && h.mbEmulation == expectedEmulation);
					for(auto mode : { kATDebugDisasmMode_6502, kATDebugDisasmMode_65C02, kATDebugDisasmMode_Z80 }) {
						h.mP = static_cast<uint8>(p); h.mbEmulation = emulation;
						ATDisassemblePredictContext(h, mode);
						AT_PORTABLE_TEST_ASSERT(context, h.mP == p && h.mbEmulation == emulation);
					}
				}
			}
		}
	}
	return true;
}
