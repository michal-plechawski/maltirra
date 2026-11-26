#include "atr8000portablefixture.h"
using namespace ATR8000Portable;
bool ATTestAltirraATR8000Selection(ATPortableTestContext& context) {
	auto f=std::make_unique<Machine>(); for(uint8 d=0;d<4;++d) { f->Select(uint8(1<<d)); f->FDC().WriteByte(0,0x08); AT_PORTABLE_TEST_ASSERT(context, f->WaitIRQ()); for(uint8 i=0;i<4;++i) AT_PORTABLE_TEST_ASSERT(context, f->Slot(i).mbMotor==(i==d)); } f->Select(0); for(uint8 i=0;i<4;++i) AT_PORTABLE_TEST_ASSERT(context, !f->Slot(i).mbMotor); AT_PORTABLE_TEST_ASSERT(context, !(f->FDC().DebugReadByte(0)&0x80)); return true;
}
bool ATTestAltirraATR8000MultiSelect(ATPortableTestContext& context) {
	auto f=std::make_unique<Machine>(); f->Select(9); f->FDC().WriteByte(0,8); AT_PORTABLE_TEST_ASSERT(context, f->WaitIRQ() && f->Slot(0).mbMotor && f->Slot(3).mbMotor && !f->Slot(1).mbMotor); for(uint8 d:{0,3}) { f->Select(uint8(1<<d)); AT_PORTABLE_TEST_ASSERT(context, f->FDC().DebugReadByte(0)&4); }
	f->Select(9); f->FDC().WriteByte(0,0x80); f->Run(100); AT_PORTABLE_TEST_ASSERT(context, !(f->FDC().DebugReadByte(0)&0x80) && !f->FDC().GetDrqStatus()); return true;
}
bool ATTestAltirraATR8000ReadSector(ATPortableTestContext& context) {
	for(bool mfm:{false,true}) { Config c; c.size=mfm ? 256 : 128; auto f=std::make_unique<Machine>(c); Bytes data(c.size); for(uint32 i=0;i<c.size;++i) data[i]=uint8(i*17+9); f->Fill(21,data); f->Program(f->Transfer(false,0,mfm).bytes); f->Run(100000); Bytes actual(c.size); f->device->ReadMemory(0x8000,actual.data(),c.size); for(auto& b:data) b=~b; AT_PORTABLE_TEST_ASSERT(context, actual==data && !(f->FDC().DebugReadByte(0)&0x1D)); } return true;
}
bool ATTestAltirraATR8000WriteSector(ATPortableTestContext& context) {
	for(bool mfm:{false,true}) for(uint8 d:{0,1,2,3}) { Config c; c.size=mfm ? 256 : 128; auto f=std::make_unique<Machine>(c); const uint32 neighbor=(d+1)%4; const auto old=f->Stored(21,neighbor); f->Program(f->Transfer(true,d,mfm).bytes); f->Run(100000); AT_PORTABLE_TEST_ASSERT(context, f->Stored(21,d)==Bytes(c.size,0xA5) && f->Stored(21,neighbor)==old && f->Image(d).IsDirty() && !(f->FDC().DebugReadByte(0)&0x7D)); } return true;
}
bool ATTestAltirraATR8000Side(ATPortableTestContext& context) {
	for(uint8 d:{0,3}) { Config c; c.size=256; c.tracks=d ? 77 : 40; c.sides=2; auto f=std::make_unique<Machine>(c); const uint32 sector=(c.tracks+1)*18+3; f->Fill(sector,Bytes(256,0x37),d); auto old=f->Stored(21,d); f->Program(f->Transfer(false,d,true,true).bytes); f->Run(100000); Bytes actual(256); f->device->ReadMemory(0x8000,actual.data(),256); AT_PORTABLE_TEST_ASSERT(context, actual==Bytes(256,0xC8)); f->Program(f->Transfer(true,d,true,true).bytes); f->Run(100000); AT_PORTABLE_TEST_ASSERT(context, f->Stored(sector,d)==Bytes(256,0xA5) && f->Stored(21,d)==old); } return true;
}
bool ATTestAltirraATR8000Seek(ATPortableTestContext& context) {
	auto f=std::make_unique<Machine>(); f->Select(9); f->FDC().WriteByte(0,8); AT_PORTABLE_TEST_ASSERT(context, f->WaitIRQ()); f->FDC().WriteByte(3,100); f->FDC().WriteByte(0,0x18); AT_PORTABLE_TEST_ASSERT(context, f->WaitIRQ()); for(uint8 d:{0,3,0}) { f->Select(uint8(1<<d)); Text text; f->FDC().DumpStatus(text); AT_PORTABLE_TEST_ASSERT(context, text.value.find(d ? "82.0" : "45.0")!=std::string::npos); } return true;
}
bool ATTestAltirraATR8000WriteProtect(ATPortableTestContext& context) {
	auto f=std::make_unique<Machine>(); const auto old=f->Stored(21); f->Slot().mDisk.SetWriteMode(kATMediaWriteMode_RO); f->Program(f->Transfer(true).bytes); f->Run(100000); AT_PORTABLE_TEST_ASSERT(context, (f->FDC().DebugReadByte(0)&0x40) && f->Stored(21)==old); f->FDC().WriteByte(0,0xD0); f->Run(100); f->Select(2); AT_PORTABLE_TEST_ASSERT(context, !(f->FDC().DebugReadByte(0)&0x40)); f->Select(3); AT_PORTABLE_TEST_ASSERT(context, f->FDC().DebugReadByte(0)&0x40); return true;
}
bool ATTestAltirraATR8000MediaChange(ATPortableTestContext& context) {
	for(bool readOnly:{false,true}) { auto f=std::make_unique<Machine>(); if(readOnly) f->Slot().mDisk.SetWriteMode(kATMediaWriteMode_RO); f->Select(1); f->device->OnDiskChanged(1,true); f->Run(100); AT_PORTABLE_TEST_ASSERT(context, bool(f->FDC().DebugReadByte(0)&0x40)==readOnly); f->device->OnDiskChanged(0,true); f->Run(100); AT_PORTABLE_TEST_ASSERT(context, f->FDC().DebugReadByte(0)&0x40); f->Run(70000); AT_PORTABLE_TEST_ASSERT(context, f->FDC().DebugReadByte(0)&0x40); f->Run(30000); AT_PORTABLE_TEST_ASSERT(context, !(f->FDC().DebugReadByte(0)&0x40)); f->Run(90000); AT_PORTABLE_TEST_ASSERT(context, f->FDC().DebugReadByte(0)&0x40); f->Run(90000); AT_PORTABLE_TEST_ASSERT(context, bool(f->FDC().DebugReadByte(0)&0x40)==readOnly); } return true;
}
bool ATTestAltirraATR8000Clock(ATPortableTestContext& context) {
	for(bool fast:{false,true}) { auto f=std::make_unique<Machine>(); f->Select(fast ? 8 : 1,false,false,fast); f->FDC().WriteByte(1,1); f->FDC().WriteByte(2,4); f->FDC().WriteByte(0,0x80); std::vector<uint32> ticks;
		for(uint32 i=0;i<40000 && !f->FDC().GetIrqStatus();++i) { f->Run(1); if(f->FDC().GetDrqStatus()) { ticks.push_back(f->host->CPUGetCycle()); f->FDC().ReadByte(3); } } AT_PORTABLE_TEST_ASSERT(context, ticks.size()==128); const double period=double(ticks.back()-ticks.front())/127; AT_PORTABLE_TEST_ASSERT(context, std::abs(period-(fast ? 32 : 64)*1.7897725)<1);
		f->Out(0x54,0); f->Out(0x56,0); Text blocked; f->FDC().DumpStatus(blocked); AT_PORTABLE_TEST_ASSERT(context, blocked.value.find("auto index disabled")!=std::string::npos); f->Out(0x54,1); Text live; f->FDC().DumpStatus(live); AT_PORTABLE_TEST_ASSERT(context, live.value.find("auto index enabled")!=std::string::npos);
	} return true;
}
