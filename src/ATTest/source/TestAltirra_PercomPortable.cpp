#include "percomportablefixture.h"
using namespace PercomPortable;

bool ATTestAltirraPercomInterfaces(ATPortableTestContext& context) {
	for(auto type:variants) { Config c; c.type=type; auto f=std::make_unique<Machine>(c); auto& d=*f->device; ATDeviceInfo info {}; d.GetDeviceInfo(info);
		AT_PORTABLE_TEST_ASSERT(context, d.AsInterface(IATDeviceScheduling::kTypeID) && d.AsInterface(IATDeviceFirmware::kTypeID) && d.AsInterface(IATDeviceDiskDrive::kTypeID) && d.AsInterface(IATDeviceSIO::kTypeID) && d.AsInterface(IATDeviceAudioOutput::kTypeID) && !d.AsInterface(0));
		AT_PORTABLE_TEST_ASSERT(context, bool(d.AsInterface(IATDeviceParent::kTypeID))==(type==Hardware::AT88SPD) && bool(d.AsInterface(ATPIAEmulator::kTypeID))==(type!=Hardware::RFD) && d.GetDisplayCPUClock()==1000000 && d.GetDisasmMode()==kATDebugDisasmMode_6809 && d.GetComputerPowerOnDelay()==20);
		vdrefptr<IATDevice> copy; ATPropertySet p; info.mpDef->mpFactoryFn(p,~copy); ATDeviceInfo other {}; copy->GetDeviceInfo(other); AT_PORTABLE_TEST_ASSERT(context, info.mpDef==other.mpDef);
	} return true;
}

bool ATTestAltirraPercomSettings(ATPortableTestContext& context) {
	for(auto type:variants) { Config c; c.type=type; c.id=13; c.use1795=true; c.dd=false; auto f=std::make_unique<Machine>(c); ATPropertySet p; f->device->GetSettings(p); VDStringW label; f->device->GetSettingsBlurb(label);
		AT_PORTABLE_TEST_ASSERT(context, p.GetUint32("drivetype3",0)==2 && (type!=Hardware::RFD || p.GetUint32("id",0)==5) && (type!=Hardware::AT88 || !p.GetBool("ddcapable",true)) && (type==Hardware::RFD || p.GetBool("use1795",false)));
		AT_PORTABLE_TEST_ASSERT(context, f->device->SetSettings(p)); p.SetUint32("drivetype2",99); AT_PORTABLE_TEST_ASSERT(context, f->device->SetSettings(p)); p.SetUint32("drivetype2",0); AT_PORTABLE_TEST_ASSERT(context, !f->device->SetSettings(p));
		AT_PORTABLE_TEST_ASSERT(context, label==(type==Hardware::RFD ? L"D6:,D7:,D8:,D9:" : L"D1:,D2:,D3:,D4:"));
	} return true;
}

bool ATTestAltirraPercomSparseSlots(ATPortableTestContext& context) {
	Config c; c.drives={1,0,0,2}; c.id=2; auto f=std::make_unique<Machine>(c); auto first=f->device->GetDiskInterfaceClient(0),second=f->device->GetDiskInterfaceClient(1); VDStringW label; f->device->GetSettingsBlurb(label);
	AT_PORTABLE_TEST_ASSERT(context, f->disks.requested==(std::vector<uint32>{2,5}) && first.mInterfaceIndex==2 && second.mInterfaceIndex==5 && !f->device->GetDiskInterfaceClient(2).mpClient && label==L"D3:,D6:");
	f->Select(1); AT_PORTABLE_TEST_ASSERT(context, f->Status()&0x80); f->Select(3); AT_PORTABLE_TEST_ASSERT(context, !(f->Status()&0x80) && f->Slot(3).mbMotor); return true;
}

bool ATTestAltirraPercomFirmware(ATPortableTestContext& context) {
	for(auto type:variants) { Config c; c.type=type; auto f=std::make_unique<Machine>(c); AT_PORTABLE_TEST_ASSERT(context, f->Firmware().GetFirmwareStatus()==ATDeviceFirmwareStatus::OK && !f->Firmware().ReloadFirmware()); Code code; code.Emit({0x86,0x67}); code.Halt(); f->Program(code.bytes); f->Run(1000);
		AT_PORTABLE_TEST_ASSERT(context, f->State().m6809.mA==0x67); VDRemoveFileEx(f->files.File(L"PERCOM.ROM").c_str()); AT_PORTABLE_TEST_ASSERT(context, !f->Firmware().ReloadFirmware() && f->Firmware().GetFirmwareStatus()==ATDeviceFirmwareStatus::Missing);
	} return true;
}

bool ATTestAltirraPercomCPU(ATPortableTestContext& context) {
	for(auto type:variants) { Config c; c.type=type; auto f=std::make_unique<Machine>(c); Code code; code.Emit({0x86,0x37}); code.Emit({0xB7,uint8(f->RAM()>>8),0x50,0x4C,0xC6,0x67,0x8E,0x12,0x34}); code.Halt(); f->Program(code.bytes); f->Run(1000); auto s=f->State();
		AT_PORTABLE_TEST_ASSERT(context, f->device->ReadByte(f->RAM()+0x50)==0x37 && s.m6809.mA==0x38 && s.m6809.mB==0x67 && s.m6809.mX==0x1234); s.m6809.mB=0x79; f->device->SetExecState(s); AT_PORTABLE_TEST_ASSERT(context, f->State().m6809.mB==0x79);
	} return true;
}

bool ATTestAltirraPercomMemory(ATPortableTestContext& context) {
	for(auto type:variants) { Config c; c.type=type; auto f=std::make_unique<Machine>(c); f->device->WriteByte(f->RAM()+0x50,0x37); AT_PORTABLE_TEST_ASSERT(context, f->device->ReadByte(f->RAM()+0x50)==0x37); f->device->WriteByte(0xFC00,0x67); AT_PORTABLE_TEST_ASSERT(context, f->device->ReadByte(0xFC00)==0x20);
		AT_PORTABLE_TEST_ASSERT(context, f->device->ReadByte(type==Hardware::RFD ? 0xF400 : 0x3C00)==0x20);
		if(type!=Hardware::RFD) { AT_PORTABLE_TEST_ASSERT(context, f->device->ReadByte(0x5C50)==0x37 && f->device->ReadByte(0xDC50)==0x37); f->device->WriteByte(0x9FFF,0x68); AT_PORTABLE_TEST_ASSERT(context, f->device->ReadByte(0x13FF)==0x68); }
	} return true;
}

bool ATTestAltirraPercomRegisters(ATPortableTestContext& context) {
	for(auto type:variants) { Config c; c.type=type; c.id=5; auto f=std::make_unique<Machine>(c); f->WriteFDC(1,0xA7); f->WriteFDC(2,0xB6); f->WriteFDC(3,0xE3);
		const uint16 alias=type==Hardware::RFD ? 0xD3D0 : 0x8FF4; AT_PORTABLE_TEST_ASSERT(context, f->device->DebugReadByte(alias+1)==uint8(~0xA7) && f->device->DebugReadByte(alias+2)==uint8(~0xB6) && f->device->DebugReadByte(alias+3)==uint8(~0xE3));
		if(type==Hardware::RFD) AT_PORTABLE_TEST_ASSERT(context, f->device->ReadByte(0xD014)==0xF1 && f->device->ReadByte(0xD000)==0xFF);
	} return true;
}

bool ATTestAltirraPercomDebugHistory(ATPortableTestContext& context) {
	for(auto type:variants) { Config c; c.type=type; std::vector<bool> done; auto f=std::make_unique<Machine>(c); Code code; code.Emit({0x86,0x67}); code.Halt(); f->Program(code.bytes); f->device->SetHistoryEnabled(true); uint16 pc=0;
		AT_PORTABLE_TEST_ASSERT(context, f->device->StepInto([&](bool ok) { done.push_back(ok); pc=f->State().m6809.mPC; })); f->Run(1000); const auto range=f->device->GetHistoryRange(); const ATCPUHistoryEntry *entry[1] {};
		AT_PORTABLE_TEST_ASSERT(context, done==std::vector<bool>{true} && pc==0xFC02 && range.second>0 && f->device->ExtractHistory(entry,range.second-1,1)==1);
	} return true;
}
