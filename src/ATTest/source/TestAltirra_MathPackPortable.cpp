// Math-pack ABI behavior on the production CPU and memory manager.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <memory>
#include <limits>
#include <string>
#include <at/attest/portabletest.h>
#include <at/atcore/decmath.h>
#include <at/atcore/ksyms.h>
#include <decmath.h>
#include <hlefpaccelerator.h>
#include "maincpuportablefixture.h"
#include "mmuportablefixture.h"

namespace {
	using namespace ATKernelSymbols;
	void Put(ATCPUEmulatorMemory& mem, uint16 a, double n) {
		const auto d = ATDecFloat::FromDouble(n); const auto *p = reinterpret_cast<const uint8 *>(&d);
		for (uint16 i = 0; i < 6; ++i) mem.WriteByte(static_cast<uint16>(a + i), p[i]);
	}
	double Get(ATCPUEmulatorMemory& mem, uint16 a = FR0) { return ATReadDecFloatAsBinary(mem, a); }
	bool Near(double a, double b) { return std::abs(a - b) <= std::max(1.0, std::abs(b)) * 2e-8; }
	bool Carry(const ATCPUEmulator& cpu) { return (cpu.GetP() & AT6502::kFlagC) != 0; }
	void Word(ATCPUEmulatorMemory& mem, uint16 a, uint16 value) { mem.WriteByte(a, static_cast<uint8>(value)); mem.WriteByte(a + 1, static_cast<uint8>(value >> 8)); }
}

bool ATTestAltirraMathPackIntegers(ATPortableTestContext& context) {
	auto h = std::make_unique<MainCPUPortableFixture>(); auto& cpu = h->mCPU; auto& mem = *cpu.GetMemory();
	for (uint32 i = 0; i < 65536; ++i) {
		Word(mem, FR0, static_cast<uint16>(i)); ATAccelIPF(cpu, mem);
		AT_PORTABLE_TEST_ASSERT(context, Get(mem) == i);
		cpu.SetFlagC(); ATAccelFPI(cpu, mem);
		AT_PORTABLE_TEST_ASSERT(context, !Carry(cpu) && mem.ReadByte(FR0) + 256U * mem.ReadByte(FR0 + 1) == i);
	}
	struct C { double n; uint32 expected; bool error; };
	for (const auto c : {C{0.49, 0, false}, C{0.5, 1, false}, C{1.5, 2, false}, C{65535.49, 65535, false}, C{65535.5, 0, true}, C{-1, 0, true}}) {
		Put(mem, FR0, c.n); ATAccelFPI(cpu, mem);
		AT_PORTABLE_TEST_ASSERT(context, Carry(cpu) == c.error);
		if (!c.error) AT_PORTABLE_TEST_ASSERT(context, mem.ReadByte(FR0) + 256U * mem.ReadByte(FR0 + 1) == c.expected);
	}
	return true;
}

bool ATTestAltirraMathPackStrings(ATPortableTestContext& context) {
	auto h = std::make_unique<MainCPUPortableFixture>(); auto& cpu = h->mCPU; auto& mem = *cpu.GetMemory();
	struct C { const char *text, *formatted; double n; };
	for (const auto c : {C{"0", "0", 0}, C{"  -12.5", "-12.5", -12.5}, C{"+123.125", "123.125", 123.125},
		C{"1E+12", "1E+12", 1e12}, C{"1.25E-03", "1.25E-03", 0.00125}, C{"0.01", "0.01", 0.01}}) {
		Word(mem, INBUFF, 0x3000); mem.WriteByte(CIX, 0);
		for (size_t i = 0; i <= std::strlen(c.text); ++i) mem.WriteByte(0x3000 + i, c.text[i]);
		cpu.SetFlagC(); ATAccelAFP(cpu, mem);
		AT_PORTABLE_TEST_ASSERT(context, !Carry(cpu) && Near(Get(mem), c.n) && mem.ReadByte(CIX) == std::strlen(c.text));
		ATAccelFASC(cpu, mem); std::string text;
		for (uint32 i = 0; i < 20; ++i) { const uint8 v = mem.ReadByte(LBUFF + i); text += static_cast<char>(v & 0x7F); if (v & 0x80) break; }
		AT_PORTABLE_TEST_ASSERT(context, text == c.formatted && mem.ReadByte(INBUFF) + 256U * mem.ReadByte(INBUFF + 1) == LBUFF);
	}
	for (const char *text : {"", "?", "1E99"}) {
		Word(mem, INBUFF, 0x3000); mem.WriteByte(CIX, 0); Put(mem, FR0, 17);
		for (size_t i = 0; i <= std::strlen(text); ++i) mem.WriteByte(0x3000 + i, text[i]);
		cpu.ClearFlagC(); ATAccelAFP(cpu, mem);
		AT_PORTABLE_TEST_ASSERT(context, Carry(cpu) && Get(mem) == 17);
	}
	Word(mem, INBUFF, 0x3000); mem.WriteByte(CIX, 253);
	mem.WriteByte(0x30FD, ' '); mem.WriteByte(0x30FE, ' '); mem.WriteByte(0x30FF, '7');
	ATAccelSKPSPC(cpu, mem);
	AT_PORTABLE_TEST_ASSERT(context, mem.ReadByte(CIX) == 255 && cpu.GetY() == 255);
	ATAccelISDIGT(cpu, mem); AT_PORTABLE_TEST_ASSERT(context, !Carry(cpu) && cpu.GetA() == 7);
	mem.WriteByte(0x30FF, ':'); ATAccelISDIGT(cpu, mem); AT_PORTABLE_TEST_ASSERT(context, Carry(cpu));
	return true;
}

bool ATTestAltirraMathPackArithmetic(ATPortableTestContext& context) {
	auto h = std::make_unique<MainCPUPortableFixture>(); auto& cpu = h->mCPU; auto& mem = *cpu.GetMemory();
	for (double a : {-1234.5, -0.125, 0.0, 1.0, 98.125, 99999.0})
		for (double b : {-19.0, -0.25, 0.0, 3.5, 1000.0}) {
			using Fn = void (*)(ATCPUEmulator&, ATCPUEmulatorMemory&);
			for (const auto fn : {Fn(ATAccelFADD), Fn(ATAccelFSUB), Fn(ATAccelFMUL), Fn(ATAccelFDIV)}) {
				Put(mem, FR0, a); Put(mem, FR1, b); cpu.SetFlagC(); fn(cpu, mem);
				if (fn == ATAccelFDIV && b == 0) { AT_PORTABLE_TEST_ASSERT(context, Carry(cpu) && Get(mem) == a); continue; }
				const double expected = fn == ATAccelFADD ? a + b : fn == ATAccelFSUB ? a - b : fn == ATAccelFMUL ? a * b : a / b;
				AT_PORTABLE_TEST_ASSERT(context, !Carry(cpu) && Near(Get(mem), expected));
			}
		}
	Put(mem, FR0, 1e97); Put(mem, FR1, 1e97); ATAccelFMUL(cpu, mem);
	AT_PORTABLE_TEST_ASSERT(context, Carry(cpu) && Near(Get(mem), 1e97));
	return true;
}

bool ATTestAltirraMathPackFunctions(ATPortableTestContext& context) {
	auto h = std::make_unique<MainCPUPortableFixture>(); auto& cpu = h->mCPU; auto& mem = *cpu.GetMemory();
	for (double invalid : {std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()}) {
		auto value = ATDecFloat::FromDouble(17);
		AT_PORTABLE_TEST_ASSERT(context, !value.SetDouble(invalid) && value.ToDouble() == 17);
	}
	for (double n : {0.01, 0.5, 1.0, 3.0, 99.0}) {
		Put(mem, FR0, n); ATAccelLOG(cpu, mem); AT_PORTABLE_TEST_ASSERT(context, !Carry(cpu) && Near(Get(mem), std::log(n)));
		Put(mem, FR0, n); ATAccelLOG10(cpu, mem); AT_PORTABLE_TEST_ASSERT(context, !Carry(cpu) && Near(Get(mem), std::log10(n)));
	}
	for (double n : {-20.0, -1.0, 0.0, 1.0, 4.0}) {
		Put(mem, FR0, n); ATAccelEXP(cpu, mem); AT_PORTABLE_TEST_ASSERT(context, !Carry(cpu) && Near(Get(mem), std::exp(n)));
		Put(mem, FR0, n); ATAccelEXP10(cpu, mem); AT_PORTABLE_TEST_ASSERT(context, !Carry(cpu) && Near(Get(mem), std::pow(10.0, n)));
	}
	for (double n : {-1.0, 0.0}) {
		Put(mem, FR0, n); ATAccelLOG(cpu, mem); AT_PORTABLE_TEST_ASSERT(context, Carry(cpu) && Get(mem) == n);
		Put(mem, FR0, n); ATAccelLOG10(cpu, mem); AT_PORTABLE_TEST_ASSERT(context, Carry(cpu) && Get(mem) == n);
	}
	Put(mem, FR0, 100); ATAccelEXP10(cpu, mem); AT_PORTABLE_TEST_ASSERT(context, Carry(cpu));
	Put(mem, FR0, 231); ATAccelEXP(cpu, mem); AT_PORTABLE_TEST_ASSERT(context, Carry(cpu));
	Put(mem, FR0, 2); Put(mem, 0x3000, 1); Put(mem, 0x3006, 2); Put(mem, 0x300C, 3);
	cpu.SetA(3); cpu.SetX(0); cpu.SetY(0x30); ATAccelPLYEVL(cpu, mem);
	AT_PORTABLE_TEST_ASSERT(context, !Carry(cpu) && Get(mem) == 11);
	Put(mem, FR0, 3); Put(mem, 0x3000, 1); cpu.SetX(0); cpu.SetY(0x30); ATAccelREDRNG(cpu, mem);
	AT_PORTABLE_TEST_ASSERT(context, !Carry(cpu) && Get(mem) == 0.5);
	return true;
}

bool ATTestAltirraMathPackMemory(ATPortableTestContext& context) {
	auto h = std::make_unique<MainCPUPortableFixture>(); auto& cpu = h->mCPU; auto& mem = *cpu.GetMemory();
	Put(mem, 0x30FD, -123.5); cpu.SetX(0xFD); cpu.SetY(0x30); ATAccelFLD0R(cpu, mem);
	AT_PORTABLE_TEST_ASSERT(context, Get(mem) == -123.5 && cpu.GetY() == 0 && (cpu.GetP() & AT6502::kFlagN) && !(cpu.GetP() & AT6502::kFlagZ));
	Word(mem, FLPTR, 0x30FD); ATAccelFLD1P(cpu, mem); AT_PORTABLE_TEST_ASSERT(context, Get(mem, FR1) == -123.5);
	Put(mem, 0x30FD, 17.5); cpu.SetX(0xFD); cpu.SetY(0x30); ATAccelFLD1R(cpu, mem); AT_PORTABLE_TEST_ASSERT(context, Get(mem, FR1) == 17.5);
	Word(mem, FLPTR, 0x30FD); ATAccelFLD0P(cpu, mem); AT_PORTABLE_TEST_ASSERT(context, Get(mem) == 17.5);
	cpu.SetX(0xFE); cpu.SetY(0x31); ATAccelFST0R(cpu, mem); AT_PORTABLE_TEST_ASSERT(context, Get(mem, 0x31FE) == 17.5);
	Word(mem, FLPTR, 0x32FF); ATAccelFST0P(cpu, mem); AT_PORTABLE_TEST_ASSERT(context, Get(mem, 0x32FF) == 17.5);
	ATAccelFMOVE(cpu, mem); AT_PORTABLE_TEST_ASSERT(context, Get(mem, FR1) == 17.5);
	cpu.SetFlagC(); ATAccelZFR0(cpu, mem);
	AT_PORTABLE_TEST_ASSERT(context, Get(mem) == 0 && Carry(cpu) && cpu.GetA() == 0 && cpu.GetX() == FR0 + 6);
	cpu.SetX(0xFC); ATAccelZF1(cpu, mem);
	for (uint16 a : {0xFC, 0xFD, 0xFE, 0xFF, 0, 1}) AT_PORTABLE_TEST_ASSERT(context, mem.ReadByte(a) == 0);
	std::fill_n(h->mRAM.data(), 256, 0xFF); cpu.SetX(5); cpu.SetY(0); ATAccelZFL(cpu, mem);
	AT_PORTABLE_TEST_ASSERT(context, std::all_of(h->mRAM.begin(), h->mRAM.begin() + 256, [](uint8 x) { return x == 0; }) && cpu.GetX() == 5);
	ATAccelLDBUFA(cpu, mem); AT_PORTABLE_TEST_ASSERT(context, mem.ReadByte(INBUFF) + 256U * mem.ReadByte(INBUFF + 1) == LBUFF);
	const uint8 unnormalized[] = {0x42, 0, 0x12, 0x34, 0, 0};
	for (size_t i = 0; i < 6; ++i) mem.WriteByte(FR0 + i, unnormalized[i]);
	ATAccelNORMALIZE(cpu, mem); AT_PORTABLE_TEST_ASSERT(context, mem.ReadByte(FR0) == 0x41 && Get(mem) == 1234 && !Carry(cpu));
	// All math-pack pointers are 16-bit, including six-byte values and
	// text buffers crossing the end of the 6502 address space.
	Put(mem, 0xFFFF, -12.5); Word(mem, FLPTR, 0xFFFF); ATAccelFLD0P(cpu, mem);
	AT_PORTABLE_TEST_ASSERT(context, Get(mem) == -12.5 && ATDebugReadDecFloatAsBinary(mem, 0xFFFF) == -12.5);
	Put(mem, FR0, 7.25); Word(mem, FLPTR, 0xFFFF); ATAccelFST0P(cpu, mem);
	AT_PORTABLE_TEST_ASSERT(context, Get(mem, 0xFFFF) == 7.25);
	Word(mem, INBUFF, 0xFFFF); mem.WriteByte(0, '7'); mem.WriteByte(1, 0); mem.WriteByte(CIX, 1); ATAccelAFP(cpu, mem);
	AT_PORTABLE_TEST_ASSERT(context, !Carry(cpu) && Get(mem) == 7);
	return true;
}

bool ATTestAltirraMathPackHooks(ATPortableTestContext& context) {
	auto h = std::make_unique<MMUPortableFixture>(); auto& cpu = h->mCPU; auto& mem = *cpu.GetMemory();
	h->mHooks.EnableOSHooks(true); h->mMMU.SetBankRegister(0xFF);
	std::unique_ptr<IATHLEFPAccelerator> accel(ATCreateHLEFPAccelerator(&cpu));
	vdvector<std::pair<uint32, bool>> hooks; accel->ListHooks(hooks);
	const uint32 addresses[] = {AFP,FASC,IPF,FPI,FADD,FSUB,FMUL,FDIV,LOG,LOG10,EXP,EXP10,SKPSPC,ISDIGT,NORMALIZE,PLYEVL,ZFR0,ZF1,ZFL,LDBUFA,FLD0R,FLD0P,FLD1R,FLD1P,FST0R,FST0P,FMOVE,REDRNG};
	AT_PORTABLE_TEST_ASSERT(context, hooks.size() == std::size(addresses));
	for (size_t i = 0; i < hooks.size(); ++i) {
		AT_PORTABLE_TEST_ASSERT(context, hooks[i].first == addresses[i] && hooks[i].second);
		AT_PORTABLE_TEST_ASSERT(context, accel->SetHookEnabled(addresses[i], false) && accel->SetHookEnabled(addresses[i], false));
		AT_PORTABLE_TEST_ASSERT(context, h->mHooks.OnHookHit(static_cast<uint16>(addresses[i])) == 0);
		AT_PORTABLE_TEST_ASSERT(context, accel->SetHookEnabled(addresses[i], true));
	}
	AT_PORTABLE_TEST_ASSERT(context, !accel->SetHookEnabled(0x1234, true));
	Put(mem, FR0, 7); Put(mem, FR1, 9);
	// The real CPU fetches JSR FADD and executes the injected RTS.
	h->mRAM[0x2000] = 0x20; h->mRAM[0x2001] = static_cast<uint8>(FADD); h->mRAM[0x2002] = static_cast<uint8>(FADD >> 8);
	h->mRAM[0x2003] = 0x4C; h->mRAM[0x2004] = 3; h->mRAM[0x2005] = 0x20;
	cpu.SetPC(0x2000);
	AT_PORTABLE_TEST_ASSERT(context, h->RunCycles(60) && Get(mem) == 16 && cpu.GetS() == 0xFD);
	h->mHooks.EnableOSHooks(false); AT_PORTABLE_TEST_ASSERT(context, h->mHooks.OnHookHit(FADD) == 0);
	h->mHooks.EnableOSHooks(true); h->mMMU.SetBankRegister(0xFE); AT_PORTABLE_TEST_ASSERT(context, h->mHooks.OnHookHit(FADD) == 0);
	h->mMMU.SetBankRegister(0xFF); accel.reset();
	AT_PORTABLE_TEST_ASSERT(context, h->mHooks.OnHookHit(FADD) == 0);
	return true;
}
