// Portable integration tests for the Covox and SimCovox device wrappers.

#include <cmath>
#include <memory>
#include <utility>
#include <vector>

#include <at/atcore/device.h>
#include <at/atcore/deviceimpl.h>
#include <at/atcore/devicepia.h>
#include <at/atcore/devicesystemcontrol.h>
#include <at/atcore/propertyset.h>
#include <at/attest/portabletest.h>
#include "covoxportablefixture.h"
#include <pia.h>

void ATCreateDeviceCovox(const ATPropertySet&, IATDevice **);
void ATCreateDeviceSimCovox(const ATPropertySet&, IATDevice **);

namespace {
	struct DeviceLease {
		DeviceLease() = default;
		DeviceLease(const DeviceLease&) = delete;
		DeviceLease& operator=(const DeviceLease&) = delete;
		DeviceLease(DeviceLease&& other) noexcept : mpDevice(std::move(other.mpDevice)) {}
		DeviceLease& operator=(DeviceLease&&) = delete;
		vdrefptr<IATDevice> mpDevice;
		~DeviceLease() {
			if (mpDevice) {
				mpDevice->Shutdown();
				mpDevice->SetManager(nullptr);
			}
		}
	};

	class CovoxController final : public IATCovoxController {
	public:
		bool IsCovoxEnabled() const override { return mbEnabled; }
		ATNotifyList<const vdfunction<void(bool)> *>& GetCovoxEnableNotifyList() override { return mChanged; }
		void SetEnabled(bool enable) {
			mbEnabled=enable;
			mChanged.InvokeAll(enable);
		}
		bool mbEnabled=true;
		ATNotifyList<const vdfunction<void(bool)> *> mChanged;
	};

	class SchedulingService final : public IATDeviceSchedulingService {
	public:
		explicit SchedulingService(ATScheduler& scheduler) : mScheduler(scheduler) {}
		ATScheduler *GetMachineScheduler() const override { return &mScheduler; }
		ATScheduler *GetSlowScheduler() const override { return &mScheduler; }
		ATScheduler& mScheduler;
	};

	class SimCovoxServices final : public IATDeviceManager {
	public:
		SimCovoxServices(ATPIAEmulator& pia, IATAudioMixer& audio, IATDeviceSchedulingService& scheduling)
			: mPIA(pia), mAudio(audio), mScheduling(scheduling) {}
		void *GetService(uint32 iid) override {
			if (iid==IATDevicePIA::kTypeID) return static_cast<IATDevicePIA *>(&mPIA);
			if (iid==IATAudioMixer::kTypeID) return &mAudio;
			if (iid==IATDeviceSchedulingService::kTypeID) return &mScheduling;
			return nullptr;
		}
		void NotifyDeviceStatusChanged(IATDevice&) override {}
		ATPIAEmulator& mPIA;
		IATAudioMixer& mAudio;
		IATDeviceSchedulingService& mScheduling;
	};

	DeviceLease CreateCovox(CovoxPortableHost& host, const ATPropertySet& settings) {
		DeviceLease lease;
		ATCreateDeviceCovox(settings, ~lease.mpDevice);
		auto *memmap=static_cast<IATDeviceMemMap *>(lease.mpDevice->AsInterface(IATDeviceMemMap::kTypeID));
		auto *scheduling=static_cast<IATDeviceScheduling *>(lease.mpDevice->AsInterface(IATDeviceScheduling::kTypeID));
		auto *audio=static_cast<IATDeviceAudioOutput *>(lease.mpDevice->AsInterface(IATDeviceAudioOutput::kTypeID));
		memmap->InitMemMap(&host.mMemory);
		scheduling->InitScheduling(&host.mScheduler,&host.mScheduler);
		audio->InitAudioOutput(&host.mAudio);
		return lease;
	}

	bool HasSubmission(const CovoxAudioHost& audio, const char *label, float left, float right) {
		for (const auto& submission : audio.mSubmissions)
			if (submission.mLabel==label && submission.mLeft==left && submission.mRight==right)
				return true;
		return false;
	}

	bool HasEdges(const CovoxAudioHost& audio) {
		for (const auto& submission : audio.mSubmissions)
			if (!submission.mEdges.empty())
				return true;
		return false;
	}

	bool Near(float a, float b) { return std::fabs(a-b) < 0.0001f; }
}

bool ATTestAltirraCovoxDevice(ATPortableTestContext& context) {
	auto defaultHost=std::make_unique<CovoxPortableHost>();
	ATPropertySet defaultSettings;
	auto defaultDevice=CreateCovox(*defaultHost,defaultSettings);
	defaultDevice.mpDevice->SetSettings(defaultSettings);
	ATPropertySet defaults;
	defaultDevice.mpDevice->GetSettings(defaults);
	AT_PORTABLE_TEST_ASSERT(context, defaults.GetUint32("base")==0xD600);
	AT_PORTABLE_TEST_ASSERT(context, defaults.GetUint32("size")==0x100 && defaults.GetUint32("channels")==4);
	auto *defaultMap=static_cast<IATDeviceMemMap *>(defaultDevice.mpDevice->AsInterface(IATDeviceMemMap::kTypeID));
	uint32 defaultLo=0,defaultHi=0;
	// The existing simulator's HLE overlap checks consume an exclusive upper bound.
	AT_PORTABLE_TEST_ASSERT(context, defaultMap->GetMappedRange(0,defaultLo,defaultHi) && defaultLo==0xD600 && defaultHi==0xD700);
	defaultDevice.mpDevice->Init();

	auto host=std::make_unique<CovoxPortableHost>();
	CovoxController controller, replacement;
	ATPropertySet settings;
	settings.SetUint32("base",0xD600);
	settings.SetUint32("size",0x100);
	settings.SetUint32("channels",1);
	auto device=CreateCovox(*host,settings);
	auto *covox=static_cast<IATDeviceCovoxControl *>(device.mpDevice->AsInterface(IATDeviceCovoxControl::kTypeID));
	auto *memmap=static_cast<IATDeviceMemMap *>(device.mpDevice->AsInterface(IATDeviceMemMap::kTypeID));
	device.mpDevice->SetSettings(settings);
	covox->InitCovoxControl(controller);
	device.mpDevice->Init();

	ATPropertySet actual;
	device.mpDevice->GetSettings(actual);
	AT_PORTABLE_TEST_ASSERT(context, actual.GetUint32("base")==0xD600 && actual.GetUint32("size")==0x100);
	AT_PORTABLE_TEST_ASSERT(context, actual.GetUint32("channels")==1);
	uint32 lo=0,hi=0;
	AT_PORTABLE_TEST_ASSERT(context, memmap->GetMappedRange(0,lo,hi) && lo==0xD600 && hi==0xD700);
	AT_PORTABLE_TEST_ASSERT(context, !memmap->GetMappedRange(1,lo,hi));

	// The actual RAM bus sees passthrough writes while the Covox source renders them.
	host->mMemory.WriteByte(0xD600,0x40);
	AT_PORTABLE_TEST_ASSERT(context, host->mRAM[0xD600]==0x40);
	auto mono=host->mAudio.Render(host->Time());
	AT_PORTABLE_TEST_ASSERT(context, !mono.mLeft.empty() && Near(mono.mLeft[0],-0.5f) && Near(mono.mRight[0],-0.5f));
	AT_PORTABLE_TEST_ASSERT(context, HasSubmission(host->mAudio,"Covox L",1.0f/256.0f,1.0f/256.0f));

	// A second enable controller replaces the first; stale controllers must stop acting.
	replacement.mbEnabled=false;
	covox->InitCovoxControl(replacement);
	covox->InitCovoxControl(replacement); // reattachment to the same controller must not duplicate registration
	AT_PORTABLE_TEST_ASSERT(context, controller.mChanged.IsEmpty());
	host->mAudio.mSubmissions.clear();
	host->mMemory.WriteByte(0xD600,0x22);
	AT_PORTABLE_TEST_ASSERT(context, host->mRAM[0xD600]==0x22);
	controller.SetEnabled(true);
	host->mMemory.WriteByte(0xD600,0x33);
	host->mAudio.Render(host->Time());
	AT_PORTABLE_TEST_ASSERT(context, !HasEdges(host->mAudio));
	replacement.SetEnabled(true);
	host->mAudio.mSubmissions.clear();
	host->mMemory.WriteByte(0xD600,0x44);
	host->mAudio.Render(host->Time());
	AT_PORTABLE_TEST_ASSERT(context, HasEdges(host->mAudio));
	device.mpDevice->Shutdown();
	AT_PORTABLE_TEST_ASSERT(context, replacement.mChanged.IsEmpty());
	AT_PORTABLE_TEST_ASSERT(context, controller.mChanged.IsEmpty());
	host->mMemory.WriteByte(0xD600,0x55);
	AT_PORTABLE_TEST_ASSERT(context, host->mRAM[0xD600]==0x55);
	AT_PORTABLE_TEST_ASSERT(context, host->mAudio.mRemoves==1 && !host->mAudio.mpSource);

	// Four-channel operation separates left/right edge buffers, then rejoins when balanced.
	auto stereoHost=std::make_unique<CovoxPortableHost>();
	ATPropertySet stereoSettings;
	stereoSettings.SetUint32("base",0xD600);
	stereoSettings.SetUint32("size",4);
	stereoSettings.SetUint32("channels",4);
	auto stereoDevice=CreateCovox(*stereoHost,stereoSettings);
	stereoDevice.mpDevice->SetSettings(stereoSettings);
	stereoDevice.mpDevice->Init();
	stereoHost->mMemory.WriteByte(0xD600,0x40);
	auto leftOnly=stereoHost->mAudio.Render(stereoHost->Time());
	AT_PORTABLE_TEST_ASSERT(context, leftOnly.mLeft[0]<0 && Near(leftOnly.mRight[0],0.0f));
	AT_PORTABLE_TEST_ASSERT(context, HasSubmission(stereoHost->mAudio,"Covox L",1.0f/256.0f,0));
	AT_PORTABLE_TEST_ASSERT(context, HasSubmission(stereoHost->mAudio,"Covox R",0,1.0f/256.0f));
	stereoHost->mMemory.WriteByte(0xD601,0x40);
	stereoHost->mMemory.WriteByte(0xD602,0x40);
	stereoHost->mMemory.WriteByte(0xD603,0x40);
	auto balanced=stereoHost->mAudio.Render(stereoHost->Time());
	AT_PORTABLE_TEST_ASSERT(context, !stereoHost->mAudio.mpSource->RequiresStereoMixingNow());

	// D280 is a non-passthrough four-register mapping; writes beyond its exact span pass through.
	auto piaCovoxHost=std::make_unique<CovoxPortableHost>();
	ATPropertySet piaSettings;
	piaSettings.SetUint32("base",0xD280);
	piaSettings.SetUint32("size",4);
	piaSettings.SetUint32("channels",1);
	auto piaDevice=CreateCovox(*piaCovoxHost,piaSettings);
	auto *piaDeviceMap=static_cast<IATDeviceMemMap *>(piaDevice.mpDevice->AsInterface(IATDeviceMemMap::kTypeID));
	piaDevice.mpDevice->SetSettings(piaSettings);
	piaDevice.mpDevice->Init();
	piaCovoxHost->mRAM[0xD280]=0xEA;
	piaCovoxHost->mMemory.WriteByte(0xD280,0x60);
	AT_PORTABLE_TEST_ASSERT(context, piaCovoxHost->mRAM[0xD280]==0xEA);
	piaCovoxHost->mMemory.WriteByte(0xD284,0x61);
	AT_PORTABLE_TEST_ASSERT(context, piaCovoxHost->mRAM[0xD284]==0x61);
	uint32 dLo=0,dHi=0;
	AT_PORTABLE_TEST_ASSERT(context, piaDeviceMap->GetMappedRange(0,dLo,dHi) && dLo==0xD280 && dHi==0xD284);

	// A single-byte reservation at the first address of D6 must block that HLE page.
	ATPropertySet single;
	single.SetUint32("base",0xD600); single.SetUint32("size",1);
	AT_PORTABLE_TEST_ASSERT(context, stereoDevice.mpDevice->SetSettings(single));
	auto *singleMap=static_cast<IATDeviceMemMap *>(stereoDevice.mpDevice->AsInterface(IATDeviceMemMap::kTypeID));
	AT_PORTABLE_TEST_ASSERT(context, singleMap->GetMappedRange(0,lo,hi) && lo==0xD600 && hi==0xD601);
	AT_PORTABLE_TEST_ASSERT(context, hi>0xD600 && lo<0xD700);
	ATPropertySet bad;
	bad.SetUint32("base",0x10000); bad.SetUint32("size",4);
	AT_PORTABLE_TEST_ASSERT(context, !stereoDevice.mpDevice->SetSettings(bad));
	bad.SetUint32("base",0xFFFF); bad.SetUint32("size",2);
	AT_PORTABLE_TEST_ASSERT(context, !stereoDevice.mpDevice->SetSettings(bad));
	AT_PORTABLE_TEST_ASSERT(context, singleMap->GetMappedRange(0,lo,hi) && lo==0xD600 && hi==0xD601);

	// Any mapping in the POKEY page consumes its own writes before and after Init.
	auto oddHost=std::make_unique<CovoxPortableHost>();
	ATPropertySet odd;
	odd.SetUint32("base",0xD2A0); odd.SetUint32("size",4);
	auto oddDevice=CreateCovox(*oddHost,odd);
	AT_PORTABLE_TEST_ASSERT(context, oddDevice.mpDevice->SetSettings(odd));
	oddDevice.mpDevice->Init();
	oddHost->mMemory.WriteByte(0xD2A0,0x40);
	AT_PORTABLE_TEST_ASSERT(context, oddHost->mRAM[0xD2A0]==0xEA);
	return true;
}

bool ATTestAltirraSimCovox(ATPortableTestContext& context) {
	auto host=std::make_unique<CovoxPortableHost>();
	auto pia=std::make_unique<ATPIAEmulator>();
	pia->Init(&host->mScheduler);
	pia->ColdReset();
	SchedulingService scheduling(host->mScheduler);
	SimCovoxServices services(*pia,host->mAudio,scheduling);
	ATPropertySet settings;
	DeviceLease sim;
	ATCreateDeviceSimCovox(settings,~sim.mpDevice);
	sim.mpDevice->SetManager(&services);
	sim.mpDevice->Init();
	AT_PORTABLE_TEST_ASSERT(context, host->mAudio.mAdds==1 && host->mAudio.mpSource);

	// Configure PIA port A as output, select the output register, and drive a byte.
	pia->WriteByte(0,0xFF); // DDRA
	pia->WriteByte(2,0x04); // CRA selects ORA
	pia->WriteByte(0,0x40); // Port A output drives SimCovox
	auto output=host->mAudio.Render(host->Time());
	AT_PORTABLE_TEST_ASSERT(context, Near(output.mLeft[0],-0.5f) && Near(output.mRight[0],-0.5f));

	sim.mpDevice->Shutdown();
	AT_PORTABLE_TEST_ASSERT(context, host->mAudio.mRemoves==1 && !host->mAudio.mpSource);
	const uint32 oldAdds=host->mAudio.mAdds;
	// Changes after Shutdown have no callback target; reinitialization attaches a fresh PIA output.
	pia->WriteByte(0,0x20);
	sim.mpDevice->Init();
	AT_PORTABLE_TEST_ASSERT(context, host->mAudio.mAdds==oldAdds+1 && host->mAudio.mpSource);
	pia->WriteByte(0,0x30);
	auto output2=host->mAudio.Render(host->Time());
	AT_PORTABLE_TEST_ASSERT(context, Near(output2.mLeft[0],-0.625f) && Near(output2.mRight[0],-0.625f));
	sim.mpDevice->Shutdown();
	AT_PORTABLE_TEST_ASSERT(context, host->mAudio.mRemoves==2 && !host->mAudio.mpSource);
	const int reused = pia->AllocOutput([](void *,uint32) {}, nullptr, 0xFF);
	AT_PORTABLE_TEST_ASSERT(context, reused == 0); // SimCovox returned its real PIA callback slot
	pia->FreeOutput(reused);
	return true;
}
