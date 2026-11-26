#include "amdcportablefixture.h"
using namespace AMDCPortable;

bool ATTestAltirraAMDCInterfaces(ATPortableTestContext& context) {
	auto f=std::make_unique<Machine>(); auto& d=*f->device; ATDeviceInfo info {}; d.GetDeviceInfo(info);
	AT_PORTABLE_TEST_ASSERT(context, !strcmp(info.mpDef->mpTag,"diskdriveamdc") && d.AsInterface(IATDeviceScheduling::kTypeID) && d.AsInterface(IATDeviceFirmware::kTypeID) && d.AsInterface(IATDeviceDiskDrive::kTypeID) && d.AsInterface(IATDeviceSIO::kTypeID) && d.AsInterface(IATDeviceAudioOutput::kTypeID) && !d.AsInterface(0));
	AT_PORTABLE_TEST_ASSERT(context, d.GetDisasmMode()==kATDebugDisasmMode_6809 && d.GetDisplayCPUClock()==1000000 && d.GetComputerPowerOnDelay()==20 && d.GetDebugTarget(0)==static_cast<IATDebugTarget *>(&d) && !d.GetDebugTarget(1));
	vdrefptr<IATDevice> copy; ATPropertySet p; info.mpDef->mpFactoryFn(p,~copy); ATDeviceInfo other {}; copy->GetDeviceInfo(other); AT_PORTABLE_TEST_ASSERT(context, other.mpDef==info.mpDef);
	return true;
}

bool ATTestAltirraAMDCSettings(ATPortableTestContext& context) {
	Config c; c.switches=0x2A5; auto f=std::make_unique<Machine>(c); ATPropertySet p; f->device->GetSettings(p); VDStringW blurb; f->device->GetSettingsBlurb(blurb);
	AT_PORTABLE_TEST_ASSERT(context, p.GetUint32("switches",0)==0xA5 && p.GetBool("drive2",false) && p.GetUint32("extdrive1",0)==2 && blurb==L"D3:,D4:,D5:,D6:");
	AT_PORTABLE_TEST_ASSERT(context, f->device->SetSettings(p)); p.SetUint32("extdrive0",99); AT_PORTABLE_TEST_ASSERT(context, f->device->SetSettings(p)); p.SetBool("drive2",false); AT_PORTABLE_TEST_ASSERT(context, !f->device->SetSettings(p));
	return true;
}

bool ATTestAltirraAMDCSparseSlots(ATPortableTestContext& context) {
	Config c; c.drive2=false; c.ext0=0; c.ext1=2; c.switches=0x10; auto f=std::make_unique<Machine>(c); const auto first=f->device->GetDiskInterfaceClient(0),second=f->device->GetDiskInterfaceClient(1);
	AT_PORTABLE_TEST_ASSERT(context, f->disks.requested==(std::vector<uint32>{1,2}) && first.mInterfaceIndex==1 && second.mInterfaceIndex==2 && !f->device->GetDiskInterfaceClient(2).mpClient);
	AT_PORTABLE_TEST_ASSERT(context, f->Slot(0).mDisk.GetClientCount()==1 && f->Slot(1).mDisk.GetClientCount()==1); f->Select(1); AT_PORTABLE_TEST_ASSERT(context, f->FDC().DebugReadByte(0)&0x80);
	f->Select(3); AT_PORTABLE_TEST_ASSERT(context, !(f->FDC().DebugReadByte(0)&0x80) && f->Slot(1).mbMotor);
	return true;
}

bool ATTestAltirraAMDCFirmware(ATPortableTestContext& context) {
	auto f=std::make_unique<Machine>(); AT_PORTABLE_TEST_ASSERT(context, f->Firmware().GetFirmwareStatus()==ATDeviceFirmwareStatus::OK && !f->Firmware().ReloadFirmware()); Code c; c.Emit({0x86,0x67}); c.Halt(); f->Program(c.bytes); f->Run(1000);
	AT_PORTABLE_TEST_ASSERT(context, f->State().m6809.mA==0x67 && f->device->ReadByte(0xEC00)==0x86 && f->device->ReadByte(0xFC00)==0x86);
	VDRemoveFileEx(f->files.File(L"AMDC.ROM").c_str()); AT_PORTABLE_TEST_ASSERT(context, !f->Firmware().ReloadFirmware() && f->Firmware().GetFirmwareStatus()==ATDeviceFirmwareStatus::Missing);
	return true;
}

bool ATTestAltirraAMDCCPU(ATPortableTestContext& context) {
	auto f=std::make_unique<Machine>(); Code c; c.Emit({0x86,0x37,0xB7,0,0x50,0x4C,0x8E,0x12,0x34,0x10,0xCE,3,0xF0,0xC6,0x67}); c.Halt(); f->Program(c.bytes); f->Run(1000); auto s=f->State();
	AT_PORTABLE_TEST_ASSERT(context, f->device->ReadByte(0x50)==0x37 && s.m6809.mA==0x38 && s.m6809.mB==0x67 && s.m6809.mX==0x1234 && s.m6809.mS==0x3F0);
	s.m6809.mB=0x79; f->device->SetExecState(s); AT_PORTABLE_TEST_ASSERT(context, f->State().m6809.mB==0x79);
	return true;
}

bool ATTestAltirraAMDCMemory(ATPortableTestContext& context) {
	auto f=std::make_unique<Machine>(); f->device->WriteByte(0x50,0x37); f->device->WriteByte(0x7FF,0x68); AT_PORTABLE_TEST_ASSERT(context, f->device->ReadByte(0x450)==0x37 && f->device->ReadByte(0x3FF)==0x68);
	f->device->WriteByte(0xFC00,0x67); AT_PORTABLE_TEST_ASSERT(context, f->device->ReadByte(0xFC00)==0x20); f->device->WriteByte(0x4001,0xA7); f->device->WriteByte(0x4002,0xB6); f->device->WriteByte(0x4003,0xE3);
	AT_PORTABLE_TEST_ASSERT(context, f->device->DebugReadByte(0x5FFD)==0xA7 && f->device->DebugReadByte(0x5FFE)==0xB6 && f->device->DebugReadByte(0x5FFF)==0xE3);
	return true;
}

bool ATTestAltirraAMDCSwitches(ATPortableTestContext& context) {
	Config c; c.switches=0xA5; auto f=std::make_unique<Machine>(c); AT_PORTABLE_TEST_ASSERT(context, f->device->ReadByte(0x8000)==0xFA && f->device->ReadByte(0x8001)==0xF5 && f->device->ReadByte(0xBFFE)==0xFA && f->device->ReadByte(0x1000)==0x30);
	Config test; test.drive2=false; test.switches=0x100; auto t=std::make_unique<Machine>(test); AT_PORTABLE_TEST_ASSERT(context, t->device->ReadByte(0x1000)==0); t->device->WriteByte(0x1800,0x67); AT_PORTABLE_TEST_ASSERT(context, t->device->ReadByte(0x1000)==0);
	return true;
}

bool ATTestAltirraAMDCDebugHistory(ATPortableTestContext& context) {
	std::vector<bool> done; auto f=std::make_unique<Machine>(); Code c; c.Emit({0x86,0x67}); c.Halt(); f->Program(c.bytes); f->device->SetHistoryEnabled(true); uint16 pc=0;
	AT_PORTABLE_TEST_ASSERT(context, f->device->StepInto([&](bool ok) { done.push_back(ok); pc=f->State().m6809.mPC; })); f->Run(1000); const auto range=f->device->GetHistoryRange(); const ATCPUHistoryEntry *entry[1] {};
	AT_PORTABLE_TEST_ASSERT(context, done==std::vector<bool>{true} && pc==0xFC02 && range.second>0 && f->device->ExtractHistory(entry,range.second-1,1)==1);
	return true;
}
