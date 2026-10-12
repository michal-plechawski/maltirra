// Portable tests for the real CPU heat-map validity and provenance algorithms.

#include <memory>

#include <at/attest/portabletest.h>
#include <cpuheatmap.h>

namespace {
	using HeatMap = ATCPUHeatMap;

	constexpr uint32 kUnknown = HeatMap::kTypeUnknown;
	constexpr uint32 kPreset = HeatMap::kTypePreset;
	constexpr uint32 kImmediate = HeatMap::kTypeImm;
	constexpr uint32 kComputed = HeatMap::kTypeComputed;
	constexpr uint32 kHardware = HeatMap::kTypeHardware;

	void Insn(HeatMap& heatMap, uint8 opcode, uint16 address = 0, uint16 pc = 0x4000, uint8 sp = 0xFF) {
		heatMap.ProcessInsn(opcode, address, pc, sp);
	}

	bool TestMemoryRanges(ATPortableTestContext& context) {
		auto heatMapStorage = std::make_unique<HeatMap>();
		HeatMap& heatMap = *heatMapStorage;

		AT_PORTABLE_TEST_ASSERT(context, heatMap.GetAStatus() == kUnknown);
		AT_PORTABLE_TEST_ASSERT(context, heatMap.GetXStatus() == kUnknown);
		AT_PORTABLE_TEST_ASSERT(context, heatMap.GetYStatus() == kUnknown);
		AT_PORTABLE_TEST_ASSERT(context, heatMap.GetAValidity() == 0);
		AT_PORTABLE_TEST_ASSERT(context, heatMap.GetPValidity() == 0x34);
		AT_PORTABLE_TEST_ASSERT(context, heatMap.GetMemoryStatus(0) == kPreset);
		AT_PORTABLE_TEST_ASSERT(context, heatMap.GetMemoryStatus(0xFFFF) == (kPreset + 0xFFFF));
		AT_PORTABLE_TEST_ASSERT(context, heatMap.GetMemoryValidity(0) == 0);
		AT_PORTABLE_TEST_ASSERT(context, heatMap.GetMemoryAccesses(0) == 0);
		heatMap.ResetMemoryRange(0, 0x10000);
		AT_PORTABLE_TEST_ASSERT(context, heatMap.GetMemoryStatus(0) == kUnknown);
		AT_PORTABLE_TEST_ASSERT(context, heatMap.GetMemoryStatus(0xFFFF) == kUnknown);
		heatMap.PresetMemoryRange(0, 0x10000);
		AT_PORTABLE_TEST_ASSERT(context, heatMap.GetMemoryStatus(0) == kPreset);
		AT_PORTABLE_TEST_ASSERT(context, heatMap.GetMemoryStatus(0xFFFF) == (kPreset + 0xFFFF));
		AT_PORTABLE_TEST_ASSERT(context, heatMap.GetMemoryValidity(0) == 0xFF);
		AT_PORTABLE_TEST_ASSERT(context, heatMap.GetMemoryValidity(0xFFFF) == 0xFF);

		// Zero-length and out-of-range requests do not touch the final valid byte.
		heatMap.MarkMemoryRangeHardware(0xFFFF, 0);
		heatMap.PresetMemoryRange(0x10000, 4);
		heatMap.ResetMemoryRange(0x10000, 1);
		AT_PORTABLE_TEST_ASSERT(context, heatMap.GetMemoryStatus(0xFFFF) == (kPreset + 0xFFFF));

		// The end of the 64K address space clips oversized lengths without wrapping.
		heatMap.MarkMemoryRangeHardware(0xFFFE, 0xFFFFFFFFu);
		AT_PORTABLE_TEST_ASSERT(context, heatMap.GetMemoryStatus(0xFFFE) == (kHardware + 0xFFFE));
		AT_PORTABLE_TEST_ASSERT(context, heatMap.GetMemoryStatus(0xFFFF) == (kHardware + 0xFFFF));
		AT_PORTABLE_TEST_ASSERT(context, heatMap.GetMemoryValidity(0xFFFF) == 0xFF);
		AT_PORTABLE_TEST_ASSERT(context, heatMap.GetMemoryAccesses(0xFFFF) == 0);
		heatMap.ResetMemoryRange(0xFFFF, 0xFFFFFFFFu);
		AT_PORTABLE_TEST_ASSERT(context, heatMap.GetMemoryStatus(0xFFFE) == (kHardware + 0xFFFE));
		AT_PORTABLE_TEST_ASSERT(context, heatMap.GetMemoryStatus(0xFFFF) == kUnknown);
		AT_PORTABLE_TEST_ASSERT(context, heatMap.GetMemoryValidity(0xFFFF) == 0);
		AT_PORTABLE_TEST_ASSERT(context, heatMap.GetMemoryAccesses(0xFFFF) == 0);

		heatMap.PresetMemoryRange(0x1234, 2);
		AT_PORTABLE_TEST_ASSERT(context, heatMap.GetMemoryStatus(0x1234) == (kPreset + 0x1234));
		AT_PORTABLE_TEST_ASSERT(context, heatMap.GetMemoryValidity(0x1234) == 0xFF);
		AT_PORTABLE_TEST_ASSERT(context, heatMap.GetMemoryStatus(0x1235) == (kPreset + 0x1235));
		heatMap.ResetMemoryRange(0x1234, 1);
		AT_PORTABLE_TEST_ASSERT(context, heatMap.GetMemoryStatus(0x1234) == kUnknown);
		AT_PORTABLE_TEST_ASSERT(context, heatMap.GetMemoryValidity(0x1234) == 0);
		AT_PORTABLE_TEST_ASSERT(context, heatMap.GetMemoryStatus(0x1235) == (kPreset + 0x1235));

		// Reset clears the full memory map as well as register validity and access history.
		heatMap.MarkMemoryRangeHardware(0x2000, 3);
		heatMap.PresetMemoryRange(0x3000, 3);
		heatMap.Reset();
		AT_PORTABLE_TEST_ASSERT(context, heatMap.GetMemoryStatus(0x2000) == (kPreset + 0x2000));
		AT_PORTABLE_TEST_ASSERT(context, heatMap.GetMemoryStatus(0x3000) == (kPreset + 0x3000));
		AT_PORTABLE_TEST_ASSERT(context, heatMap.GetMemoryValidity(0x2000) == 0);
		AT_PORTABLE_TEST_ASSERT(context, heatMap.GetMemoryValidity(0x3000) == 0);
		AT_PORTABLE_TEST_ASSERT(context, heatMap.GetMemoryAccesses(0x2000) == 0);
		return true;
	}

	bool TestPropagation(ATPortableTestContext& context) {
		auto heatMapStorage = std::make_unique<HeatMap>();
		HeatMap& heatMap = *heatMapStorage;

		// Immediate loads establish fully-known byte values and flags.
		Insn(heatMap, 0xA9, 0, 0x4100); // LDA #imm
		AT_PORTABLE_TEST_ASSERT(context, heatMap.GetAStatus() == (kImmediate + 0x4100));
		AT_PORTABLE_TEST_ASSERT(context, heatMap.GetAValidity() == 0xFF);
		AT_PORTABLE_TEST_ASSERT(context, (heatMap.GetPValidity() & 0x82) == 0x82);
		Insn(heatMap, 0xA2, 0, 0x4102); // LDX #imm
		AT_PORTABLE_TEST_ASSERT(context, heatMap.GetXStatus() == (kImmediate + 0x4102));
		AT_PORTABLE_TEST_ASSERT(context, heatMap.GetXValidity() == 0xFF);
		Insn(heatMap, 0xA0, 0, 0x4104); // LDY #imm
		AT_PORTABLE_TEST_ASSERT(context, heatMap.GetYStatus() == (kImmediate + 0x4104));
		AT_PORTABLE_TEST_ASSERT(context, heatMap.GetYValidity() == 0xFF);

		// Stores retain register provenance/validity and record writes independently.
		Insn(heatMap, 0x8D, 0x2000, 0x4200); // STA abs
		Insn(heatMap, 0x8E, 0x2001, 0x4202); // STX abs
		Insn(heatMap, 0x8C, 0x2002, 0x4204); // STY abs
		AT_PORTABLE_TEST_ASSERT(context, heatMap.GetMemoryStatus(0x2000) == (kImmediate + 0x4100));
		AT_PORTABLE_TEST_ASSERT(context, heatMap.GetMemoryValidity(0x2000) == 0xFF);
		AT_PORTABLE_TEST_ASSERT(context, heatMap.GetMemoryAccesses(0x2000) == HeatMap::kAccessWrite);
		AT_PORTABLE_TEST_ASSERT(context, heatMap.GetMemoryStatus(0x2001) == (kImmediate + 0x4102));
		AT_PORTABLE_TEST_ASSERT(context, heatMap.GetMemoryValidity(0x2001) == 0xFF);
		AT_PORTABLE_TEST_ASSERT(context, heatMap.GetMemoryAccesses(0x2001) == HeatMap::kAccessWrite);
		AT_PORTABLE_TEST_ASSERT(context, heatMap.GetMemoryStatus(0x2002) == (kImmediate + 0x4104));
		AT_PORTABLE_TEST_ASSERT(context, heatMap.GetMemoryValidity(0x2002) == 0xFF);
		AT_PORTABLE_TEST_ASSERT(context, heatMap.GetMemoryAccesses(0x2002) == HeatMap::kAccessWrite);

		// Loads consume stored provenance and independently add read access.
		Insn(heatMap, 0xAD, 0x2000, 0x4300); // LDA abs
		AT_PORTABLE_TEST_ASSERT(context, heatMap.GetAStatus() == (kImmediate + 0x4100));
		AT_PORTABLE_TEST_ASSERT(context, heatMap.GetAValidity() == 0xFF);
		AT_PORTABLE_TEST_ASSERT(context, heatMap.GetMemoryAccesses(0x2000) == (HeatMap::kAccessRead | HeatMap::kAccessWrite));

		// Exercise every indexed/unindexed opcode spelling for the three load families.
		const uint8 loadA[] { 0xA1, 0xA5, 0xAD, 0xB1, 0xB5, 0xB9, 0xBD };
		const uint8 loadX[] { 0xA6, 0xAE, 0xB6, 0xBE };
		const uint8 loadY[] { 0xA4, 0xAC, 0xB4, 0xBC };
		uint16 address = 0x2100;
		for(uint32 i = 0; i < sizeof loadA; ++i, ++address) {
			heatMap.PresetMemoryRange(address, 1);
			Insn(heatMap, loadA[i], address, static_cast<uint16>(0x4310 + i * 2));
			AT_PORTABLE_TEST_ASSERT(context, heatMap.GetAStatus() == (kPreset + address));
			AT_PORTABLE_TEST_ASSERT(context, heatMap.GetAValidity() == 0xFF);
			AT_PORTABLE_TEST_ASSERT(context, heatMap.GetMemoryAccesses(address) == HeatMap::kAccessRead);
		}
		for(uint32 i = 0; i < sizeof loadX; ++i, ++address) {
			heatMap.PresetMemoryRange(address, 1);
			Insn(heatMap, loadX[i], address, static_cast<uint16>(0x4330 + i * 2));
			AT_PORTABLE_TEST_ASSERT(context, heatMap.GetXStatus() == (kPreset + address));
			AT_PORTABLE_TEST_ASSERT(context, heatMap.GetXValidity() == 0xFF);
			AT_PORTABLE_TEST_ASSERT(context, heatMap.GetMemoryAccesses(address) == HeatMap::kAccessRead);
		}
		for(uint32 i = 0; i < sizeof loadY; ++i, ++address) {
			heatMap.PresetMemoryRange(address, 1);
			Insn(heatMap, loadY[i], address, static_cast<uint16>(0x4350 + i * 2));
			AT_PORTABLE_TEST_ASSERT(context, heatMap.GetYStatus() == (kPreset + address));
			AT_PORTABLE_TEST_ASSERT(context, heatMap.GetYValidity() == 0xFF);
			AT_PORTABLE_TEST_ASSERT(context, heatMap.GetMemoryAccesses(address) == HeatMap::kAccessRead);
		}

		// Likewise cover every STA/STX/STY addressing-mode opcode with real provenance.
		const uint8 storeA[] { 0x81, 0x85, 0x8D, 0x91, 0x95, 0x99, 0x9D };
		const uint8 storeX[] { 0x86, 0x8E, 0x96 };
		const uint8 storeY[] { 0x84, 0x8C, 0x94 };
		address = 0x2400;
		for(uint32 i = 0; i < sizeof storeA; ++i, ++address) {
			Insn(heatMap, storeA[i], address, static_cast<uint16>(0x4360 + i * 2));
			AT_PORTABLE_TEST_ASSERT(context, heatMap.GetMemoryStatus(address) == heatMap.GetAStatus());
			AT_PORTABLE_TEST_ASSERT(context, heatMap.GetMemoryValidity(address) == heatMap.GetAValidity());
			AT_PORTABLE_TEST_ASSERT(context, heatMap.GetMemoryAccesses(address) == HeatMap::kAccessWrite);
		}
		for(uint32 i = 0; i < sizeof storeX; ++i, ++address) {
			Insn(heatMap, storeX[i], address, static_cast<uint16>(0x4380 + i * 2));
			AT_PORTABLE_TEST_ASSERT(context, heatMap.GetMemoryStatus(address) == heatMap.GetXStatus());
			AT_PORTABLE_TEST_ASSERT(context, heatMap.GetMemoryValidity(address) == heatMap.GetXValidity());
			AT_PORTABLE_TEST_ASSERT(context, heatMap.GetMemoryAccesses(address) == HeatMap::kAccessWrite);
		}
		for(uint32 i = 0; i < sizeof storeY; ++i, ++address) {
			Insn(heatMap, storeY[i], address, static_cast<uint16>(0x43A0 + i * 2));
			AT_PORTABLE_TEST_ASSERT(context, heatMap.GetMemoryStatus(address) == heatMap.GetYStatus());
			AT_PORTABLE_TEST_ASSERT(context, heatMap.GetMemoryValidity(address) == heatMap.GetYValidity());
			AT_PORTABLE_TEST_ASSERT(context, heatMap.GetMemoryAccesses(address) == HeatMap::kAccessWrite);
		}

		// LDX's Z validity must derive from X, not the unrelated A register.
		heatMap.Reset();
		heatMap.PresetMemoryRange(0x2200, 1);
		Insn(heatMap, 0xAE, 0x2200, 0x4400); // LDX abs
		AT_PORTABLE_TEST_ASSERT(context, heatMap.GetXValidity() == 0xFF);
		AT_PORTABLE_TEST_ASSERT(context, heatMap.GetPValidity() & 0x02);
		AT_PORTABLE_TEST_ASSERT(context, heatMap.GetMemoryAccesses(0x2200) == HeatMap::kAccessRead);
		Insn(heatMap, 0xA9, 0, 0x4402); // A known must not make an unknown X look valid.
		heatMap.ResetMemoryRange(0x2201, 1);
		Insn(heatMap, 0xAE, 0x2201, 0x4404);
		AT_PORTABLE_TEST_ASSERT(context, heatMap.GetXValidity() == 0);
		AT_PORTABLE_TEST_ASSERT(context, !(heatMap.GetPValidity() & 0x02));
		Insn(heatMap, 0xAE, 0x2200, 0x4406);

		// Transfers preserve the source byte's lineage; TSX creates known computed data.
		Insn(heatMap, 0x8A, 0, 0x4500); // TXA
		AT_PORTABLE_TEST_ASSERT(context, heatMap.GetAStatus() == (kPreset + 0x2200));
		AT_PORTABLE_TEST_ASSERT(context, heatMap.GetAValidity() == 0xFF);
		Insn(heatMap, 0xA8, 0, 0x4502); // TAY
		AT_PORTABLE_TEST_ASSERT(context, heatMap.GetYStatus() == (kPreset + 0x2200));
		AT_PORTABLE_TEST_ASSERT(context, heatMap.GetYValidity() == 0xFF);
		Insn(heatMap, 0x98, 0, 0x4503); // TYA
		AT_PORTABLE_TEST_ASSERT(context, heatMap.GetAStatus() == (kPreset + 0x2200));
		Insn(heatMap, 0xAA, 0, 0x4503); // TAX
		AT_PORTABLE_TEST_ASSERT(context, heatMap.GetXStatus() == (kPreset + 0x2200));
		Insn(heatMap, 0xBA, 0, 0x4504); // TSX
		AT_PORTABLE_TEST_ASSERT(context, heatMap.GetXStatus() == kComputed);
		AT_PORTABLE_TEST_ASSERT(context, heatMap.GetXValidity() == 0xFF);

		// Immediate arithmetic/logic retain the source lineage in the original
		// heat-map algorithm; validity additionally depends on known carry.
		Insn(heatMap, 0x38); // SEC makes carry known
		Insn(heatMap, 0x69, 0, 0x4600); // ADC #imm: carry validity is known
		AT_PORTABLE_TEST_ASSERT(context, heatMap.GetAStatus() == (kPreset + 0x2200));
		AT_PORTABLE_TEST_ASSERT(context, heatMap.GetAValidity() == 0xFF);
		Insn(heatMap, 0x29, 0, 0x4602); // AND #imm
		AT_PORTABLE_TEST_ASSERT(context, heatMap.GetAStatus() == (kPreset + 0x2200));
		AT_PORTABLE_TEST_ASSERT(context, heatMap.GetAValidity() == 0xFF);
		Insn(heatMap, 0xE8, 0, 0x4604); // INX
		AT_PORTABLE_TEST_ASSERT(context, heatMap.GetXStatus() == (kComputed + 0x4604));
		AT_PORTABLE_TEST_ASSERT(context, heatMap.GetXValidity() == 0xFF);
		Insn(heatMap, 0xCA, 0, 0x4605); // DEX
		AT_PORTABLE_TEST_ASSERT(context, heatMap.GetXStatus() == (kComputed + 0x4605));
		AT_PORTABLE_TEST_ASSERT(context, heatMap.GetXValidity() == 0xFF);
		Insn(heatMap, 0x88, 0, 0x4606); // DEY
		AT_PORTABLE_TEST_ASSERT(context, heatMap.GetYStatus() == (kComputed + 0x4606));
		AT_PORTABLE_TEST_ASSERT(context, heatMap.GetYValidity() == 0xFF);
		Insn(heatMap, 0xC8, 0, 0x4607); // INY
		AT_PORTABLE_TEST_ASSERT(context, heatMap.GetYStatus() == (kComputed + 0x4607));
		AT_PORTABLE_TEST_ASSERT(context, heatMap.GetYValidity() == 0xFF);

		// Accumulator shifts/rotates propagate byte and carry validity.
		Insn(heatMap, 0x0A, 0, 0x4700); // ASL A
		AT_PORTABLE_TEST_ASSERT(context, heatMap.GetAValidity() == 0xFF);
		AT_PORTABLE_TEST_ASSERT(context, (heatMap.GetPValidity() & 0x83) == 0x83);
		Insn(heatMap, 0x2A, 0, 0x4702); // ROL A
		AT_PORTABLE_TEST_ASSERT(context, heatMap.GetAValidity() == 0xFF);
		Insn(heatMap, 0x4A, 0, 0x4704); // LSR A
		AT_PORTABLE_TEST_ASSERT(context, heatMap.GetAValidity() == 0xFF);
		Insn(heatMap, 0x6A, 0, 0x4706); // ROR A
		AT_PORTABLE_TEST_ASSERT(context, heatMap.GetAValidity() == 0xFF);

		// Memory RMW operations must retain hardware mapping while recording access.
		const uint8 rmwOpcodes[] {
			0x06, 0x0E, 0x16, 0x1E, 0x26, 0x2E, 0x36, 0x3E,
			0x46, 0x4E, 0x56, 0x5E, 0x66, 0x6E, 0x76, 0x7E,
			0xC6, 0xD6, 0xCE, 0xDE, 0xE6, 0xF6, 0xEE, 0xFE
		};
		for(uint32 i = 0; i < sizeof rmwOpcodes; ++i) {
			heatMap.Reset();
			const uint16 target = static_cast<uint16>(0x0100 + i);
			heatMap.MarkMemoryRangeHardware(target, 1);
			Insn(heatMap, 0x08, 0, 0x47F0, static_cast<uint8>(i - 1)); // status write makes hardware validity partial
			AT_PORTABLE_TEST_ASSERT(context, heatMap.GetMemoryValidity(target) == 0x34);
			Insn(heatMap, rmwOpcodes[i], target, static_cast<uint16>(0x4800 + i * 2));
			AT_PORTABLE_TEST_ASSERT(context, heatMap.GetMemoryStatus(target) == (kHardware + target));
			AT_PORTABLE_TEST_ASSERT(context, heatMap.GetMemoryValidity(target) == 0x34);
			AT_PORTABLE_TEST_ASSERT(context, heatMap.GetMemoryAccesses(target) == (HeatMap::kAccessRead | HeatMap::kAccessWrite));
		}

		// Each read-modify-write family writes computed provenance even when all bits are known.
		const uint8 validRmwOpcodes[] {
			0x06, 0x0E, 0x16, 0x1E, 0x26, 0x2E, 0x36, 0x3E,
			0x46, 0x4E, 0x56, 0x5E, 0x66, 0x6E, 0x76, 0x7E,
			0xC6, 0xD6, 0xCE, 0xDE, 0xE6, 0xF6, 0xEE, 0xFE
		};
		for(uint32 i = 0; i < sizeof validRmwOpcodes; ++i) {
			heatMap.Reset();
			heatMap.PresetMemoryRange(0x3100, 1);
			Insn(heatMap, 0x38); // ROL/ROR also require a known input carry bit.
			const uint16 pc = static_cast<uint16>(0x4900 + i * 2);
			Insn(heatMap, validRmwOpcodes[i], 0x3100, pc);
			AT_PORTABLE_TEST_ASSERT(context, heatMap.GetMemoryStatus(0x3100) == (kComputed + pc));
			AT_PORTABLE_TEST_ASSERT(context, heatMap.GetMemoryValidity(0x3100) == 0xFF);
		}

		// Reset-memory followed by store checks unknown memory becomes tainted by its source.
		heatMap.Reset();
		heatMap.ResetMemoryRange(0x3200, 1);
		Insn(heatMap, 0xA9, 0, 0x4A00);
		Insn(heatMap, 0x85, 0x3200, 0x4A02);
		AT_PORTABLE_TEST_ASSERT(context, heatMap.GetMemoryStatus(0x3200) == (kImmediate + 0x4A00));
		AT_PORTABLE_TEST_ASSERT(context, heatMap.GetMemoryValidity(0x3200) == 0xFF);
		AT_PORTABLE_TEST_ASSERT(context, heatMap.GetMemoryAccesses(0x3200) == HeatMap::kAccessWrite);
		return true;
	}

	bool TestStack(ATPortableTestContext& context) {
		auto heatMapStorage = std::make_unique<HeatMap>();
		HeatMap& heatMap = *heatMapStorage;

		// PHP and BRK store status validity at post-push SP + 1, wrapping in page one.
		Insn(heatMap, 0x08, 0, 0x5000, 0xFE); // PHP pushes at $01FF
		AT_PORTABLE_TEST_ASSERT(context, heatMap.GetMemoryValidity(0x01FF) == 0x34);
		Insn(heatMap, 0x00, 0, 0x5002, 0xFF); // BRK wraps to $0100
		AT_PORTABLE_TEST_ASSERT(context, heatMap.GetMemoryValidity(0x0100) == 0x34);

		// PHA stores A's lineage; PLA at post-increment SP consumes it and sets access bits.
		Insn(heatMap, 0xA9, 0, 0x5100); // LDA #imm
		Insn(heatMap, 0x48, 0, 0x5102, 0xFF); // PHA wraps to $0100
		AT_PORTABLE_TEST_ASSERT(context, heatMap.GetMemoryStatus(0x0100) == (kImmediate + 0x5100));
		AT_PORTABLE_TEST_ASSERT(context, heatMap.GetMemoryValidity(0x0100) == 0xFF);
		AT_PORTABLE_TEST_ASSERT(context, heatMap.GetMemoryAccesses(0x0100) == HeatMap::kAccessWrite);
		Insn(heatMap, 0x68, 0, 0x5104, 0x00); // PLA reads $0100
		AT_PORTABLE_TEST_ASSERT(context, heatMap.GetAStatus() == (kImmediate + 0x5100));
		AT_PORTABLE_TEST_ASSERT(context, heatMap.GetAValidity() == 0xFF);
		AT_PORTABLE_TEST_ASSERT(context, heatMap.GetMemoryAccesses(0x0100) == (HeatMap::kAccessRead | HeatMap::kAccessWrite));

		// Exercise the opposite wrap edge: post-SP zero means push address $0101.
		heatMap.Reset();
		Insn(heatMap, 0xA9, 0, 0x5200);
		Insn(heatMap, 0x48, 0, 0x5202, 0x00);
		AT_PORTABLE_TEST_ASSERT(context, heatMap.GetMemoryStatus(0x0101) == (kImmediate + 0x5200));
		AT_PORTABLE_TEST_ASSERT(context, heatMap.GetMemoryValidity(0x0101) == 0xFF);
		AT_PORTABLE_TEST_ASSERT(context, heatMap.GetMemoryAccesses(0x0101) == HeatMap::kAccessWrite);
		Insn(heatMap, 0x68, 0, 0x5204, 0x01);
		AT_PORTABLE_TEST_ASSERT(context, heatMap.GetAStatus() == (kImmediate + 0x5200));
		AT_PORTABLE_TEST_ASSERT(context, heatMap.GetMemoryAccesses(0x0101) == (HeatMap::kAccessRead | HeatMap::kAccessWrite));
		return true;
	}

	bool TestMemoryALU(ATPortableTestContext& context) {
		auto storage = std::make_unique<HeatMap>();
		auto& heatMap = *storage;
		// Every memory-addressing spelling of ORA/AND/EOR/ADC/SBC combines the
		// register and memory masks; the result is attributed to the instruction.
		const uint8 opcodes[] {
			0x01, 0x05, 0x0D, 0x11, 0x15, 0x19, 0x1D,
			0x21, 0x25, 0x2D, 0x31, 0x35, 0x39, 0x3D,
			0x41, 0x45, 0x4D, 0x51, 0x55, 0x59, 0x5D,
			0x61, 0x65, 0x6D, 0x71, 0x75, 0x79, 0x7D,
			0xE1, 0xE5, 0xED, 0xF1, 0xF5, 0xF9, 0xFD
		};
		for (const uint8 opcode : opcodes) {
			for (const bool known : { false, true }) {
				heatMap.Reset();
				if (known)
					heatMap.PresetMemoryRange(0x3000, 1);
				else
					heatMap.ResetMemoryRange(0x3000, 1);
				Insn(heatMap, 0xA9, 0, 0x6000);
				Insn(heatMap, 0x18); // Arithmetic uses a known carry input.
				Insn(heatMap, opcode, 0x3000, 0x6002);
				AT_PORTABLE_TEST_ASSERT(context, heatMap.GetAStatus() == kComputed + 0x6002);
				AT_PORTABLE_TEST_ASSERT(context, heatMap.GetAValidity() == (known ? 0xFF : 0));
				AT_PORTABLE_TEST_ASSERT(context, heatMap.GetMemoryAccesses(0x3000) == HeatMap::kAccessRead);
				AT_PORTABLE_TEST_ASSERT(context, heatMap.GetMemoryStatus(0x3000) == (known ? kPreset + 0x3000 : kUnknown));
			}
		}
		return true;
	}
}

bool ATTestAltirraCPUHeatMapMemory(ATPortableTestContext& context) {
	return TestMemoryRanges(context);
}

bool ATTestAltirraCPUHeatMapPropagation(ATPortableTestContext& context) {
	return TestPropagation(context) && TestMemoryALU(context);
}

bool ATTestAltirraCPUHeatMapStack(ATPortableTestContext& context) {
	return TestStack(context);
}
