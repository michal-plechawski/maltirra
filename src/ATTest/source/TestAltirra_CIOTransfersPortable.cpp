// Transfer ABI, bounded bursts, accelerated records, and asynchronous progress.
#include "cioportablefixture.h"
using namespace CIOPortable;

namespace {
	void Transfer(Machine& f, uint16 addr, uint16 len, uint8 command) {
		f.Word(ATKernelSymbols::ICBALZ,addr); f.Word(ATKernelSymbols::ICBLLZ,len); f.Mem(ATKernelSymbols::ICCOMZ,command);
		f.h.mCPU.SetS(0xFD); f.Mem(0x1FF,0xC0);
	}
	uint8 Accelerate(Machine& f, uint16 addr, uint16 len, uint8 command=kATCIOCmd_PutChars, uint8 ch=3, uint8 permissions=12) {
		const uint16 i=ch<<4; f.Mem(ATKernelSymbols::ICHID+i,uint8(f.Find('H'))); f.Mem(ATKernelSymbols::ICDNO+i,1);
		f.Mem(ATKernelSymbols::ICCMD+i,command); f.Mem(ATKernelSymbols::ICAX1+i,permissions);
		f.Word(ATKernelSymbols::ICBAL+i,addr); f.Word(ATKernelSymbols::ICBLL+i,len); f.h.mCPU.SetX(uint8(i));
		return f.h.mHooks.OnHookHit(ATKernelSymbols::CIOV);
	}
}

bool ATTestAltirraCIOBytes(ATPortableTestContext& context) {
	auto f=std::make_unique<Machine>(); auto& d=f->Add(); f->Open(); f->hook->SetBurstTransfersEnabled(false);
	AT_PORTABLE_TEST_ASSERT(context, !f->hook->GetBurstTransfersEnabled()); d.input={0x71,0x82};
	AT_PORTABLE_TEST_ASSERT(context, f->Generic(2) == 0x60 && f->h.mCPU.GetA() == 0x71 && f->h.mCPU.GetY() == 1);
	AT_PORTABLE_TEST_ASSERT(context, f->Generic(2) == 0x60 && f->h.mCPU.GetA() == 0x82);
	AT_PORTABLE_TEST_ASSERT(context, f->Generic(2) == 0x60 && f->h.mCPU.GetY() == kATCIOStat_EndOfFile && f->h.mCPU.GetA() == 0);
	f->h.mCPU.SetA(0x59); AT_PORTABLE_TEST_ASSERT(context, f->Generic(3) == 0x60 && d.output == std::vector<uint8>{0x59});
	// A device can consume the byte and still be busy flushing it. Retry with
	// zero bytes, preserving the byte only once in its output stream.
	uint32 calls=0; d.onPut=[&](const void *,uint32 n,uint32& actual) -> sint32 { actual=n; return ++calls==1 ? -1 : 1; };
	f->h.mCPU.SetA(0x6B); f->Generic(3); f->Poll();
	AT_PORTABLE_TEST_ASSERT(context, calls == 2 && d.requests[d.requests.size()-2] == 1 && d.requests.back() == 0 && d.output == std::vector<uint8>({0x59,0x6B}));
	return true;
}

bool ATTestAltirraCIOBurst(ATPortableTestContext& context) {
	auto f=std::make_unique<Machine>(); auto& d=f->Add(); f->Open(); d.input.resize(1500);
	for (uint32 i=0; i<d.input.size(); ++i) d.input[i]=uint8(i*13+7);
	Transfer(*f,0x6000,1500,kATCIOCmd_GetChars);
	AT_PORTABLE_TEST_ASSERT(context, f->Generic(2) == 0x60 && d.requests.back() == 1024 && f->h.mCPU.GetA() == d.input[1023]);
	AT_PORTABLE_TEST_ASSERT(context, f->Word(ATKernelSymbols::ICBALZ) == 0x63FF && f->Word(ATKernelSymbols::ICBLLZ) == 477);
	for (uint16 i=0; i<1023; ++i) AT_PORTABLE_TEST_ASSERT(context, f->Mem(0x6000+i) == d.input[i]);
	AT_PORTABLE_TEST_ASSERT(context, f->Mem(0x63FF) == 0);
	uint32 calls=0,offset=0; std::array<uint8,64> expected; for (uint32 i=0; i<64; ++i) expected[i]=uint8(i+0x31);
	d.onGet=[&](void *p,uint32 n,uint32& actual) -> sint32 { ++calls; actual=std::min<uint32>({n,17,64-offset}); memcpy(p,expected.data()+offset,actual); offset+=actual; return offset==64 ? 1 : -1; };
	Transfer(*f,0x7000,64,kATCIOCmd_GetChars); f->Generic(2); for (uint32 i=0; i<4 && offset<64; ++i) f->Poll();
	AT_PORTABLE_TEST_ASSERT(context, calls == 4 && f->Word(ATKernelSymbols::ICBALZ) == 0x703F && f->Word(ATKernelSymbols::ICBLLZ) == 1 && f->h.mCPU.GetA() == expected[63]);
	AT_PORTABLE_TEST_ASSERT(context, d.requests[d.requests.size()-4] == 64 && d.requests[d.requests.size()-3] == 47 && d.requests[d.requests.size()-2] == 30 && d.requests.back() == 13);
	for (uint16 i=0; i<63; ++i) AT_PORTABLE_TEST_ASSERT(context, f->Mem(0x7000+i) == expected[i]);
	for (uint16 i=0; i<1024; ++i) f->Mem(0x6000+i,uint8(i));
	Transfer(*f,0x6000,1200,kATCIOCmd_PutChars); f->h.mCPU.SetA('K'); f->Generic(3);
	AT_PORTABLE_TEST_ASSERT(context, d.output.size() == 1024 && d.output[0] == 'K' && d.output[1] == 1 && d.output.back() == 0xFF && f->Word(ATKernelSymbols::ICBLLZ) == 177);
	d.onGet=[](void *,uint32 n,uint32& actual) -> sint32 { actual=n+1; return -1; };
	Transfer(*f,0x6000,1024,kATCIOCmd_GetChars); f->Generic(2);
	AT_PORTABLE_TEST_ASSERT(context, f->h.mCPU.GetY() == kATCIOStat_DeviceError);
	return true;
}

bool ATTestAltirraCIOWriteAccel(ATPortableTestContext& context) {
	auto f=std::make_unique<Machine>(); auto& d=f->Add(); f->Establish(); f->Open("H:OUT",3); f->hook->SetCIOPatchEnabled('H',true);
	for (uint16 i=0; i<3; ++i) f->Mem(0x5000+i,uint8('A'+i));
	AT_PORTABLE_TEST_ASSERT(context, Accelerate(*f,0x5000,3) == 0x60 && d.output == std::vector<uint8>({'A','B','C'}));
	AT_PORTABLE_TEST_ASSERT(context, d.channel == 3 && f->h.mCPU.GetA() == 'C' && f->Mem(ATKernelSymbols::CIOCHR) == 'C' && f->Mem(ATKernelSymbols::ICSTA+0x30) == 1 && f->Mem(ATKernelSymbols::ICSTAZ) == 1);
	AT_PORTABLE_TEST_ASSERT(context, f->Mem(ATKernelSymbols::ICHIDZ) == f->Find('H') && f->Mem(ATKernelSymbols::ICDNOZ) == 1 && f->Mem(ATKernelSymbols::ICCOMZ) == kATCIOCmd_PutChars && f->Word(ATKernelSymbols::ICBALZ) == 0x5000);
	for (uint16 i=0; i<12; ++i) AT_PORTABLE_TEST_ASSERT(context, f->Mem(ATKernelSymbols::ICHIDZ+i) == f->Mem(ATKernelSymbols::ICHID+0x30+i));
	d.output.clear(); f->Mem(0x5001,0x9B); Accelerate(*f,0x5000,3,kATCIOCmd_PutRecord);
	AT_PORTABLE_TEST_ASSERT(context, d.output == std::vector<uint8>({'A',0x9B}));
	d.output.clear(); f->Mem(0x5001,'B'); Accelerate(*f,0x5000,3,kATCIOCmd_PutRecord);
	AT_PORTABLE_TEST_ASSERT(context, d.output == std::vector<uint8>({'A','B','C',0x9B}));
	d.output.clear(); f->h.mCPU.SetA('Q'); Accelerate(*f,0x5000,0);
	AT_PORTABLE_TEST_ASSERT(context, d.output == std::vector<uint8>{'Q'});
	d.output.clear(); uint32 calls=0;
	d.onPut=[&](const void *,uint32 n,uint32& actual) -> sint32 { actual=std::min<uint32>(2,n); return ++calls<3 ? IATDeviceCIO::kCIOStatus_PendingInterruptable : 1; };
	for (uint16 i=0; i<6; ++i) f->Mem(0x5000+i,uint8(i+10));
	Accelerate(*f,0x5000,6); f->Poll(); f->Poll();
	AT_PORTABLE_TEST_ASSERT(context, d.output == std::vector<uint8>({10,11,12,13,14,15}) && d.requests[d.requests.size()-3] == 6 && d.requests.back() == 2 && f->Mem(ATKernelSymbols::ICSTA+0x30) == 1);
	d.onPut=[](const void *,uint32,uint32& actual) -> sint32 { actual=0; return IATDeviceCIO::kCIOStatus_PendingInterruptable; };
	f->Mem(ATKernelSymbols::BRKKEY,0); Accelerate(*f,0x5000,2);
	AT_PORTABLE_TEST_ASSERT(context, f->h.mCPU.GetY() == kATCIOStat_Break && d.aborts == 1);
	return true;
}

bool ATTestAltirraCIOWriteGuards(ATPortableTestContext& context) {
	auto f=std::make_unique<Machine>(); auto& d=f->Add(); f->Establish(); f->Open("H:OUT",3); f->hook->SetCIOPatchEnabled('H',true);
	for (auto range : {std::pair<uint16,uint16>{0,10},{0x70,32},{0x80,129},{0x100,1},{0x3FF,2},{0xFFFE,4},{0x400,65535}}) {
		const auto before=d.puts; AT_PORTABLE_TEST_ASSERT(context, Accelerate(*f,range.first,range.second) == 0 && d.puts == before);
	}
	f->hook->SetCIOPatchEnabled('H',false); AT_PORTABLE_TEST_ASSERT(context, !f->hook->GetCIOPatchEnabled('H') && Accelerate(*f,0x5000,10) == 0);
	f->hook->SetCIOPatchEnabled('H',true); AT_PORTABLE_TEST_ASSERT(context, Accelerate(*f,0x5000,10,kATCIOCmd_PutChars,3,4) == 0 && !d.puts);
	AT_PORTABLE_TEST_ASSERT(context, Accelerate(*f,0x80,128) == 0x60 && d.output.size() == 128);
	d.output.clear();
	AT_PORTABLE_TEST_ASSERT(context, Accelerate(*f,0x400,0xFC00) == 0x60 && d.output.size() == 0xFC00 && d.output.back() == 0xEA);
	f->h.mCPU.SetX(0x81); AT_PORTABLE_TEST_ASSERT(context, f->h.mHooks.OnHookHit(ATKernelSymbols::CIOV) == 0);
	return true;
}
