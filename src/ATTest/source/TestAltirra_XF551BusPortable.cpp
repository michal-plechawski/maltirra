#include "xf551portablefixture.h"
using namespace XF551Portable;

bool ATTestAltirraXF551ReadSector(ATPortableTestContext& context) {
	auto f=std::make_unique<Machine>(); Bytes data(128); for(uint32 i=0;i<128;++i) data[i]=uint8(i*17+9); f->Fill(3,data);
	f->Program(Transfer(false).bytes); f->Run(300000);
	Bytes actual(128); f->device->DebugReadMemory(0x1040,actual.data(),128); for(auto& b:data) b=~b;
	AT_PORTABLE_TEST_ASSERT(context, actual==data && !(f->FDC().DebugReadByte(0)&0x1D) && f->disks.slot.mbMotor && !f->FDC().GetDrqStatus());
	return true;
}

bool ATTestAltirraXF551WriteSector(ATPortableTestContext& context) {
	for(bool mfm : {false,true}) {
		auto f=std::make_unique<Machine>(0,mfm ? 256 : 128); f->Program(Transfer(true,mfm).bytes); f->Run(300000);
		AT_PORTABLE_TEST_ASSERT(context, f->Stored(3)==Bytes(mfm ? 256 : 128,0xA5) && f->Image().IsDirty() && !(f->FDC().DebugReadByte(0)&0x7D));
	}
	return true;
}

bool ATTestAltirraXF551Side(ATPortableTestContext& context) {
	auto f=std::make_unique<Machine>(0,128,2); const uint32 index=79*18+14;
	f->Program(Transfer(true,false,true).bytes); f->Run(300000);
	AT_PORTABLE_TEST_ASSERT(context, f->Stored(index)==Bytes(128,0xA5) && f->Stored(3)!=Bytes(128,0xA5));
	return true;
}

bool ATTestAltirraXF551WriteProtect(ATPortableTestContext& context) {
	auto f=std::make_unique<Machine>(); const Bytes original(128,0x17); f->Fill(3,original); f->disks.slot.mDisk.SetWriteMode(kATMediaWriteMode_RO);
	f->Program(Transfer(true).bytes); f->Run(300000);
	AT_PORTABLE_TEST_ASSERT(context, (f->FDC().DebugReadByte(0)&0x40) && f->Stored(3)==original && !f->FDC().GetDrqStatus());
	return true;
}

bool ATTestAltirraXF551DiskChange(ATPortableTestContext& context) {
	auto f=std::make_unique<Machine>(); f->device->OnDiskChanged(true); f->Run(100);
	AT_PORTABLE_TEST_ASSERT(context, f->FDC().DebugReadByte(0)&0x40); f->Run(70000);
	AT_PORTABLE_TEST_ASSERT(context, f->FDC().DebugReadByte(0)&0x40); f->Run(30000);
	AT_PORTABLE_TEST_ASSERT(context, !(f->FDC().DebugReadByte(0)&0x40)); f->Run(90000);
	AT_PORTABLE_TEST_ASSERT(context, f->FDC().DebugReadByte(0)&0x40); f->Run(90000);
	AT_PORTABLE_TEST_ASSERT(context, !(f->FDC().DebugReadByte(0)&0x40));
	f->disks.slot.mDisk.UnloadDisk(); f->Run(300000); AT_PORTABLE_TEST_ASSERT(context, !f->disks.slot.mDisk.IsDiskLoaded());
	f->disks.slot.mDisk.CreateDisk(720,3,128); f->Run(300000); AT_PORTABLE_TEST_ASSERT(context, f->disks.slot.mDisk.IsDiskLoaded());
	return true;
}

bool ATTestAltirraXF551SerialReceive(ATPortableTestContext& context) {
	auto f=std::make_unique<Machine>(); Code c; c.Emit({0x36,6,0xBD,0x11}); c.Jump(0); c.Emit({0xBD,0x22}); c.Jump(0); f->Program(c.bytes); f->Run(500);
	AT_PORTABLE_TEST_ASSERT(context, f->State().m8048.mReg[0][5]==0x11);
	f->device->OnReceiveByte(0xFF,false,1000); f->Run(700); AT_PORTABLE_TEST_ASSERT(context, f->State().m8048.mReg[0][5]==0x22);
	f->Run(1100); AT_PORTABLE_TEST_ASSERT(context, f->State().m8048.mReg[0][5]==0x11);
	f->Run(10000); AT_PORTABLE_TEST_ASSERT(context, f->State().m8048.mReg[0][5]==0x11);
	return true;
}

bool ATTestAltirraXF551SerialTransmit(ATPortableTestContext& context) {
	auto f=std::make_unique<Machine>(); f->host->ReceiveClock();
	// Echo inverted T0 through P1.7, exercising 8048 timing, the drive's bit
	// queue, production SIO raw input and actual POKEY serial reception.
	Code c; c.Emit({0x36,7}); c.Port(0x3B); c.Jump(0); c.Port(0xBB); c.Jump(0); f->Program(c.bytes); f->Run(1000);
	f->host->mManager.PokeyWriteSIO(0xA6,false,93,f->host->mMachine.mChips.mVideo.mScheduler.GetTick64(),false,false);
	const auto received=f->host->Collect(3000);
	AT_PORTABLE_TEST_ASSERT(context, received==Bytes{0xA6});
	return true;
}

bool ATTestAltirraXF551Command(ATPortableTestContext& context) {
	auto f=std::make_unique<Machine>(); Bytes code(0x24,0); code[0]=4; code[1]=0x20; code[3]=0x1D; code[4]=0x93; code[0x20]=5; code[0x21]=4; code[0x22]=0x21;
	f->Program(code); f->Run(1000); AT_PORTABLE_TEST_ASSERT(context, f->State().m8048.mReg[0][5]==0);
	f->host->mMachine.mChips.mPOKEY.SetCommandLine(true); f->Run(1000); AT_PORTABLE_TEST_ASSERT(context, f->State().m8048.mReg[0][5]!=0);
	f->host->mMachine.mChips.mPOKEY.SetCommandLine(false); f->Run(2000); const auto count=f->State().m8048.mReg[0][5]; f->Run(2000);
	AT_PORTABLE_TEST_ASSERT(context, f->State().m8048.mReg[0][5]==count);
	return true;
}

bool ATTestAltirraXF551Audio(ATPortableTestContext& context) {
	auto f=std::make_unique<Machine>(); auto *audio=static_cast<IATDeviceAudioOutput *>(f->device->AsInterface(IATDeviceAudioOutput::kTypeID)); audio->InitAudioOutput(&f->mixer); f->disks.slot.mDisk.SetDriveSoundsEnabled(true);
	f->FDC().WriteByte(0,0x58); f->Run(20000);
	AT_PORTABLE_TEST_ASSERT(context, f->disks.slot.mbMotor && DriveBasePortable::Audible(f->mixer.Render()));
	for(uint32 i=0;i<32;++i) f->mixer.Render(); // let the finite step sample finish
	f->device->PeripheralColdReset(); AT_PORTABLE_TEST_ASSERT(context, !f->disks.slot.mbMotor && !DriveBasePortable::Audible(f->mixer.Render()));
	return true;
}

bool ATTestAltirraXF551ResetShutdown(ATPortableTestContext& context) {
	auto f=std::make_unique<Machine>(); Code c; c.Emit({0x36,6,0xBD,0x11}); c.Jump(0); c.Emit({0xBD,0x22}); c.Jump(0); f->Program(c.bytes);
	f->device->OnReceiveByte(0,false,1000); f->Run(1800); f->device->WarmReset(); f->Run(1000);
	AT_PORTABLE_TEST_ASSERT(context, f->State().m8048.mReg[0][5]==0x11);
	f->device->WriteByte(0x1040,0x57); f->device->ComputerColdReset(); AT_PORTABLE_TEST_ASSERT(context, f->device->DebugReadByte(0x1040)==0x57);
	f->device->PeripheralColdReset(); AT_PORTABLE_TEST_ASSERT(context, f->device->DebugReadByte(0x1040)==0 && !f->FDC().GetDrqStatus());
	f->FDC().WriteByte(0,0x58); f->Run(20000); AT_PORTABLE_TEST_ASSERT(context, f->disks.slot.mbMotor);
	f->device->OnReceiveByte(0xAA,false,93); f->device->OnDiskChanged(true); f->device->SetHistoryEnabled(true); f->Stop();
	AT_PORTABLE_TEST_ASSERT(context, !f->disks.slot.mbMotor && f->disks.slot.mDisk.GetClientCount()==0 && !f->device->GetHistoryEnabled());
	f->device->Shutdown(); f->device=nullptr; f->host->Tick(400000);
	return true;
}
