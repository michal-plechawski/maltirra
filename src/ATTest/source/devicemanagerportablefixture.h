// Production device manager; small devices expose observable lifecycle/bus/state.
#ifndef f_ATTEST_DEVICEMANAGERPORTABLEFIXTURE_H
#define f_ATTEST_DEVICEMANAGERPORTABLEFIXTURE_H
#include <algorithm>
#include <array>
#include <cstring>
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <vd2/system/vdtypes.h>
#include <vd2/system/error.h>
#include <vd2/system/file.h>
#include <vd2/system/registry.h>
#include <vd2/system/registrymemory.h>
#include <at/attest/portabletest.h>
#include <at/atcore/deviceimpl.h>
#include <at/atcore/deviceparent.h>
#include <at/atcore/devicesnapshot.h>
#include <at/atcore/propertyset.h>
#include <at/atcore/snapshotimpl.h>
#include <devicemanager.h>
#include <savestateio.h>

namespace DeviceManagerPortable {
	void CreateProbe(const ATPropertySet&, IATDevice **);
	void CreateHub(const ATPropertySet&, IATDevice **);
	inline const ATDeviceDefinition kProbe {"probe", "probe", L"Probe", CreateProbe, 0};
	inline const ATDeviceDefinition kHub {"hub", "hub", L"Hub", CreateHub, 0};
	inline const ATDeviceDefinition kHidden {"hidden", "hidden", L"Hidden", nullptr, kATDeviceDefFlag_Hidden};
	inline const ATDeviceDefinition kInternal {"internal", "internal", L"Internal", nullptr, kATDeviceDefFlag_Internal};
	struct Port { static constexpr uint32 kTypeID = 0x50524F42; virtual uint32 Marker() const = 0; };
	struct Service { static constexpr uint32 kTypeID = 0x50525356; uint32 mValue = 0x1234; };
	struct Stats { std::vector<std::string> mCalls; uint32 mShutdown = 0, mDestroyed = 0, mLoads = 0; };
	class State final : public ATSnapExchangeObject<State, "PortableDeviceManagerState"> {
	public:
		template<typename T> void Exchange(T& ex) { ex.Transfer("value", &mValue); }
		uint32 mValue = 0;
	};
	class Bus final : public IATDeviceBus {
	public:
		Bus(IATDeviceParent *parent, uint32 index, const char *tag) : mpParent(parent), mIndex(index), mTag(tag) {}
		const wchar_t *GetBusName() const override { return L"Test bus"; }
		const char *GetBusTag() const override { return mTag.c_str(); }
		const char *GetSupportedType(uint32 index) override { return index ? nullptr : "probe"; }
		void GetChildDevices(vdfastvector<IATDevice *>& out) override { for (const auto& p : mChildren) out.push_back(p); }
		void GetChildDevicePrefix(uint32, VDStringW& s) override { s = L"Unit"; }
		void AddChildDevice(IATDevice *p) override {
			if (!mbAccept || p->GetParent()) return;
			mChildren.emplace_back(p); p->SetParent(mpParent, mIndex);
		}
		void RemoveChildDevice(IATDevice *p) override {
			auto it = std::find_if(mChildren.begin(), mChildren.end(), [p](const auto& q) { return p == q.get(); });
			if (it != mChildren.end()) { p->SetParent(nullptr, 0); mChildren.erase(it); }
		}
		void Clear() { for (const auto& p : mChildren) p->SetParent(nullptr, 0); mChildren.clear(); }
		IATDeviceParent *mpParent; uint32 mIndex; std::string mTag; bool mbAccept = true;
		std::vector<vdrefptr<IATDevice>> mChildren;
	};
	class Device final : public ATDeviceT<Port, IATDeviceParent, IATDeviceSnapshot> {
	public:
		explicit Device(const ATDeviceDefinition& def = kProbe) : mDef(def), mBuses {{{this, 0, "slot"}, {this, 1, "aux"}}} {}
		~Device() { ++mStats->mDestroyed; }
		void GetDeviceInfo(ATDeviceInfo& info) override { info.mpDef = &mDef; }
		uint32 Marker() const override { return mValue; }
		void GetSettings(ATPropertySet& out) override { out = mSettings; }
		bool SetSettings(const ATPropertySet& props) override { mSettings = props; mValue = props.GetUint32("value"); return !props.GetBool("restart"); }
		void Init() override { mStats->mCalls.push_back("init"); if (mpInit) mpInit(); Fail(1); }
		void ColdReset() override { mStats->mCalls.push_back("cold"); Fail(2); }
		void PeripheralColdReset() override { mStats->mCalls.push_back("peripheral"); Fail(3); }
		void ComputerColdReset() override { mStats->mCalls.push_back("computer"); Fail(4); }
		void Shutdown() override { ++mStats->mShutdown; mStats->mCalls.push_back("shutdown"); for (auto& b : mBuses) b.Clear(); }
		IATDeviceBus *GetDeviceBus(uint32 i) override { return !std::strcmp(mDef.mpTag,kHub.mpTag) && i < mBuses.size() ? &mBuses[i] : nullptr; }
		void GetSnapshotStatus(ATSnapshotStatus& s) const override { if (mSettings.GetBool("partial")) s.mbPartialAccuracy = true; }
		vdrefptr<IATObjectState> SaveState(ATSnapshotContext&) const override {
			if (mbSkipSave) return {};
			vdrefptr<State> s {new State}; s->mValue = mValue; return s;
		}
		void LoadState(const IATObjectState *s, ATSnapshotContext&) override { ++mStats->mLoads; mValue = s ? atser_cast<const State&>(*s).mValue : 0; }
		bool HasManager() const { return mpDeviceManager != nullptr; }
		Service *GetProbeService() const { return GetService<Service>(); }
		void StatusChanged() { NotifyStatusChanged(); }
		const ATDeviceDefinition& mDef; ATPropertySet mSettings; uint32 mValue = 0;
		std::shared_ptr<Stats> mStats = std::make_shared<Stats>();
		std::function<void()> mpInit; bool mbSkipSave = false; std::array<Bus, 2> mBuses;
	private:
		void Fail(uint32 phase) { if (mSettings.GetUint32("fail") == phase) throw MyError("Portable device initialization failure"); }
	};
	inline void CreateProbe(const ATPropertySet&, IATDevice **out) { auto *p = new Device; p->AddRef(); *out = p; }
	inline void CreateHub(const ATPropertySet&, IATDevice **out) { auto *p = new Device(kHub); p->AddRef(); *out = p; }
	struct Fixture {
		Fixture() { mManager.Init(); for (auto *p : {&kProbe, &kHub, &kHidden, &kInternal}) mManager.AddDeviceDefinition(p); }
		~Fixture() { mManager.RemoveAllDevices(true); }
		Device *Add(const char *tag = "probe", bool child = false, uint32 value = 0) {
			ATPropertySet props; props.SetUint32("value", value);
			return static_cast<Device *>(mManager.AddDevice(tag, props, child));
		}
		ATDeviceManager mManager;
	};
	struct Observer final : IATDeviceChangeCallback {
		explicit Observer(ATDeviceManager& m) : mManager(m) { mManager.AddDeviceChangeCallback(Port::kTypeID, this); }
		~Observer() { mManager.RemoveDeviceChangeCallback(Port::kTypeID, this); }
		void OnDeviceAdded(uint32, IATDevice *, void *) override { mEvents.push_back("added"); if (mpAdded) mpAdded(); }
		void OnDeviceRemoving(uint32, IATDevice *, void *) override { mEvents.push_back("removing"); }
		void OnDeviceRemoved(uint32, IATDevice *, void *) override { mEvents.push_back("removed"); }
		ATDeviceManager& mManager; std::vector<std::string> mEvents; std::function<void()> mpAdded;
	};
	struct Registry {
		Registry() : mpPrevious(VDGetRegistryProvider()), mPreviousKey(VDRegistryAppKey::getDefaultKey()) { VDSetRegistryProvider(&mProvider); VDRegistryAppKey::setDefaultKey("DeviceManagerPortableTests"); }
		~Registry() { VDRegistryAppKey::setDefaultKey(mPreviousKey.c_str()); VDSetRegistryProvider(mpPrevious); }
		IVDRegistryProvider *mpPrevious; VDStringA mPreviousKey; VDRegistryProviderMemory mProvider;
	};
	inline vdrefptr<IATObjectState> RoundTrip(IATObjectState& state) {
		VDMemoryBufferStream output; std::unique_ptr<IATSaveStateSerializer> w(ATCreateSaveStateSerializer()); w->Serialize(output, state, L"Device manager state");
		output.Seek(0); vdrefptr<IATSerializable> decoded; std::unique_ptr<IATSaveStateDeserializer> r(ATCreateSaveStateDeserializer()); r->Deserialize(output, ~decoded);
		return vdrefptr<IATObjectState>(static_cast<IATObjectState *>(decoded.get()));
	}
}
#endif
