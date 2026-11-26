#ifndef f_ATTEST_ATR8000PORTABLEFIXTURE_H
#define f_ATTEST_ATR8000PORTABLEFIXTURE_H
#include <deque>
#include <diskdriveatr8000.h>
#include "percomportablefixture.h"
namespace ATR8000Portable {
	using AMDCPortable::Bytes;
	using AMDCPortable::Text;
	struct Config : AMDCPortable::Config { std::array<uint32,4> drives {1,1,1,2}; bool command=false; };
	struct Code {
		void Emit(std::initializer_list<uint8> v) { bytes.insert(bytes.end(),v); }
		void Write(uint16 a,uint8 v) { Emit({0x3E,v,0x32,uint8(a),uint8(a>>8)}); }
		void Out(uint8 p,uint8 v) { Emit({0x3E,v,0xD3,p}); }
		void Halt() { const uint16 a=uint16(bytes.size()); Emit({0xC3,uint8(a),uint8(a>>8)}); }
		Bytes bytes;
	};
	struct Serial final : ATDeviceT<IATDeviceSerial> {
		void GetDeviceInfo(ATDeviceInfo& info) override { info={}; }
		void SetOnStatusChange(const vdfunction<void(const ATDeviceSerialStatus&)>& fn) override { onStatus=fn; }
		void SetTerminalState(const ATDeviceSerialTerminalState& state) override { controls.push_back(state); }
		ATDeviceSerialStatus GetStatus() override { return status; }
		void SetOnReadReady(vdfunction<void()> fn) override { onRead=std::move(fn); }
		bool Read(uint32 baud,uint8& c,bool& fe) override { uint32 actual=0; fe=false; return Read(actual,c) && baud==actual; }
		bool Read(uint32& baud,uint8& c) override { if(receive.empty()) return false; baud=receive.front().first; c=receive.front().second; receive.pop_front(); return true; }
		void Write(uint32 baud,uint8 c) override { sent.push_back({baud,c}); }
		void FlushBuffers() override { receive.clear(); }
		std::deque<std::pair<uint32,uint8>> receive; std::vector<std::pair<uint32,uint8>> sent; std::vector<ATDeviceSerialTerminalState> controls;
		ATDeviceSerialStatus status {}; vdfunction<void(const ATDeviceSerialStatus&)> onStatus; vdfunction<void()> onRead;
	};
	struct Machine {
		explicit Machine(const Config& c=Config{}) : config(c),host(std::make_unique<SIOPortableFixture>()),disks(c) {
			Code code; code.Halt(); MakeROM(code.bytes); files.Write(L"ATR8000.ROM",rom); ATFirmwareInfo info {}; info.mPath=files.File(L"ATR8000.ROM"); info.mName=L"Portable ATR8000 test firmware"; info.mType=kATFirmwareType_ATR8000; firmware.AddFirmware(info); firmware.SetDefaultFirmware(info.mType,ATGetFirmwareIdFromPath(info.mPath.c_str()));
			host->mMachine.mChips.mPOKEY.SetCommandLine(c.command);
			device=new ATDeviceDiskDriveATR8000; device->SetManager(&services); ATPropertySet p; for(uint32 i=0;i<4;++i) { VDStringA key; key.sprintf("drivetype%u",i); p.SetUint32(key.c_str(),c.drives[i]); } device->SetSettings(p);
			device->InitScheduling(&host->mMachine.mChips.mVideo.mScheduler,&host->mMachine.mSlowScheduler); device->InitDiskDrive(&disks); device->InitSIO(&host->mManager); Firmware().InitFirmware(&firmware); device->Init(); device->PeripheralColdReset();
		}
		~Machine() { Stop(); }
		void Stop() { if(device && !closed) { device->Shutdown(); closed=true; } }
		void Run(uint32 n) { host->Tick(n); device->RunUntilSynced(); }
		void MakeROM(const Bytes& code) { rom.assign(4096,0); std::copy(code.begin(),code.end(),rom.begin()); }
		void Reload() { files.Write(L"ATR8000.ROM",rom); Firmware().ReloadFirmware(); }
		void Program(const Bytes& code) { MakeROM(code); Reload(); device->PeripheralColdReset(); }
		void Handler(uint16 address,const Bytes& code) { std::copy(code.begin(),code.end(),rom.begin()+address); Reload(); device->PeripheralColdReset(); }
		IATDeviceFirmware& Firmware() { return *static_cast<IATDeviceFirmware *>(device->AsInterface(IATDeviceFirmware::kTypeID)); }
		ATCPUExecState State() { ATCPUExecState s {}; device->GetExecState(s); return s; }
		void Jump(uint16 pc) { auto s=State(); s.mZ80.mPC=pc; device->SetExecState(s); }
		void Out(uint8 p,uint8 v) { constexpr uint16 base=0xF200; const uint8 code[]={0x3E,v,0xD3,p,0xC3,4,0xF2}; device->WriteMemory(base,code,sizeof code); Jump(base); Run(100); }
		uint8 In(uint8 p) { constexpr uint16 base=0xF200; const uint8 code[]={0xDB,p,0x32,0,0xF3,0xC3,5,0xF2}; device->WriteMemory(base,code,sizeof code); Jump(base); Run(100); return device->ReadByte(0xF300); }
		ATFDCEmulator& FDC() { return *static_cast<ATFDCEmulator *>(device->AsInterface(ATFDCEmulator::kTypeID)); }
		ATCTCEmulator& CTC() { return *static_cast<ATCTCEmulator *>(device->AsInterface(ATCTCEmulator::kTypeID)); }
		DiskInterfaceProbe& Slot(uint32 physical=0) { return disks.Slot(physical); }
		IATDiskImage& Image(uint32 physical=0) { return *Slot(physical).mDisk.GetDiskImage(); }
		void Fill(uint32 sector,const Bytes& b,uint32 physical=0) { if(!Image(physical).WriteVirtualSector(sector,b.data(),uint32(b.size()))) std::abort(); }
		Bytes Stored(uint32 sector,uint32 physical=0) { Bytes b(Image(physical).GetSectorSize(sector)); Image(physical).ReadVirtualSector(sector,b.data(),uint32(b.size())); return b; }
		void Select(uint8 mask,bool mfm=false,bool side=false,bool fast=false) { Out(0x30,uint8(mask|(mfm ? 0 : 0x80)|(side ? 0x20 : 0)|(fast ? 0 : 0x40))); }
		bool WaitIRQ(uint32 limit=2000000) { Run(4); while(limit-- && !FDC().GetIrqStatus()) Run(1); return FDC().GetIrqStatus(); }
		Code Transfer(bool write,uint8 drive=0,bool mfm=false,bool side=false) const {
			Code c; c.Out(0x30,uint8((1<<drive)|(mfm ? 0 : 0x80)|(side ? 0x20 : 0)|0x40)); c.Out(0x41,1); c.Out(0x42,4); c.Out(0x40,write ? 0xA0 : 0x80); c.Emit({0x21,0,0x80,0x06,uint8(mfm ? 0 : 128)}); const uint8 loop=uint8(c.bytes.size()); c.Emit({0xDB,0x40,0xE6,2,0x28,0xFA});
			if(write) c.Out(0x43,0x5A); else c.Emit({0xDB,0x43,0x77,0x23}); const uint8 branch=uint8(c.bytes.size()); c.Emit({0x10,uint8(loop-branch-2)}); c.Halt(); return c;
		}
		AMDCPortable::RegistryScope registry; HostDevicePortable::Sandbox files; ATFirmwareManager firmware; Config config; PercomPortable::Services services;
		std::unique_ptr<SIOPortableFixture> host; AMDCPortable::Disks disks; DriveBasePortable::Mixer mixer; vdrefptr<ATDeviceDiskDriveATR8000> device; Bytes rom; bool closed=false;
	};
}
#endif
