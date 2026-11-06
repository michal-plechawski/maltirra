// Portable tests for shared disassembler reference decoding and symbol services.

#include <array>
#include <algorithm>
#include <memory>
#include <vector>

#include <at/attest/portabletest.h>
#include <at/atcpu/history.h>
#include <at/atcpu/execstate.h>
#include <vd2/system/vdstl_fastvector.h>
#include <at/atdebugger/target.h>
#include <at/atdebugger/symbols.h>
#include <vd2/system/VDString.h>
#include <disasm.h>
#include <memorymanager.h>

namespace {
	struct DisasmTarget final : IATDebugTarget {
		DisasmTarget() : mRAM(0x1000000) {
			mMemory.Init();
			mMemory.SetHighMemoryEnabled(true);
			mpLayer = mMemory.CreateLayer(kATMemoryPri_BaseRAM, mRAM.data(), 0, 0x10000, false);
			mMemory.EnableLayer(mpLayer, true);
		}
		~DisasmTarget() { mMemory.DeleteLayer(mpLayer); }
		void *AsInterface(uint32 id) override { return id == 0 ? this : nullptr; }
		const char *GetName() override { return "portable disassembly target"; }
		ATDebugDisasmMode GetDisasmMode() override { return kATDebugDisasmMode_6502; }
		float GetDisplayCPUClock() const override { return 1.0f; }
		void GetExecState(ATCPUExecState& s) override { s = mState; }
		void SetExecState(const ATCPUExecState& s) override { mState = s; }
		sint32 GetTimeSkew() override { return 0; }
		uint8 ReadByte(uint32 a) override { return DebugReadByte(a); }
		void ReadMemory(uint32 a, void *p, uint32 n) override { for (uint32 i=0;i<n;++i) static_cast<uint8 *>(p)[i]=ReadByte(a+i); }
		uint8 DebugReadByte(uint32 a) override { mReadAddresses.push_back(a); return mMemory.CPUDebugExtReadByte((uint16)a, (uint8)(a >> 16)); }
		void DebugReadMemory(uint32 a, void *p, uint32 n) override { for (uint32 i=0;i<n;++i) static_cast<uint8 *>(p)[i]=DebugReadByte(a+i); }
		void WriteByte(uint32 a, uint8 v) override { mMemory.ExtWriteByte((uint16)a, (uint8)(a >> 16), v); }
		void WriteMemory(uint32 a, const void *p, uint32 n) override { for (uint32 i=0;i<n;++i) WriteByte(a+i,static_cast<const uint8 *>(p)[i]); }
		ATMemoryManager mMemory;
		std::vector<uint8> mRAM;
		ATCPUExecState mState {};
		ATMemoryLayer *mpLayer = nullptr;
		std::vector<uint32> mReadAddresses;
		void ClearReads() { mReadAddresses.clear(); }
	};

	ATCPUHistoryEntry MakeInsn(uint16 pc, uint8 op, uint8 b1=0, uint8 b2=0) {
		ATCPUHistoryEntry h {};
		h.mPC=pc; h.mOpcode[0]=op; h.mOpcode[1]=b1; h.mOpcode[2]=b2; h.mbEmulation=true; h.mP=0x30; h.mEA=0xFFFFFFFF;
		return h;
	}
	ATDisasmResult Render(VDStringA& s, DisasmTarget& t, const ATDisasmServices& sv, ATDebugDisasmMode mode, const ATCPUHistoryEntry& h, bool live=true, bool history=false, bool labels=false, bool symbols=true) {
		return ATDisassembleInsn(s, sv, &t, mode, h, live, history, false, false, labels, false, false, true, symbols, false);
	}
	bool ReadsAre(const DisasmTarget& target, std::initializer_list<uint32> expected) {
		auto actual = target.mReadAddresses;
		std::vector<uint32> addresses(expected);
		std::sort(actual.begin(), actual.end());
		std::sort(addresses.begin(), addresses.end());
		return actual == addresses;
	}
}

bool ATTestAltirraDisasmReferences(ATPortableTestContext& context) {
	auto owner = std::make_unique<DisasmTarget>();
	auto& target = *owner;
	ATDisasmServices services;
	auto h=MakeInsn(0x2000,0xAD,0x34,0x12); // LDA $1234
	VDStringA line;
	Render(line,target,services,kATDebugDisasmMode_6502,h,true,false);
	AT_PORTABLE_TEST_ASSERT(context, h.mEA == 0xFFFFFFFF);
	AT_PORTABLE_TEST_ASSERT(context, ReadsAre(target, { 0x1234 }));
	const uint32 reads=target.mReadAddresses.size();
	line.clear(); Render(line,target,services,kATDebugDisasmMode_6502,h,false,true);
	AT_PORTABLE_TEST_ASSERT(context, target.mReadAddresses.size() == reads);
	AT_PORTABLE_TEST_ASSERT(context, h.mEA == 0xFFFFFFFF);
	line.clear(); auto result=Render(line,target,services,kATDebugDisasmMode_6502,h,true,false);
	AT_PORTABLE_TEST_ASSERT(context, result.mOperandStart > 0 && result.mOperandEnd >= result.mOperandStart);
	AT_PORTABLE_TEST_ASSERT(context, result.mOperandEnd < line.size() && line.c_str()[result.mOperandEnd] == ' ');
	// Absolute indexed history references come from recorded EA, with reads suppressed.
	h=MakeInsn(0x3000,0xBD,0x00,0x40); h.mX=3; h.mEA=0x4003;
	target.ClearReads(); line.clear(); Render(line,target,services,kATDebugDisasmMode_6502,h,false,true);
	AT_PORTABLE_TEST_ASSERT(context, h.mEA == 0x4003);
	AT_PORTABLE_TEST_ASSERT(context, target.mReadAddresses.empty());
	AT_PORTABLE_TEST_ASSERT(context, line.find(";$4003") != VDStringA::npos);
	// NMOS indirect JMP wraps its high-byte read within the instruction page.
	h=MakeInsn(0x3100,0x6C,0xFF,0x12);
	target.mRAM[0x12FF]=0x78; target.mRAM[0x1200]=0x56; target.mRAM[0x1300]=0x9A;
	target.ClearReads(); line.clear(); Render(line,target,services,kATDebugDisasmMode_6502,h,true,false);
	AT_PORTABLE_TEST_ASSERT(context, ReadsAre(target, { 0x12FF, 0x1200, 0x5678 }));
	AT_PORTABLE_TEST_ASSERT(context, line.find("[$5678]") != VDStringA::npos);
	target.ClearReads(); line.clear(); Render(line,target,services,kATDebugDisasmMode_65C02,h,true,false);
	AT_PORTABLE_TEST_ASSERT(context, ReadsAre(target, { 0x12FF, 0x1300, 0x9A78 }));
	h.mEA = 0xBEEF;
	target.ClearReads(); line.clear(); Render(line,target,services,kATDebugDisasmMode_6502,h,false,true);
	AT_PORTABLE_TEST_ASSERT(context, target.mReadAddresses.empty() && line.find(";$BEEF") != VDStringA::npos);
	// 816 indirect JMP reads its pointer in bank zero, but retains PBK.
	h=MakeInsn(0x3100,0x6C,0xFF,0xFF); h.mK=0x34;
	target.mRAM[0xFFFF]=0x78; target.mRAM[0]=0x56;
	target.ClearReads(); line.clear(); Render(line,target,services,kATDebugDisasmMode_65C816,h,true,false);
	AT_PORTABLE_TEST_ASSERT(context, ReadsAre(target, { 0xFFFF, 0, 0x345678 }));
	AT_PORTABLE_TEST_ASSERT(context, line.find("[$34:5678]") != VDStringA::npos);
	// Absolute control transfers use PBK while ordinary absolute data uses DBK.
	h=MakeInsn(0x3200,0x20,0x78,0x56); h.mK=0x34; h.mbEmulation=false;
	target.ClearReads(); line.clear(); Render(line,target,services,kATDebugDisasmMode_65C816,h,true,false);
	AT_PORTABLE_TEST_ASSERT(context, line.find("[$34:5678]") != VDStringA::npos && ReadsAre(target, { 0x345678 }));
	// 816 absolute indexed uses DBK and carries across a bank boundary.
	h=MakeInsn(0x4000,0xBD,0xFF,0xFF); h.mbEmulation=false; h.mB=0x21; h.mK=0x34; h.mX=1;
	target.ClearReads(); line.clear(); Render(line,target,services,kATDebugDisasmMode_65C816,h,true,false);
	AT_PORTABLE_TEST_ASSERT(context, line.find("[$22:0000]") != VDStringA::npos);
	AT_PORTABLE_TEST_ASSERT(context, ReadsAre(target, { 0x220000 }));
	// Long indexed wraps at 24 bits and needs no pointer reads.
	h=MakeInsn(0x4000,0xBF,0xFF,0xFF); h.mOpcode[3]=0xFF; h.mExt.mXH=0; h.mX=1;
	target.ClearReads(); line.clear(); Render(line,target,services,kATDebugDisasmMode_65C816,h,true,false);
	AT_PORTABLE_TEST_ASSERT(context, line.find("[$00:0000]") != VDStringA::npos && target.mReadAddresses.size()==1 && target.mReadAddresses[0]==0);
	// Direct-page indirect long reads three adjacent pointer bytes in bank zero.
	h=MakeInsn(0x4100,0xA7,0x10); h.mbEmulation=false; h.mD=0x1200;
	target.mRAM[0x1210]=0x34; target.mRAM[0x1211]=0x56; target.mRAM[0x1212]=0x78;
	target.ClearReads(); line.clear(); Render(line,target,services,kATDebugDisasmMode_65C816,h,true,false);
	AT_PORTABLE_TEST_ASSERT(context, target.mReadAddresses.size() == 4);
	AT_PORTABLE_TEST_ASSERT(context, target.mReadAddresses[0]==0x1210 && target.mReadAddresses[1]==0x1211 && target.mReadAddresses[2]==0x1212);
	AT_PORTABLE_TEST_ASSERT(context, target.mReadAddresses[3]==0x785634);
	AT_PORTABLE_TEST_ASSERT(context, line.find("[$78:5634]") != VDStringA::npos);
	// Stack indirect indexed reads bank-zero pointers and data through DBK.
	h=MakeInsn(0x4200,0xB3,0x02); h.mbEmulation=false; h.mB=0x44; h.mK=0x33; h.mY=1; h.mExt.mYH=0; h.mS=0xFE; h.mExt.mSH=0x01;
	target.mRAM[0x0200]=0xFF; target.mRAM[0x0201]=0xFF;
	target.ClearReads(); line.clear(); Render(line,target,services,kATDebugDisasmMode_65C816,h,true,false);
	AT_PORTABLE_TEST_ASSERT(context, ReadsAre(target, { 0x0200, 0x0201, 0x450000 }));
	AT_PORTABLE_TEST_ASSERT(context, line.find("[$45:0000]") != VDStringA::npos);
	// 816 M width controls data reads; stores suppress them entirely.
	h=MakeInsn(0x4300,0xAD,0x00,0x50); h.mbEmulation=false; h.mP=0; target.mRAM[0x5000]=0x11; target.mRAM[0x5001]=0x22;
	target.ClearReads(); line.clear(); Render(line,target,services,kATDebugDisasmMode_65C816,h,true,false);
	AT_PORTABLE_TEST_ASSERT(context, ReadsAre(target, { 0x5000, 0x5001 }));
	AT_PORTABLE_TEST_ASSERT(context, line.find("= $2211") != VDStringA::npos);
	h=MakeInsn(0x4300,0xAD,0x00,0x50); h.mbEmulation=false; h.mP=0x20;
	target.ClearReads(); line.clear(); Render(line,target,services,kATDebugDisasmMode_65C816,h,true,false);
	AT_PORTABLE_TEST_ASSERT(context, target.mReadAddresses.size()==1 && target.mReadAddresses[0]==0x5000);
	h=MakeInsn(0x4300,0x8D,0x00,0x50); h.mbEmulation=false;
	target.ClearReads(); line.clear(); Render(line,target,services,kATDebugDisasmMode_65C816,h,true,false);
	AT_PORTABLE_TEST_ASSERT(context, target.mReadAddresses.size()==0);
	h=MakeInsn(0x4300,0x4E,0x00,0x50); h.mbEmulation=false;
	target.ClearReads(); line.clear(); Render(line,target,services,kATDebugDisasmMode_65C816,h,true,false);
	AT_PORTABLE_TEST_ASSERT(context, target.mReadAddresses.size()==0);
	return true;
}

bool ATTestAltirraDisasmSymbols(ATPortableTestContext& context) {
	auto owner = std::make_unique<DisasmTarget>();
	auto& target = *owner;
	ATDisasmServices services;
	uint32 requestedFlags=0;
	services.mpLookupSymbol=[&](uint32 address,uint32 flags,ATSymbol& symbol) {
		requestedFlags=flags;
		if(address<0x1200 || address>0x1240) return false;
		symbol.mpName="scope.label"; symbol.mOffset=0x1230; symbol.mFlags=kATSymbol_Read|kATSymbol_Execute; symbol.mLine=0; symbol.mFileId=0;
		return true;
	};
	auto h=MakeInsn(0x2000,0xAD,0x34,0x12);
	VDStringA line;
	Render(line,target,services,kATDebugDisasmMode_6502,h,false,false,true,true);
	AT_PORTABLE_TEST_ASSERT(context, requestedFlags == (kATSymbol_Read|kATSymbol_Execute));
	AT_PORTABLE_TEST_ASSERT(context, line.find("scope.label+4") != VDStringA::npos);
	line.clear(); Render(line,target,services,kATDebugDisasmMode_6502,h,false,false,true,false);
	AT_PORTABLE_TEST_ASSERT(context, line.find("scope.label") == VDStringA::npos);
	// Store symbols ask for write access, while path labels are a separate fallback.
	h=MakeInsn(0x2000,0x8D,0x34,0x12); line.clear(); Render(line,target,services,kATDebugDisasmMode_6502,h,false,false,true,true);
	AT_PORTABLE_TEST_ASSERT(context, requestedFlags == kATSymbol_Write);
	services.mpLookupSymbol=[](uint32,uint32,ATSymbol&) { return false; };
	uint32 pathChecks=0; services.mpIsPathStart=[&](uint16 address) { ++pathChecks; return address==0x2000; };
	h=MakeInsn(0x2000,0xAD,0x34,0x12); line.clear(); Render(line,target,services,kATDebugDisasmMode_6502,h,false,false,true,true);
	AT_PORTABLE_TEST_ASSERT(context, line.find("L2000") != VDStringA::npos && pathChecks==1);
	// Namespace removal keeps the final namespace separator and label.
	services.mpLookupSymbol=[](uint32 address,uint32,ATSymbol& symbol) {
		if(address!=0x1234) return false;
		symbol.mpName="scope.label"; symbol.mOffset=0x1234; symbol.mFlags=kATSymbol_Read; symbol.mLine=0; symbol.mFileId=0; return true;
	};
	h=MakeInsn(0x1234,0xEA);
	line.clear(); ATDisassembleInsn(line,services,&target,kATDebugDisasmMode_6502,h,false,false,false,false,true,false,false,false,false,false);
	AT_PORTABLE_TEST_ASSERT(context, line.find(".label") != VDStringA::npos && line.find("scope.label") == VDStringA::npos);
	// 65C02 BBR0 resolves the branch target as an independent exact symbol.
	uint32 exactCalls=0; services.mpLookupSymbol=[&](uint32 address,uint32 flags,ATSymbol& symbol) {
		++exactCalls; if(address!=0x2006) return false;
		symbol.mpName="branch.dest"; symbol.mOffset=address; symbol.mFlags=kATSymbol_Read|kATSymbol_Execute; symbol.mLine=0; symbol.mFileId=0; return true;
	};
	h=MakeInsn(0x2000,0x0F,0x44,0x03); line.clear(); Render(line,target,services,kATDebugDisasmMode_65C02,h,false,false,true,true);
	AT_PORTABLE_TEST_ASSERT(context, exactCalls>=2 && line.find("branch.dest") != VDStringA::npos);
	exactCalls=0;
	line.clear(); Render(line,target,services,kATDebugDisasmMode_65C02,h,false,false,false,false);
	AT_PORTABLE_TEST_ASSERT(context, exactCalls == 0 && line.find("branch.dest") == VDStringA::npos);
	// Branch symbol requests wrap within the instruction's 16-bit bank.
	services.mpLookupSymbol=[](uint32 address,uint32,ATSymbol& symbol) {
		if (address != 5 && address != 4) return false;
		symbol = {}; symbol.mpName="wrapped.dest"; symbol.mOffset=address; return true;
	};
	h=MakeInsn(0xFFFF,0x0F,0x44,0x03);
	line.clear(); Render(line,target,services,kATDebugDisasmMode_65C02,h,false,false,false,true);
	AT_PORTABLE_TEST_ASSERT(context, line.find("wrapped.dest") != VDStringA::npos);
	h=MakeInsn(0xFFFF,0xD0,0x03);
	line.clear(); Render(line,target,services,kATDebugDisasmMode_6502,h,false,false,false,true);
	AT_PORTABLE_TEST_ASSERT(context, line.find("wrapped.dest") != VDStringA::npos);
	return true;
}
