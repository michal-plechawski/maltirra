#include "indusgtportablefixture.h"
using namespace IndusGTPortable;

bool ATTestAltirraIndusGTStatus(ATPortableTestContext& context) {
	auto f=std::make_unique<Machine>(); auto& d=*f->device; const uint8 initial=d.DebugReadByte(0x1000); AT_PORTABLE_TEST_ASSERT(context, d.DebugReadByte(0x1000)==initial && d.ReadByte(0x1001)==initial && d.ReadByte(0x1000)==initial && !(d.DebugReadByte(0x1000)&0xF0));
	AT_PORTABLE_TEST_ASSERT(context, (d.ReadByte(0x2000)&0x34)==0x24); f->host->mManager.SetReadyState(true); AT_PORTABLE_TEST_ASSERT(context, d.ReadByte(0x2000)&0x10); f->host->mManager.SetReadyState(false); AT_PORTABLE_TEST_ASSERT(context, !(d.ReadByte(0x2000)&0x10));
	f->host->mMachine.mChips.mPOKEY.SetCommandLine(true); AT_PORTABLE_TEST_ASSERT(context, !(d.ReadByte(0x2000)&0x20)); f->host->mMachine.mChips.mPOKEY.SetCommandLine(false); AT_PORTABLE_TEST_ASSERT(context, d.ReadByte(0x2000)&0x20);
	auto attached=std::make_unique<Machine>(0,128,true,true); AT_PORTABLE_TEST_ASSERT(context, (attached->device->ReadByte(0x2000)&0x30)==0x10);
	return true;
}

bool ATTestAltirraIndusGTLED(ATPortableTestContext& context) {
	auto f=std::make_unique<Machine>(); AT_PORTABLE_TEST_ASSERT(context, f->disks.slot.mLED==0); Code c; c.Write(0x4000,0xAA); c.Write(0x5000,0x55); c.Halt(); f->Program(c.bytes); f->Run(1000);
	AT_PORTABLE_TEST_ASSERT(context, f->disks.slot.mLED==0x552A); f->device->DebugReadByte(0x4000); AT_PORTABLE_TEST_ASSERT(context, f->disks.slot.mLED==0x552A); f->device->ReadByte(0x4000); AT_PORTABLE_TEST_ASSERT(context, f->disks.slot.mLED==0x2A);
	f->device->ReadByte(0x5000); AT_PORTABLE_TEST_ASSERT(context, f->disks.slot.mLED==0); f->device->WriteByte(0x5000,0); f->device->PeripheralColdReset(); AT_PORTABLE_TEST_ASSERT(context, f->disks.slot.mLED==0);
	return true;
}

bool ATTestAltirraIndusGTStepper(ATPortableTestContext& context) {
	auto f=std::make_unique<Machine>(); AT_PORTABLE_TEST_ASSERT(context, !(f->FDC().DebugReadByte(0)&4)); f->device->WriteByte(0x3000,2); Text inward; f->FDC().DumpStatus(inward);
	AT_PORTABLE_TEST_ASSERT(context, inward.value.find("physical track: 10.5")!=std::string::npos); f->device->WriteByte(0x3000,1); Text back; f->FDC().DumpStatus(back); AT_PORTABLE_TEST_ASSERT(context, back.value.find("physical track: 10.0")!=std::string::npos);
	const uint8 phases[]={1,2,4,8}; for(uint32 p=19;p>=1;--p) f->device->WriteByte(0x3000,phases[p&3]);
	Text near; f->FDC().DumpStatus(near); AT_PORTABLE_TEST_ASSERT(context, near.value.find("physical track: 0.5")!=std::string::npos && (f->FDC().DebugReadByte(0)&4));
	f->device->WriteByte(0x3000,1); Text zero; f->FDC().DumpStatus(zero); AT_PORTABLE_TEST_ASSERT(context, zero.value.find("physical track: 0.0")!=std::string::npos);
	return true;
}

bool ATTestAltirraIndusGTMotorAudio(ATPortableTestContext& context) {
	auto f=std::make_unique<Machine>(); f->device->InitAudioOutput(&f->mixer); f->disks.slot.mDisk.SetDriveSoundsEnabled(true); f->Port(11);
	AT_PORTABLE_TEST_ASSERT(context, f->disks.slot.mbMotor && DriveBasePortable::Audible(f->mixer.samples.Render())); f->Port(10); AT_PORTABLE_TEST_ASSERT(context, !f->disks.slot.mbMotor && !DriveBasePortable::Audible(f->mixer.samples.Render()));
	f->device->WriteByte(0x3000,2); AT_PORTABLE_TEST_ASSERT(context, DriveBasePortable::Audible(f->mixer.samples.Render()));
	return true;
}

bool ATTestAltirraIndusGTSpeaker(ATPortableTestContext& context) {
	auto f=std::make_unique<Machine>(); f->device->InitAudioOutput(&f->mixer); const uint32 start=f->host->CPUGetCycle(); f->Port(1); f->Port(0); f->mixer.FlushRaw(start); const auto& e=f->mixer.edgePlayer.edges;
	AT_PORTABLE_TEST_ASSERT(context, e.size()==2 && e[0].mDeltaValue==-1 && e[1].mDeltaValue==1 && uint32(e[0].mTime-start)>=200 && uint32(e[0].mTime-start)<240 && uint32(e[1].mTime-e[0].mTime)>0 && uint32(e[1].mTime-e[0].mTime)<200 && f->mixer.edgePlayer.volumes.front()==1);
	f->Stop(); AT_PORTABLE_TEST_ASSERT(context, !f->mixer.raw);
	return true;
}

bool ATTestAltirraIndusGTGeometry(ATPortableTestContext& context) {
	auto f=std::make_unique<Machine>(); AT_PORTABLE_TEST_ASSERT(context, f->device->IsImageSupported(f->Image()));
	for(uint32 k=0;k<5;++k) { auto g=f->Image().GetGeometry(); if(k==0) g.mSideCount=2; if(k==1) g.mTrackCount=41; if(k==2) g.mSectorSize=512; if(k==3) g.mSectorsPerTrack=27; if(k==4) g.mbHighDensity=true; g.mTotalSectorCount=g.mSideCount*g.mTrackCount*g.mSectorsPerTrack; vdrefptr<IATDiskImage> image; ATCreateDiskImage(g,~image); AT_PORTABLE_TEST_ASSERT(context, !f->device->IsImageSupported(*image)); }
	return true;
}

bool ATTestAltirraIndusGTDebugHistory(ATPortableTestContext& context) {
	std::vector<bool> done; auto f=std::make_unique<Machine>(); Code c; c.Emit({0x3E,0x67}); c.Halt(); f->Program(c.bytes); f->device->SetHistoryEnabled(true); uint16 pc=0;
	AT_PORTABLE_TEST_ASSERT(context, f->device->StepInto([&](bool ok) { done.push_back(ok); pc=f->State().mZ80.mPC; })); f->Run(1000); const auto range=f->device->GetHistoryRange(); const ATCPUHistoryEntry *entry[1] {};
	AT_PORTABLE_TEST_ASSERT(context, done==std::vector<bool>{true} && pc==2 && range.second>0 && f->device->ExtractHistory(entry,range.second-1,1)==1);
	return true;
}

bool ATTestAltirraIndusGTIRQ(ATPortableTestContext& context) {
	auto f=std::make_unique<Machine>(); Bytes code(0x48,0); const uint8 start[]={0x31,0xF0,0x77,0x3E,0,0x32,0,0x70,0xED,0x56,0xFB,0x18,0xFE}; std::copy(std::begin(start),std::end(start),code.begin());
	const uint8 handler[]={0x3A,0,0x70,0x3C,0x32,0,0x70,0xD3,0,0xFB,0xED,0x4D}; std::copy(std::begin(handler),std::end(handler),code.begin()+0x38); f->Program(code); f->Run(1000);
	AT_PORTABLE_TEST_ASSERT(context, f->device->ReadByte(0x7000)==0); f->host->mMachine.mChips.mPOKEY.SetCommandLine(true); f->Run(1000);
	AT_PORTABLE_TEST_ASSERT(context, f->device->ReadByte(0x7000)==1); f->Run(1000); AT_PORTABLE_TEST_ASSERT(context, f->device->ReadByte(0x7000)==1);
	return true;
}
