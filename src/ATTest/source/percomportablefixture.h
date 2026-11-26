// Real Percom hardware with native test ROMs and production host peripherals.
#ifndef f_ATTEST_PERCOMPORTABLEFIXTURE_H
#define f_ATTEST_PERCOMPORTABLEFIXTURE_H
#include <diskdrivepercom.h>
#include <printeroutput.h>
#include "amdcportablefixture.h"
namespace PercomPortable {
	using AMDCPortable::Bytes;
	using AMDCPortable::Code;
	using AMDCPortable::Text;
	using Hardware=ATDeviceDiskDrivePercom::HardwareType;
	constexpr Hardware variants[]={Hardware::RFD,Hardware::AT88,Hardware::AT88SPD};
	struct Config : AMDCPortable::Config { Hardware type=Hardware::RFD; std::array<uint32,4> drives {1,1,1,2}; uint32 id=0; bool dd=true,use1795=false; };
	struct Services final : IATDeviceManager {
		void *GetService(uint32 iid) override { return iid==IATPrinterOutputManager::kTypeID ? static_cast<IATPrinterOutputManager *>(&printers) : nullptr; }
		void NotifyDeviceStatusChanged(IATDevice&) override {}
		ATPrinterOutputManager printers;
	};
	struct Printer final : ATDevice, IATPrinterOutput {
		int AddRef() override { return ATDevice::AddRef(); }
		int Release() override { return ATDevice::Release(); }
		void GetDeviceInfo(ATDeviceInfo& info) override { info={}; }
		void *AsInterface(uint32 iid) override { return iid==IATPrinterOutput::kTypeID ? static_cast<IATPrinterOutput *>(this) : ATDevice::AsInterface(iid); }
		bool WantUnicode() const override { return false; }
		void WriteRaw(const uint8 *p,size_t n) override { bytes.insert(bytes.end(),p,p+n); }
		Bytes bytes;
	};
	struct Machine {
		explicit Machine(const Config& c=Config{}) : config(c),host(std::make_unique<SIOPortableFixture>()),disks(c) {
			Code code; code.Halt(); MakeROM(code.bytes); files.Write(L"PERCOM.ROM",rom); ATFirmwareInfo info {}; info.mPath=files.File(L"PERCOM.ROM"); info.mName=L"Portable Percom test firmware"; info.mType=FirmwareType(); firmware.AddFirmware(info); firmware.SetDefaultFirmware(info.mType,ATGetFirmwareIdFromPath(info.mPath.c_str()));
			device=new ATDeviceDiskDrivePercom(c.type); device->SetManager(&services); ATPropertySet p; p.SetUint32("id",c.id); p.SetBool("ddcapable",c.dd); p.SetBool("use1795",c.use1795); for(uint32 i=0;i<4;++i) { VDStringA key; key.sprintf("drivetype%u",i); p.SetUint32(key.c_str(),c.drives[i]); } device->SetSettings(p);
			device->InitScheduling(&host->mMachine.mChips.mVideo.mScheduler,&host->mMachine.mSlowScheduler); device->InitDiskDrive(&disks); device->InitSIO(&host->mManager); Firmware().InitFirmware(&firmware); device->Init(); device->PeripheralColdReset();
		}
		~Machine() { Stop(); }
		ATFirmwareType FirmwareType() const { return config.type==Hardware::RFD ? kATFirmwareType_Percom : config.type==Hardware::AT88 ? kATFirmwareType_PercomAT : kATFirmwareType_PercomATSPD; }
		uint16 RAM() const { return config.type==Hardware::RFD ? 0xDC00 : 0x1000; }
		uint16 FDC() const { return config.type==Hardware::RFD ? 0xD010 : 4; }
		uint16 ACIA() const { return config.type==Hardware::RFD ? 0xD030 : 12; }
		uint32 Base() const { return config.type==Hardware::RFD ? config.id&7 : 0; }
		void Stop() { if(device && !closed) { device->Shutdown(); closed=true; } }
		void Run(uint32 n) { host->Tick(n); device->RunUntilSynced(); }
		void MakeROM(const Bytes& code) { rom.assign(config.type==Hardware::AT88SPD ? 4096 : 2048,0x12); std::copy(code.begin(),code.end(),rom.begin()+rom.size()-1024); rom[rom.size()-0x310]=0x3B; for(uint16 v:{0xFFF6,0xFFF8,0xFFFC}) Vector(v,0xFCF0); Vector(0xFFFE,0xFC00); }
		void Vector(uint16 address,uint16 target) { rom.at(address&(rom.size()-1))=uint8(target>>8); rom.at((address+1)&(rom.size()-1))=uint8(target); }
		void Reload() { files.Write(L"PERCOM.ROM",rom); Firmware().ReloadFirmware(); }
		void Program(const Bytes& code) { MakeROM(code); Reload(); device->PeripheralColdReset(); }
		void Handler(uint16 address,const Bytes& code,uint16 vector) { std::copy(code.begin(),code.end(),rom.begin()+(address&(rom.size()-1))); Vector(vector,address); Reload(); device->PeripheralColdReset(); }
		IATDeviceFirmware& Firmware() { return *static_cast<IATDeviceFirmware *>(device->AsInterface(IATDeviceFirmware::kTypeID)); }
		ATCPUExecState State() { ATCPUExecState s {}; device->GetExecState(s); return s; }
		ATFDCEmulator& Chip() { return *static_cast<ATFDCEmulator *>(device->AsInterface(ATFDCEmulator::kTypeID)); }
		DiskInterfaceProbe& Slot(uint32 physical=0) { return disks.Slot(Base()+physical); }
		IATDiskImage& Image(uint32 physical=0) { return *Slot(physical).mDisk.GetDiskImage(); }
		void Fill(uint32 sector,const Bytes& b,uint32 physical=0) { if(!Image(physical).WriteVirtualSector(sector,b.data(),uint32(b.size()))) std::abort(); }
		Bytes Stored(uint32 sector,uint32 physical=0) { Bytes b(Image(physical).GetSectorSize(sector)); Image(physical).ReadVirtualSector(sector,b.data(),uint32(b.size())); return b; }
		void PortB(uint8 value) { device->WriteByte(11,0); device->WriteByte(10,0xFF); device->WriteByte(11,4); device->WriteByte(10,value); }
		void EmitSelect(Code& c,uint8 drive=0,bool mfm=false,bool side=false) const {
			if(config.type==Hardware::RFD) { c.Write(0xD014,uint8(8|(drive<<1)|(side ? 1 : 0))); c.Write(0xD018,mfm ? 4 : 0); }
			else { c.Write(11,0); c.Write(10,0xFF); c.Write(11,4); static const uint8 order[]={0,2,1,3}; c.Write(10,config.type==Hardware::AT88 ? uint8(0x10|(1<<drive)|(mfm ? 0 : 0x40)|(side ? 0x20 : 0)) : uint8(4|order[drive]|(mfm ? 0 : 0x10)|(side ? 8 : 0))); }
		}
		void Select(uint8 drive=0,bool mfm=false,bool side=false) { Code c; EmitSelect(c,drive,mfm,side); for(size_t i=0;i<c.bytes.size();i+=5) device->WriteByte(uint16(c.bytes[i+3]*256+c.bytes[i+4]),c.bytes[i+1]); }
		void WriteFDC(uint8 reg,uint8 value) { device->WriteByte(uint16(FDC()+reg),uint8(~value)); }
		uint8 Status() { return uint8(~device->DebugReadByte(FDC())); }
		bool WaitIRQ(uint32 limit=2000000) { Run(4); while(limit-- && (Status()&1)) Run(1); return !(Status()&1); }
		Code Transfer(bool write,uint8 drive=0,bool mfm=false,bool side=false) const {
			Code c; EmitSelect(c,drive,mfm,side); c.Write(uint16(FDC()+1),uint8(~10)); c.Write(uint16(FDC()+2),uint8(~4)); c.Write(FDC(),uint8(~((write ? 0xA0 : 0x80)|(write && config.type==Hardware::AT88 && !mfm ? 8 : 0)|(side && config.use1795 ? 2 : 0))));
			c.Emit({0x8E,uint8(RAM()>>8),0x80,0xC6,uint8(mfm ? 0 : 128)}); const uint8 loop=uint8(c.bytes.size()); c.Emit({0xB6,uint8(FDC()>>8),uint8(FDC()),0x85,2,0x26,0xF9});
			if(write) c.Write(uint16(FDC()+3),0x5A); else c.Emit({0xB6,uint8(FDC()>>8),uint8(FDC()+3),0xA7,0x80}); c.Emit({0x5A}); const uint8 branch=uint8(c.bytes.size()); c.Emit({0x26,uint8(loop-branch-2)}); c.Halt(); return c;
		}
		AMDCPortable::RegistryScope registry; HostDevicePortable::Sandbox files; ATFirmwareManager firmware; Config config; Services services;
		std::unique_ptr<SIOPortableFixture> host; AMDCPortable::Disks disks; DriveBasePortable::Mixer mixer; vdrefptr<ATDeviceDiskDrivePercom> device; Bytes rom; bool closed=false;
	};
}
#endif
