#include "percomportablefixture.h"
#include <cstdio>
using namespace PercomPortable;

bool ATTestAltirraPercomSelection(ATPortableTestContext& context) {
	for(auto type:variants) { Config c; c.type=type; auto f=std::make_unique<Machine>(c); for(uint8 drive=0;drive<4;++drive) { f->Select(drive); for(uint8 i=0;i<4;++i) AT_PORTABLE_TEST_ASSERT(context, f->Slot(i).mbMotor==(i==drive)); }
		if(type==Hardware::RFD) f->device->WriteByte(0xD014,0); else f->PortB(0); for(uint8 i=0;i<4;++i) AT_PORTABLE_TEST_ASSERT(context, !f->Slot(i).mbMotor);
		if(type==Hardware::AT88) { f->PortB(0x13); AT_PORTABLE_TEST_ASSERT(context, f->Status()&0x80); }
	} return true;
}

bool ATTestAltirraPercomReadSector(ATPortableTestContext& context) {
	for(auto type:variants) for(bool mfm:{false,true}) { Config c; c.type=type; c.size=mfm ? 256 : 128; auto f=std::make_unique<Machine>(c); Bytes payload(c.size); for(uint32 i=0;i<c.size;++i) payload[i]=uint8(i*17+9); f->Fill(183,payload); f->Program(f->Transfer(false,0,mfm).bytes); f->Run(100000); Bytes actual(c.size); f->device->ReadMemory(f->RAM()+0x80,actual.data(),c.size);
		AT_PORTABLE_TEST_ASSERT(context, actual==payload && !(f->Status()&0x1D));
	} return true;
}

bool ATTestAltirraPercomWriteSector(ATPortableTestContext& context) {
	for(auto type:variants) for(bool mfm:{false,true}) for(uint8 drive:{0,1,2,3}) { Config c; c.type=type; c.size=mfm ? 256 : 128; auto f=std::make_unique<Machine>(c); const uint32 neighbor=(drive+1)%4; const auto other=f->Stored(183,neighbor); f->Program(f->Transfer(true,drive,mfm).bytes); f->Run(100000);
		if(f->Stored(183,drive)!=Bytes(c.size,0x5A) || (f->Status()&0x7D)) printf("Percom write: variant=%u MFM=%u drive=%u PC=%04X status=%02X first=%02X last=%02X\n",unsigned(type),unsigned(mfm),unsigned(drive),unsigned(f->State().m6809.mPC),unsigned(f->Status()),unsigned(f->Stored(183,drive).front()),unsigned(f->Stored(183,drive).back()));
		AT_PORTABLE_TEST_ASSERT(context, f->Stored(183,drive)==Bytes(c.size,0x5A) && f->Stored(183,neighbor)==other && f->Image(drive).IsDirty() && !(f->Status()&0x7D));
	} return true;
}

bool ATTestAltirraPercomSide(ATPortableTestContext& context) {
	for(auto type:variants) for(bool mode:{false,true}) { if(type==Hardware::RFD && mode) continue; Config c; c.type=type; c.use1795=mode; c.size=256; c.tracks=80; c.sides=2; auto f=std::make_unique<Machine>(c); const uint32 sector=149*18+13; f->Fill(sector,Bytes(256,0x37),3); auto other=f->Stored(183,3); f->Program(f->Transfer(false,3,true,true).bytes); f->Run(100000); Bytes actual(256); f->device->ReadMemory(f->RAM()+0x80,actual.data(),256);
		AT_PORTABLE_TEST_ASSERT(context, actual==Bytes(256,0x37) && !(f->Status()&0x1D)); f->Program(f->Transfer(true,3,true,true).bytes); f->Run(100000); AT_PORTABLE_TEST_ASSERT(context, f->Stored(sector,3)==Bytes(256,0x5A) && f->Stored(183,3)==other);
	} return true;
}

bool ATTestAltirraPercomSeek(ATPortableTestContext& context) {
	for(auto type:variants) { Config c; c.type=type; auto f=std::make_unique<Machine>(c); for(uint8 drive:{0,3}) { f->Select(drive,true); f->WriteFDC(0,0); AT_PORTABLE_TEST_ASSERT(context, f->WaitIRQ() && (f->Status()&4)); f->WriteFDC(3,drive ? 100 : 60); f->WriteFDC(0,0x10); AT_PORTABLE_TEST_ASSERT(context, f->WaitIRQ()); Text text; f->Chip().DumpStatus(text); AT_PORTABLE_TEST_ASSERT(context, text.value.find(drive ? "90.0" : "45.0")!=std::string::npos); } }
	return true;
}

bool ATTestAltirraPercomPositions(ATPortableTestContext& context) {
	for(auto type:variants) { Config c; c.type=type; auto f=std::make_unique<Machine>(c); f->Select(0,true); f->WriteFDC(1,10); f->WriteFDC(3,12); f->WriteFDC(0,0x10); AT_PORTABLE_TEST_ASSERT(context, f->WaitIRQ()); for(uint8 drive:{0,1,0}) { f->Select(drive,true); Text text; f->Chip().DumpStatus(text); AT_PORTABLE_TEST_ASSERT(context, text.value.find(drive ? "10.0" : "12.0")!=std::string::npos); } }
	return true;
}

bool ATTestAltirraPercomMediaChange(ATPortableTestContext& context) {
	for(auto type:variants) { Config c; c.type=type; auto f=std::make_unique<Machine>(c); f->Select(0,true); f->device->OnDiskChanged(1,true); f->Run(100); AT_PORTABLE_TEST_ASSERT(context, !(f->Status()&0xC0)); f->device->OnDiskChanged(0,true); f->Run(100); AT_PORTABLE_TEST_ASSERT(context, (f->Status()&0xC0)==0x80); f->Run(100000); AT_PORTABLE_TEST_ASSERT(context, (f->Status()&0xC0)==0xC0); f->Run(90000); AT_PORTABLE_TEST_ASSERT(context, (f->Status()&0xC0)==0x80); f->Run(90000); AT_PORTABLE_TEST_ASSERT(context, !(f->Status()&0xC0)); }
	return true;
}

bool ATTestAltirraPercomWriteProtect(ATPortableTestContext& context) {
	Config at; at.type=Hardware::AT88; auto pia=std::make_unique<Machine>(at); pia->Select(0,true); pia->device->WriteByte(9,4); AT_PORTABLE_TEST_ASSERT(context, pia->device->ReadByte(8)&0x40); pia->Slot().mDisk.SetWriteMode(kATMediaWriteMode_RO); AT_PORTABLE_TEST_ASSERT(context, !(pia->device->ReadByte(8)&0x40)); pia->Slot().mDisk.SetWriteMode(kATMediaWriteMode_VRW); AT_PORTABLE_TEST_ASSERT(context, pia->device->ReadByte(8)&0x40);
	for(auto type:variants) { Config c; c.type=type; auto f=std::make_unique<Machine>(c); const auto original=f->Stored(183); f->Slot().mDisk.SetWriteMode(kATMediaWriteMode_RO); f->Program(f->Transfer(true).bytes); f->Run(100000); AT_PORTABLE_TEST_ASSERT(context, (f->Status()&0x40) && f->Stored(183)==original); f->WriteFDC(0,0xD0); f->Run(100); f->Select(1); AT_PORTABLE_TEST_ASSERT(context, !(f->Status()&0x40)); f->Select(0); AT_PORTABLE_TEST_ASSERT(context, f->Status()&0x40); }
	return true;
}
