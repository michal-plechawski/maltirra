#include "atr8000portablefixture.h"
using namespace ATR8000Portable;
bool ATTestAltirraATR8000Interfaces(ATPortableTestContext& context) {
	auto f=std::make_unique<Machine>(); auto& d=*f->device; ATDeviceInfo info {}; d.GetDeviceInfo(info);
	AT_PORTABLE_TEST_ASSERT(context, !strcmp(info.mpDef->mpTag,"diskdriveatr8000") && d.AsInterface(IATDeviceFirmware::kTypeID) && d.AsInterface(IATDeviceDiskDrive::kTypeID) && d.AsInterface(IATDeviceSIO::kTypeID) && d.AsInterface(IATDeviceAudioOutput::kTypeID) && d.AsInterface(IATDeviceParent::kTypeID) && d.AsInterface(IATDeviceButtons::kTypeID) && !d.AsInterface(0));
	AT_PORTABLE_TEST_ASSERT(context, d.GetDisplayCPUClock()==4000000 && d.GetDisasmMode()==kATDebugDisasmMode_Z80 && d.GetComputerPowerOnDelay()==20 && d.GetDeviceBus(0) && d.GetDeviceBus(1) && !d.GetDeviceBus(2));
	vdrefptr<IATDevice> copy; ATPropertySet p; info.mpDef->mpFactoryFn(p,~copy); ATDeviceInfo other {}; copy->GetDeviceInfo(other); AT_PORTABLE_TEST_ASSERT(context, other.mpDef==info.mpDef); return true;
}
bool ATTestAltirraATR8000Settings(ATPortableTestContext& context) {
	auto f=std::make_unique<Machine>(); ATPropertySet p; f->device->GetSettings(p); VDStringW text; f->device->GetSettingsBlurb(text); AT_PORTABLE_TEST_ASSERT(context, p.GetUint32("drivetype3",0)==2 && text==L"D1:,D2:,D3:,D4:" && !wcscmp(p.GetString("signal1",L""),L"rts") && !wcscmp(p.GetString("signal2",L""),L"cts"));
	AT_PORTABLE_TEST_ASSERT(context, f->device->SetSettings(p)); p.SetUint32("drivetype2",99); AT_PORTABLE_TEST_ASSERT(context, f->device->SetSettings(p)); p.SetString("signal1",L"dtr"); p.SetString("signal2",L"cd"); AT_PORTABLE_TEST_ASSERT(context, f->device->SetSettings(p)); ATPropertySet other; f->device->GetSettings(other); AT_PORTABLE_TEST_ASSERT(context, !wcscmp(other.GetString("signal1",L""),L"dtr") && !wcscmp(other.GetString("signal2",L""),L"cd")); p.SetUint32("drivetype2",0); AT_PORTABLE_TEST_ASSERT(context, !f->device->SetSettings(p)); return true;
}
bool ATTestAltirraATR8000SparseSlots(ATPortableTestContext& context) {
	Config c; c.drives={1,0,0,2}; auto f=std::make_unique<Machine>(c); auto first=f->device->GetDiskInterfaceClient(0),last=f->device->GetDiskInterfaceClient(1); AT_PORTABLE_TEST_ASSERT(context, f->disks.requested==(std::vector<uint32>{0,3}) && first.mInterfaceIndex==0 && last.mInterfaceIndex==3 && !f->device->GetDiskInterfaceClient(2).mpClient); f->Select(2); AT_PORTABLE_TEST_ASSERT(context, !(f->FDC().DebugReadByte(0)&0x80)); f->Select(8); AT_PORTABLE_TEST_ASSERT(context, !(f->FDC().DebugReadByte(0)&0x80)); return true;
}
bool ATTestAltirraATR8000Firmware(ATPortableTestContext& context) {
	auto f=std::make_unique<Machine>(); AT_PORTABLE_TEST_ASSERT(context, f->Firmware().GetFirmwareStatus()==ATDeviceFirmwareStatus::OK && !f->Firmware().ReloadFirmware()); Code c; c.Emit({0x3E,0x67}); c.Halt(); f->Program(c.bytes); f->Run(1000); AT_PORTABLE_TEST_ASSERT(context, f->State().mZ80.mA==0x67); VDRemoveFileEx(f->files.File(L"ATR8000.ROM").c_str()); AT_PORTABLE_TEST_ASSERT(context, !f->Firmware().ReloadFirmware() && f->Firmware().GetFirmwareStatus()==ATDeviceFirmwareStatus::Missing); return true;
}
bool ATTestAltirraATR8000CPU(ATPortableTestContext& context) {
	auto f=std::make_unique<Machine>(); Code c; c.Write(0x8000,0x37); c.Emit({0x3C,0x21,0x34,0x12,0xDD,0x21,0x78,0x56,0x06,0x67}); c.Halt(); f->Program(c.bytes); f->Run(1000); auto s=f->State(); AT_PORTABLE_TEST_ASSERT(context, f->device->ReadByte(0x8000)==0x37 && s.mZ80.mA==0x38 && s.mZ80.mB==0x67 && s.mZ80.mH==0x12 && s.mZ80.mL==0x34 && s.mZ80.mIX==0x5678); s.mZ80.mB=0x79; f->device->SetExecState(s); AT_PORTABLE_TEST_ASSERT(context, f->State().mZ80.mB==0x79); return true;
}
bool ATTestAltirraATR8000Memory(ATPortableTestContext& context) {
	auto f=std::make_unique<Machine>(); f->device->WriteByte(0x8000,0x37); f->device->WriteByte(0xFFFF,0x68); AT_PORTABLE_TEST_ASSERT(context, f->device->ReadByte(0x8000)==0x37 && f->device->ReadByte(0xFFFF)==0x68); for(uint32 a=0;a<0x8000;a+=4096) AT_PORTABLE_TEST_ASSERT(context, f->device->ReadByte(a)==0xC3); f->device->WriteByte(0,0x67); AT_PORTABLE_TEST_ASSERT(context, f->device->ReadByte(0)==0xC3); return true;
}
bool ATTestAltirraATR8000ROMBank(ATPortableTestContext& context) {
	auto f=std::make_unique<Machine>(); f->device->WriteByte(0x50,0x37); f->device->WriteByte(0x1050,0x68); f->Out(0x52,1); AT_PORTABLE_TEST_ASSERT(context, f->device->ReadByte(0x50)==0x37 && f->device->ReadByte(0x1050)==0x68); f->Out(0x52,0); AT_PORTABLE_TEST_ASSERT(context, f->device->ReadByte(0x50)==0 && f->device->ReadByte(0x1050)==0); f->device->DeviceReset(); AT_PORTABLE_TEST_ASSERT(context, f->device->ReadByte(0)==0xC3); f->Out(0x52,1); AT_PORTABLE_TEST_ASSERT(context, f->device->ReadByte(0x50)==0x37); return true;
}
bool ATTestAltirraATR8000DebugHistory(ATPortableTestContext& context) {
	std::vector<bool> done; auto f=std::make_unique<Machine>(); Code c; c.Emit({0x3E,0x67}); c.Halt(); f->Program(c.bytes); f->device->SetHistoryEnabled(true); uint16 pc=0; AT_PORTABLE_TEST_ASSERT(context, f->device->StepInto([&](bool ok) { done.push_back(ok); pc=f->State().mZ80.mPC; })); f->Run(1000); const auto range=f->device->GetHistoryRange(); const ATCPUHistoryEntry *entry[1] {}; AT_PORTABLE_TEST_ASSERT(context, done==std::vector<bool>{true} && pc==2 && range.second>0 && f->device->ExtractHistory(entry,range.second-1,1)==1); return true;
}
