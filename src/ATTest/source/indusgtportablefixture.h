// Full Indus GT, native ROM/image files, real Z80/FDC and production SIO/POKEY.
#ifndef f_ATTEST_INDUSGTPORTABLEFIXTURE_H
#define f_ATTEST_INDUSGTPORTABLEFIXTURE_H
#include <array>
#include <cmath>
#include <cstring>
#include <memory>
#include <string>
#include <vd2/system/registry.h>
#include <vd2/system/registrymemory.h>
#include <at/atcore/propertyset.h>
#include <at/atcore/consoleoutput.h>
#include <diskdriveindusgt.h>
#include <firmwaremanager.h>
#include "diskinterfaceportablefixture.h"
#include "diskdrivebaseportablefixture.h"
#include "hostdeviceportablefixture.h"
#include "siomanagerportablefixture.h"
namespace IndusGTPortable {
	using Bytes=std::vector<uint8>;
	struct RegistryScope {
		IVDRegistryProvider *previous=VDGetRegistryProvider(); VDStringA key {VDRegistryAppKey::getDefaultKey()}; VDRegistryProviderMemory provider;
		RegistryScope() { VDSetRegistryProvider(&provider); VDRegistryAppKey::setDefaultKey("IndusGTPortableTests"); }
		~RegistryScope() { VDRegistryAppKey::setDefaultKey(key.c_str()); VDSetRegistryProvider(previous); }
	};
	struct Disks final : IATDiskDriveManager {
		ATDiskInterface *GetDiskInterface(uint32 i) override { selected=i; return &slot.mDisk; }
		DiskInterfaceProbe slot; uint32 selected=99;
	};
	struct Code {
		void Emit(std::initializer_list<uint8> v) { bytes.insert(bytes.end(),v); }
		void Write(uint16 a,uint8 v) { Emit({0x3E,v,0x32,uint8(a),uint8(a>>8)}); }
		void Port(uint8 p) { Emit({0xD3,p}); }
		void Halt() { const uint16 a=uint16(bytes.size()); Emit({0xC3,uint8(a),uint8(a>>8)}); }
		Bytes bytes;
	};
	struct EdgePlayer final : IATSyncAudioEdgePlayer {
		void AddEdges(const ATSyncAudioEdge *p,size_t n,float volume) override { if(n) { edges.insert(edges.end(),p,p+n); volumes.push_back(volume); } }
		void AddEdgeBuffer(ATSyncAudioEdgeBuffer *) override { std::abort(); }
		std::vector<ATSyncAudioEdge> edges; std::vector<float> volumes;
	};
	struct Mixer final : IATAudioMixer {
		void AddSyncAudioSource(IATSyncAudioSource *p) override { if(raw) std::abort(); raw=p; }
		void RemoveSyncAudioSource(IATSyncAudioSource *p) override { if(raw!=p) std::abort(); raw=nullptr; }
		void AddAsyncAudioSource(IATAudioAsyncSource&) override { std::abort(); }
		void RemoveAsyncAudioSource(IATAudioAsyncSource&) override { std::abort(); }
		IATSyncAudioSamplePlayer& GetSamplePlayer() override { return samples.player; }
		IATSyncAudioSamplePlayer& GetEdgeSamplePlayer() override { std::abort(); }
		IATSyncAudioSamplePlayer& GetAsyncSamplePlayer() override { std::abort(); }
		IATSyncAudioEdgePlayer& GetEdgePlayer() override { return edgePlayer; }
		void AddInternalAudioTap(IATInternalAudioTap *) override { std::abort(); }
		void RemoveInternalAudioTap(IATInternalAudioTap *) override { std::abort(); }
		void BlockInternalAudio() override { std::abort(); }
		void UnblockInternalAudio() override { std::abort(); }
		void FlushRaw(uint32 start=0) { std::array<float,kATAudioMixCount> levels; levels.fill(1); ATSyncAudioMixInfo m {}; m.mStartTime=start; m.mCount=1024; m.mpMixLevels=levels.data(); raw->WriteAudio(m); }
		DriveBasePortable::Mixer samples; EdgePlayer edgePlayer; IATSyncAudioSource *raw=nullptr;
	};
	struct Machine {
		explicit Machine(uint32 id=0,uint32 size=128,bool ready=false,bool command=false) : host(std::make_unique<SIOPortableFixture>()) {
			disks.slot.mDisk.CreateDisk(720,3,size); disks.slot.mDisk.SetWriteMode(kATMediaWriteMode_VRW);
			Code code; code.Halt(); MakeROM(code.bytes); files.Write(L"INDUSGT.ROM",rom);
			ATFirmwareInfo info {}; info.mPath=files.File(L"INDUSGT.ROM"); info.mName=L"Portable Indus GT test firmware"; info.mType=kATFirmwareType_IndusGT; firmware.AddFirmware(info);
			firmware.SetDefaultFirmware(kATFirmwareType_IndusGT,ATGetFirmwareIdFromPath(info.mPath.c_str()));
			device=new ATDeviceDiskDriveIndusGT; ATPropertySet settings; settings.SetUint32("id",id); device->SetSettings(settings);
			host->mManager.SetReadyState(ready); host->mMachine.mChips.mPOKEY.SetCommandLine(command);
			device->InitScheduling(&host->mMachine.mChips.mVideo.mScheduler,&host->mMachine.mSlowScheduler); device->InitDiskDrive(&disks); device->InitSIO(&host->mManager); Firmware().InitFirmware(&firmware); device->Init(); device->PeripheralColdReset();
		}
		~Machine() { Stop(); }
		void Stop() { if(device && !closed) { device->Shutdown(); closed=true; } }
		void Run(uint32 n) { host->Tick(n); device->RunUntilSynced(); }
		void MakeROM(const Bytes& code) { rom.assign(4096,0); std::copy(code.begin(),code.end(),rom.begin()); }
		void Reload(const Bytes& bytes) { rom=bytes; files.Write(L"INDUSGT.ROM",rom); Firmware().ReloadFirmware(); }
		void Program(const Bytes& code) { MakeROM(code); Reload(rom); device->PeripheralColdReset(); }
		IATDeviceFirmware& Firmware() { return *static_cast<IATDeviceFirmware *>(device->AsInterface(IATDeviceFirmware::kTypeID)); }
		ATCPUExecState State() { ATCPUExecState s {}; device->GetExecState(s); return s; }
		void Jump(uint16 pc) { auto s=State(); s.mZ80.mPC=pc; device->SetExecState(s); }
		void Port(uint8 p) { Code code; code.Port(p); code.Halt(); constexpr uint16 base=0xA200; auto& b=code.bytes; b[b.size()-2]=uint8(base+2); b[b.size()-1]=uint8(base>>8); device->WriteMemory(base,b.data(),uint32(b.size())); Jump(base); Run(100); }
		ATFDCEmulator& FDC() { return *static_cast<ATFDCEmulator *>(device->AsInterface(ATFDCEmulator::kTypeID)); }
		IATDiskImage& Image() { return *disks.slot.mDisk.GetDiskImage(); }
		void Fill(uint32 index,const Bytes& b) { if(!Image().WriteVirtualSector(index,b.data(),uint32(b.size()))) std::abort(); }
		Bytes Stored(uint32 index) { Bytes b(Image().GetSectorSize(index)); Image().ReadVirtualSector(index,b.data(),uint32(b.size())); return b; }
		RegistryScope registry; HostDevicePortable::Sandbox files; ATFirmwareManager firmware;
		std::unique_ptr<SIOPortableFixture> host; Disks disks; Mixer mixer;
		vdrefptr<ATDeviceDiskDriveIndusGT> device; Bytes rom; bool closed=false;
	};
	inline Code Transfer(bool write,bool mfm=false) {
		Code c; c.Port(11); c.Port(mfm ? 8 : 9); c.Write(0x6001,10); c.Write(0x6002,4); c.Write(0x6000,write ? 0xA0 : 0x80);
		c.Emit({0x21,0,0x70,0x06,uint8(mfm ? 0 : 128)}); const uint8 loop=uint8(c.bytes.size()); c.Emit({0x3A,0,0x20,0xE6,0x40,0x28,0xF9});
		if(write) c.Write(0x6003,0x5A); else c.Emit({0x3A,3,0x60,0x77,0x23});
		const uint8 branch=uint8(c.bytes.size()); c.Emit({0x10,uint8(loop-branch-2)}); c.Halt(); return c;
	}
	struct Text final : ATConsoleOutput { std::string value; void WriteLine(const char *s) override { value+=s; } };
}
#endif
