#include "fullDrivePortableFixture.h"
using namespace FullDrivePortable;

bool ATTestAltirraFullDriveReadSector(ATPortableTestContext& context) {
	for(Type type : {Drive::kDeviceType_810,Drive::kDeviceType_1050}) {
		auto f=std::make_unique<Machine>(type); Bytes data(128); for(uint32 i=0;i<128;++i) data[i]=uint8(i*17+9); f->Fill(183,data); f->Program(Transfer(f->is1050,false).bytes); f->Run(100000);
		Bytes result(128); f->device->DebugReadMemory(0x80,result.data(),128); if(f->is1050) for(auto& v:data) v=~v;
		AT_PORTABLE_TEST_ASSERT(context, result==data && !(f->FDC().DebugReadByte(0)&0x1D) && !f->FDC().GetDrqStatus());
	}
	return true;
}

bool ATTestAltirraFullDriveWriteSector(ATPortableTestContext& context) {
	for(Type type : {Drive::kDeviceType_810,Drive::kDeviceType_1050}) for(bool mfm : {false,true}) {
		if(mfm && type==Drive::kDeviceType_810) continue;
		auto f=std::make_unique<Machine>(type,0,mfm ? 256 : 128); f->Program(Transfer(f->is1050,true,mfm).bytes); f->Run(100000);
		AT_PORTABLE_TEST_ASSERT(context, f->Stored(183)==Bytes(mfm ? 256 : 128,0xA5) && f->Image().IsDirty() && !(f->FDC().DebugReadByte(0)&0x7D));
	}
	return true;
}

bool ATTestAltirraFullDriveStepper(ATPortableTestContext& context) {
	struct Text : ATConsoleOutput { std::string text; void WriteLine(const char *s) override { text+=s; } };
	for(Type type : {Drive::kDeviceType_810,Drive::kDeviceType_1050}) {
		auto f=std::make_unique<Machine>(type); const uint8 first=f->is1050 ? 0x1C : 0x18,next=f->is1050 ? 0x2C : 0x30; f->device->WriteByte(f->RIOT()+2,first); f->device->WriteByte(f->RIOT()+3,0x3C); f->device->WriteByte(f->RIOT()+2,next);
		Text inward; f->FDC().DumpStatus(inward); AT_PORTABLE_TEST_ASSERT(context, inward.text.find(f->is1050 ? "physical track: 10.5" : "physical track: 11.0")!=std::string::npos);
		f->device->WriteByte(f->RIOT()+2,first); Text outward; f->FDC().DumpStatus(outward); AT_PORTABLE_TEST_ASSERT(context, outward.text.find("physical track: 10.0")!=std::string::npos);
	}
	return true;
}

bool ATTestAltirraFullDriveDensityMotor(ATPortableTestContext& context) {
	for(Type type : {Drive::kDeviceType_810,Drive::kDeviceType_1050}) {
		auto f=std::make_unique<Machine>(type); auto *audio=static_cast<IATDeviceAudioOutput *>(f->device->AsInterface(IATDeviceAudioOutput::kTypeID)); audio->InitAudioOutput(&f->mixer); f->disks.slot.mDisk.SetDriveSoundsEnabled(true);
		f->Motor(false); AT_PORTABLE_TEST_ASSERT(context, !f->disks.slot.mbMotor && !DriveBasePortable::Audible(f->mixer.Render())); f->Motor(true);
		AT_PORTABLE_TEST_ASSERT(context, f->disks.slot.mbMotor && DriveBasePortable::Audible(f->mixer.Render())); f->Motor(false); AT_PORTABLE_TEST_ASSERT(context, !DriveBasePortable::Audible(f->mixer.Render()));
	}
	return true;
}

bool ATTestAltirraFullDriveDiskChange(ATPortableTestContext& context) {
	for(Type type : {Drive::kDeviceType_810,Drive::kDeviceType_1050}) {
		auto f=std::make_unique<Machine>(type); f->device->OnDiskChanged(true); f->Run(100); AT_PORTABLE_TEST_ASSERT(context, f->FDC().DebugReadByte(0)&0x40); f->Run(70000);
		AT_PORTABLE_TEST_ASSERT(context, f->FDC().DebugReadByte(0)&0x40); f->Run(30000); AT_PORTABLE_TEST_ASSERT(context, !(f->FDC().DebugReadByte(0)&0x40)); f->Run(90000);
		AT_PORTABLE_TEST_ASSERT(context, f->FDC().DebugReadByte(0)&0x40); f->Run(90000); AT_PORTABLE_TEST_ASSERT(context, !(f->FDC().DebugReadByte(0)&0x40));
	}
	return true;
}

bool ATTestAltirraFullDriveSerial(ATPortableTestContext& context) {
	auto f=std::make_unique<Machine>(); f->host->ReceiveClock(); Code c; c.Write(0x283,1); const uint8 loop=uint8(c.bytes.size()); c.Emit({0xAD,0x82,2,0x29,0x40,0xF0,4,0xA9,0,0xF0,2,0xA9,1,0x8D,0x82,2,0x4C,loop,0xFC});
	f->Program(c.bytes); f->Run(1000); f->host->mManager.PokeyWriteSIO(0xA6,false,93,f->host->mMachine.mChips.mVideo.mScheduler.GetTick64(),false,false);
	const auto received=f->host->Collect(3000);
	AT_PORTABLE_TEST_ASSERT(context, received==Bytes{0xA6});
	auto a=std::make_unique<Machine>(Drive::kDeviceType_810); a->device->OnReceiveByte(0xFF,false,1000); a->Run(700); AT_PORTABLE_TEST_ASSERT(context, a->device->ReadByte(0x302)&0x80); a->Run(1100); AT_PORTABLE_TEST_ASSERT(context, !(a->device->ReadByte(0x302)&0x80));
	a->host->mMachine.mChips.mPOKEY.SetCommandLine(true); AT_PORTABLE_TEST_ASSERT(context, a->device->ReadByte(0x302)&0x40);
	return true;
}

bool ATTestAltirraFullDriveTiming(ATPortableTestContext& context) {
	for(bool mfm : {false,true}) {
		auto f=std::make_unique<Machine>(Drive::kDeviceType_1050,0,mfm ? 256 : 128); f->Motor(true,mfm); f->FDC().WriteByte(1,10); f->FDC().WriteByte(2,4); f->FDC().WriteByte(0,0x80); std::vector<uint32> ticks;
		for(uint32 i=0;i<40000 && !f->FDC().GetIrqStatus();++i) { f->Run(1); if(f->FDC().GetDrqStatus()) { ticks.push_back(f->host->CPUGetCycle()); f->FDC().ReadByte(3); } }
		AT_PORTABLE_TEST_ASSERT(context, ticks.size()==(mfm ? 256 : 128)); const double dt=double(ticks.back()-ticks.front())/(ticks.size()-1);
		AT_PORTABLE_TEST_ASSERT(context, std::abs(dt-(mfm ? 32 : 64)*1.7897725)<1);
	}
	return true;
}

bool ATTestAltirraFullDriveResetShutdown(ATPortableTestContext& context) {
	for(Type type : {Drive::kDeviceType_810,Drive::kDeviceType_1050,Drive::kDeviceType_SuperArchiver_BitWriter}) {
		auto f=std::make_unique<Machine>(type); f->device->WriteByte(0x180,0x37); f->device->OnReceiveByte(0,false,1000); f->Run(100); f->device->WarmReset(); f->Run(1000);
		AT_PORTABLE_TEST_ASSERT(context, !(f->device->ReadByte(f->RIOT()+2)&(f->is1050 ? 0x40 : 0x80)) && f->device->ReadByte(0x180)==0x37);
		f->device->PeripheralColdReset(); AT_PORTABLE_TEST_ASSERT(context, f->device->ReadByte(0x180)==0 && !f->FDC().GetDrqStatus()); f->Motor(true);
		f->device->OnReceiveByte(0xAA,false,93); f->device->OnDiskChanged(true); f->device->SetHistoryEnabled(true); f->Stop();
		AT_PORTABLE_TEST_ASSERT(context, !f->disks.slot.mbMotor && f->disks.slot.mDisk.GetClientCount()==0 && !f->device->GetHistoryEnabled()); f->device->Shutdown(); f->device=nullptr; f->host->Tick(400000);
	}
	return true;
}
