#include "amdcportablefixture.h"
using namespace AMDCPortable;

bool ATTestAltirraAMDCSelection(ATPortableTestContext& context) {
	auto f=std::make_unique<Machine>();
	for(uint8 drive=0;drive<4;++drive) { f->Select(drive); for(uint8 i=0;i<4;++i) AT_PORTABLE_TEST_ASSERT(context, f->Slot(i).mbMotor==(i==drive)); }
	f->device->WriteByte(0x6000,0); for(uint8 i=0;i<4;++i) AT_PORTABLE_TEST_ASSERT(context, !f->Slot(i).mbMotor);
	AT_PORTABLE_TEST_ASSERT(context, f->FDC().DebugReadByte(0)&0x80); return true;
}

bool ATTestAltirraAMDCReadSector(ATPortableTestContext& context) {
	for(bool mfm : {false,true}) {
		Config c; c.size=mfm ? 256 : 128; auto f=std::make_unique<Machine>(c); Bytes payload(c.size); for(uint32 i=0;i<c.size;++i) payload[i]=uint8(i*17+9);
		f->Fill(183,payload); f->Program(Transfer(false,0,mfm).bytes); f->Run(100000); Bytes actual(c.size); f->device->ReadMemory(0x80,actual.data(),c.size); for(auto& b:payload) b=~b;
		AT_PORTABLE_TEST_ASSERT(context, actual==payload && !(f->FDC().DebugReadByte(0)&0x1D) && !f->FDC().GetDrqStatus());
	} return true;
}

bool ATTestAltirraAMDCWriteSector(ATPortableTestContext& context) {
	for(bool mfm : {false,true}) for(uint8 drive=0;drive<4;++drive) {
		Config c; c.size=mfm ? 256 : 128; auto f=std::make_unique<Machine>(c); const auto original=f->Stored(183,(drive+1)%4);
		f->Program(Transfer(true,drive,mfm).bytes); f->Run(100000);
		AT_PORTABLE_TEST_ASSERT(context, f->Stored(183,drive)==Bytes(c.size,0xA5) && f->Stored(183,(drive+1)%4)==original && f->Image(drive).IsDirty() && !(f->FDC().DebugReadByte(0)&0x7D));
	} return true;
}

bool ATTestAltirraAMDCSide(ATPortableTestContext& context) {
	Config c; c.size=256; c.tracks=80; c.sides=2; auto f=std::make_unique<Machine>(c); const uint32 sector=90*18+3; f->Fill(sector,Bytes(256,0x37),3); const auto other=f->Stored(183,3);
	f->Program(Transfer(false,3,true,true).bytes); f->Run(100000); Bytes actual(256); f->device->ReadMemory(0x80,actual.data(),256);
	AT_PORTABLE_TEST_ASSERT(context, actual==Bytes(256,0xC8) && !(f->FDC().DebugReadByte(0)&0x1D));
	f->Program(Transfer(true,3,true,true).bytes); f->Run(100000); AT_PORTABLE_TEST_ASSERT(context, f->Stored(sector,3)==Bytes(256,0xA5) && f->Stored(183,3)==other); return true;
}

bool ATTestAltirraAMDCSeek(ATPortableTestContext& context) {
	auto f=std::make_unique<Machine>();
	for(uint8 drive : {0,3}) { f->Select(drive); f->FDC().WriteByte(0,0); AT_PORTABLE_TEST_ASSERT(context, f->WaitIRQ() && (f->FDC().DebugReadByte(0)&4)); f->FDC().WriteByte(3,drive ? 100 : 60); f->FDC().WriteByte(0,0x10);
		AT_PORTABLE_TEST_ASSERT(context, f->WaitIRQ(2000000)); Text status; f->FDC().DumpStatus(status); AT_PORTABLE_TEST_ASSERT(context, status.value.find(drive ? "90.0" : "45.0")!=std::string::npos);
	} return true;
}

bool ATTestAltirraAMDCPositions(ATPortableTestContext& context) {
	auto f=std::make_unique<Machine>(); f->Select(0); f->FDC().WriteByte(1,10); f->FDC().WriteByte(3,12); f->FDC().WriteByte(0,0x10); AT_PORTABLE_TEST_ASSERT(context, f->WaitIRQ());
	for(uint8 drive : {0,1,0}) { f->Select(drive); Text status; f->FDC().DumpStatus(status); AT_PORTABLE_TEST_ASSERT(context, status.value.find(drive ? "10.0" : "12.0")!=std::string::npos); } return true;
}

bool ATTestAltirraAMDCMediaChange(ATPortableTestContext& context) {
	auto f=std::make_unique<Machine>(); f->Select(0); f->device->OnDiskChanged(1,true); f->Run(100); AT_PORTABLE_TEST_ASSERT(context, !(f->FDC().DebugReadByte(0)&0xC0));
	f->device->OnDiskChanged(0,true); f->Run(100); AT_PORTABLE_TEST_ASSERT(context, (f->FDC().DebugReadByte(0)&0xC0)==0x80);
	f->Run(100000); AT_PORTABLE_TEST_ASSERT(context, (f->FDC().DebugReadByte(0)&0xC0)==0xC0);
	f->Run(90000); AT_PORTABLE_TEST_ASSERT(context, (f->FDC().DebugReadByte(0)&0xC0)==0x80);
	f->Run(90000); AT_PORTABLE_TEST_ASSERT(context, !(f->FDC().DebugReadByte(0)&0xC0)); return true;
}

bool ATTestAltirraAMDCWriteProtect(ATPortableTestContext& context) {
	auto f=std::make_unique<Machine>(); const auto original=f->Stored(183); f->Slot().mDisk.SetWriteMode(kATMediaWriteMode_RO); f->Program(Transfer(true).bytes); f->Run(100000);
	AT_PORTABLE_TEST_ASSERT(context, (f->FDC().DebugReadByte(0)&0x40) && f->Stored(183)==original);
	// Type II status retains the last command result; Type I polls the live sensor.
	f->FDC().WriteByte(0,0xD0); f->Run(100); f->Select(1); AT_PORTABLE_TEST_ASSERT(context, !(f->FDC().DebugReadByte(0)&0x40)); f->Select(0); AT_PORTABLE_TEST_ASSERT(context, f->FDC().DebugReadByte(0)&0x40); return true;
}
