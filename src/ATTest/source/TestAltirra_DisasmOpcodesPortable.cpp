// Portable disassembler and opcode-length regression tests.

#include <cstring>

#include <at/attest/portabletest.h>
#include <at/atcpu/history.h>
#include <at/atdebugger/target.h>
#include <at/atcore/address.h>
#include <vd2/system/VDString.h>
#include <disasm.h>

namespace {

ATCPUHistoryEntry MakeHistory(uint16 pc, uint8 opcode, uint8 operand0 = 0, uint8 operand1 = 0, uint8 operand2 = 0) {
	ATCPUHistoryEntry history {};
	history.mPC = pc;
	history.mEA = 0xFFFFFFFFU;
	history.mGlobalPCBase = 0;
	history.mOpcode[0] = opcode;
	history.mOpcode[1] = operand0;
	history.mOpcode[2] = operand1;
	history.mOpcode[3] = operand2;
	return history;
}

bool Check6502Golden(ATPortableTestContext& context) {
	struct Golden {
		uint8 opcode;
		uint8 arg0;
		uint8 arg1;
		ATDebugDisasmMode mode;
		uint16 pc;
		const char *text;
	};
	// Representative immediate, zero-page indexed, absolute indexed, indirect,
	// relative, and CMOS-only addressing forms. The byte fields are fully
	// initialized so the formatter never depends on stale history data.
	static constexpr Golden kCases[] = {
		{ 0xA9, 0x42, 0, kATDebugDisasmMode_6502, 0x8000, "LDA #$42" },
		{ 0xB5, 0x23, 0, kATDebugDisasmMode_6502, 0x8000, "LDA $23,X            ;$23" },
		{ 0xBD, 0x34, 0x12, kATDebugDisasmMode_6502, 0x8000, "LDA $1234,X          ;$1234" },
		{ 0x6C, 0x34, 0x12, kATDebugDisasmMode_6502, 0x8000, "JMP ($1234)" },
		{ 0xD0, 0xFC, 0, kATDebugDisasmMode_6502, 0x8000, "BNE $7FFE" },
		{ 0xA9, 0x42, 0, kATDebugDisasmMode_6502, 0xFFFF, "LDA #$42" },
		{ 0x80, 0xFC, 0, kATDebugDisasmMode_65C02, 0x8000, "BRA $7FFE" },
		{ 0xA9, 0x34, 0x12, kATDebugDisasmMode_65C816, 0x8000, "LDA #$34" },
	};
	for (const Golden& test : kCases) {
		ATCPUHistoryEntry history = MakeHistory(test.pc, test.opcode, test.arg0, test.arg1);
		history.mbEmulation = true;
		VDStringA line;
		const ATDisasmResult result = ATDisassembleInsn(line, ATDisasmServices {}, nullptr,
			test.mode, history, false, true, false, false, false);
		const unsigned expectedLength = ATGetOpcodeLength(test.opcode, history.mP,
			history.mbEmulation, test.mode);
		AT_PORTABLE_TEST_ASSERT(context, result.mNextPC == static_cast<uint16>(test.pc + expectedLength));
		AT_PORTABLE_TEST_ASSERT(context, result.mOperandEnd >= result.mOperandStart);
		AT_PORTABLE_TEST_ASSERT(context, !strcmp(line.c_str(), test.text));
		if (test.pc == 0xFFFF) {
			VDStringA decorated;
			const auto decoratedResult = ATDisassembleInsn(decorated, ATDisasmServices {}, nullptr, test.mode, history,
				false, true, true, true, false, true, true);
			AT_PORTABLE_TEST_ASSERT(context, decorated.c_str()[0] != 0);
			AT_PORTABLE_TEST_ASSERT(context, decorated.find("FFFF") != VDStringA::npos);
			AT_PORTABLE_TEST_ASSERT(context, decorated.find("A9") != VDStringA::npos);
			AT_PORTABLE_TEST_ASSERT(context, decorated.find("lda") != VDStringA::npos);
			AT_PORTABLE_TEST_ASSERT(context, decorated.find("LDA") == VDStringA::npos);
			AT_PORTABLE_TEST_ASSERT(context, decoratedResult.mOperandStart > 0 && decoratedResult.mOperandEnd >= decoratedResult.mOperandStart);
			AT_PORTABLE_TEST_ASSERT(context, result.mNextPC == 1);
		}
	}
	for(bool shortWidth : { false, true }) {
		for(uint8 opcode : { uint8(0xA9), uint8(0xA2) }) {
			auto h = MakeHistory(0x8000, opcode, 0x34, 0x12);
			h.mbEmulation = false;
			h.mP = shortWidth ? 0x30 : 0;
			VDStringA line;
			ATDisassembleInsn(line, ATDisasmServices{}, nullptr, kATDebugDisasmMode_65C816, h, false, true, false, false, false);
			const char *expected = opcode == 0xA9 ? (shortWidth ? "LDA #$34" : "LDA #$1234") : (shortWidth ? "LDX #$34" : "LDX #$1234");
			AT_PORTABLE_TEST_ASSERT(context, !strcmp(line.c_str(), expected));
		}
	}
	struct GlobalCase { uint32 mBase; const char *mpExpected; };
	for(const auto& c : { GlobalCase{ kATAddressSpace_CART | 0x020000, "cart:021234: NOP" },
		GlobalCase{ kATAddressSpace_PORTB | 0x020000, " 02'1234: NOP" },
		GlobalCase{ kATAddressSpace_CB | 0x020000, "t:02'1234: NOP" } }) {
		auto h = MakeHistory(0x1234, 0xEA); h.mGlobalPCBase = c.mBase;
		VDStringA line;
		ATDisassembleInsn(line, ATDisasmServices{}, nullptr, kATDebugDisasmMode_6502, h,
			false, true, true, false, false, false, false, true, false, true);
		AT_PORTABLE_TEST_ASSERT(context, !strcmp(line.c_str(), c.mpExpected));
	}
	return true;
}

bool CheckOpcodeLengthCoverage(ATPortableTestContext& context) {
	for (unsigned opcode = 0; opcode < 256; ++opcode) {
		for (unsigned subMode = kATCPUSubMode_6502; subMode < kATCPUSubModeCount; ++subMode) {
			const int length = ATGetOpcodeLength(static_cast<uint8>(opcode), static_cast<ATCPUSubMode>(subMode));
			AT_PORTABLE_TEST_ASSERT(context, length >= 1 && length <= 4);
			ATDebugDisasmMode legacyMode = kATDebugDisasmMode_6502;
			uint8 p = 0;
			bool emulation = false;
			if (subMode == kATCPUSubMode_65C02)
				legacyMode = kATDebugDisasmMode_65C02;
			else if (subMode >= kATCPUSubMode_65C816_Emulation) {
				legacyMode = kATDebugDisasmMode_65C816;
				emulation = subMode == kATCPUSubMode_65C816_Emulation;
				if (subMode == kATCPUSubMode_65C816_NativeM8X16) p = 0x20;
				if (subMode == kATCPUSubMode_65C816_NativeM16X8) p = 0x10;
				if (subMode == kATCPUSubMode_65C816_NativeM8X8) p = 0x30;
			}
			AT_PORTABLE_TEST_ASSERT(context, length == ATGetOpcodeLength(static_cast<uint8>(opcode), p, emulation, legacyMode));
		}
		for (unsigned p = 0; p < 256; ++p) {
			const int emulationLength = ATGetOpcodeLength(static_cast<uint8>(opcode), static_cast<uint8>(p), true, kATDebugDisasmMode_65C816);
			const int nativeLength = ATGetOpcodeLength(static_cast<uint8>(opcode), static_cast<uint8>(p), false, kATDebugDisasmMode_65C816);
			AT_PORTABLE_TEST_ASSERT(context, emulationLength >= 1 && emulationLength <= 4);
			AT_PORTABLE_TEST_ASSERT(context, nativeLength >= 1 && nativeLength <= 4);
		}
		// These table helpers exclude prefix bytes; zero means an absent entry.
		AT_PORTABLE_TEST_ASSERT(context, ATGetOpcodeLengthZ80(static_cast<uint8>(opcode)) <= 4);
		AT_PORTABLE_TEST_ASSERT(context, ATGetOpcodeLengthZ80ED(static_cast<uint8>(opcode)) <= 4);
		AT_PORTABLE_TEST_ASSERT(context, ATGetOpcodeLengthZ80CB(static_cast<uint8>(opcode)) == 1);
		AT_PORTABLE_TEST_ASSERT(context, ATGetOpcodeLengthZ80DDFD(static_cast<uint8>(opcode)) <= 4);
		AT_PORTABLE_TEST_ASSERT(context, ATGetOpcodeLengthZ80DDFDCB(static_cast<uint8>(opcode)) == 2);
	}
	// Decode every byte value through each 6502-family table. The chosen
	// sentinel EA prevents accidental reads through pointer addressing modes.
	static constexpr ATDebugDisasmMode kModes[] = {
		kATDebugDisasmMode_6502, kATDebugDisasmMode_65C02, kATDebugDisasmMode_65C816
	};
	static constexpr uint8 k816Flags[] = { 0x00, 0x20, 0x10, 0x30, 0x30 };
	for (ATDebugDisasmMode mode : kModes) {
		for (unsigned op = 0; op < 256; ++op) {
			const unsigned flagCount = mode == kATDebugDisasmMode_65C816 ? 5 : 1;
			for (unsigned flagIndex = 0; flagIndex < flagCount; ++flagIndex) {
				ATCPUHistoryEntry history = MakeHistory(0x4000, static_cast<uint8>(op), 0xFF, 0xFF, 0xFF);
				history.mbEmulation = mode != kATDebugDisasmMode_65C816 || flagIndex == 4;
				history.mP = k816Flags[flagIndex];
				VDStringA line;
				const auto decoded = ATDisassembleInsn(line, ATDisasmServices {}, nullptr, mode,
					history, false, true, false, false, false);
				const int expected = ATGetOpcodeLength(static_cast<uint8>(op), history.mP,
					history.mbEmulation, mode);
				AT_PORTABLE_TEST_ASSERT(context, decoded.mNextPC == static_cast<uint16>(history.mPC + expected));
				AT_PORTABLE_TEST_ASSERT(context, line.c_str()[0] != 0);
			}
		}
	}
	return true;
}

bool CheckOtherCPUFormatting(ATPortableTestContext& context) {
	struct Case { ATDebugDisasmMode mode; uint8 opcode; uint8 a; uint8 b; uint8 c; };
	static constexpr Case kCases[] = {
		{ kATDebugDisasmMode_Z80, 0x00, 0, 0, 0 },
		{ kATDebugDisasmMode_Z80, 0xCB, 0x11, 0, 0 },
		{ kATDebugDisasmMode_Z80, 0xED, 0x44, 0, 0 },
		{ kATDebugDisasmMode_Z80, 0xDD, 0x7E, 0xFE, 0 },
		{ kATDebugDisasmMode_Z80, 0xFD, 0x7E, 0x02, 0 },
		{ kATDebugDisasmMode_Z80, 0xDD, 0xCB, 0xFE, 0x46 },
		{ kATDebugDisasmMode_8048, 0x23, 0, 0, 0 },
		{ kATDebugDisasmMode_8051, 0x04, 0, 0, 0 },
		{ kATDebugDisasmMode_6809, 0x12, 0, 0, 0 },
	};
	for (const Case& test : kCases) {
		ATCPUHistoryEntry history = MakeHistory(0x1000, test.opcode, test.a, test.b, test.c);
		VDStringA line;
		const ATDisasmResult result = ATDisassembleInsn(line, ATDisasmServices {}, nullptr,
			test.mode, history, false, true, false, false, false);
		AT_PORTABLE_TEST_ASSERT(context, line.c_str()[0] != 0);
		if ((test.mode == kATDebugDisasmMode_Z80 && test.opcode == 0x00)
			|| (test.mode == kATDebugDisasmMode_8048 && test.opcode == 0x23)
			|| (test.mode == kATDebugDisasmMode_8051 && test.opcode == 0x04)
			|| (test.mode == kATDebugDisasmMode_6809 && test.opcode == 0x12)) {
			const char *expected = test.mode == kATDebugDisasmMode_8048 ? "1000:   MOV     A,#00H"
				: test.mode == kATDebugDisasmMode_8051 ? "1000:   INC     A" : "1000:   NOP";
			AT_PORTABLE_TEST_ASSERT(context, !strcmp(line.c_str(), expected));
		}
		if (test.mode == kATDebugDisasmMode_Z80 && test.opcode == 0xCB)
			AT_PORTABLE_TEST_ASSERT(context, !strcmp(line.c_str(), "1000:   RL      C"));
		if (test.mode == kATDebugDisasmMode_Z80 && test.opcode == 0xED)
			AT_PORTABLE_TEST_ASSERT(context, !strcmp(line.c_str(), "1000:   NEG"));
		if (test.mode == kATDebugDisasmMode_Z80 && test.opcode == 0xDD && test.a == 0x7E)
			AT_PORTABLE_TEST_ASSERT(context, !strcmp(line.c_str(), "1000:   LD      A,(IX-02h)"));
		if (test.mode == kATDebugDisasmMode_Z80 && test.opcode == 0xFD)
			AT_PORTABLE_TEST_ASSERT(context, !strcmp(line.c_str(), "1000:   LD      A,(IY+02h)"));
		if (test.mode == kATDebugDisasmMode_Z80 && test.opcode == 0xDD && test.a == 0xCB)
			AT_PORTABLE_TEST_ASSERT(context, !strcmp(line.c_str(), "1000:   BIT     0,(IX-02h)"));
		AT_PORTABLE_TEST_ASSERT(context, result.mNextPC != history.mPC);
	}
	return true;
}

} // namespace

bool ATTestAltirraDisasm6502(ATPortableTestContext& context) {
	return Check6502Golden(context) && CheckOpcodeLengthCoverage(context);
}

bool ATTestAltirraDisasmOtherCPUs(ATPortableTestContext& context) {
	if (!CheckOtherCPUFormatting(context)) return false;
	for(auto mode : { kATDebugDisasmMode_Z80, kATDebugDisasmMode_8048, kATDebugDisasmMode_8051, kATDebugDisasmMode_6809 }) {
		for(uint32 op = 0; op < 256; ++op) {
			for(uint8 prefix : { uint8(0), uint8(0xCB), uint8(0xED), uint8(0xDD), uint8(0xFD) }) {
				if (mode != kATDebugDisasmMode_Z80 && prefix) continue;
				auto h = prefix ? MakeHistory(0xFFFF, prefix, static_cast<uint8>(op), 0xFE, 0x46)
					: MakeHistory(0xFFFF, static_cast<uint8>(op), 0x80, 0x12, 0x34);
				h.mB = 0x56; // Fifth history byte for the longest 6809 forms.
				VDStringA line;
				const auto r = ATDisassembleInsn(line, ATDisasmServices{}, nullptr, mode, h, false, true, true, true, false);
				const uint16 length = static_cast<uint16>(r.mNextPC - h.mPC);
				AT_PORTABLE_TEST_ASSERT(context, length >= 1 && length <= (mode == kATDebugDisasmMode_6809 ? 5 : 4));
				AT_PORTABLE_TEST_ASSERT(context, !line.empty() && r.mOperandEnd <= line.size());
				AT_PORTABLE_TEST_ASSERT(context, r.mOperandStart <= r.mOperandEnd);
			}
		}
	}
	// AJMP uses the 8051's post-instruction 2K page, not the 8048 bank bit.
	for(uint16 pc : { uint16(0x2800), uint16(0x2FFF) }) {
		auto h = MakeHistory(pc, 0x01, 0x42);
		VDStringA line;
		ATDisassembleInsn(line, ATDisasmServices{}, nullptr, kATDebugDisasmMode_8051, h, false, true, false, false, false);
		AT_PORTABLE_TEST_ASSERT(context, line.find(pc == 0x2800 ? "2842H" : "3042H") != VDStringA::npos);
	}
	return true;
}
