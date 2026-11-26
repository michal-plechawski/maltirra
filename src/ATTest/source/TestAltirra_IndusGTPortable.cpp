#include "indusgtportablefixture.h"
using namespace IndusGTPortable;

bool ATTestAltirraIndusGTInterfaces(ATPortableTestContext& context) {
	auto f=std::make_unique<Machine>(); auto& d=*f->device; ATDeviceInfo info {}; d.GetDeviceInfo(info);
	AT_PORTABLE_TEST_ASSERT(context, !strcmp(info.mpDef->mpTag,"diskdriveindusgt") && d.AsInterface(IATDeviceFirmware::kTypeID) && d.AsInterface(IATDeviceDiskDrive::kTypeID) && d.AsInterface(IATDeviceSIO::kTypeID) && d.AsInterface(IATDeviceAudioOutput::kTypeID) && d.AsInterface(IATDeviceButtons::kTypeID) && !d.AsInterface(0));
	AT_PORTABLE_TEST_ASSERT(context, d.GetDebugTarget(0)==static_cast<IATDebugTarget *>(&d) && !d.GetDebugTarget(1) && d.GetDisasmMode()==kATDebugDisasmMode_Z80 && d.GetDisplayCPUClock()==4000000 && f->disks.slot.mDisk.GetClientCount()==1);
	AT_PORTABLE_TEST_ASSERT(context, d.GetDiskInterfaceClient(0).mpClient==static_cast<IATDiskInterfaceClient *>(&d) && !d.GetDiskInterfaceClient(1).mpClient);
	vdrefptr<IATDevice> copy; ATPropertySet p; info.mpDef->mpFactoryFn(p,~copy); ATDeviceInfo other {}; copy->GetDeviceInfo(other); AT_PORTABLE_TEST_ASSERT(context, other.mpDef==info.mpDef);
	return true;
}

bool ATTestAltirraIndusGTSettings(ATPortableTestContext& context) {
	for(uint32 id=0;id<4;++id) {
		auto f=std::make_unique<Machine>(id); ATPropertySet p; f->device->GetSettings(p); VDStringW text; f->device->GetSettingsBlurb(text);
		AT_PORTABLE_TEST_ASSERT(context, p.GetUint32("id",99)==id && f->disks.selected==id && text[1]==wchar_t('1'+id) && (f->device->DebugReadByte(0x1000)&3)==id);
		p.SetUint32("id",id+4); AT_PORTABLE_TEST_ASSERT(context, f->device->SetSettings(p)); p.SetUint32("id",(id+1)&3); AT_PORTABLE_TEST_ASSERT(context, !f->device->SetSettings(p));
	}
	return true;
}

bool ATTestAltirraIndusGTFirmware(ATPortableTestContext& context) {
	auto f=std::make_unique<Machine>(); AT_PORTABLE_TEST_ASSERT(context, f->Firmware().GetFirmwareStatus()==ATDeviceFirmwareStatus::OK && !f->Firmware().ReloadFirmware() && !f->Firmware().GetWritableFirmwareDesc(0) && !f->Firmware().IsWritableFirmwareDirty(0));
	Code c; c.Emit({0x3E,0x67}); c.Halt(); f->Program(c.bytes); f->Run(1000); AT_PORTABLE_TEST_ASSERT(context, f->State().mZ80.mA==0x67);
	VDRemoveFileEx(f->files.File(L"INDUSGT.ROM").c_str()); AT_PORTABLE_TEST_ASSERT(context, !f->Firmware().ReloadFirmware() && f->Firmware().GetFirmwareStatus()==ATDeviceFirmwareStatus::Missing && f->device->DebugReadByte(0)==0x3E);
	return true;
}

bool ATTestAltirraIndusGTCPU(ATPortableTestContext& context) {
	auto f=std::make_unique<Machine>(); Code c; c.Emit({0x31,0xF0,0x77,0x3E,0x37,0x32,0,0x70,0x3C,0xDD,0x21,0x34,0x12}); c.Halt(); f->Program(c.bytes); f->Run(1000);
	AT_PORTABLE_TEST_ASSERT(context, f->device->DebugReadByte(0x7000)==0x37 && f->State().mZ80.mA==0x38 && f->State().mZ80.mIX==0x1234 && f->State().mZ80.mPC==13 && f->State().mZ80.mSP==0x77F0);
	return true;
}

bool ATTestAltirraIndusGTMemory(ATPortableTestContext& context) {
	auto f=std::make_unique<Machine>(); auto& d=*f->device; d.WriteByte(0x7000,0x37); d.WriteByte(0x77FF,0x68); d.WriteByte(0x8000,0x79); d.WriteByte(0xFFFF,0x52);
	AT_PORTABLE_TEST_ASSERT(context, d.ReadByte(0x7800)==0x37 && d.ReadByte(0x7FFF)==0x68 && d.ReadByte(0x8000)==0x79 && d.ReadByte(0xFFFF)==0x52);
	d.WriteByte(0,0x67); AT_PORTABLE_TEST_ASSERT(context, d.ReadByte(0)==0xC3); d.WriteByte(0x6001,0xA7); d.WriteByte(0x6002,0xB6); d.WriteByte(0x6003,0xE3);
	AT_PORTABLE_TEST_ASSERT(context, d.DebugReadByte(0x6FFD)==0xA7 && d.DebugReadByte(0x6FFE)==0xB6 && d.DebugReadByte(0x6FFF)==0xE3);
	return true;
}

bool ATTestAltirraIndusGTExtendedRAM(ATPortableTestContext& context) {
	auto f=std::make_unique<Machine>(); f->device->WriteByte(0x7000,0x37); f->device->WriteByte(0xF000,0x68); f->Port(15);
	f->device->WriteByte(0,0x79); f->device->WriteByte(0x1000,0x52); f->device->WriteByte(0x2000,0x67); f->device->WriteByte(0x7000,0x43);
	AT_PORTABLE_TEST_ASSERT(context, f->device->ReadByte(0)==0x79 && f->device->ReadByte(0x1000)==0x52 && f->device->ReadByte(0x2000)==0x67 && f->device->ReadByte(0xF000)==0x68);
	f->Port(14); AT_PORTABLE_TEST_ASSERT(context, f->device->ReadByte(0)==0xC3 && f->device->ReadByte(0x7000)==0x37 && f->device->ReadByte(0x7800)==0x37);
	f->Port(15); AT_PORTABLE_TEST_ASSERT(context, f->device->ReadByte(0)==0x79 && f->device->ReadByte(0x7000)==0x43);
	return true;
}

bool ATTestAltirraIndusGTButtons(ATPortableTestContext& context) {
	auto f=std::make_unique<Machine>(); f->device->ReadByte(0x1000);
	for(auto button : {kATDeviceButton_IndusGTTrack,kATDeviceButton_IndusGTId,kATDeviceButton_IndusGTError}) {
		const uint8 mask=button==kATDeviceButton_IndusGTTrack ? 0x10 : button==kATDeviceButton_IndusGTId ? 0x20 : 0x40;
		AT_PORTABLE_TEST_ASSERT(context, f->device->GetSupportedButtons()&(1u<<button)); f->device->ActivateButton(button,true);
		AT_PORTABLE_TEST_ASSERT(context, f->device->IsButtonDepressed(button) && (f->device->DebugReadByte(0x1000)&mask)); f->device->ReadByte(0x1000); AT_PORTABLE_TEST_ASSERT(context, f->device->ReadByte(0x1001)&mask);
		f->device->ActivateButton(button,false); f->device->ReadByte(0x1000); AT_PORTABLE_TEST_ASSERT(context, !f->device->IsButtonDepressed(button) && !(f->device->ReadByte(0x1001)&mask));
	}
	return true;
}

bool ATTestAltirraIndusGTButtonMacros(ATPortableTestContext& context) {
	auto f=std::make_unique<Machine>(); f->device->ReadByte(0x1000); f->device->ActivateButton(kATDeviceButton_IndusGTChangeDensity,true);
	AT_PORTABLE_TEST_ASSERT(context, f->device->ReadByte(0x1000)&0x20); f->Run(200000); f->device->ActivateButton(kATDeviceButton_IndusGTBootCPM,true);
	f->Run(500000); AT_PORTABLE_TEST_ASSERT(context, (f->device->ReadByte(0x1000)&0x50)==0x50); f->Run(1180000);
	// Density expires first. It must not cancel the later CP/M action.
	AT_PORTABLE_TEST_ASSERT(context, f->device->ReadByte(0x1000)&0x20); f->device->ReadByte(0x1000); AT_PORTABLE_TEST_ASSERT(context, f->device->ReadByte(0x1001)&0x20);
	f->Run(300000); f->device->ReadByte(0x1000); AT_PORTABLE_TEST_ASSERT(context, !(f->device->ReadByte(0x1001)&0x70));
	return true;
}
