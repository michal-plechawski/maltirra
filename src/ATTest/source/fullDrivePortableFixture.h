// Full 810/1050 hardware, real drive CPU and native ROM/image files.
#ifndef f_ATTEST_FULLDRIVEPORTABLEFIXTURE_H
#define f_ATTEST_FULLDRIVEPORTABLEFIXTURE_H
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <memory>
#include <string>
#include <vd2/system/registry.h>
#include <vd2/system/registrymemory.h>
#include <at/atcore/propertyset.h>
#include <at/atcore/consoleoutput.h>
#include <at/atemulation/diskutils.h>
#include <diskdrivefull.h>
#include <firmwaremanager.h>
#include <trace.h>
#include "diskinterfaceportablefixture.h"
#include "diskdrivebaseportablefixture.h"
#include "hostdeviceportablefixture.h"
#include "siomanagerportablefixture.h"
namespace FullDrivePortable {
	using Drive=ATDeviceDiskDriveFull;
	using Type=Drive::DeviceType;
	using Bytes=std::vector<uint8>;
	inline bool Is1050(Type t) { return t!=Drive::kDeviceType_810 && t!=Drive::kDeviceType_Happy810 && t!=Drive::kDeviceType_810Archiver && t!=Drive::kDeviceType_810Turbo; }
	inline ATFirmwareType FirmwareType(Type t) {
		constexpr ATFirmwareType types[]={kATFirmwareType_810,kATFirmwareType_Happy810,kATFirmwareType_810Archiver,kATFirmwareType_1050,kATFirmwareType_USDoubler,kATFirmwareType_Speedy1050,kATFirmwareType_Happy1050,kATFirmwareType_SuperArchiver,kATFirmwareType_SuperArchiver,kATFirmwareType_TOMS1050,kATFirmwareType_Tygrys1050,kATFirmwareType_1050Duplicator,kATFirmwareType_1050Turbo,kATFirmwareType_1050TurboII,kATFirmwareType_ISPlate,kATFirmwareType_810Turbo};
		return types[t];
	}
	struct RegistryScope {
		IVDRegistryProvider *previous=VDGetRegistryProvider(); VDStringA key {VDRegistryAppKey::getDefaultKey()}; VDRegistryProviderMemory provider;
		RegistryScope() { VDSetRegistryProvider(&provider); VDRegistryAppKey::setDefaultKey("FullDrivePortableTests"); }
		~RegistryScope() { VDRegistryAppKey::setDefaultKey(key.c_str()); VDSetRegistryProvider(previous); }
	};
	struct Disks final : IATDiskDriveManager {
		ATDiskInterface *GetDiskInterface(uint32 i) override { selected=i; return &slot.mDisk; }
		DiskInterfaceProbe slot; uint32 selected=99;
	};
	struct Code {
		void Emit(std::initializer_list<uint8> b) { bytes.insert(bytes.end(),b); }
		void Write(uint16 addr,uint8 v) { Emit({0xA9,v,0x8D,uint8(addr),uint8(addr>>8)}); }
		void Halt() { const uint16 pc=uint16(0xFC00+bytes.size()); Emit({0x4C,uint8(pc),uint8(pc>>8)}); }
		Bytes bytes;
	};
	struct Machine {
		explicit Machine(Type t=Drive::kDeviceType_1050,uint32 id=0,uint32 size=128) : type(t),is1050(Is1050(t)),host(std::make_unique<SIOPortableFixture>()) {
			disks.slot.mDisk.CreateDisk(720,3,size); disks.slot.mDisk.SetWriteMode(kATMediaWriteMode_VRW);
			device=new Drive(is1050,type); ATPropertySet settings; settings.SetUint32("id",id); device->SetSettings(settings);
			Code c; c.Halt(); MakeROM(c.bytes); files.Write(L"DRIVE.ROM",rom);
			ATFirmwareInfo info {}; info.mPath=files.File(L"DRIVE.ROM"); info.mName=L"Portable drive test firmware"; info.mType=FirmwareType(t); firmware.AddFirmware(info);
			firmware.SetDefaultFirmware(FirmwareType(t),ATGetFirmwareIdFromPath(info.mPath.c_str()));
			device->InitScheduling(&host->mMachine.mChips.mVideo.mScheduler,&host->mMachine.mSlowScheduler); device->InitDiskDrive(&disks); device->InitSIO(&host->mManager); device->InitFirmware(&firmware); device->Init(); device->PeripheralColdReset();
		}
		~Machine() { Stop(); }
		void Stop() { if(device && !closed) { device->Shutdown(); closed=true; } }
		void Run(uint32 n) { host->Tick(n); device->RunUntilSynced(); }
		void MakeROM(const Bytes& code) {
			rom.assign(0x4000,0xEA);
			for(uint32 i=0;i<rom.size();i+=0x400) { std::copy(code.begin(),code.end(),rom.begin()+i); rom[i+0x3FC]=0; rom[i+0x3FD]=0xFC; }
		}
		void Reload(const Bytes& bytes) { rom=bytes; files.Write(L"DRIVE.ROM",rom); device->ReloadFirmware(); }
		void Program(const Bytes& code) { MakeROM(code); Reload(rom); device->PeripheralColdReset(); }
		ATCPUExecState State() { ATCPUExecState s {}; device->GetExecState(s); return s; }
		ATFDCEmulator& FDC() { return *static_cast<ATFDCEmulator *>(device->AsInterface(ATFDCEmulator::kTypeID)); }
		IATDiskImage& Image() { return *disks.slot.mDisk.GetDiskImage(); }
		uint16 RIOT() const { return is1050 ? 0x280 : 0x300; }
		uint16 FDCAddress() const { return is1050 ? 0x400 : 0; }
		void Fill(uint32 index,const Bytes& b) { if(!Image().WriteVirtualSector(index,b.data(),uint32(b.size()))) std::abort(); }
		Bytes Stored(uint32 index) { Bytes b(Image().GetSectorSize(index)); Image().ReadVirtualSector(index,b.data(),uint32(b.size())); return b; }
		void Motor(bool on,bool mfm=false) {
			if(is1050) { device->WriteByte(RIOT(),0); device->WriteByte(RIOT()+1,0x28); device->WriteByte(RIOT(),uint8((on ? 0 : 8)|(mfm ? 0 : 0x20))); }
			else { device->WriteByte(RIOT(),uint8((on ? 2 : 0)|8)); device->WriteByte(RIOT()+1,type==Drive::kDeviceType_810Turbo ? 0x0A : 2); }
		}
		void PIA(uint8 r,uint8 v) { device->WriteByte(0x200+r,v); }
		void BitWriterSetup() { PIA(2,0x38); PIA(0,0xFF); PIA(2,0x3C); PIA(0,0xFF); PIA(3,0x38); PIA(1,0x1F); PIA(3,0x3C); PIA(1,0x1F); PIA(1,0x1D); }
		void BitWriterByte(uint8 v) { PIA(0,v); PIA(1,0x1C); PIA(1,0x1D); }
		void BitWriterNext() { for(uint32 i=0;i<8;++i) { PIA(1,0x19); PIA(1,0x1D); } }
		RegistryScope registry; HostDevicePortable::Sandbox files; ATFirmwareManager firmware;
		Type type; bool is1050; std::unique_ptr<SIOPortableFixture> host; Disks disks; DriveBasePortable::Mixer mixer;
		vdrefptr<Drive> device; Bytes rom; bool closed=false;
	};
	inline Code Transfer(bool is1050,bool write,bool mfm=false) {
		Code c; const uint16 port=is1050 ? 0x280 : 0x300,fdc=is1050 ? 0x400 : 0;
		if(is1050) { c.Write(port,0); c.Write(port+1,0x28); if(!mfm) c.Write(port,0x20); }
		c.Write(fdc+1,is1050 ? 10 : uint8(~10)); c.Write(fdc+2,is1050 ? 4 : uint8(~4)); c.Write(fdc,is1050 ? (write ? 0xA0 : 0x80) : uint8(~(write ? 0xA8 : 0x80)));
		c.Emit({0xA2,0}); const uint8 loop=uint8(c.bytes.size()); c.Emit({0xAD,uint8(port),uint8(port>>8),0x10,0xFB});
		if(write) c.Write(fdc+3,is1050 ? 0x5A : 0xA5);
		else c.Emit({0xAD,uint8(fdc+3),uint8((fdc+3)>>8),0x95,0x80});
		c.Emit({0xE8}); if(!mfm) c.Emit({0xE0,0x80});
		const uint8 branch=uint8(c.bytes.size()); c.Emit({0xD0,uint8(loop-branch-2)}); c.Halt(); return c;
	}
}
#endif
