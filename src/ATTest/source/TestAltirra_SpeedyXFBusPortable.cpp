#include "speedyxfportablefixture.h"
using namespace SpeedyXFPortable;

bool ATTestAltirraSpeedyXFReadSector(ATPortableTestContext& context) {
	for(bool mfm : {false,true}) {
		auto f=std::make_unique<Machine>(0,mfm ? 256 : 128); Bytes payload(mfm ? 256 : 128); for(uint32 i=0;i<payload.size();++i) payload[i]=uint8(i*17+9); f->Fill(183,payload); f->Program(Transfer(false,mfm).bytes); f->Run(100000);
		Bytes result(payload.size()); f->device->ReadMemory(0x8000,result.data(),uint32(result.size())); for(auto& b:payload) b=~b;
		AT_PORTABLE_TEST_ASSERT(context, result==payload && !(f->FDC().DebugReadByte(0)&0x1D) && !f->FDC().GetDrqStatus());
	}
	return true;
}

bool ATTestAltirraSpeedyXFWriteSector(ATPortableTestContext& context) {
	for(bool mfm : {false,true}) {
		auto f=std::make_unique<Machine>(0,mfm ? 256 : 128); f->Program(Transfer(true,mfm).bytes); f->Run(100000);
		AT_PORTABLE_TEST_ASSERT(context, f->Stored(183)==Bytes(mfm ? 256 : 128,0xA5) && f->Image().IsDirty() && !(f->FDC().DebugReadByte(0)&0x7D));
	}
	auto ro=std::make_unique<Machine>(); const auto original=ro->Stored(183); ro->disks.slot.mDisk.SetWriteMode(kATMediaWriteMode_RO); ro->Program(Transfer(true).bytes); ro->Run(100000);
	AT_PORTABLE_TEST_ASSERT(context, (ro->FDC().DebugReadByte(0)&0x40) && ro->Stored(183)==original);
	return true;
}

bool ATTestAltirraSpeedyXFSide(ATPortableTestContext& context) {
	auto f=std::make_unique<Machine>(0,256,2); constexpr uint32 sector=69*18+3; Bytes data(256,0x37); f->Fill(sector,data); f->Program(Transfer(false,true,true).bytes); f->Run(100000); Bytes actual(256); f->device->ReadMemory(0x8000,actual.data(),256);
	AT_PORTABLE_TEST_ASSERT(context, actual==Bytes(256,0xC8)); f->Program(Transfer(true,true,true).bytes); f->Run(100000);
	AT_PORTABLE_TEST_ASSERT(context, f->Stored(sector)==Bytes(256,0xA5) && f->Stored(183)!=Bytes(256,0xA5));
	return true;
}

bool ATTestAltirraSpeedyXFStepper(ATPortableTestContext& context) {
	auto f=std::make_unique<Machine>(); Text reset; f->FDC().DumpStatus(reset); AT_PORTABLE_TEST_ASSERT(context, reset.value.find("physical track: 10.0")!=std::string::npos);
	f->Step(false); Text inward; f->FDC().DumpStatus(inward); AT_PORTABLE_TEST_ASSERT(context, inward.value.find("physical track: 11.0")!=std::string::npos && !(f->FDC().DebugReadByte(0)&4));
	for(uint32 i=0;i<11;++i) f->Step(true); Text zero; f->FDC().DumpStatus(zero); AT_PORTABLE_TEST_ASSERT(context, zero.value.find("physical track: 0.0")!=std::string::npos && (f->FDC().DebugReadByte(0)&4));
	return true;
}

bool ATTestAltirraSpeedyXFReady(ATPortableTestContext& context) {
	auto f=std::make_unique<Machine>(); f->host->mManager.SetReadyState(false); AT_PORTABLE_TEST_ASSERT(context, f->device->ReadByte(0x282)&2); f->host->mManager.SetReadyState(true); AT_PORTABLE_TEST_ASSERT(context, !(f->device->ReadByte(0x282)&2));
	f->host->mMachine.mChips.mPOKEY.SetCommandLine(true); AT_PORTABLE_TEST_ASSERT(context, f->device->ReadByte(0x282)&0x80); f->host->mMachine.mChips.mPOKEY.SetCommandLine(false); AT_PORTABLE_TEST_ASSERT(context, !(f->device->ReadByte(0x282)&0x80));
	f->device->OnReceiveByte(0xFF,false,1000); f->Run(700); AT_PORTABLE_TEST_ASSERT(context, f->device->ReadByte(0x282)&0x40); f->Run(1100); AT_PORTABLE_TEST_ASSERT(context, !(f->device->ReadByte(0x282)&0x40));
	f->device->OnReceiveByte(0,false,0); f->Run(1000); AT_PORTABLE_TEST_ASSERT(context, !(f->device->ReadByte(0x282)&0x40));
	auto attached=std::make_unique<Machine>(0,128,1,true,true);
	AT_PORTABLE_TEST_ASSERT(context, (attached->device->ReadByte(0x282)&0x82)==0x80);
	return true;
}

bool ATTestAltirraSpeedyXFSerial(ATPortableTestContext& context) {
	auto f=std::make_unique<Machine>(); f->host->ReceiveClock(); Code c; c.Write(0x283,1); const uint8 loop=uint8(c.bytes.size()); c.Emit({0xAD,0x82,2,0x29,0x40,0xF0,4,0xA9,0,0xF0,2,0xA9,1,0x8D,0x82,2,0x4C,loop,0xFC}); f->Program(c.bytes); f->Run(1000);
	f->host->mManager.PokeyWriteSIO(0xA6,false,93,f->host->mMachine.mChips.mVideo.mScheduler.GetTick64(),false,false); AT_PORTABLE_TEST_ASSERT(context, f->host->Collect(3000)==Bytes{0xA6});
	return true;
}

bool ATTestAltirraSpeedyXFTiming(ATPortableTestContext& context) {
	for(bool mfm : {false,true}) {
		auto f=std::make_unique<Machine>(0,mfm ? 256 : 128); f->Motor(true,mfm); f->FDC().WriteByte(1,10); f->FDC().WriteByte(2,4); f->FDC().WriteByte(0,0x88); std::vector<uint32> ticks;
		for(uint32 i=0;i<40000 && !f->FDC().GetIrqStatus();++i) { f->Run(1); if(f->FDC().GetDrqStatus()) { ticks.push_back(f->host->CPUGetCycle()); f->FDC().ReadByte(3); } }
		AT_PORTABLE_TEST_ASSERT(context, ticks.size()==(mfm ? 256 : 128)); const double period=double(ticks.back()-ticks.front())/(ticks.size()-1);
		AT_PORTABLE_TEST_ASSERT(context, std::abs(period-(mfm ? 32 : 64)*0.96*1.7897725)<1);
	}
	return true;
}

bool ATTestAltirraSpeedyXFAudio(ATPortableTestContext& context) {
	auto f=std::make_unique<Machine>(); auto *audio=static_cast<IATDeviceAudioOutput *>(f->device->AsInterface(IATDeviceAudioOutput::kTypeID)); audio->InitAudioOutput(&f->mixer); f->disks.slot.mDisk.SetDriveSoundsEnabled(true); f->Motor(true);
	AT_PORTABLE_TEST_ASSERT(context, f->disks.slot.mbMotor && DriveBasePortable::Audible(f->mixer.Render())); f->Motor(false); AT_PORTABLE_TEST_ASSERT(context, !f->disks.slot.mbMotor && !DriveBasePortable::Audible(f->mixer.Render())); f->Step(false);
	AT_PORTABLE_TEST_ASSERT(context, DriveBasePortable::Audible(f->mixer.Render()));
	return true;
}

bool ATTestAltirraSpeedyXFDiskChange(ATPortableTestContext& context) {
	auto f=std::make_unique<Machine>(); f->device->OnDiskChanged(true); f->Run(100); AT_PORTABLE_TEST_ASSERT(context, (f->device->DebugReadByte(0x400)&0xC0)==0xC0); f->Run(70000);
	AT_PORTABLE_TEST_ASSERT(context, f->device->DebugReadByte(0x400)&0x40); f->Run(30000); AT_PORTABLE_TEST_ASSERT(context, !(f->device->DebugReadByte(0x400)&0x40)); f->Run(90000);
	AT_PORTABLE_TEST_ASSERT(context, f->device->DebugReadByte(0x400)&0x40); f->Run(90000); AT_PORTABLE_TEST_ASSERT(context, !(f->device->DebugReadByte(0x400)&0xC0));
	f->disks.slot.mDisk.UnloadDisk(); f->Run(300000); AT_PORTABLE_TEST_ASSERT(context, f->device->DebugReadByte(0x400)&0x80);
	return true;
}

bool ATTestAltirraSpeedyXFResetShutdown(ATPortableTestContext& context) {
	auto f=std::make_unique<Machine>(); f->device->WriteByte(0x200,0x67); f->device->WriteByte(0x8000,0x37); f->device->OnReceiveByte(0,false,1000); f->Run(100); f->device->WarmReset(); f->Run(1000);
	AT_PORTABLE_TEST_ASSERT(context, !(f->device->ReadByte(0x282)&0x40) && f->device->ReadByte(0x8000)==0x37); f->device->ComputerColdReset(); AT_PORTABLE_TEST_ASSERT(context, f->device->ReadByte(0x200)==0x67);
	f->device->PeripheralColdReset(); AT_PORTABLE_TEST_ASSERT(context, f->device->ReadByte(0x200)==0 && f->device->ReadByte(0x8000)==0 && !f->FDC().GetDrqStatus());
	f->Motor(true); f->device->WriteByte(0x4000,0x55); f->device->OnReceiveByte(0xAA,false,93); f->device->OnDiskChanged(true); f->device->SetHistoryEnabled(true); f->Stop();
	AT_PORTABLE_TEST_ASSERT(context, !f->disks.slot.mbMotor && f->disks.slot.mLED==-1 && f->disks.slot.mDisk.GetClientCount()==0 && !f->device->GetHistoryEnabled()); f->device->Shutdown(); f->device=nullptr; f->host->Tick(400000);
	return true;
}
