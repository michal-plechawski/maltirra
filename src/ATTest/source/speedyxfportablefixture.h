// Full Speedy XF hardware, native ROMs/images and production SIO/POKEY.
#ifndef f_ATTEST_SPEEDYXFPORTABLEFIXTURE_H
#define f_ATTEST_SPEEDYXFPORTABLEFIXTURE_H
#include <array>
#include <cmath>
#include <cstring>
#include <memory>
#include <string>
#include <vd2/system/registry.h>
#include <vd2/system/registrymemory.h>
#include <at/atcore/propertyset.h>
#include <at/atcore/consoleoutput.h>
#include <diskdrivespeedyxf.h>
#include <firmwaremanager.h>
#include <trace.h>
#include "diskinterfaceportablefixture.h"
#include "diskdrivebaseportablefixture.h"
#include "hostdeviceportablefixture.h"
#include "siomanagerportablefixture.h"
namespace SpeedyXFPortable {
	using Bytes=std::vector<uint8>;
	struct RegistryScope {
		IVDRegistryProvider *previous=VDGetRegistryProvider(); VDStringA key {VDRegistryAppKey::getDefaultKey()}; VDRegistryProviderMemory provider;
		RegistryScope() { VDSetRegistryProvider(&provider); VDRegistryAppKey::setDefaultKey("SpeedyXFPortableTests"); }
		~RegistryScope() { VDRegistryAppKey::setDefaultKey(key.c_str()); VDSetRegistryProvider(previous); }
	};
	struct Disks final : IATDiskDriveManager {
		ATDiskInterface *GetDiskInterface(uint32 i) override { selected=i; return &slot.mDisk; }
		DiskInterfaceProbe slot; uint32 selected=99;
	};
	struct Code {
		void Emit(std::initializer_list<uint8> v) { bytes.insert(bytes.end(),v); }
		void Write(uint16 address,uint8 v) { Emit({0xA9,v,0x8D,uint8(address),uint8(address>>8)}); }
		void Halt() { const uint16 pc=uint16(0xFC00+bytes.size()); Emit({0x4C,uint8(pc),uint8(pc>>8)}); }
		Bytes bytes;
	};
	struct Machine {
		explicit Machine(uint32 id=0,uint32 size=128,uint32 sides=1,bool ready=false,bool command=false) : host(std::make_unique<SIOPortableFixture>()) {
			ATDiskGeometryInfo g {}; g.mSectorSize=uint16(size); g.mBootSectorCount=3; g.mTrackCount=40; g.mSideCount=uint8(sides); g.mSectorsPerTrack=18; g.mTotalSectorCount=40*18*sides; g.mbMFM=size>128;
			disks.slot.mDisk.CreateDisk(g); disks.slot.mDisk.SetWriteMode(kATMediaWriteMode_VRW);
			Code code; code.Halt(); MakeROM(code.bytes); files.Write(L"SPEEDYXF.ROM",rom);
			ATFirmwareInfo info {}; info.mPath=files.File(L"SPEEDYXF.ROM"); info.mName=L"Portable Speedy XF test firmware"; info.mType=kATFirmwareType_SpeedyXF; firmware.AddFirmware(info);
			firmware.SetDefaultFirmware(kATFirmwareType_SpeedyXF,ATGetFirmwareIdFromPath(info.mPath.c_str()));
			device=new ATDeviceDiskDriveSpeedyXF; ATPropertySet settings; settings.SetUint32("id",id); device->SetSettings(settings);
			host->mManager.SetReadyState(ready); host->mMachine.mChips.mPOKEY.SetCommandLine(command);
			device->InitScheduling(&host->mMachine.mChips.mVideo.mScheduler,&host->mMachine.mSlowScheduler); device->InitDiskDrive(&disks); device->InitSIO(&host->mManager); device->InitFirmware(&firmware); device->Init(); device->PeripheralColdReset();
		}
		~Machine() { Stop(); }
		void Stop() { if(device && !closed) { device->Shutdown(); closed=true; } }
		void Run(uint32 n) { host->Tick(n); device->RunUntilSynced(); }
		void MakeROM(const Bytes& code) { rom.assign(0x10000,0xEA); for(uint32 i=0;i<rom.size();i+=0x400) { std::copy(code.begin(),code.end(),rom.begin()+i); rom[i+0x3FC]=0; rom[i+0x3FD]=0xFC; } }
		void Reload(const Bytes& bytes) { rom=bytes; files.Write(L"SPEEDYXF.ROM",rom); device->ReloadFirmware(); }
		void Program(const Bytes& code) { MakeROM(code); Reload(rom); device->PeripheralColdReset(); }
		ATCPUExecState State() { ATCPUExecState s {}; device->GetExecState(s); return s; }
		ATFDCEmulator& FDC() { return *static_cast<ATFDCEmulator *>(device->AsInterface(ATFDCEmulator::kTypeID)); }
		IATDiskImage& Image() { return *disks.slot.mDisk.GetDiskImage(); }
		void Fill(uint32 index,const Bytes& b) { if(!Image().WriteVirtualSector(index,b.data(),uint32(b.size()))) std::abort(); }
		Bytes Stored(uint32 index) { Bytes b(Image().GetSectorSize(index)); Image().ReadVirtualSector(index,b.data(),uint32(b.size())); return b; }
		void Motor(bool on,bool mfm=false) { device->WriteByte(0x280,0); device->WriteByte(0x281,0x28); device->WriteByte(0x280,uint8((on ? 0 : 8)|(mfm ? 0 : 0x20))); }
		void Step(bool outward) { const uint8 direction=outward ? 4 : 0; device->WriteByte(0x282,direction|8); device->WriteByte(0x283,0x0C); device->WriteByte(0x282,direction); }
		RegistryScope registry; HostDevicePortable::Sandbox files; ATFirmwareManager firmware;
		std::unique_ptr<SIOPortableFixture> host; Disks disks; DriveBasePortable::Mixer mixer;
		vdrefptr<ATDeviceDiskDriveSpeedyXF> device; Bytes rom; bool closed=false;
	};
	inline Code Transfer(bool write,bool mfm=false,bool side=false) {
		Code c; c.Write(0x280,0); c.Write(0x281,0x28); if(!mfm) c.Write(0x280,0x20); c.Write(0x4004,side ? 0x10 : 0);
		c.Write(0x401,10); c.Write(0x402,4); c.Write(0x400,write ? 0xA8 : 0x88); c.Emit({0xA2,0});
		const uint8 loop=uint8(c.bytes.size()); c.Emit({0xAD,0x80,2,0x10,0xFB});
		if(write) c.Write(0x403,0x5A); else c.Emit({0xAD,3,4,0x9D,0,0x80});
		c.Emit({0xE8}); if(!mfm) c.Emit({0xE0,0x80}); const uint8 branch=uint8(c.bytes.size()); c.Emit({0xD0,uint8(loop-branch-2)}); c.Halt(); return c;
	}
	struct Text final : ATConsoleOutput { std::string value; void WriteLine(const char *s) override { value+=s; } };
}
#endif
