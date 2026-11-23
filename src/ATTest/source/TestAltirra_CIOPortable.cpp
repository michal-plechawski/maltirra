// Device registration, public memory services, command dispatch and lifecycle.
#include "cioportablefixture.h"
using namespace CIOPortable;

bool ATTestAltirraCIORegistration(ATPortableTestContext& context) {
	auto f=std::make_unique<Machine>(); auto& a=f->Add('X',1u<<1); auto& b=f->Add('X',1u<<2);
	AT_PORTABLE_TEST_ASSERT(context, f->hook->HasCIODevice('X') && !f->hook->HasCIODevice('Y') && f->Find('X') < 0);
	f->Establish(); const int id=f->Find('X');
	AT_PORTABLE_TEST_ASSERT(context, id == 3 && f->Word(ATKernelSymbols::HATABS+id+1) == 0xD570);
	AT_PORTABLE_TEST_ASSERT(context, f->Open("X2:FILE",2,2) == 0x60 && b.opens == 1 && !a.opens && b.channel == 2 && b.deviceNo == 2 && b.filename == "X2:FILE");
	AT_PORTABLE_TEST_ASSERT(context, f->Open("X:OTHER",3) == 0x60 && a.opens == 1 && a.channel == 3);
	AT_PORTABLE_TEST_ASSERT(context, f->Open("X3:BAD",4,3) == 0x60 && f->h.mCPU.GetY() == kATCIOStat_UnkDevice && b.opens == 1 && a.opens == 1);
	f->hook->SetPBIHookEnabled(true); AT_PORTABLE_TEST_ASSERT(context, f->Word(ATKernelSymbols::HATABS+id+1) == 0xE48F);
	f->Page(0xD6); f->hook->SetPBIHookEnabled(false);
	AT_PORTABLE_TEST_ASSERT(context, f->Word(ATKernelSymbols::HATABS+id+1) == 0xD670 && f->Mem(0xD672) == 0x72);
	// D3 overlays only the CIO window. PIA reads and writes outside it, and
	// writes through it, must continue to reach the production chip.
	f->Page(0xD3); AT_PORTABLE_TEST_ASSERT(context, f->Mem(0xD372) == 0x72 && f->Mem(0xD302) == f->h.mPIA.DebugReadByte(2));
	f->Mem(0xD342,0x3C); AT_PORTABLE_TEST_ASSERT(context, (f->h.mPIA.DebugReadByte(2) & 0x3F) == 0x3C && f->Mem(0xD342) == 0x42);
	f->Page(0xD5); AT_PORTABLE_TEST_ASSERT(context, f->Mem(0xD342) == f->h.mPIA.DebugReadByte(2));
	AT_PORTABLE_TEST_ASSERT(context, ATGetHLECIOHook(&f->Manager()) == f->hook.get() && f->hook->AsInterface(IATDeviceCIOManager::kTypeID) == &f->Manager() && !f->hook->AsInterface(0));
	return true;
}

bool ATTestAltirraCIOFullTable(ATPortableTestContext& context) {
	auto f=std::make_unique<Machine>(); auto& d=f->Add('Z');
	std::array<uint8,36> full {};
	for (uint16 i=0; i<12; ++i) { full[3*i]=uint8('a'+i); full[3*i+1]=uint8(i); full[3*i+2]=0x60; }
	for (uint16 i=0; i<36; ++i) f->Mem(ATKernelSymbols::HATABS+i,full[i]);
	f->Establish(); for (uint16 i=0; i<36; ++i) AT_PORTABLE_TEST_ASSERT(context, f->Mem(ATKernelSymbols::HATABS+i) == full[i]);
	AT_PORTABLE_TEST_ASSERT(context, f->Find('Z') < 0);
	f->Mem(ATKernelSymbols::HATABS+6,0); f->Manager().NotifyCIODevicesChanged(&d);
	AT_PORTABLE_TEST_ASSERT(context, f->Find('Z') == 6);
	// Keep foreign handlers, including one after a hole, with no duplicates.
	auto g=std::make_unique<Machine>(); g->Mem(ATKernelSymbols::HATABS+6,'H'); g->Word(ATKernelSymbols::HATABS+7,0x1234);
	auto& h=g->Add(); g->Establish(); uint32 count=0;
	for (uint16 i=0; i<36; i+=3) count += g->Mem(ATKernelSymbols::HATABS+i)=='H';
	AT_PORTABLE_TEST_ASSERT(context, count == 1 && g->Word(ATKernelSymbols::HATABS+g->Find('H')+1) == 0x1234);
	g->Manager().RemoveCIODevice(&h); AT_PORTABLE_TEST_ASSERT(context, g->Find('H') >= 0 && g->Word(ATKernelSymbols::HATABS+g->Find('H')+1) == 0x1234);
	return true;
}

bool ATTestAltirraCIOMemory(ATPortableTestContext& context) {
	auto f=std::make_unique<Machine>(); auto& m=f->Manager(); std::array<uint8,8> buf; buf.fill(0xCC);
	AT_PORTABLE_TEST_ASSERT(context, m.ReadFilename(nullptr,0,0) == 0);
	for (uint8 end : {uint8(0),uint8(0x14),uint8(0x16),uint8(0x9B),uint8(0xFF)}) {
		f->Mem(0x5000,'H'); f->Mem(0x5001,':'); f->Mem(0x5002,end);
		AT_PORTABLE_TEST_ASSERT(context, m.ReadFilename(buf.data(),buf.size(),0x5000) == 2 && buf[0] == 'H' && buf[1] == ':' && buf[2] == 0);
	}
	AT_PORTABLE_TEST_ASSERT(context, m.ReadFilename(buf.data(),1,0x5000) == 0 && buf[0] == 0);
	// Disable ROM for direct memory-service wrap tests.
	f->h.mMMU.SetBankRegister(0xFE); const std::array<uint8,4> bytes {'A','B','C',0x9B};
	m.WriteMemory(0xFFFE,bytes.data(),bytes.size());
	AT_PORTABLE_TEST_ASSERT(context, m.ReadFilename(buf.data(),buf.size(),0xFFFE) == 3 && std::string(reinterpret_cast<char *>(buf.data())) == "ABC");
	m.ReadMemory(buf.data(),0xFFFE,4); AT_PORTABLE_TEST_ASSERT(context, std::equal(bytes.begin(),bytes.end(),buf.begin()));
	m.WriteMemory(0,nullptr,0); m.ReadMemory(nullptr,0,0); m.WriteByte(0x1234,0x59);
	AT_PORTABLE_TEST_ASSERT(context, m.ReadByte(0x1234) == 0x59 && !m.IsBreakActive());
	f->Mem(ATKernelSymbols::BRKKEY,0); AT_PORTABLE_TEST_ASSERT(context, m.IsBreakActive());
	return true;
}

bool ATTestAltirraCIOCommands(ATPortableTestContext& context) {
	auto f=std::make_unique<Machine>(); auto& d=f->Add();
	AT_PORTABLE_TEST_ASSERT(context, f->Open("H:FILE",7,1,12,0x57) == 0x60 && f->h.mCPU.GetY() == 1 && d.aux[0] == 12 && d.aux[1] == 0x57);
	AT_PORTABLE_TEST_ASSERT(context, f->Open("H:AGAIN",7) == 0x60 && f->h.mCPU.GetY() == kATCIOStat_IOCBInUse && d.opens == 1);
	AT_PORTABLE_TEST_ASSERT(context, f->Generic(4,7) == 0x60 && f->h.mCPU.GetY() == 1);
	for (uint16 i=0; i<4; ++i) AT_PORTABLE_TEST_ASSERT(context, f->Mem(ATKernelSymbols::DVSTAT+i) == 0x41+i);
	f->Mem(ATKernelSymbols::ICCOMZ,0x28); f->Word(ATKernelSymbols::ICBALZ,0x4567); f->Word(ATKernelSymbols::ICBLLZ,0x1234);
	f->Mem(ATKernelSymbols::ICAX1Z,1); f->Mem(ATKernelSymbols::ICAX2Z,2);
	for (uint16 i=2; i<6; ++i) f->Mem(ATKernelSymbols::ICAX1+0x70+i,uint8(i+1));
	AT_PORTABLE_TEST_ASSERT(context, f->Generic(5,7) == 0x60 && d.command == 0x28 && d.address == 0x4567 && d.length == 0x1234 && d.aux == (std::array<uint8,6>{1,2,3,4,5,6}));
	AT_PORTABLE_TEST_ASSERT(context, f->Mem(ATKernelSymbols::ICAX1Z) == 0x11 && f->Mem(ATKernelSymbols::ICAX2Z) == 0x12 && f->Mem(ATKernelSymbols::ICAX1+0x75) == 0x16);
	AT_PORTABLE_TEST_ASSERT(context, f->Generic(1,7) == 0x60 && d.closes == 1);
	d.openStatus=kATCIOStat_FileNotFound; f->Open();
	AT_PORTABLE_TEST_ASSERT(context, f->h.mCPU.GetY() == kATCIOStat_FileNotFound);
	f->Generic(2); AT_PORTABLE_TEST_ASSERT(context, !d.gets && f->h.mCPU.GetY() == kATCIOStat_UnkDevice);
	f->h.mCPU.SetX(0x11); AT_PORTABLE_TEST_ASSERT(context, f->h.mHooks.OnHookHit(0xD571) == 0x60 && f->h.mCPU.GetY() == kATCIOStat_UnkDevice);
	return true;
}

bool ATTestAltirraCIOAsync(ATPortableTestContext& context) {
	uint32 closing=0;
	auto f=std::make_unique<Machine>(); auto& d=f->Add(); f->Open();
	d.onGet=[](void *,uint32,uint32& n) { n=0; return IATDeviceCIO::kCIOStatus_PendingInterruptable; };
	f->h.mCPU.SetS(0xFD); AT_PORTABLE_TEST_ASSERT(context, f->Generic(2) == 0x60 && f->h.mCPU.GetS() == 0xF9);
	f->Mem(ATKernelSymbols::BRKKEY,0); AT_PORTABLE_TEST_ASSERT(context, f->Poll() == 0x60 && f->h.mCPU.GetY() == kATCIOStat_Break && d.aborts == 1);
	d.onGet=[](void *,uint32,uint32& n) { n=0; return IATDeviceCIO::kCIOStatus_PendingNonInterruptable; };
	f->h.mCPU.SetS(0xFD); f->Generic(2); f->Poll();
	AT_PORTABLE_TEST_ASSERT(context, d.aborts == 1 && f->h.mCPU.GetS() == 0xF5);
	// A different command must abort the previous asynchronous operation once.
	f->Generic(4); AT_PORTABLE_TEST_ASSERT(context, d.aborts == 2 && d.statuses == 1 && f->h.mCPU.GetY() == 1);
	AT_PORTABLE_TEST_ASSERT(context, f->Poll() == 0x60 && f->h.mCPU.GetY() == kATCIOStat_UnkDevice && d.aborts == 2);
	d.onClose=[&]() -> sint32 { return ++closing<2 ? -1 : 1; };
	f->Generic(1); f->Poll(); AT_PORTABLE_TEST_ASSERT(context, closing == 2 && d.closes == 2);
	f->Open(); AT_PORTABLE_TEST_ASSERT(context, d.opens == 2 && f->h.mCPU.GetY() == 1);
	return true;
}

bool ATTestAltirraCIOLifecycle(ATPortableTestContext& context) {
	auto f=std::make_unique<Machine>(); auto& d=f->Add(); f->Establish(); f->Open();
	d.onGet=[](void *,uint32,uint32& n) { n=0; return -1; }; f->Generic(2);
	f->h.mHooks.CallResetHooks(); AT_PORTABLE_TEST_ASSERT(context, d.aborts == 1 && d.closes == 1 && f->h.mHooks.OnHookHit(0xD57F) == 0);
	for (uint16 i=3; i<36; ++i) f->Mem(ATKernelSymbols::HATABS+i,0); // OS rebuilds its table after reset
	f->Page(0xD5); f->Establish(); f->Open(); f->Generic(2); f->Manager().RemoveCIODevice(&d);
	AT_PORTABLE_TEST_ASSERT(context, d.aborts == 2 && d.closes == 2 && !f->hook->HasCIODevice('H'));
	// Removed handlers survive while native IOCBs still refer to them, then
	// disappear; compaction must update other open native IOCB IDs.
	auto& a=f->Add('H'); auto& b=f->Add('Z'); (void)b;
	const int hID=f->Find('H'), zID=f->Find('Z'); f->Mem(ATKernelSymbols::ICHID+0x10,uint8(hID)); f->Mem(ATKernelSymbols::ICHID+0x20,uint8(zID));
	f->Manager().RemoveCIODevice(&a); AT_PORTABLE_TEST_ASSERT(context, f->Find('H') == hID);
	f->Mem(ATKernelSymbols::ICHID+0x10,0xFF); f->Manager().NotifyCIODevicesChanged(&b);
	AT_PORTABLE_TEST_ASSERT(context, f->Find('H') < 0 && f->Mem(ATKernelSymbols::ICHID+0x20) == f->Find('Z'));
	f->Open("Z:FILE"); b.onGet=[](void *,uint32,uint32& n) { n=0; return -1; }; f->Generic(2);
	f->hook.reset(); AT_PORTABLE_TEST_ASSERT(context, b.aborts == 1 && b.closes == 1 && f->h.mHooks.OnHookHit(0xD571) == 0 && f->h.mHooks.OnHookHit(0xD57F) == 0);
	auto g=std::make_unique<Machine>(); auto& pending=g->Add(); pending.onOpen=[] { return -1; }; g->Open(); g->hook.reset();
	AT_PORTABLE_TEST_ASSERT(context, pending.aborts == 1 && pending.closes == 0);
	auto self=std::make_unique<Machine>(); auto& unplug=self->Add(); self->Open();
	unplug.onClose=[&] { self->Manager().RemoveCIODevice(&unplug); return 1; }; self->Manager().RemoveCIODevice(&unplug);
	AT_PORTABLE_TEST_ASSERT(context, unplug.closes == 1 && !self->hook->HasCIODevice('H'));
	auto during=std::make_unique<Machine>(); auto& removed=during->Add(); removed.onOpen=[&] { during->Manager().RemoveCIODevice(&removed); return 1; };
	during->Open(); AT_PORTABLE_TEST_ASSERT(context, removed.aborts == 1 && !during->hook->HasCIODevice('H') && during->h.mCPU.GetY() == kATCIOStat_UnkDevice);
	return true;
}
