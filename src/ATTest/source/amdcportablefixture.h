// Complete AMDC controller, real 6809/FDC/ACIA and four native disk slots.
#ifndef f_ATTEST_AMDCPORTABLEFIXTURE_H
#define f_ATTEST_AMDCPORTABLEFIXTURE_H
#include <array>
#include <cmath>
#include <cstring>
#include <memory>
#include <string>
#include <vd2/system/registry.h>
#include <vd2/system/registrymemory.h>
#include <at/atcore/propertyset.h>
#include <at/atcore/consoleoutput.h>
#include <diskdriveamdc.h>
#include <firmwaremanager.h>
#include "diskinterfaceportablefixture.h"
#include "diskdrivebaseportablefixture.h"
#include "hostdeviceportablefixture.h"
#include "siomanagerportablefixture.h"
namespace AMDCPortable {
	using Bytes=std::vector<uint8>;
	struct Config { uint32 switches=0,size=128,tracks=40,sides=1; bool drive2=true; uint32 ext0=1,ext1=2; };
	struct RegistryScope {
		IVDRegistryProvider *previous=VDGetRegistryProvider(); VDStringA key {VDRegistryAppKey::getDefaultKey()}; VDRegistryProviderMemory provider;
		RegistryScope() { VDSetRegistryProvider(&provider); VDRegistryAppKey::setDefaultKey("AMDCPortableTests"); }
		~RegistryScope() { VDRegistryAppKey::setDefaultKey(key.c_str()); VDSetRegistryProvider(previous); }
	};
	struct Disks final : IATDiskDriveManager {
		explicit Disks(const Config& c) : config(c) {}
		DiskInterfaceProbe& Slot(uint32 i) {
			if(!slots.at(i)) {
				slots[i]=std::make_unique<DiskInterfaceProbe>(i); ATDiskGeometryInfo g {}; g.mSectorSize=uint16(config.size); g.mBootSectorCount=3; g.mTrackCount=config.tracks; g.mSideCount=uint8(config.sides); g.mSectorsPerTrack=18; g.mTotalSectorCount=config.tracks*config.sides*18; g.mbMFM=config.size>128;
				slots[i]->mDisk.CreateDisk(g); slots[i]->mDisk.SetWriteMode(kATMediaWriteMode_VRW);
			}
			return *slots[i];
		}
		ATDiskInterface *GetDiskInterface(uint32 i) override { requested.push_back(i); return &Slot(i).mDisk; }
		Config config; std::array<std::unique_ptr<DiskInterfaceProbe>,16> slots; std::vector<uint32> requested;
	};
	struct Code {
		void Emit(std::initializer_list<uint8> v) { bytes.insert(bytes.end(),v); }
		void Write(uint16 address,uint8 value) { Emit({0x86,value,0xB7,uint8(address>>8),uint8(address)}); }
		void Halt() { Emit({0x20,0xFE}); }
		Bytes bytes;
	};
	struct Machine {
		explicit Machine(const Config& c=Config{}) : config(c),host(std::make_unique<SIOPortableFixture>()),disks(c) {
			Code code; code.Halt(); MakeROM(code.bytes); files.Write(L"AMDC.ROM",rom);
			ATFirmwareInfo info {}; info.mPath=files.File(L"AMDC.ROM"); info.mName=L"Portable AMDC test firmware"; info.mType=kATFirmwareType_AMDC; firmware.AddFirmware(info);
			firmware.SetDefaultFirmware(kATFirmwareType_AMDC,ATGetFirmwareIdFromPath(info.mPath.c_str()));
			device=new ATDeviceDiskDriveAMDC; ATPropertySet settings; settings.SetBool("drive2",c.drive2); settings.SetUint32("extdrive0",c.ext0); settings.SetUint32("extdrive1",c.ext1); settings.SetUint32("switches",c.switches); device->SetSettings(settings);
			device->InitScheduling(&host->mMachine.mChips.mVideo.mScheduler,&host->mMachine.mSlowScheduler); device->InitDiskDrive(&disks); device->InitSIO(&host->mManager); Firmware().InitFirmware(&firmware); device->Init(); device->PeripheralColdReset();
		}
		~Machine() { Stop(); }
		void Stop() { if(device && !closed) { device->Shutdown(); closed=true; } }
		void Run(uint32 n) { host->Tick(n); device->RunUntilSynced(); }
		void MakeROM(const Bytes& code) { rom.assign(4096,0x12); std::copy(code.begin(),code.end(),rom.begin()+0xC00); Vector(0xFFE,0xFC00); }
		void Vector(uint16 offset,uint16 address) { rom.at(offset)=uint8(address>>8); rom.at(offset+1)=uint8(address); }
		void Reload() { files.Write(L"AMDC.ROM",rom); Firmware().ReloadFirmware(); }
		void Program(const Bytes& code) { MakeROM(code); Reload(); device->PeripheralColdReset(); }
		void InstallHandler(uint16 offset,const Bytes& code,uint16 vectorOffset) { std::copy(code.begin(),code.end(),rom.begin()+offset); Vector(vectorOffset,uint16(0xF000+offset)); Reload(); device->PeripheralColdReset(); }
		IATDeviceFirmware& Firmware() { return *static_cast<IATDeviceFirmware *>(device->AsInterface(IATDeviceFirmware::kTypeID)); }
		ATCPUExecState State() { ATCPUExecState s {}; device->GetExecState(s); return s; }
		ATFDCEmulator& FDC() { return *static_cast<ATFDCEmulator *>(device->AsInterface(ATFDCEmulator::kTypeID)); }
		uint32 Base() const { return (config.switches>>4)&3; }
		DiskInterfaceProbe& Slot(uint32 ordinal=0) { return disks.Slot(Base()+ordinal); }
		IATDiskImage& Image(uint32 ordinal=0) { return *Slot(ordinal).mDisk.GetDiskImage(); }
		void Fill(uint32 sector,const Bytes& b,uint32 ordinal=0) { if(!Image(ordinal).WriteVirtualSector(sector,b.data(),uint32(b.size()))) std::abort(); }
		Bytes Stored(uint32 sector,uint32 ordinal=0) { Bytes b(Image(ordinal).GetSectorSize(sector)); Image(ordinal).ReadVirtualSector(sector,b.data(),uint32(b.size())); return b; }
		void Select(uint8 physical,bool mfm=false,bool fast=false) { device->WriteByte(0x6000,uint8(0x10|physical|(mfm ? 0 : 0x80)|(fast ? 8 : 0))); }
		bool WaitIRQ(uint32 limit=500000) { while(limit-- && !FDC().GetIrqStatus()) Run(1); return FDC().GetIrqStatus(); }
		RegistryScope registry; HostDevicePortable::Sandbox files; ATFirmwareManager firmware; Config config;
		std::unique_ptr<SIOPortableFixture> host; Disks disks; DriveBasePortable::Mixer mixer;
		vdrefptr<ATDeviceDiskDriveAMDC> device; Bytes rom; bool closed=false;
	};
	inline Code Transfer(bool write,uint8 drive=0,bool mfm=false,bool side=false) {
		Code c; c.Write(0x6000,uint8(0x10|drive|(mfm ? 0 : 0x80))); c.Write(0x4001,10); c.Write(0x4002,4); c.Write(0x4000,uint8((write ? 0xA0 : 0x80)|(side ? 2 : 0)));
		c.Emit({0x8E,0,0x80,0xC6,uint8(mfm ? 0 : 128)}); const uint8 loop=uint8(c.bytes.size()); c.Emit({0xB6,0x40,0,0x85,2,0x27,0xF9});
		if(write) c.Write(0x4003,0x5A); else c.Emit({0xB6,0x40,3,0xA7,0x80});
		c.Emit({0x5A}); const uint8 branch=uint8(c.bytes.size()); c.Emit({0x26,uint8(loop-branch-2)}); c.Halt(); return c;
	}
	struct Text final : ATConsoleOutput { std::string value; void WriteLine(const char *s) override { value+=s; } };
}
#endif
