// Portable integration tests for the SlightSID device wrapper.

#include <memory>
#include <string>

#include <at/atcore/consoleoutput.h>
#include <at/atcore/device.h>
#include <at/atcore/deviceimpl.h>
#include <at/atcore/propertyset.h>
#include <at/attest/portabletest.h>
#include "covoxportablefixture.h"
#include "maincpuportablefixture.h"
#include <slightsid.h>

void ATCreateDeviceSlightSID(const ATPropertySet&, IATDevice **);

namespace {
	struct DeviceLease {
		vdrefptr<IATDevice> mpDevice;
		~DeviceLease() {
			if (mpDevice) {
				mpDevice->Shutdown();
				mpDevice->SetManager(nullptr);
			}
		}
	};

	class ConsoleCapture final : public ATConsoleOutput {
	public:
		void WriteLine(const char *line) override { mLines.emplace_back(line); }
		std::vector<std::string> mLines;
	};

	void WireDevice(IATDevice& device, CovoxPortableHost& host) {
		auto *memmap=static_cast<IATDeviceMemMap *>(device.AsInterface(IATDeviceMemMap::kTypeID));
		auto *scheduling=static_cast<IATDeviceScheduling *>(device.AsInterface(IATDeviceScheduling::kTypeID));
		auto *audio=static_cast<IATDeviceAudioOutput *>(device.AsInterface(IATDeviceAudioOutput::kTypeID));
		memmap->InitMemMap(&host.mMemory);
		scheduling->InitScheduling(&host.mScheduler,&host.mScheduler);
		audio->InitAudioOutput(&host.mAudio);
	}
}

bool ATTestAltirraSlightSIDDevice(ATPortableTestContext& context) {
	auto host=std::make_unique<CovoxPortableHost>();
	ATPropertySet settings;
	DeviceLease lease;
	ATCreateDeviceSlightSID(settings,~lease.mpDevice);
	AT_PORTABLE_TEST_ASSERT(context, !!lease.mpDevice);

	ATDeviceInfo info {};
	lease.mpDevice->GetDeviceInfo(info);
	AT_PORTABLE_TEST_ASSERT(context, info.mpDef && std::string(info.mpDef->mpTag)=="slightsid");
	auto *memmap=static_cast<IATDeviceMemMap *>(lease.mpDevice->AsInterface(IATDeviceMemMap::kTypeID));
	auto *diagnostics=static_cast<IATDeviceDiagnostics *>(lease.mpDevice->AsInterface(IATDeviceDiagnostics::kTypeID));
	auto *sid=static_cast<ATSlightSIDEmulator *>(lease.mpDevice->AsInterface(ATSlightSIDEmulator::kTypeID));
	AT_PORTABLE_TEST_ASSERT(context, memmap && diagnostics && sid);
	AT_PORTABLE_TEST_ASSERT(context, lease.mpDevice->AsInterface(IATDeviceScheduling::kTypeID));
	AT_PORTABLE_TEST_ASSERT(context, lease.mpDevice->AsInterface(IATDeviceAudioOutput::kTypeID));

	WireDevice(*lease.mpDevice,*host);
	lease.mpDevice->Init();
	AT_PORTABLE_TEST_ASSERT(context, host->mAudio.mAdds==1 && host->mAudio.mpSource==sid);
	uint32 lo=0,hi=0;
	AT_PORTABLE_TEST_ASSERT(context, memmap->GetMappedRange(0,lo,hi) && lo==0xD500 && hi==0xD600);
	AT_PORTABLE_TEST_ASSERT(context, !memmap->GetMappedRange(1,lo,hi));
	const uint8 reserved25=sid->GetRegisters()[25];
	const uint8 reserved31=sid->GetRegisters()[31];

	// SID register reads are constant, and writes inside the 32-byte window are consumed.
	host->mRAM[0xD500]=0xA5;
	host->mRAM[0xD518]=0xC3;
	host->mRAM[0xD51F]=0x5A;
	host->mMemory.WriteByte(0xD500,0x34);
	host->mMemory.WriteByte(0xD518,0x0F);
	AT_PORTABLE_TEST_ASSERT(context, sid->GetRegisters()[0]==0x34 && sid->GetRegisters()[24]==0x0F);
	AT_PORTABLE_TEST_ASSERT(context, host->mRAM[0xD500]==0xA5 && host->mRAM[0xD518]!=0x0F);
	AT_PORTABLE_TEST_ASSERT(context, host->mMemory.ReadByte(0xD500)==0x33);
	AT_PORTABLE_TEST_ASSERT(context, host->mMemory.ReadByte(0xD51F)==0x33);

	// Reserved SID offsets still decode on the board, consume writes and read
	// $33; the unwritable register cache itself must remain unchanged.
	for (uint32 address : { 0xD519U, 0xD51FU }) {
		host->mRAM[address]=0xEA;
		host->mMemory.WriteByte(address,0x12);
		AT_PORTABLE_TEST_ASSERT(context, host->mRAM[address]==0xEA && host->mMemory.ReadByte(address)==0x33);
	}
	// Addresses outside the 32-byte SID window pass through to underlying RAM.
	const uint32 passthroughAddresses[]={ 0xD520,0xD4FF,0xD600 };
	for (uint32 address : passthroughAddresses) {
		const uint8 value=(uint8)(address ^ (address >> 8));
		host->mRAM[address]=0xEA;
		host->mMemory.WriteByte(address,value);
		AT_PORTABLE_TEST_ASSERT(context, host->mRAM[address]==value);
		AT_PORTABLE_TEST_ASSERT(context, host->mMemory.ReadByte(address)==value);
	}
	AT_PORTABLE_TEST_ASSERT(context, sid->GetRegisters()[25]==reserved25 && sid->GetRegisters()[31]==reserved31);

	ConsoleCapture console;
	diagnostics->DumpStatus(console);
	AT_PORTABLE_TEST_ASSERT(context, console.mLines.size()==4);

	// Reset rebuilds the mapping without registering a second audio source.
	lease.mpDevice->ColdReset();
	AT_PORTABLE_TEST_ASSERT(context, host->mAudio.mAdds==1 && sid->GetRegisters()[0]==0);
	host->mRAM[0xD500]=0xA5;
	host->mMemory.WriteByte(0xD500,0x12);
	AT_PORTABLE_TEST_ASSERT(context, host->mRAM[0xD500]==0xA5 && sid->GetRegisters()[0]==0x12);

	lease.mpDevice->Shutdown();
	AT_PORTABLE_TEST_ASSERT(context, host->mAudio.mRemoves==1 && host->mAudio.mpSource==nullptr);
	host->mRAM[0xD500]=0xEA;
	host->mMemory.WriteByte(0xD500,0x77);
	AT_PORTABLE_TEST_ASSERT(context, host->mRAM[0xD500]==0x77);

	// Rewire and reinitialize the same factory object after a paired shutdown.
	WireDevice(*lease.mpDevice,*host);
	lease.mpDevice->Init();
	AT_PORTABLE_TEST_ASSERT(context, host->mAudio.mAdds==2 && host->mAudio.mpSource==sid);
	AT_PORTABLE_TEST_ASSERT(context, sid->GetRegisters()[0]==0);
	host->mRAM[0xD500]=0xEA;
	host->mMemory.WriteByte(0xD500,0x56);
	AT_PORTABLE_TEST_ASSERT(context, host->mRAM[0xD500]==0xEA && sid->GetRegisters()[0]==0x56);
	lease.mpDevice->Shutdown();
	AT_PORTABLE_TEST_ASSERT(context, host->mAudio.mRemoves==2 && host->mAudio.mpSource==nullptr);

	// A real 6502 writes SID frequency through STA and reads the board's
	// constant $33 value through LDA, using the production memory manager.
	auto machine=std::make_unique<MainCPUPortableFixture>();
	CovoxAudioHost audio;
	ATSlightSIDEmulator cpuSID;
	cpuSID.Init(&machine->mMemory,&machine->mFastScheduler,&audio);
	machine->Load(0x2000,{0xA9,0x34,0x8D,0x00,0xD5,0xAD,0x00,0xD5});
	AT_PORTABLE_TEST_ASSERT(context, machine->RunCycles(10));
	AT_PORTABLE_TEST_ASSERT(context, cpuSID.GetRegisters()[0]==0x34 && machine->mCPU.GetA()==0x33);
	AT_PORTABLE_TEST_ASSERT(context, machine->mRAM[0xD500]==0xEA);
	return true;
}
