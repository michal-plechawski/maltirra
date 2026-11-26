// Full XF551, production 8048/FDC, native ROM files and the real SIO/POKEY bus.
#ifndef f_ATTEST_XF551PORTABLEFIXTURE_H
#define f_ATTEST_XF551PORTABLEFIXTURE_H
#include <cstring>
#include <vd2/system/registry.h>
#include <vd2/system/registrymemory.h>
#include <at/atcore/propertyset.h>
#include <firmwaremanager.h>
#include <diskdrivexf551.h>
#include "diskinterfaceportablefixture.h"
#include "diskdrivebaseportablefixture.h"
#include "hostdeviceportablefixture.h"
#include "siomanagerportablefixture.h"
namespace XF551Portable {
	using Bytes=std::vector<uint8>;
	struct RegistryScope {
		IVDRegistryProvider *previous=VDGetRegistryProvider(); VDStringA key {VDRegistryAppKey::getDefaultKey()}; VDRegistryProviderMemory provider;
		RegistryScope() { VDSetRegistryProvider(&provider); VDRegistryAppKey::setDefaultKey("XF551PortableTests"); }
		~RegistryScope() { VDRegistryAppKey::setDefaultKey(key.c_str()); VDSetRegistryProvider(previous); }
	};
	struct Disks final : IATDiskDriveManager {
		ATDiskInterface *GetDiskInterface(uint32 i) override { selected=i; return &slot.mDisk; }
		DiskInterfaceProbe slot; uint32 selected=99;
	};
	struct Code {
		void Emit(std::initializer_list<uint8> v) { bytes.insert(bytes.end(),v); }
		void Port(uint8 value) { Emit({0x23,value,0x39}); } // MOV A,#value; OUTL P1,A
		void Write(uint8 reg,uint8 value,bool mfm=false,bool side=false) { Port(uint8(0x10|(mfm ? 0 : 8)|(side ? 0x40 : 0)|reg)); Emit({0x23,value,0x91}); }
		void Jump(uint16 pc) { Emit({uint8(0x04|((pc>>3)&0xE0)),uint8(pc)}); }
		void Halt() { Jump(uint16(bytes.size())); }
		Bytes bytes;
	};
	struct Machine {
		explicit Machine(uint32 id=0,uint32 size=128,uint32 sides=1) : host(std::make_unique<SIOPortableFixture>()) {
			ATDiskGeometryInfo g {}; g.mSectorSize=uint16(size); g.mBootSectorCount=3; g.mTrackCount=40; g.mSideCount=uint8(sides); g.mSectorsPerTrack=18; g.mTotalSectorCount=40*18*sides; g.mbMFM=size>128;
			disks.slot.mDisk.CreateDisk(g); disks.slot.mDisk.SetWriteMode(kATMediaWriteMode_VRW);
			rom.assign(4096,0); rom[0]=4; files.Write(L"XF551.ROM",rom);
			ATFirmwareInfo info {}; info.mPath=files.File(L"XF551.ROM"); info.mName=L"Portable XF551 test firmware"; info.mType=kATFirmwareType_XF551; firmware.AddFirmware(info);
			firmware.SetDefaultFirmware(kATFirmwareType_XF551,ATGetFirmwareIdFromPath(info.mPath.c_str()));
			device=new ATDeviceDiskDriveXF551; ATPropertySet settings; settings.SetUint32("id",id); device->SetSettings(settings);
			device->InitScheduling(&host->mMachine.mChips.mVideo.mScheduler,&host->mMachine.mSlowScheduler);
			device->InitDiskDrive(&disks); device->InitSIO(&host->mManager); device->InitFirmware(&firmware);
			device->Init(); device->PeripheralColdReset();
		}
		~Machine() { Stop(); }
		void Stop() { if(device && !closed) { device->Shutdown(); closed=true; } }
		void Run(uint32 n) { host->Tick(n); device->RunUntilSynced(); }
		void Program(const Bytes& code,uint16 entry=0) {
			rom.assign(4096,0); rom[0]=4; std::copy(code.begin(),code.end(),rom.begin()+entry); files.Write(L"XF551.ROM",rom); device->ReloadFirmware(); device->PeripheralColdReset();
			if(entry) { auto s=State(); s.m8048.mPC=entry; device->SetExecState(s); }
		}
		ATCPUExecState State() { ATCPUExecState s {}; device->GetExecState(s); return s; }
		ATFDCEmulator& FDC() { return *static_cast<ATFDCEmulator *>(device->AsInterface(ATFDCEmulator::kTypeID)); }
		IATDiskImage& Image() { return *disks.slot.mDisk.GetDiskImage(); }
		void Restore() { FDC().SetDensity(false); FDC().WriteByte(0,0x08); Run(300000); FDC().ReadByte(0); }
		void Fill(uint32 index,const Bytes& b) { if(!Image().WriteVirtualSector(index,b.data(),uint32(b.size()))) std::abort(); }
		Bytes Stored(uint32 index) { Bytes b(Image().GetSectorSize(index)); Image().ReadVirtualSector(index,b.data(),uint32(b.size())); return b; }
		RegistryScope registry; HostDevicePortable::Sandbox files; ATFirmwareManager firmware;
		std::unique_ptr<SIOPortableFixture> host; Disks disks; DriveBasePortable::Mixer mixer;
		vdrefptr<ATDeviceDiskDriveXF551> device; Bytes rom; bool closed=false;
	};
	inline Code Transfer(bool write,bool mfm=false,bool side=false,uint8 sector=4) {
		Code c; c.Emit({0xB9,0}); c.Write(0,0x08); c.Port(0x38);
		const uint8 restore=uint8(c.bytes.size()); c.Emit({0x81,0x53,1,0x96,restore});
		c.Write(1,0,mfm,side); c.Write(2,sector,mfm,side); c.Write(0,write ? 0xA8 : 0x88,mfm,side);
		c.Port(uint8((write ? 0x13 : 0x33)|(mfm ? 0 : 8)|(side ? 0x40 : 0)));
		c.Emit({0xB8,0x40,0xBA,uint8(mfm ? 0 : 128)}); const uint8 loop=uint8(c.bytes.size());
		c.Emit({0x56,uint8(loop+4)}); c.Jump(loop);
		if(write) c.Emit({0x23,0x5A,0x91}); else c.Emit({0x81,0xA0,0x18});
		c.Emit({0xEA,loop}); c.Halt(); return c;
	}
}
#endif
