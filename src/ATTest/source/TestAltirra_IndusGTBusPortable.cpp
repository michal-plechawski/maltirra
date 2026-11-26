#include "indusgtportablefixture.h"
using namespace IndusGTPortable;

bool ATTestAltirraIndusGTReadSector(ATPortableTestContext& context) {
	for(bool mfm : {false,true}) {
		auto f=std::make_unique<Machine>(0,mfm ? 256 : 128); Bytes payload(mfm ? 256 : 128); for(uint32 i=0;i<payload.size();++i) payload[i]=uint8(i*17+9); f->Fill(183,payload); f->Program(Transfer(false,mfm).bytes); f->Run(100000); Bytes actual(payload.size()); f->device->ReadMemory(0x7000,actual.data(),uint32(actual.size())); for(auto& b:payload) b=~b;
		AT_PORTABLE_TEST_ASSERT(context, actual==payload && !(f->FDC().DebugReadByte(0)&0x1D) && !f->FDC().GetDrqStatus());
	}
	return true;
}

bool ATTestAltirraIndusGTWriteSector(ATPortableTestContext& context) {
	for(bool mfm : {false,true}) {
		auto f=std::make_unique<Machine>(0,mfm ? 256 : 128); f->Program(Transfer(true,mfm).bytes); f->Run(100000);
		AT_PORTABLE_TEST_ASSERT(context, f->Stored(183)==Bytes(mfm ? 256 : 128,0xA5) && f->Image().IsDirty() && !(f->FDC().DebugReadByte(0)&0x7D));
	}
	return true;
}

bool ATTestAltirraIndusGTWriteProtect(ATPortableTestContext& context) {
	auto f=std::make_unique<Machine>(); const auto original=f->Stored(183); f->disks.slot.mDisk.SetWriteMode(kATMediaWriteMode_RO); f->device->ReadByte(0x1000);
	f->Program(Transfer(true).bytes); f->Run(100000); AT_PORTABLE_TEST_ASSERT(context, (f->FDC().DebugReadByte(0)&0x40) && f->Stored(183)==original);
	return true;
}

bool ATTestAltirraIndusGTSerial(ATPortableTestContext& context) {
	auto f=std::make_unique<Machine>(); f->host->ReceiveClock(); Code c; c.Emit({0x3A,0,0x20,0xE6,4,0x28,4,0xD3,4,0x18,2,0xD3,5,0xC3,0,0}); f->Program(c.bytes); f->Run(1000);
	const auto start=f->host->mMachine.mChips.mVideo.mScheduler.GetTick64(); f->host->mManager.PokeyBeginWriteSIO(0xA6,false,93);
	auto received=f->host->Collect(1000); f->host->mManager.PokeyWriteSIO(0xA6,false,93,start,false,false); const auto rest=f->host->Collect(2000); received.insert(received.end(),rest.begin(),rest.end());
	AT_PORTABLE_TEST_ASSERT(context, received==Bytes{0xA6});
	return true;
}

bool ATTestAltirraIndusGTReceiveClock(ATPortableTestContext& context) {
	auto f=std::make_unique<Machine>(); AT_PORTABLE_TEST_ASSERT(context, f->device->ReadByte(0x2000)&4); f->device->OnBeginReceiveByte(0xFF,false,1000); f->Run(100);
	AT_PORTABLE_TEST_ASSERT(context, !(f->device->ReadByte(0x2000)&4)); const uint8 early=f->device->ReadByte(0x2000)&1; f->Run(600); const uint8 late=f->device->ReadByte(0x2000)&1;
	AT_PORTABLE_TEST_ASSERT(context, early!=late); f->Run(500); AT_PORTABLE_TEST_ASSERT(context, f->device->ReadByte(0x2000)&4); f->device->OnTruncateByte();
	AT_PORTABLE_TEST_ASSERT(context, (f->device->ReadByte(0x2000)&5)==5); f->device->OnBeginReceiveByte(0,false,0); f->Run(100); AT_PORTABLE_TEST_ASSERT(context, f->device->ReadByte(0x2000)&4);
	return true;
}

bool ATTestAltirraIndusGTDiskChange(ATPortableTestContext& context) {
	auto f=std::make_unique<Machine>(); f->device->OnDiskChanged(true); f->Run(100); AT_PORTABLE_TEST_ASSERT(context, f->FDC().DebugReadByte(0)&0x40); f->Run(70000);
	AT_PORTABLE_TEST_ASSERT(context, f->FDC().DebugReadByte(0)&0x40); f->Run(30000); AT_PORTABLE_TEST_ASSERT(context, !(f->FDC().DebugReadByte(0)&0x40)); f->Run(90000);
	AT_PORTABLE_TEST_ASSERT(context, f->FDC().DebugReadByte(0)&0x40); f->Run(90000); AT_PORTABLE_TEST_ASSERT(context, !(f->FDC().DebugReadByte(0)&0x40));
	f->disks.slot.mDisk.UnloadDisk(); f->Run(300000); AT_PORTABLE_TEST_ASSERT(context, !f->disks.slot.mDisk.IsDiskLoaded());
	return true;
}

bool ATTestAltirraIndusGTTiming(ATPortableTestContext& context) {
	for(bool mfm : {false,true}) {
		auto f=std::make_unique<Machine>(0,mfm ? 256 : 128); f->Port(11); f->Port(mfm ? 8 : 9); f->FDC().WriteByte(1,10); f->FDC().WriteByte(2,4); f->FDC().WriteByte(0,0x80); std::vector<uint32> ticks;
		for(uint32 i=0;i<40000 && !f->FDC().GetIrqStatus();++i) { f->Run(1); if(f->FDC().GetDrqStatus()) { ticks.push_back(f->host->CPUGetCycle()); f->FDC().ReadByte(3); } }
		AT_PORTABLE_TEST_ASSERT(context, ticks.size()==(mfm ? 256 : 128)); const double period=double(ticks.back()-ticks.front())/(ticks.size()-1); AT_PORTABLE_TEST_ASSERT(context, std::abs(period-(mfm ? 32 : 64)*1.7897725)<1);
	}
	return true;
}

bool ATTestAltirraIndusGTResetShutdown(ATPortableTestContext& context) {
	auto f=std::make_unique<Machine>(); f->device->WriteByte(0x7000,0x37); f->device->OnBeginReceiveByte(0,false,1000); f->Run(100); f->device->WarmReset(); f->Run(1000);
	AT_PORTABLE_TEST_ASSERT(context, (f->device->ReadByte(0x2000)&4) && f->device->ReadByte(0x7000)==0x37); f->Port(11); f->device->WriteByte(0x4000,0);
	f->device->ActivateButton(kATDeviceButton_IndusGTBootCPM,true); f->device->ActivateButton(kATDeviceButton_IndusGTChangeDensity,true);
	f->device->PeripheralColdReset(); f->device->ReadByte(0x1000); AT_PORTABLE_TEST_ASSERT(context, f->device->ReadByte(0x7000)==0xFF && !f->disks.slot.mbMotor && f->disks.slot.mLED==0 && !(f->device->ReadByte(0x1001)&0x70));
	f->Port(11); f->device->OnBeginReceiveByte(0xAA,false,93); f->device->OnDiskChanged(true); f->device->SetHistoryEnabled(true); f->Stop();
	AT_PORTABLE_TEST_ASSERT(context, !f->disks.slot.mbMotor && f->disks.slot.mLED==-1 && f->disks.slot.mDisk.GetClientCount()==0 && !f->device->GetHistoryEnabled()); f->device->Shutdown(); f->device=nullptr; f->host->Tick(400000);
	return true;
}
