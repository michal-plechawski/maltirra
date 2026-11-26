#include "speedyxfportablefixture.h"
using namespace SpeedyXFPortable;

bool ATTestAltirraSpeedyXFInterfaces(ATPortableTestContext& context) {
	auto f=std::make_unique<Machine>(); ATDeviceInfo info {}; auto& d=*f->device; d.GetDeviceInfo(info);
	AT_PORTABLE_TEST_ASSERT(context, !strcmp(info.mpDef->mpTag,"diskdrivespeedyxf") && d.AsInterface(IATDeviceFirmware::kTypeID) && d.AsInterface(IATDeviceDiskDrive::kTypeID) && d.AsInterface(IATDeviceSIO::kTypeID) && d.AsInterface(IATDeviceAudioOutput::kTypeID) && d.AsInterface(ATRIOT6532Emulator::kTypeID) && !d.AsInterface(0));
	AT_PORTABLE_TEST_ASSERT(context, d.GetDebugTarget(0)==static_cast<IATDebugTarget *>(&d) && !d.GetDebugTarget(1) && d.GetDisasmMode()==kATDebugDisasmMode_65C02 && d.GetDisplayCPUClock()==1000000 && !d.GetComputerPowerOnDelay());
	AT_PORTABLE_TEST_ASSERT(context, d.GetDiskInterfaceClient(0).mpClient==static_cast<IATDiskInterfaceClient *>(&d) && !d.GetDiskInterfaceClient(1).mpClient && f->disks.slot.mDisk.GetClientCount()==1);
	vdrefptr<IATDevice> copy; ATPropertySet p; info.mpDef->mpFactoryFn(p,~copy); ATDeviceInfo other {}; copy->GetDeviceInfo(other); AT_PORTABLE_TEST_ASSERT(context, other.mpDef==info.mpDef);
	return true;
}

bool ATTestAltirraSpeedyXFSettings(ATPortableTestContext& context) {
	for(uint32 id=0;id<4;++id) {
		auto f=std::make_unique<Machine>(id); ATPropertySet p; f->device->GetSettings(p); VDStringW text; f->device->GetSettingsBlurb(text); const uint8 ids[]={3,1,0,2};
		AT_PORTABLE_TEST_ASSERT(context, p.GetUint32("id",99)==id && f->disks.selected==id && text[1]==wchar_t('1'+id) && (f->device->DebugReadByte(0x280)&3)==ids[id]);
		p.SetUint32("id",id+4); AT_PORTABLE_TEST_ASSERT(context, f->device->SetSettings(p)); p.SetUint32("id",(id+1)&3); AT_PORTABLE_TEST_ASSERT(context, !f->device->SetSettings(p));
	}
	return true;
}

bool ATTestAltirraSpeedyXFFirmware(ATPortableTestContext& context) {
	auto f=std::make_unique<Machine>(); AT_PORTABLE_TEST_ASSERT(context, f->device->GetFirmwareStatus()==ATDeviceFirmwareStatus::OK && !f->device->ReloadFirmware() && !f->device->GetWritableFirmwareDesc(0) && !f->device->IsWritableFirmwareDirty(0));
	Code code; code.Emit({0xA9,0x67}); code.Halt(); f->Program(code.bytes); f->Run(1000); AT_PORTABLE_TEST_ASSERT(context, f->State().m6502.mA==0x67);
	VDRemoveFileEx(f->files.File(L"SPEEDYXF.ROM").c_str()); AT_PORTABLE_TEST_ASSERT(context, f->device->ReloadFirmware() && f->device->GetFirmwareStatus()==ATDeviceFirmwareStatus::Missing && f->device->DebugReadByte(0xFC00)==0);
	return true;
}

bool ATTestAltirraSpeedyXFROMBanks(ATPortableTestContext& context) {
	auto f=std::make_unique<Machine>(); Bytes rom(65536,0x37); std::fill(rom.begin()+32768,rom.end(),0x68); f->Reload(rom);
	AT_PORTABLE_TEST_ASSERT(context, f->device->ReadByte(0xC000)==0x68 && f->device->ReadByte(0x2000)==0x68); f->device->WriteByte(0x282,0); f->device->WriteByte(0x283,0x20);
	AT_PORTABLE_TEST_ASSERT(context, f->device->ReadByte(0xC000)==0x37 && f->device->ReadByte(0x2000)==0x37); f->device->PeripheralColdReset();
	AT_PORTABLE_TEST_ASSERT(context, f->device->ReadByte(0xC000)==0x68); f->device->WriteByte(0xC000,0x12); AT_PORTABLE_TEST_ASSERT(context, f->device->ReadByte(0xC000)==0x68);
	f->Reload(Bytes(32768,0x79)); AT_PORTABLE_TEST_ASSERT(context, f->device->ReadByte(0xC000)==0x79); f->device->WriteByte(0x282,0); f->device->WriteByte(0x283,0x20); AT_PORTABLE_TEST_ASSERT(context, f->device->ReadByte(0xC000)==0x79);
	return true;
}

bool ATTestAltirraSpeedyXFCPU(ATPortableTestContext& context) {
	auto f=std::make_unique<Machine>(); Code code; code.Write(0x40,0x67); code.Emit({0x64,0x40,0xA9,0x37,0x1A}); code.Halt(); f->Program(code.bytes); f->Run(1000);
	AT_PORTABLE_TEST_ASSERT(context, f->device->ReadByte(0x40)==0 && f->State().m6502.mA==0x38 && f->State().m6502.mPC==0xFC0A && f->device->GetTimeSkew()>=-8);
	return true;
}

bool ATTestAltirraSpeedyXFMemory(ATPortableTestContext& context) {
	auto f=std::make_unique<Machine>(); auto& d=*f->device; d.WriteByte(0x40,0x37); d.WriteByte(0x140,0x68); d.WriteByte(0x8000,0x79); d.WriteByte(0xBFFF,0x52); d.WriteByte(0x200,0xA6);
	AT_PORTABLE_TEST_ASSERT(context, d.ReadByte(0x40)==0x37 && d.ReadByte(0x140)==0x68 && d.ReadByte(0x8000)==0x79 && d.ReadByte(0xBFFF)==0x52 && d.ReadByte(0x300)==0xA6 && d.ReadByte(0x6000)==0xFF);
	d.WriteByte(0x8000,0xE3); AT_PORTABLE_TEST_ASSERT(context, d.DebugReadByte(0x8000)==0xE3);
	return true;
}

bool ATTestAltirraSpeedyXFRegisters(ATPortableTestContext& context) {
	auto f=std::make_unique<Machine>(); auto& d=*f->device; d.WriteByte(0x401,0xA7); d.WriteByte(0x402,0xB6); d.WriteByte(0x403,0xE3);
	AT_PORTABLE_TEST_ASSERT(context, d.DebugReadByte(0x401)==0xA7 && d.DebugReadByte(0x402)==0xB6 && d.DebugReadByte(0x403)==0xE3 && d.ReadByte(0x403)==0xE3);
	d.WriteByte(0x400,0xD8); f->Run(100); AT_PORTABLE_TEST_ASSERT(context, f->FDC().GetIrqStatus()); d.DebugReadByte(0x400); AT_PORTABLE_TEST_ASSERT(context, f->FDC().GetIrqStatus()); d.ReadByte(0x400); AT_PORTABLE_TEST_ASSERT(context, !f->FDC().GetIrqStatus());
	return true;
}

bool ATTestAltirraSpeedyXFLED(ATPortableTestContext& context) {
	auto f=std::make_unique<Machine>(); Code c; c.Write(0x4000,0x55); c.Write(0x4001,0xAA); c.Halt(); f->Program(c.bytes); f->Run(1000);
	AT_PORTABLE_TEST_ASSERT(context, f->disks.slot.mLED==0x55AA); f->device->WriteByte(0x4010,0x79); AT_PORTABLE_TEST_ASSERT(context, f->disks.slot.mLED==0x79AA); f->device->PeripheralColdReset(); AT_PORTABLE_TEST_ASSERT(context, f->disks.slot.mLED==0);
	return true;
}

bool ATTestAltirraSpeedyXFGeometry(ATPortableTestContext& context) {
	auto f=std::make_unique<Machine>(0,256,2); AT_PORTABLE_TEST_ASSERT(context, f->device->IsImageSupported(f->Image()));
	for(uint32 kind=0;kind<4;++kind) {
		auto g=f->Image().GetGeometry(); if(kind==0) g.mbHighDensity=true; if(kind==1) g.mSectorSize=512; if(kind==2) g.mTrackCount=41; if(kind==3) g.mSectorsPerTrack=19;
		g.mTotalSectorCount=g.mTrackCount*g.mSideCount*g.mSectorsPerTrack; vdrefptr<IATDiskImage> image; ATCreateDiskImage(g,~image); AT_PORTABLE_TEST_ASSERT(context, !f->device->IsImageSupported(*image));
	}
	return true;
}

bool ATTestAltirraSpeedyXFDebugTrace(ATPortableTestContext& context) {
	ATTraceContext trace {}; trace.mpCollection=new ATTraceCollection; std::vector<bool> done; auto f=std::make_unique<Machine>(); Code c; c.Emit({0xA9,0x67}); c.Halt(); f->Program(c.bytes); f->device->SetHistoryEnabled(true); uint16 pc=0;
	AT_PORTABLE_TEST_ASSERT(context, f->device->StepInto([&](bool ok) { done.push_back(ok); pc=f->State().m6502.mPC; })); f->Run(1000); const auto range=f->device->GetHistoryRange(); const ATCPUHistoryEntry *entry[1] {};
	AT_PORTABLE_TEST_ASSERT(context, done==std::vector<bool>{true} && pc==0xFC02 && range.second>0 && f->device->ExtractHistory(entry,range.second-1,1)==1);
	f->device->SetTraceContext(&trace); f->FDC().WriteByte(0,0xD8); f->Run(100); f->device->SetTraceContext(nullptr); f->device->SetTraceContext(nullptr);
	AT_PORTABLE_TEST_ASSERT(context, trace.mpCollection->GetGroupCount()==1 && trace.mpCollection->GetGroup(0)->GetChannelCount()==2);
	return true;
}
