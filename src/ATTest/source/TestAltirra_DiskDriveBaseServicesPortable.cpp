#include <vd2/system/registry.h>
#include <vd2/system/registrymemory.h>
#include <firmwaremanager.h>
#include "diskdrivebaseportablefixture.h"
#include "hostdeviceportablefixture.h"
#include "siomanagerportablefixture.h"
using namespace DriveBasePortable;

bool ATTestAltirraDriveBaseChange(ATPortableTestContext& context) {
	ATScheduler s; s.SetRate(VDFraction(1000,1)); ATDiskDriveChangeHandler h; std::vector<std::optional<bool>> wp,ready;
	h.SetOutputStateFns([&](auto v) { wp.push_back(v); },[&](auto v) { ready.push_back(v); }); h.Init(&s); h.ChangeDisk();
	AT_PORTABLE_TEST_ASSERT(context, !h.GetWriteProtectState().has_value()); Tick(s,1);
	AT_PORTABLE_TEST_ASSERT(context, h.GetWriteProtectState()==false && h.GetDiskReadyState()==false); Tick(s,50);
	AT_PORTABLE_TEST_ASSERT(context, h.GetWriteProtectState()==true && h.GetDiskReadyState()==false); Tick(s,50);
	AT_PORTABLE_TEST_ASSERT(context, h.GetWriteProtectState()==false); Tick(s,50);
	AT_PORTABLE_TEST_ASSERT(context, !h.GetWriteProtectState().has_value() && !h.GetDiskReadyState().has_value());
	AT_PORTABLE_TEST_ASSERT(context, wp==(std::vector<std::optional<bool>>{false,true,false,std::nullopt}) && ready==(std::vector<std::optional<bool>>{false,std::nullopt})); h.Shutdown();
	return true;
}

bool ATTestAltirraDriveBaseChangeReset(ATPortableTestContext& context) {
	ATScheduler s; s.SetRate(VDFraction(1,1)); ATDiskDriveChangeHandler h; uint32 calls=0;
	h.SetOutputStateFns([&](auto) { ++calls; },[](auto) {}); h.Init(&s); h.ChangeDisk(); Tick(s,4);
	AT_PORTABLE_TEST_ASSERT(context, calls==4 && !h.GetDiskReadyState().has_value()); h.ChangeDisk(); Tick(s,1); h.Reset();
	const uint32 resetCalls=calls; Tick(s,100); AT_PORTABLE_TEST_ASSERT(context, calls==resetCalls && !h.GetWriteProtectState().has_value());
	h.ChangeDisk(); h.Shutdown(); Tick(s,100); AT_PORTABLE_TEST_ASSERT(context, calls==resetCalls);
	return true;
}

bool ATTestAltirraDriveBaseFirmware(ATPortableTestContext& context) {
	struct Scope {
		IVDRegistryProvider *previous=VDGetRegistryProvider(); VDStringA key {VDRegistryAppKey::getDefaultKey()}; VDRegistryProviderMemory registry;
		Scope() { VDSetRegistryProvider(&registry); VDRegistryAppKey::setDefaultKey("DriveBaseFirmwareTests"); }
		~Scope() { VDRegistryAppKey::setDefaultKey(key.c_str()); VDSetRegistryProvider(previous); }
	} scope;
	HostDevicePortable::Sandbox files; HostDevicePortable::Bytes bytes(4096); for (size_t i=0; i<bytes.size(); ++i) bytes[i]=uint8(i*17+5); files.Write(L"DRIVE.ROM",bytes);
	ATFirmwareManager manager; ATDiskDriveFirmwareControl fw; std::array<uint8,4096> rom {};
	AT_PORTABLE_TEST_ASSERT(context, !fw.ReloadFirmware() && fw.GetFirmwareStatus()==ATDeviceFirmwareStatus::Missing);
	fw.Init(rom.data(),rom.size(),kATFirmwareType_1050); fw.InitFirmware(&manager);
	AT_PORTABLE_TEST_ASSERT(context, fw.GetFirmwareStatus()==ATDeviceFirmwareStatus::Missing);
	ATFirmwareInfo info {}; info.mPath=files.File(L"DRIVE.ROM"); info.mName=L"Drive test ROM"; info.mType=kATFirmwareType_1050; manager.AddFirmware(info);
	manager.SetDefaultFirmware(kATFirmwareType_1050,ATGetFirmwareIdFromPath(info.mPath.c_str()));
	AT_PORTABLE_TEST_ASSERT(context, fw.ReloadFirmware() && std::equal(rom.begin(),rom.end(),bytes.begin()) && fw.GetFirmwareStatus()==ATDeviceFirmwareStatus::OK && !fw.ReloadFirmware());
	AT_PORTABLE_TEST_ASSERT(context, !fw.GetWritableFirmwareDesc(0) && !fw.IsWritableFirmwareDirty(0));
	VDRemoveFileEx(info.mPath.c_str()); fw.ReloadFirmware(); AT_PORTABLE_TEST_ASSERT(context, fw.GetFirmwareStatus()==ATDeviceFirmwareStatus::Missing); fw.Shutdown();
	return true;
}

bool ATTestAltirraDriveBaseAudio(ATPortableTestContext& context) {
	Mixer a,b; ATDiskDriveAudioPlayer x,y;
	x.SetRotationSoundEnabled(true); x.InitAudioOutput(&a); y.InitAudioOutput(&b);
	x.SetRotationSoundEnabled(true); x.SetRotationSoundEnabled(true); y.SetRotationSoundEnabled(true);
	const auto first=a.Render(),reference=b.Render(); AT_PORTABLE_TEST_ASSERT(context, Audible(first) && first==reference);
	x.SetRotationSoundEnabled(false); AT_PORTABLE_TEST_ASSERT(context, !Audible(a.Render()));
	x.PlayStepSound(kATAudioSampleId_DiskStep1,1,128*28); const auto delayed=a.Render();
	AT_PORTABLE_TEST_ASSERT(context, std::all_of(delayed.begin(),delayed.begin()+128,[](float v) { return v==0; }) && Audible(delayed));
	x.Shutdown(); y.Shutdown(); AT_PORTABLE_TEST_ASSERT(context, !x.GetMixer());
	return true;
}

bool ATTestAltirraDriveBaseWire(ATPortableTestContext& context) {
	auto f=std::make_unique<SIOPortableFixture>(); f->ReceiveClock(); auto& s=f->mMachine.mChips.mVideo.mScheduler;
	ATDiskDriveSerialByteTransmitQueue bytes; bytes.Init(&s,&f->mManager); bytes.AddTransmitByte(s.GetTick()+100,0xA6,93);
	AT_PORTABLE_TEST_ASSERT(context, f->Collect(2500)==std::vector<uint8>{0xA6}); bytes.Shutdown();
	ATDiskDriveSerialBitTransmitQueue bits; bits.Init(&s,&f->mManager); const uint32 rate=f->mManager.GetCyclesPerBitRecv();
	Frame(bits,s.GetTick()+100,0x5B,rate); AT_PORTABLE_TEST_ASSERT(context, f->Collect(2500)==std::vector<uint8>{0x5B}); bits.Shutdown();
	return true;
}
