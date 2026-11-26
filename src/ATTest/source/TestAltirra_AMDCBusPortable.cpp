#include "amdcportablefixture.h"
using namespace AMDCPortable;

bool ATTestAltirraAMDCACIA(ATPortableTestContext& context) {
	auto f=std::make_unique<Machine>(); f->device->WriteByte(0x2000,0x15); f->host->mManager.PokeyWriteSIO(0xE3,false,93,0,false,false); f->Run(100);
	AT_PORTABLE_TEST_ASSERT(context, (f->device->ReadByte(0x2000)&0x11)==1 && f->device->DebugReadByte(0x3FFF)==0xE3 && (f->device->ReadByte(0x2000)&1));
	f->device->ReadByte(0x2001); AT_PORTABLE_TEST_ASSERT(context, !(f->device->ReadByte(0x2000)&1)); f->host->mManager.PokeyWriteSIO(0x67,false,200,0,false,false); AT_PORTABLE_TEST_ASSERT(context, f->device->ReadByte(0x2000)&0x10); return true;
}

bool ATTestAltirraAMDCEcho(ATPortableTestContext& context) {
	auto f=std::make_unique<Machine>(); f->host->ReceiveClock(); Code c; c.Write(0x2000,0x15); c.Emit({0xB6,0x20,0,0x85,1,0x27,0xF9,0xB6,0x20,1,0xB7,0x20,1,0x20,0xF1}); f->Program(c.bytes); f->Run(1000);
	f->host->mManager.PokeyWriteSIO(0xA6,false,93,f->host->mMachine.mChips.mVideo.mScheduler.GetTick64(),false,false); const auto received=f->host->Collect(4000);
	AT_PORTABLE_TEST_ASSERT(context, received==Bytes{0xA6}); return true;
}

bool ATTestAltirraAMDCRates(ATPortableTestContext& context) {
	for(bool fast : {false,true}) { auto f=std::make_unique<Machine>(); f->host->ReceiveClock(); if(fast) { f->host->mMachine.mChips.mPOKEY.WriteByte(4,17); f->Run(114); }
		f->Select(0,false,fast); f->device->WriteByte(0x2000,fast ? 0x14 : 0x15); f->device->WriteByte(0x2001,0xA6); const auto bytes=f->host->Collect(3000); AT_PORTABLE_TEST_ASSERT(context, bytes==Bytes{0xA6});
	} return true;
}

bool ATTestAltirraAMDCCTS(ATPortableTestContext& context) {
	auto f=std::make_unique<Machine>(); auto& pokey=f->host->mMachine.mChips.mPOKEY; pokey.SetCommandLine(true); f->Run(100); AT_PORTABLE_TEST_ASSERT(context, f->device->ReadByte(0x2000)&8);
	pokey.SetCommandLine(false); f->Run(200); AT_PORTABLE_TEST_ASSERT(context, f->device->ReadByte(0x2000)&8); f->Run(700); AT_PORTABLE_TEST_ASSERT(context, !(f->device->ReadByte(0x2000)&8));
	pokey.SetCommandLine(true); f->Run(100); f->device->PeripheralColdReset(); f->Run(100); AT_PORTABLE_TEST_ASSERT(context, f->device->ReadByte(0x2000)&8); return true;
}

bool ATTestAltirraAMDCFIRQ(ATPortableTestContext& context) {
	auto f=std::make_unique<Machine>(); Bytes payload(128); for(uint32 i=0;i<128;++i) payload[i]=uint8(7+i*13); f->Fill(183,payload); Code c;
	c.Emit({0x10,0xCE,3,0xF0,0x8E,0,0x80,0xC6,0,0x1C,0xBF}); c.Write(0x6000,0x90); c.Write(0x4001,10); c.Write(0x4002,4); c.Write(0x4000,0x80); c.Emit({0xC1,0x80,0x26,0xFC}); c.Halt();
	f->MakeROM(c.bytes); f->InstallHandler(0xC80,{0xB6,0x40,3,0xA7,0x80,0x5C,0x3B},0xFF6); f->Run(100000); Bytes actual(128); f->device->ReadMemory(0x80,actual.data(),128); for(auto& b:payload) b=~b;
	AT_PORTABLE_TEST_ASSERT(context, actual==payload && f->State().m6809.mB==0x80 && f->State().m6809.mS==0x3F0 && !f->FDC().GetDrqStatus()); return true;
}

bool ATTestAltirraAMDCWatchdog(ATPortableTestContext& context) {
	auto f=std::make_unique<Machine>(); Code c; c.Emit({0x10,0xCE,3,0xF0,0x7F,0,0x10}); c.Halt(); f->MakeROM(c.bytes); f->InstallHandler(0xC90,{0x7C,0,0x10,0x3B},0xFFC);
	f->device->DebugReadByte(0xC000); f->Run(1900000); AT_PORTABLE_TEST_ASSERT(context, f->device->ReadByte(0x10)==0); f->device->ReadByte(0xC000); f->Run(1700000); AT_PORTABLE_TEST_ASSERT(context, f->device->ReadByte(0x10)==0); f->Run(200000);
	AT_PORTABLE_TEST_ASSERT(context, f->device->ReadByte(0x10)==1 && f->State().m6809.mS==0x3F0); return true;
}

bool ATTestAltirraAMDCAudio(ATPortableTestContext& context) {
	auto f=std::make_unique<Machine>(); static_cast<IATDeviceAudioOutput *>(f->device->AsInterface(IATDeviceAudioOutput::kTypeID))->InitAudioOutput(&f->mixer); f->Slot().mDisk.SetDriveSoundsEnabled(true); f->Select(0);
	AT_PORTABLE_TEST_ASSERT(context, f->Slot().mbMotor && DriveBasePortable::Audible(f->mixer.Render())); f->Slot(1).mDisk.SetDriveSoundsEnabled(false); f->Select(1);
	AT_PORTABLE_TEST_ASSERT(context, !f->Slot().mbMotor && f->Slot(1).mbMotor && !DriveBasePortable::Audible(f->mixer.Render())); f->Slot(1).mDisk.SetDriveSoundsEnabled(true); AT_PORTABLE_TEST_ASSERT(context, DriveBasePortable::Audible(f->mixer.Render())); return true;
}

bool ATTestAltirraAMDCResetShutdown(ATPortableTestContext& context) {
	auto f=std::make_unique<Machine>(); f->host->ReceiveClock(); f->Select(0); f->device->WriteByte(0x50,0x37); f->device->WriteByte(0x2000,0x15); f->host->mManager.PokeyWriteSIO(0x67,false,93,0,false,false); f->device->WarmReset();
	AT_PORTABLE_TEST_ASSERT(context, f->device->ReadByte(0x50)==0x37 && f->device->ReadByte(0x2001)==0x67); f->device->WriteByte(0x2001,0xA6); f->device->ReadByte(0xC000); f->device->PeripheralColdReset();
	AT_PORTABLE_TEST_ASSERT(context, f->device->ReadByte(0x50)==0xA5 && !f->Slot().mbMotor && (f->FDC().DebugReadByte(0)&0x80)); AT_PORTABLE_TEST_ASSERT(context, f->host->Collect(4000).empty());
	f->Select(3); f->device->ReadByte(0xC000); f->device->OnDiskChanged(3,true); f->device->SetHistoryEnabled(true); f->Stop();
	for(uint32 i=0;i<4;++i) AT_PORTABLE_TEST_ASSERT(context, !f->Slot(i).mbMotor && f->Slot(i).mDisk.GetClientCount()==0);
	AT_PORTABLE_TEST_ASSERT(context, !f->device->GetHistoryEnabled()); f->device->Shutdown(); f->device=nullptr; f->host->Tick(2000000); return true;
}
