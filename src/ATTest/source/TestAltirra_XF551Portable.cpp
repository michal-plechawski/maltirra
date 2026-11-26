#include "xf551portablefixture.h"
using namespace XF551Portable;

bool ATTestAltirraXF551Interfaces(ATPortableTestContext& context) {
	auto f=std::make_unique<Machine>(); auto& d=*f->device; ATDeviceInfo info {}; d.GetDeviceInfo(info);
	AT_PORTABLE_TEST_ASSERT(context, !strcmp(info.mpDef->mpTag,"diskdrivexf551") && d.AsInterface(IATDeviceScheduling::kTypeID) && d.AsInterface(IATDeviceFirmware::kTypeID) && d.AsInterface(IATDeviceDiskDrive::kTypeID) && d.AsInterface(IATDeviceSIO::kTypeID) && d.AsInterface(IATDeviceAudioOutput::kTypeID) && !d.AsInterface(0));
	AT_PORTABLE_TEST_ASSERT(context, d.GetDebugTarget(0)==static_cast<IATDebugTarget *>(&d) && !d.GetDebugTarget(1) && d.GetDisasmMode()==kATDebugDisasmMode_8048 && std::abs(d.GetDisplayCPUClock()-8333333.0)<1);
	AT_PORTABLE_TEST_ASSERT(context, f->disks.slot.mDisk.GetClientCount()==1 && d.GetDiskInterfaceClient(0).mpClient==static_cast<IATDiskInterfaceClient *>(&d) && !d.GetDiskInterfaceClient(1).mpClient);
	vdrefptr<IATDevice> created; ATPropertySet p; info.mpDef->mpFactoryFn(p,~created); ATDeviceInfo other {}; created->GetDeviceInfo(other);
	AT_PORTABLE_TEST_ASSERT(context, other.mpDef==info.mpDef);
	return true;
}

bool ATTestAltirraXF551Settings(ATPortableTestContext& context) {
	for(uint32 id=0;id<4;++id) {
		auto f=std::make_unique<Machine>(id); Code c; c.Emit({0x0A,0xAD}); c.Halt(); f->Program(c.bytes); f->Run(500);
		ATPropertySet p; f->device->GetSettings(p); VDStringW blurb; f->device->GetSettingsBlurb(blurb);
		AT_PORTABLE_TEST_ASSERT(context, f->disks.selected==id && p.GetUint32("id",99)==id && blurb.size()==3 && blurb[1]==wchar_t('1'+id) && f->State().m8048.mReg[0][5]==uint8(0x3F|(id<<6)));
		p.SetUint32("id",id+4); AT_PORTABLE_TEST_ASSERT(context, f->device->SetSettings(p)); p.SetUint32("id",(id+1)&3); AT_PORTABLE_TEST_ASSERT(context, !f->device->SetSettings(p));
	}
	return true;
}

bool ATTestAltirraXF551Firmware(ATPortableTestContext& context) {
	auto f=std::make_unique<Machine>(); AT_PORTABLE_TEST_ASSERT(context, f->device->GetFirmwareStatus()==ATDeviceFirmwareStatus::OK && !f->device->ReloadFirmware() && !f->device->GetWritableFirmwareDesc(0) && !f->device->IsWritableFirmwareDirty(0));
	Code c; c.Emit({0xBD,0x67}); c.Halt(); f->Program(c.bytes); f->Run(500);
	AT_PORTABLE_TEST_ASSERT(context, f->State().m8048.mReg[0][5]==0x67 && f->device->DebugReadByte(1)==0x67);
	VDRemoveFileEx(f->files.File(L"XF551.ROM").c_str()); AT_PORTABLE_TEST_ASSERT(context, f->device->ReloadFirmware() && f->device->GetFirmwareStatus()==ATDeviceFirmwareStatus::Missing && f->device->DebugReadByte(1)==0);
	return true;
}

bool ATTestAltirraXF551CPU(ATPortableTestContext& context) {
	auto f=std::make_unique<Machine>(); Code c; c.Emit({0x23,0x37,0xB8,0x40,0xA0,0x17,0xA9}); c.Halt(); f->Program(c.bytes); f->Run(1000);
	AT_PORTABLE_TEST_ASSERT(context, f->device->DebugReadByte(0x1040)==0x37 && f->State().m8048.mReg[0][1]==0x38 && f->State().m8048.mPC==7 && f->device->GetTimeSkew()>=-8);
	f->device->WriteByte(0x1041,0x52); AT_PORTABLE_TEST_ASSERT(context, f->device->ReadByte(0x1041)==0x52);
	f->device->WriteByte(1,0xFF); AT_PORTABLE_TEST_ASSERT(context, f->device->ReadByte(1)==0x37);
	return true;
}

bool ATTestAltirraXF551ROMBanks(ATPortableTestContext& context) {
	auto f=std::make_unique<Machine>(); Code c; c.Emit({0xBD,0x68}); c.Halt(); f->Program(c.bytes,0x800); f->Run(500);
	AT_PORTABLE_TEST_ASSERT(context, f->State().m8048.mReg[0][5]==0x68 && f->device->DebugReadByte(0x801)==0x68);
	// A two-byte instruction at 07FF wraps its operand to bank byte zero.
	f->rom.assign(4096,0); f->rom[0]=0x72; f->rom[0x7FF]=0xBD; f->rom[1]=4; f->rom[2]=1; f->files.Write(L"XF551.ROM",f->rom); f->device->ReloadFirmware(); f->device->PeripheralColdReset();
	auto s=f->State(); s.m8048.mPC=0x7FF; f->device->SetExecState(s); f->Run(500);
	AT_PORTABLE_TEST_ASSERT(context, f->State().m8048.mReg[0][5]==0x72 && f->State().m8048.mPC==1);
	return true;
}

bool ATTestAltirraXF551Registers(ATPortableTestContext& context) {
	auto f=std::make_unique<Machine>(); Code c; c.Emit({0xB9,0}); c.Write(1,0x27); c.Port(0x39); c.Emit({0x81,0xAD}); c.Port(0x19); c.Emit({0x81,0xAE}); c.Halt(); f->Program(c.bytes); f->Run(1000);
	AT_PORTABLE_TEST_ASSERT(context, f->State().m8048.mReg[0][5]==0x27 && f->State().m8048.mReg[0][6]==0xFF && f->FDC().DebugReadByte(1)==0x27);
	Code reset; reset.Port(8); reset.Halt(); f->Program(reset.bytes); f->Run(500);
	AT_PORTABLE_TEST_ASSERT(context, f->FDC().DebugReadByte(1)==0 && !f->FDC().GetDrqStatus());
	return true;
}

bool ATTestAltirraXF551Geometry(ATPortableTestContext& context) {
	auto f=std::make_unique<Machine>(); AT_PORTABLE_TEST_ASSERT(context, f->device->IsImageSupported(f->Image()));
	for(auto spec : {std::pair<uint32,uint32>{81,128},{40,512}}) {
		ATDiskGeometryInfo g {}; g.mTrackCount=spec.first; g.mSideCount=1; g.mSectorSize=uint16(spec.second); g.mSectorsPerTrack=18; g.mTotalSectorCount=spec.first*18;
		vdrefptr<IATDiskImage> image; ATCreateDiskImage(g,~image); AT_PORTABLE_TEST_ASSERT(context, !f->device->IsImageSupported(*image));
	}
	return true;
}

bool ATTestAltirraXF551Debug(ATPortableTestContext& context) {
	std::vector<bool> done; auto f=std::make_unique<Machine>(); Code c; c.Emit({0xBD,0x43}); c.Halt(); f->Program(c.bytes); f->device->SetHistoryEnabled(true);
	uint16 stopped=0; AT_PORTABLE_TEST_ASSERT(context, f->device->StepInto([&](bool ok) { done.push_back(ok); stopped=f->State().m8048.mPC; })); f->Run(1000);
	const auto range=f->device->GetHistoryRange(); const ATCPUHistoryEntry *entries[1] {};
	AT_PORTABLE_TEST_ASSERT(context, done==std::vector<bool>{true} && stopped==2 && range.second>0 && uint32(range.second-range.first)==0x20000 && f->device->ExtractHistory(entries,0,1)==1 && entries[0]->mOpcode[0]==0xBD);
	f->device->SetHistoryEnabled(false); const auto empty=f->device->GetHistoryRange(); AT_PORTABLE_TEST_ASSERT(context, empty.first==empty.second);
	return true;
}
