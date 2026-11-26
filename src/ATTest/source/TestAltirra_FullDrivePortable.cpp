#include "fullDrivePortableFixture.h"
using namespace FullDrivePortable;

bool ATTestAltirraFullDriveVariants(ATPortableTestContext& context) {
	for(uint32 i=0;i<Drive::kDeviceTypeCount;++i) {
		auto f=std::make_unique<Machine>(Type(i)); Code c; c.Write(0x180,0x67); c.Halt(); f->Program(c.bytes); f->Run(1000); ATDeviceInfo info {}; f->device->GetDeviceInfo(info);
		AT_PORTABLE_TEST_ASSERT(context, info.mpDef && f->device->ReadByte(0x180)==0x67 && f->State().m6502.mPC==0xFC05 && f->device->GetDisasmMode()==(i==Drive::kDeviceType_Speedy1050 ? kATDebugDisasmMode_65C02 : kATDebugDisasmMode_6502));
		AT_PORTABLE_TEST_ASSERT(context, f->device->AsInterface(IATDeviceFirmware::kTypeID) && f->device->AsInterface(IATDeviceDiskDrive::kTypeID) && f->device->AsInterface(IATDeviceSIO::kTypeID) && f->device->AsInterface(IATDeviceAudioOutput::kTypeID) && f->device->AsInterface(ATRIOT6532Emulator::kTypeID) && !f->device->AsInterface(0));
		AT_PORTABLE_TEST_ASSERT(context, bool(f->device->AsInterface(ATPIAEmulator::kTypeID))==(i==Drive::kDeviceType_SuperArchiver_BitWriter) && f->device->GetComputerPowerOnDelay()==(i==Drive::kDeviceType_1050Duplicator ? 10 : 0));
		vdrefptr<IATDevice> copy; ATPropertySet p; info.mpDef->mpFactoryFn(p,~copy); ATDeviceInfo other {}; copy->GetDeviceInfo(other); AT_PORTABLE_TEST_ASSERT(context, other.mpDef==info.mpDef);
	}
	return true;
}

bool ATTestAltirraFullDriveSettings(ATPortableTestContext& context) {
	for(Type type : {Drive::kDeviceType_810,Drive::kDeviceType_1050}) for(uint32 id=0;id<4;++id) {
		auto f=std::make_unique<Machine>(type,id); ATPropertySet p; f->device->GetSettings(p); VDStringW text; f->device->GetSettingsBlurb(text);
		const uint8 ids810[]={5,4,0,1},ids1050[]={3,1,0,2}; const uint8 mask=f->is1050 ? 3 : 5;
		AT_PORTABLE_TEST_ASSERT(context, p.GetUint32("id",99)==id && f->disks.selected==id && text[1]==wchar_t('1'+id) && (f->device->DebugReadByte(f->RIOT())&mask)==(f->is1050 ? ids1050[id] : ids810[id]));
		p.SetUint32("id",id+4); AT_PORTABLE_TEST_ASSERT(context, f->device->SetSettings(p)); p.SetUint32("id",(id+1)&3); AT_PORTABLE_TEST_ASSERT(context, !f->device->SetSettings(p));
	}
	auto happy=std::make_unique<Machine>(Drive::kDeviceType_Happy810); ATPropertySet p; p.SetBool("autospeed",true); p.SetFloat("autospeedrate",std::numeric_limits<float>::quiet_NaN()); happy->device->SetSettings(p); happy->device->GetSettings(p);
	AT_PORTABLE_TEST_ASSERT(context, p.GetBool("autospeed",false) && p.GetFloat("autospeedrate",0)==266);
	return true;
}

bool ATTestAltirraFullDriveFirmware(ATPortableTestContext& context) {
	auto f=std::make_unique<Machine>(); AT_PORTABLE_TEST_ASSERT(context, f->device->GetFirmwareStatus()==ATDeviceFirmwareStatus::OK && !f->device->ReloadFirmware() && !f->device->GetWritableFirmwareDesc(0) && !f->device->IsWritableFirmwareDirty(0));
	Code c; c.Emit({0xA9,0x43}); c.Halt(); f->Program(c.bytes); f->Run(1000); AT_PORTABLE_TEST_ASSERT(context, f->State().m6502.mA==0x43);
	VDRemoveFileEx(f->files.File(L"DRIVE.ROM").c_str()); AT_PORTABLE_TEST_ASSERT(context, f->device->ReloadFirmware() && f->device->GetFirmwareStatus()==ATDeviceFirmwareStatus::Missing && f->device->DebugReadByte(0xFC00)==0);
	return true;
}

bool ATTestAltirraFullDriveCompactFirmware(ATPortableTestContext& context) {
	for(uint32 size : {0xC00u,0x1000u,0x1800u,0x2000u}) {
		auto f=std::make_unique<Machine>(Drive::kDeviceType_Happy810); Bytes rom(size,0x37); if(size>=0x1800) std::fill(rom.begin()+size/2,rom.end(),0x68); f->Reload(rom);
		AT_PORTABLE_TEST_ASSERT(context, f->device->DebugReadByte(0x1400)==(size>=0x1800 ? 0x68 : 0x37));
		f->device->ReadByte(0x1FF8); AT_PORTABLE_TEST_ASSERT(context, f->device->DebugReadByte(0x1400)==0x37);
	}
	for(Type type : {Drive::kDeviceType_Speedy1050,Drive::kDeviceType_ISPlate}) {
		auto f=std::make_unique<Machine>(type); f->Reload(Bytes(type==Drive::kDeviceType_Speedy1050 ? 0x2000 : 0x1000,0x79));
		AT_PORTABLE_TEST_ASSERT(context, f->device->DebugReadByte(0xF000)==0x79);
	}
	return true;
}

bool ATTestAltirraFullDriveButtons(ATPortableTestContext& context) {
	auto f=std::make_unique<Machine>(Drive::kDeviceType_Happy1050);
	AT_PORTABLE_TEST_ASSERT(context, f->device->GetSupportedButtons()&(1u<<kATDeviceButton_HappyWPEnable)); f->device->ActivateButton(kATDeviceButton_HappyWPEnable,true);
	AT_PORTABLE_TEST_ASSERT(context, f->device->IsButtonDepressed(kATDeviceButton_HappyWPEnable) && f->FDC().GetWriteProtectOverride2()==kATFDCWPOverride_WriteProtect);
	f->device->ActivateButton(kATDeviceButton_HappyWPDisable,true); AT_PORTABLE_TEST_ASSERT(context, !f->device->IsButtonDepressed(kATDeviceButton_HappyWPEnable) && f->FDC().GetWriteProtectOverride2()==kATFDCWPOverride_WriteEnable);
	f->device->ActivateButton(kATDeviceButton_HappyWPDisable,false); f->device->ActivateButton(kATDeviceButton_HappySlow,true); AT_PORTABLE_TEST_ASSERT(context, f->device->IsButtonDepressed(kATDeviceButton_HappySlow));
	ATPropertySet settings; settings.SetBool("wpenable",true); f->device->SetSettings(settings);
	AT_PORTABLE_TEST_ASSERT(context, f->FDC().GetWriteProtectOverride2()==kATFDCWPOverride_WriteProtect);
	settings.SetBool("wpdisable",true); f->device->SetSettings(settings);
	AT_PORTABLE_TEST_ASSERT(context, f->FDC().GetWriteProtectOverride2()==kATFDCWPOverride_None && !f->device->IsButtonDepressed(kATDeviceButton_HappyWPEnable) && !f->device->IsButtonDepressed(kATDeviceButton_HappyWPDisable));
	return true;
}

bool ATTestAltirraFullDriveGeometry(ATPortableTestContext& context) {
	for(Type type : {Drive::kDeviceType_810,Drive::kDeviceType_1050,Drive::kDeviceType_810Turbo}) {
		auto f=std::make_unique<Machine>(type); AT_PORTABLE_TEST_ASSERT(context, f->device->IsImageSupported(f->Image()));
		ATDiskGeometryInfo g=f->Image().GetGeometry(); g.mSectorSize=256; g.mbMFM=true; vdrefptr<IATDiskImage> image; ATCreateDiskImage(g,~image);
		AT_PORTABLE_TEST_ASSERT(context, f->device->IsImageSupported(*image)==(type!=Drive::kDeviceType_810)); g.mSideCount=2; g.mTotalSectorCount*=2; ATCreateDiskImage(g,~image);
		AT_PORTABLE_TEST_ASSERT(context, !f->device->IsImageSupported(*image));
	}
	return true;
}

bool ATTestAltirraFullDriveDebug(ATPortableTestContext& context) {
	std::vector<bool> done; auto f=std::make_unique<Machine>(Drive::kDeviceType_Speedy1050); Code c; c.Emit({0xA9,0x43}); c.Halt(); f->Program(c.bytes); f->device->SetHistoryEnabled(true); uint16 pc=0;
	AT_PORTABLE_TEST_ASSERT(context, f->device->StepInto([&](bool ok) { done.push_back(ok); pc=f->State().m6502.mPC; })); f->Run(1000);
	const ATCPUHistoryEntry *entry[1] {}; const auto range=f->device->GetHistoryRange();
	AT_PORTABLE_TEST_ASSERT(context, done==std::vector<bool>{true} && pc==0xFC02 && range.second>0 && f->device->ExtractHistory(entry,range.second-1,1)==1 && entry[0]->mPC>=0xFC00);
	return true;
}

bool ATTestAltirraFullDriveTrace(ATPortableTestContext& context) {
	ATTraceContext trace {}; trace.mpCollection=new ATTraceCollection; auto f=std::make_unique<Machine>(); f->Motor(true); f->device->SetTraceContext(&trace); f->FDC().WriteByte(1,10); f->FDC().WriteByte(2,4); f->FDC().WriteByte(0,0x80);
	for(uint32 i=0;i<20000;++i) { f->Run(1); if(f->FDC().GetDrqStatus()) f->FDC().ReadByte(3); }
	f->device->SetTraceContext(nullptr); f->device->SetTraceContext(nullptr); AT_PORTABLE_TEST_ASSERT(context, trace.mpCollection->GetGroupCount()==1 && trace.mpCollection->GetGroup(0)->GetChannelCount()==2);
	return true;
}
