// Actual CPU polling, PBI ABI, ROM-vector discovery, editor, printer and tape.
#include <cmath>
#include <tuple>
#include "cioportablefixture.h"
#include "siomanagerportablefixture.h"
using namespace CIOPortable;

bool ATTestAltirraCIOCPU(ATPortableTestContext& context) {
	auto f=std::make_unique<Machine>(); auto& d=f->Add(); f->Open(); uint32 calls=0;
	d.onGet=[&](void *p,uint32,uint32& n) -> sint32 { n=0; if (++calls<4) return -1; *static_cast<uint8 *>(p)=0x5A; n=1; return 1; };
	const std::array<uint8,9> code {0x20,0x75,0xD5,0x8D,0,0x40,0x4C,6,0x20};
	for (uint16 i=0; i<code.size(); ++i) f->Mem(0x2000+i,code[i]);
	f->h.mCPU.SetS(0xFD); f->h.mCPU.SetX(0x10); f->h.mCPU.SetPC(0x2000);
	AT_PORTABLE_TEST_ASSERT(context, f->h.RunCycles(1000) && calls == 4 && f->Mem(0x4000) == 0x5A && f->h.mCPU.GetS() == 0xFD);
	AT_PORTABLE_TEST_ASSERT(context, f->h.mCPU.GetPC() >= 0x2006 && f->h.mCPU.GetPC() <= 0x2009 && f->h.mCPU.GetY() == 1);
	return true;
}

bool ATTestAltirraCIOPBI(ATPortableTestContext& context) {
	auto f=std::make_unique<Machine>(); auto& d=f->Add(); f->hook->SetPBIHookEnabled(true); f->Establish();
	f->Mem(0x5000,'H'); f->Mem(0x5001,':'); f->Mem(0x5002,0); f->Word(ATKernelSymbols::ICBALZ,0x5000);
	f->Mem(ATKernelSymbols::ICDNOZ,1); f->Mem(ATKernelSymbols::ICAX1Z,12); f->h.mCPU.SetX(0x10);
	f->hook->TryAccelPBIRequest(0);
	AT_PORTABLE_TEST_ASSERT(context, d.opens == 1 && f->h.mCPU.GetY() == 1 && (f->h.mCPU.GetP() & (AT6502::kFlagC|AT6502::kFlagV)) == AT6502::kFlagC);
	uint32 calls=0; d.onGet=[&](void *p,uint32,uint32& n) -> sint32 { n=0; if (++calls==1) return -1; n=1; *static_cast<uint8 *>(p)=0x6B; return 1; };
	f->h.mCPU.SetS(0xFD); f->hook->TryAccelPBIRequest(2);
	AT_PORTABLE_TEST_ASSERT(context, f->h.mCPU.GetP() & AT6502::kFlagV);
	f->hook->TryAccelPBIRequest(6); AT_PORTABLE_TEST_ASSERT(context, calls == 2 && f->h.mCPU.GetA() == 0x6B && !(f->h.mCPU.GetP() & AT6502::kFlagV) && f->h.mCPU.GetS() == 0xFD);
	f->hook->TryAccelPBIRequest(1); AT_PORTABLE_TEST_ASSERT(context, d.closes == 1 && f->h.mCPU.GetP() & AT6502::kFlagC);
	f->h.mCPU.SetP(0x64); f->hook->TryAccelPBIRequest(255); AT_PORTABLE_TEST_ASSERT(context, !(f->h.mCPU.GetP() & AT6502::kFlagV));
	return true;
}

bool ATTestAltirraCIOROM(ATPortableTestContext& context) {
	auto f=std::make_unique<Machine>(); f->screen.reset(ATCreateVirtualScreenHandler(*f->h.mCPU.GetMemory()));
	f->Mem(ATKernelSymbols::DMACTL,2); f->Mem(ATKernelSymbols::RMARGN,39); f->screen->Enable();
	// Separate exact-sized ROM allocations make boundary probes visible to ASan.
	for (auto test : {std::tuple<uint16,uint8,bool>{0xC100,0xEA,true},{0xC100,0x60,false},{0xC100,0xA0,false},{0xCFFE,0xA0,true},{0xD100,0xEA,false},{0xE100,0xA0,false},{0xFFFD,0xA0,false},{0xFFFE,0xA0,true},{0xFFFF,0xA0,true}}) {
		const auto [pc,opcode,expected]=test; std::vector<uint8> lower(0x1000,0xEA), upper(0x2800,0x60);
		const uint32 vec=ATKernelSymbols::EDITRV+6-0xD800; upper[vec]=uint8(pc-1); upper[vec+1]=uint8((pc-1)>>8);
		if (pc<0xD000) { const uint32 i=pc-0xC000; lower[i]=opcode; if (i+2<lower.size()) lower[i+2]=0x60; }
		else if (pc>=0xD800) { const uint32 i=pc-0xD800; upper[i]=opcode; }
		f->h.mHooks.CallInitHooks(lower.data(),upper.data()); f->h.mCPU.SetA('X');
		AT_PORTABLE_TEST_ASSERT(context, f->h.mHooks.OnHookHit(pc) == (expected ? 0x60 : 0));
	}
	// A stub at C000 must not invalidate a different lower-ROM routine.
	f->h.mLowerKernel[0]=0xA0; f->h.mLowerKernel[2]=0x60; f->ROMWord(ATKernelSymbols::EDITRV+6,0xC100); f->ROMInit();
	AT_PORTABLE_TEST_ASSERT(context, f->h.mHooks.OnHookHit(0xC100) == 0x60);
	f->h.mHooks.CallInitHooks(nullptr,f->h.mUpperKernel.data()); AT_PORTABLE_TEST_ASSERT(context, f->h.mHooks.OnHookHit(0xC100) == 0);
	return true;
}

bool ATTestAltirraCIOPrinter(ATPortableTestContext& context) {
	auto f=std::make_unique<Machine>(); auto& d=f->Add('P',1u<<7);
	for (uint16 i=1; i<=4; ++i) f->ROMWord(ATKernelSymbols::PRINTV+2*i,uint16(0xC300+2*i)); f->ROMInit();
	f->Mem(ATKernelSymbols::ICHID+0x10,0); f->Mem(ATKernelSymbols::ICDNO+0x10,7); f->Mem(ATKernelSymbols::ICDNOZ,1);
	f->h.mCPU.SetX(0x10); f->h.mCPU.SetA('K');
	AT_PORTABLE_TEST_ASSERT(context, f->h.mHooks.OnHookHit(0xC306) == 0x60 && d.output == std::vector<uint8>{'K'} && d.deviceNo == 7 && d.channel == 1 && !d.opens);
	f->Mem(ATKernelSymbols::ICDNOZ,7); AT_PORTABLE_TEST_ASSERT(context, f->h.mHooks.OnHookHit(0xC308) == 0x60 && d.statuses == 1);
	AT_PORTABLE_TEST_ASSERT(context, f->h.mHooks.OnHookHit(0xC302) == 0 && d.closes == 1); // native CLOSE still executes
	AT_PORTABLE_TEST_ASSERT(context, f->Open("P7:",1,7) == 0 && !d.opens);
	f->hook->SetCIOPatchEnabled('P',false); AT_PORTABLE_TEST_ASSERT(context, f->h.mHooks.OnHookHit(0xC306) == 0);
	f->hook->SetCIOPatchEnabled('P',true); f->h.mMMU.SetBankRegister(0xFE); AT_PORTABLE_TEST_ASSERT(context, f->h.mHooks.OnHookHit(0xC306) == 0);
	return true;
}

bool ATTestAltirraCIOEditor(ATPortableTestContext& context) {
	auto f=std::make_unique<Machine>(); auto& h=f->h; f->screen.reset(ATCreateVirtualScreenHandler(*h.mCPU.GetMemory()));
	f->ROMWord(ATKernelSymbols::EDITRV,0xC100); f->ROMWord(ATKernelSymbols::EDITRV+4,0xC104); f->ROMWord(ATKernelSymbols::EDITRV+6,0xC106);
	f->ROMWord(ATKernelSymbols::SCRENV,0xC200); f->ROMWord(ATKernelSymbols::SCRENV+4,0xC204); f->ROMWord(ATKernelSymbols::SCRENV+6,0xC206); f->ROMInit();
	f->Mem(ATKernelSymbols::LMARGN,2); f->Mem(ATKernelSymbols::RMARGN,39); f->Word(ATKernelSymbols::COLCRS,2);
	f->Mem(ATKernelSymbols::CH,0xFF); f->Mem(ATKernelSymbols::SHFLOK,0x40); f->Mem(ATKernelSymbols::DMACTL,2);
	f->Word(ATKernelSymbols::SDLSTL,0x3000); f->Word(ATKernelSymbols::SAVMSC,0x4000); f->Mem(0x3003,0x42); f->Mem(0x3004,0); f->Mem(0x3005,0x40);
	AT_PORTABLE_TEST_ASSERT(context, h.mHooks.OnHookHit(0xC100) == 0 && h.mHooks.OnHookHit(0xC104) == 0x60);
	f->screen->CheckForDisplayListReplaced(); h.mCPU.SetA('X'); AT_PORTABLE_TEST_ASSERT(context, h.mHooks.OnHookHit(0xC106) == 0x60);
	h.mCPU.SetA(0x1C); AT_PORTABLE_TEST_ASSERT(context, h.mHooks.OnHookHit(0xC206) == 0x60);
	f->Word(ATKernelSymbols::COLCRS,2); AT_PORTABLE_TEST_ASSERT(context, h.mHooks.OnHookHit(0xC204) == 0x60 && h.mCPU.GetA() == 'X' && f->Word(ATKernelSymbols::COLCRS) == 3);
	f->screen->PushLine("Z"); const std::array<uint8,14> code {0xA9,'!',0x20,6,0xC1,0x20,4,0xC1,0x8D,0,0x41,0x4C,11,0x20};
	for (uint16 i=0; i<code.size(); ++i) f->Mem(0x2000+i,code[i]);
	h.mCPU.SetPC(0x2000); h.mCPU.SetS(0xFD);
	AT_PORTABLE_TEST_ASSERT(context, h.RunCycles(500) && f->Mem(0x4100) == 'Z' && h.mCPU.GetS() == 0xFD);
	f->screen.reset(); AT_PORTABLE_TEST_ASSERT(context, h.mHooks.OnHookHit(0xC106) == 0);
	return true;
}

bool ATTestAltirraCIOCassette(ATPortableTestContext& context) {
	auto f=std::make_unique<SIOPortableFixture>(); auto& tape=*f->mMachine.mTape;
	vdrefptr<IATCassetteImage> image; ATCreateNewCassetteImage(~image); ATCassetteWriteCursor cursor {};
	image->WriteBlankData(cursor,uint32(20*kATCassetteDataSampleRate),false); tape.Load(image,nullptr,false); tape.Stop();
	bool available=true; ATHLECIOConnections c;
	c.mpGetCassette=[&]() -> ATCassetteEmulator * { return available ? &tape : nullptr; };
	c.mpIsCassetteSIOPatchEnabled=[] { return true; };
	std::unique_ptr<IATHLECIOHook,void(*)(IATHLECIOHook *)> hook(ATCreateHLECIOHook(&f->mCPU,std::move(c),&f->mMachine.mMemory),ATDestroyHLECIOHook);
	f->Mem(ATKernelSymbols::PACTL,0x3C); f->Mem(ATKernelSymbols::FEOF,0xFF); f->Mem(ATKernelSymbols::WMODE,0xFF); f->Mem(ATKernelSymbols::CH,0x31);
	AT_PORTABLE_TEST_ASSERT(context, f->mHooks.OnHookHit(ATKernelSymbols::CSOPIV) == 0x60 && tape.IsPlayEnabled() && tape.IsMotorRunning() && std::abs(tape.GetPosition()-9.0f)<0.02f);
	AT_PORTABLE_TEST_ASSERT(context, f->Mem(ATKernelSymbols::BPTR) == 0x80 && f->Mem(ATKernelSymbols::BLIM) == 0x80 && f->Mem(ATKernelSymbols::FEOF) == 0 && f->Mem(ATKernelSymbols::WMODE) == 0 && f->Mem(ATKernelSymbols::CH) == 0xFF);
	std::array<uint8,0x2800> upper; upper.fill(0xEA); const uint32 v=ATKernelSymbols::CASETV-0xD800;
	upper[v]=0xFF; upper[v+1]=0xE0; f->mHooks.CallInitHooks(nullptr,upper.data());
	tape.Stop(); tape.SeekToTime(0); f->Mem(ATKernelSymbols::ICAX1Z,8);
	AT_PORTABLE_TEST_ASSERT(context, f->mHooks.OnHookHit(0xE100) == 0 && !tape.IsPlayEnabled());
	f->Mem(ATKernelSymbols::ICAX1Z,4); f->Mem(ATKernelSymbols::ICAX2Z,0x80);
	AT_PORTABLE_TEST_ASSERT(context, f->mHooks.OnHookHit(0xE100) == 0x60 && f->Mem(ATKernelSymbols::FTYPE) == 0x80 && std::abs(tape.GetPosition()-9.0f)<0.02f);
	available=false; tape.Stop(); AT_PORTABLE_TEST_ASSERT(context, f->mHooks.OnHookHit(ATKernelSymbols::CSOPIV) == 0 && !tape.IsPlayEnabled());
	return true;
}
