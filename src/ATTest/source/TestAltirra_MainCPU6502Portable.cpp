// Cycle and bus tests for the production main 6502 and 65C02 CPU.

#include <algorithm>
#include <memory>

#include <at/attest/portabletest.h>
#include <cpuflags.h>
#include "maincpuportablefixture.h"

namespace {
	bool CheckCycles(ATPortableTestContext& context, MainCPUPortableFixture& f, int expected) {
		const int actual = f.Step();
		AT_PORTABLE_TEST_ASSERT(context, actual == expected);
		return true;
	}

	uint32 CountWrites(const MainCPUPortableFixture& f, uint32 address) {
		return static_cast<uint32>(std::count_if(f.mBus.begin(), f.mBus.end(), [address](const auto& e) {
			return e.mbWrite && e.mAddress == address;
		}));
	}

	bool HasRead(const MainCPUPortableFixture& f, uint32 address, uint8 value) {
		return std::any_of(f.mBus.begin(), f.mBus.end(), [address, value](const auto& e) {
			return !e.mbWrite && e.mAddress == address && e.mValue == value;
		});
	}

	bool Test6502Addressing(ATPortableTestContext& context, MainCPUPortableFixture& f) {
		struct Case { uint8 op, b0, b1; uint8 x, y; uint8 cycles; uint16 valueAddress; };
		static constexpr Case kCases[] = {
			{ 0xA9, 0x5A, 0,    0, 0, 2, 0 },       // immediate
			{ 0xA5, 0x40, 0,    0, 0, 3, 0x0040 },  // zero page
			{ 0xB5, 0x40, 0,    1, 0, 4, 0x0041 },  // zero page,X
			{ 0xAD, 0x00, 0x40, 0, 0, 4, 0x4000 }, // absolute
			{ 0xBD, 0x00, 0x40, 1, 0, 4, 0x4001 }, // absolute,X, same page
			{ 0xB9, 0x00, 0x40, 0, 1, 4, 0x4001 }, // absolute,Y, same page
			{ 0xA1, 0x40, 0,    1, 0, 6, 0x4000 }, // (zero page,X)
			{ 0xB1, 0x40, 0,    0, 1, 5, 0x4001 }, // (zero page),Y, same page
		};

		for (const Case& test : kCases) {
			f.Prepare(kATCPUMode_6502, 0x2000);
			f.Load(0x2000, { test.op, test.b0, test.b1 });
			f.mCPU.SetX(test.x);
			f.mCPU.SetY(test.y);
			if (test.op == 0xA1 || test.op == 0xB1) {
				const uint16 pointer = test.op == 0xA1 ? 0x41 : 0x40;
				f.mRAM[pointer] = 0x00;
				f.mRAM[pointer + 1] = 0x40;
			}
			if (test.valueAddress)
				f.mRAM[test.valueAddress] = 0x5A;
			AT_PORTABLE_TEST_ASSERT(context, CheckCycles(context, f, test.cycles));
			AT_PORTABLE_TEST_ASSERT(context, f.mCPU.GetA() == 0x5A);
			AT_PORTABLE_TEST_ASSERT(context, f.mCPU.GetPC() == 0x2000 + (test.op == 0xA9 ? 2 : test.op == 0xA5 || test.op == 0xB5 || test.op == 0xA1 || test.op == 0xB1 ? 2 : 3));
		}

		// Absolute,X page crossing adds exactly one read cycle and reaches the
		// carried address; the bus observer sees the real CPU memory transaction.
		f.Prepare(kATCPUMode_6502, 0x20FD);
		f.Load(0x20FD, { 0xBD, 0xFF, 0x20 });
		f.mCPU.SetX(1);
		f.mRAM[0x2100] = 0xA6;
		f.EnableBusTrace();
		AT_PORTABLE_TEST_ASSERT(context, CheckCycles(context, f, 5));
		AT_PORTABLE_TEST_ASSERT(context, f.mCPU.GetA() == 0xA6);
		AT_PORTABLE_TEST_ASSERT(context, HasRead(f, 0x2100, 0xA6));
		return true;
	}

	bool Test6502ArithmeticAndBranch(ATPortableTestContext& context, MainCPUPortableFixture& f) {
		f.Prepare(kATCPUMode_6502, 0x2200, 1, false, AT6502::kFlagI);
		f.mCPU.SetA(0x50);
		f.Load(0x2200, { 0x69, 0x50 }); // ADC #$50
		AT_PORTABLE_TEST_ASSERT(context, CheckCycles(context, f, 2));
		AT_PORTABLE_TEST_ASSERT(context, f.mCPU.GetA() == 0xA0);
		AT_PORTABLE_TEST_ASSERT(context, (f.mCPU.GetP() & (AT6502::kFlagN | AT6502::kFlagV | AT6502::kFlagC | AT6502::kFlagZ)) == (AT6502::kFlagN | AT6502::kFlagV));

		f.Prepare(kATCPUMode_6502, 0x2200, 1, false, AT6502::kFlagI | AT6502::kFlagC);
		f.mCPU.SetA(0);
		f.Load(0x2200, { 0xE9, 0x01 }); // SBC #$01
		AT_PORTABLE_TEST_ASSERT(context, CheckCycles(context, f, 2));
		AT_PORTABLE_TEST_ASSERT(context, f.mCPU.GetA() == 0xFF);
		AT_PORTABLE_TEST_ASSERT(context, (f.mCPU.GetP() & (AT6502::kFlagN | AT6502::kFlagV | AT6502::kFlagC | AT6502::kFlagZ)) == AT6502::kFlagN);

		// NMOS decimal ADC retains the silicon's binary Z flag and decimal N/V.
		f.Prepare(kATCPUMode_6502, 0x2200, 1, false, AT6502::kFlagI | AT6502::kFlagD);
		f.mCPU.SetA(0x49);
		f.Load(0x2200, { 0x69, 0x51 });
		AT_PORTABLE_TEST_ASSERT(context, CheckCycles(context, f, 2));
		AT_PORTABLE_TEST_ASSERT(context, f.mCPU.GetA() == 0x00);
		AT_PORTABLE_TEST_ASSERT(context, (f.mCPU.GetP() & (AT6502::kFlagN | AT6502::kFlagV | AT6502::kFlagC | AT6502::kFlagZ)) == (AT6502::kFlagN | AT6502::kFlagV | AT6502::kFlagC));

		f.Prepare(kATCPUMode_6502, 0x2200, 1, false, AT6502::kFlagI | AT6502::kFlagD | AT6502::kFlagC);
		f.mCPU.SetA(0x00);
		f.Load(0x2200, { 0xE9, 0x01 }); // decimal SBC #$01
		AT_PORTABLE_TEST_ASSERT(context, CheckCycles(context, f, 2));
		AT_PORTABLE_TEST_ASSERT(context, f.mCPU.GetA() == 0x99);
		AT_PORTABLE_TEST_ASSERT(context, (f.mCPU.GetP() & (AT6502::kFlagN | AT6502::kFlagV | AT6502::kFlagC | AT6502::kFlagZ)) == AT6502::kFlagN);

		// Taken branch crossing a page has the base two cycles plus both taken
		// and page-cross penalties.
		f.Prepare(kATCPUMode_6502, 0x22FE, 1, false, AT6502::kFlagI);
		f.Load(0x22FE, { 0xD0, 0xFC }); // BNE $22FC
		AT_PORTABLE_TEST_ASSERT(context, CheckCycles(context, f, 4));
		AT_PORTABLE_TEST_ASSERT(context, f.mCPU.GetPC() == 0x22FC);
		return true;
	}

	bool Test6502StackInterruptAndRMW(ATPortableTestContext& context, MainCPUPortableFixture& f) {
		f.Prepare(kATCPUMode_6502, 0x2000);
		f.Load(0x2000, { 0x20, 0x00, 0x30 }); // JSR $3000
		f.Load(0x3000, { 0x60 });             // RTS
		AT_PORTABLE_TEST_ASSERT(context, CheckCycles(context, f, 6));
		AT_PORTABLE_TEST_ASSERT(context, f.mCPU.GetPC() == 0x3000 && f.mCPU.GetS() == 0xFB);
		AT_PORTABLE_TEST_ASSERT(context, f.mRAM[0x01FD] == 0x20 && f.mRAM[0x01FC] == 0x02);
		AT_PORTABLE_TEST_ASSERT(context, CheckCycles(context, f, 6));
		AT_PORTABLE_TEST_ASSERT(context, f.mCPU.GetPC() == 0x2003 && f.mCPU.GetS() == 0xFD);

		f.Prepare(kATCPUMode_6502, 0x2000, 1, false, AT6502::kFlagI);
		f.Load(0x2000, { 0x00, 0xEA }); // BRK plus padding byte
		f.mRAM[0xFFFE] = 0x00; f.mRAM[0xFFFF] = 0x40;
		AT_PORTABLE_TEST_ASSERT(context, CheckCycles(context, f, 7));
		AT_PORTABLE_TEST_ASSERT(context, f.mCPU.GetPC() == 0x4000 && f.mCPU.GetS() == 0xFA);
		AT_PORTABLE_TEST_ASSERT(context, f.mRAM[0x01FD] == 0x20 && f.mRAM[0x01FC] == 0x02);
		AT_PORTABLE_TEST_ASSERT(context, (f.mRAM[0x01FB] & 0x30) == 0x30);

		f.Prepare(kATCPUMode_6502, 0x2000);
		f.Load(0x2000, { 0xEE, 0x00, 0x40 }); // INC $4000
		f.mRAM[0x4000] = 0x7F;
		f.EnableBusTrace();
		AT_PORTABLE_TEST_ASSERT(context, CheckCycles(context, f, 6));
		AT_PORTABLE_TEST_ASSERT(context, f.mRAM[0x4000] == 0x80);
		AT_PORTABLE_TEST_ASSERT(context, CountWrites(f, 0x4000) == 2);
		uint32 writeCount = 0;
		for (const auto& access : f.mBus) {
			if (access.mbWrite && access.mAddress == 0x4000) {
				AT_PORTABLE_TEST_ASSERT(context, access.mValue == (writeCount++ ? 0x80 : 0x7F));
			}
		}
		AT_PORTABLE_TEST_ASSERT(context, writeCount == 2);
		return true;
	}

	bool Test6502IndirectAndIllegal(ATPortableTestContext& context, MainCPUPortableFixture& f) {
		f.Prepare(kATCPUMode_6502, 0x2000);
		f.Load(0x2000, { 0x6C, 0xFF, 0x12 }); // JMP ($12FF), NMOS page-wrap bug
		f.mRAM[0x12FF] = 0x56;
		f.mRAM[0x1200] = 0x34;
		f.mRAM[0x1300] = 0x99;
		f.EnableBusTrace();
		AT_PORTABLE_TEST_ASSERT(context, CheckCycles(context, f, 5));
		AT_PORTABLE_TEST_ASSERT(context, f.mCPU.GetPC() == 0x3456);
		AT_PORTABLE_TEST_ASSERT(context, HasRead(f, 0x12FF, 0x56));
		AT_PORTABLE_TEST_ASSERT(context, HasRead(f, 0x1200, 0x34));

		f.Prepare(kATCPUMode_6502, 0x2400);
		f.mCPU.SetIllegalInsnsEnabled(true);
		f.Load(0x2400, { 0xA7, 0x40 }); // LAX $40, undocumented load of A and X
		f.mRAM[0x40] = 0x5A;
		AT_PORTABLE_TEST_ASSERT(context, CheckCycles(context, f, 3));
		AT_PORTABLE_TEST_ASSERT(context, f.mCPU.GetA() == 0x5A && f.mCPU.GetX() == 0x5A);
		f.Prepare(kATCPUMode_6502, 0x2400);
		f.Load(0x2400, { 0x07, 0x41 }); // SLO $41, undocumented ASL then ORA
		f.mRAM[0x41] = 0x81; f.mCPU.SetA(1);
		AT_PORTABLE_TEST_ASSERT(context, CheckCycles(context, f, 5));
		AT_PORTABLE_TEST_ASSERT(context, f.mRAM[0x41] == 2 && f.mCPU.GetA() == 3);
		AT_PORTABLE_TEST_ASSERT(context, (f.mCPU.GetP() & AT6502::kFlagC) != 0);
		AT_PORTABLE_TEST_ASSERT(context, CountWrites(f, 0x41) == 2);

		f.Prepare(kATCPUMode_6502, 0x2400);
		f.mCPU.SetIllegalInsnsEnabled(false);
		f.Load(0x2400, { 0x02 }); // KIL is rejected when undocumented opcodes are disabled
		AT_PORTABLE_TEST_ASSERT(context, f.Tick() == kATSimEvent_None); // opcode fetch
		AT_PORTABLE_TEST_ASSERT(context, f.Tick() == kATSimEvent_CPUIllegalInsn);
		return true;
	}

	bool Test65C02Instructions(ATPortableTestContext& context, MainCPUPortableFixture& f) {
		f.Prepare(kATCPUMode_65C02, 0x2000);
		f.Load(0x2000, { 0x64, 0x40 }); // STZ $40
		f.mRAM[0x40] = 0xA5;
		AT_PORTABLE_TEST_ASSERT(context, CheckCycles(context, f, 3));
		AT_PORTABLE_TEST_ASSERT(context, f.mRAM[0x40] == 0);

		f.Prepare(kATCPUMode_65C02, 0x2100, 1, false, AT6502::kFlagI);
		f.Load(0x2100, { 0x80, 0x02 }); // BRA $2104
		AT_PORTABLE_TEST_ASSERT(context, CheckCycles(context, f, 3));
		AT_PORTABLE_TEST_ASSERT(context, f.mCPU.GetPC() == 0x2104);

		f.Prepare(kATCPUMode_65C02, 0x2200);
		f.mCPU.SetX(0xA6);
		f.Load(0x2200, { 0xDA, 0xFA }); // PHX; PLX
		AT_PORTABLE_TEST_ASSERT(context, CheckCycles(context, f, 3));
		AT_PORTABLE_TEST_ASSERT(context, f.mCPU.GetS() == 0xFC && f.mRAM[0x01FD] == 0xA6);
		f.mCPU.SetX(0);
		AT_PORTABLE_TEST_ASSERT(context, CheckCycles(context, f, 4));
		AT_PORTABLE_TEST_ASSERT(context, f.mCPU.GetX() == 0xA6 && f.mCPU.GetS() == 0xFD);

		// TSB sets Z from A & memory then writes the OR result; TRB does the
		// corresponding test and clear.
		f.Prepare(kATCPUMode_65C02, 0x2300, 1, false, AT6502::kFlagI);
		f.mCPU.SetA(0x0F);
		f.mRAM[0x0042] = 0xF0;
		f.Load(0x2300, { 0x04, 0x42 }); // TSB $42
		f.EnableBusTrace();
		AT_PORTABLE_TEST_ASSERT(context, CheckCycles(context, f, 5));
		AT_PORTABLE_TEST_ASSERT(context, f.mRAM[0x0042] == 0xFF);
		AT_PORTABLE_TEST_ASSERT(context, CountWrites(f, 0x0042) == 1);
		AT_PORTABLE_TEST_ASSERT(context, (f.mCPU.GetP() & AT6502::kFlagZ) != 0);

		f.Prepare(kATCPUMode_65C02, 0x2400, 1, false, AT6502::kFlagI);
		f.mCPU.SetA(0xF0);
		f.mRAM[0x0042] = 0x0F;
		f.Load(0x2400, { 0x14, 0x42 }); // TRB $42
		AT_PORTABLE_TEST_ASSERT(context, CheckCycles(context, f, 5));
		AT_PORTABLE_TEST_ASSERT(context, f.mRAM[0x0042] == 0x0F);
		AT_PORTABLE_TEST_ASSERT(context, (f.mCPU.GetP() & AT6502::kFlagZ) != 0);

		// CMOS (zp) is a real indirect mode: two-byte zero-page pointer and one
		// final data read.
		f.Prepare(kATCPUMode_65C02, 0x2500);
		f.mRAM[0x0040] = 0x78; f.mRAM[0x0041] = 0x56;
		f.mRAM[0x5678] = 0xC3;
		f.Load(0x2500, { 0xB2, 0x40 }); // LDA ($40)
		AT_PORTABLE_TEST_ASSERT(context, CheckCycles(context, f, 5));
		AT_PORTABLE_TEST_ASSERT(context, f.mCPU.GetA() == 0xC3);

		// The CMOS JMP (abs) fixes the NMOS page wrap and takes six cycles.
		f.Prepare(kATCPUMode_65C02, 0x2600);
		f.Load(0x2600, { 0x6C, 0xFF, 0x12 });
		f.mRAM[0x12FF] = 0x56;
		f.mRAM[0x1300] = 0x78;
		f.mRAM[0x1200] = 0x34;
		f.EnableBusTrace();
		AT_PORTABLE_TEST_ASSERT(context, CheckCycles(context, f, 6));
		AT_PORTABLE_TEST_ASSERT(context, f.mCPU.GetPC() == 0x7856);
		AT_PORTABLE_TEST_ASSERT(context, HasRead(f, 0x1300, 0x78));
		return true;
	}
}

bool ATTestAltirraMainCPU6502(ATPortableTestContext& context) {
	auto fixture = std::make_unique<MainCPUPortableFixture>();
	MainCPUPortableFixture& f = *fixture;
	return Test6502Addressing(context, f)
		&& Test6502ArithmeticAndBranch(context, f)
		&& Test6502StackInterruptAndRMW(context, f)
		&& Test6502IndirectAndIllegal(context, f);
}

bool ATTestAltirraMainCPU65C02(ATPortableTestContext& context) {
	auto fixture = std::make_unique<MainCPUPortableFixture>();
	MainCPUPortableFixture& f = *fixture;
	return Test65C02Instructions(context, f);
}
