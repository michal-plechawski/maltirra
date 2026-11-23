// Tree paths, JSON configuration, state matching and persistent device storage.
#include <limits>
#include "devicemanagerportablefixture.h"
using namespace DeviceManagerPortable;

bool ATTestAltirraDevicePaths(ATPortableTestContext& context) {
	Fixture f; auto& m = f.mManager; auto *a = f.Add(); auto *b = f.Add(); auto *hub = f.Add("hub");
	hub->mBuses[1].mTag = "slot";
	auto *c = f.Add("probe", true); auto *d = f.Add("probe", true); hub->mBuses[1].AddChildDevice(c); hub->mBuses[1].AddChildDevice(d);
	AT_PORTABLE_TEST_ASSERT(context, m.GetPathForDevice(a) == "/probe" && m.GetPathForDevice(b) == "/probe.1");
	AT_PORTABLE_TEST_ASSERT(context, m.GetPathForDevice(d) == "/hub/slot.1/probe.1");
	for (Device *dev : {a, b, hub, c, d}) {
		const auto path = m.GetPathForDevice(dev); auto parsed = m.ParsePath(path.c_str());
		AT_PORTABLE_TEST_ASSERT(context, parsed.mbValid && parsed.mpDevice == dev && !parsed.mpDeviceBus);
	}
	auto bus = m.ParsePath("/hub/slot.1");
	AT_PORTABLE_TEST_ASSERT(context, bus.mbValid && !bus.mpDevice && bus.mpDeviceBus == &hub->mBuses[1] && bus.mDeviceBusIndex == 1);
	for (const char *bad : {"", "/", "probe", "/probe.2", "/probe.", "/probe.-1", "/probe.4294967296", "/probe.1x", "/hub/slot.2", "/hub/slot.1/nope", "/hub/slot.1/probe.1/", "/hub//probe"})
		AT_PORTABLE_TEST_ASSERT(context, !m.ParsePath(bad).mbValid);
	AT_PORTABLE_TEST_ASSERT(context, !m.ParsePath(nullptr).mbValid);
	return true;
}

bool ATTestAltirraDeviceJSON(ATPortableTestContext& context) {
	Fixture f; auto& m = f.mManager; ATPropertySet props;
	props.SetBool("enabled",true); props.SetInt32("negative", -123); props.SetUint32("maximum", 0xFFFFFFFF);
	props.SetDouble("fraction", 0.125); props.SetString("label", L"Kartridż \"test\" 🎮");
	VDStringW text; m.SerializeProps(props, text); ATPropertySet restored; m.DeserializeProps(restored,text.c_str());
	AT_PORTABLE_TEST_ASSERT(context, restored.GetBool("enabled") && restored.GetInt32("negative") == -123 && restored.GetUint32("maximum") == 0xFFFFFFFF);
	AT_PORTABLE_TEST_ASSERT(context, restored.GetDouble("fraction") == 0.125 && VDStringW(restored.GetString("label")) == props.GetString("label"));
	auto *hub = f.Add("hub",false,13); auto *a = f.Add("probe",true,21); auto *b = f.Add("probe",true,34);
	hub->mBuses[0].AddChildDevice(a); hub->mBuses[1].AddChildDevice(b);
	text.clear(); m.SerializeDevice(nullptr,text,true,true);
	Fixture copy; copy.mManager.DeserializeDevices(nullptr,nullptr,text.c_str());
	AT_PORTABLE_TEST_ASSERT(context, copy.mManager.GetDeviceCount() == 3);
	auto p = copy.mManager.ParsePath("/hub/aux/probe");
	AT_PORTABLE_TEST_ASSERT(context, p.mbValid && static_cast<Device *>(p.mpDevice)->mValue == 34);
	const uint32 count = copy.mManager.GetDeviceCount(); copy.mManager.DeserializeDevices(nullptr,nullptr,L"[broken");
	AT_PORTABLE_TEST_ASSERT(context, copy.mManager.GetDeviceCount() == count);
	return true;
}

bool ATTestAltirraDeviceAttachment(ATPortableTestContext& context) {
	Fixture f; auto& m = f.mManager; auto *hub = f.Add("hub");
	m.DeserializeDevices(hub,&hub->mBuses[1],L"{\"tag\":\"probe\",\"params\":{\"value\":55}}");
	auto *d = m.GetDeviceByTag("probe");
	AT_PORTABLE_TEST_ASSERT(context, d && d->GetParentBusIndex() == 1 && d->GetParent() == hub);
	hub->mBuses[1].mbAccept = false;
	m.DeserializeDevices(hub,&hub->mBuses[1],L"{\"tag\":\"probe\"}");
	AT_PORTABLE_TEST_ASSERT(context, m.GetDeviceCount() == 2);
	vdrefptr<Device> internal {new Device(kInternal)}; m.AddDevice(internal);
	m.DeserializeDevices(nullptr,nullptr,L"[{\"tag\":\"_internal\",\"params\":{\"value\":77}},{\"tag\":\"internal\"}]");
	AT_PORTABLE_TEST_ASSERT(context, internal->mValue == 77 && m.GetDeviceCount() == 3);
	return true;
}

bool ATTestAltirraDeviceSnapshots(ATPortableTestContext& context) {
	Fixture f; auto *hub = f.Add("hub",false,1); hub->mbSkipSave = true;
	auto *a = f.Add("probe",true,21); auto *b = f.Add("probe",true,34); hub->mBuses[0].AddChildDevice(a); hub->mBuses[1].AddChildDevice(b);
	a->mSettings.SetBool("partial",true); ATSnapshotStatus status {}; f.mManager.GetSnapshotStatus(status);
	AT_PORTABLE_TEST_ASSERT(context, status.mbPartialAccuracy);
	auto state = f.mManager.SaveState(); auto decoded = RoundTrip(*state);
	Fixture copy; auto *copyHub = copy.Add("hub"); auto *copyB = copy.Add("probe",true); auto *copyA = copy.Add("probe",true);
	copyHub->mBuses[1].AddChildDevice(copyB); copyHub->mBuses[0].AddChildDevice(copyA);
	copy.mManager.LoadState(decoded);
	AT_PORTABLE_TEST_ASSERT(context, copyA->mValue == 21 && copyB->mValue == 34 && copyHub->mValue == 0);
	copy.mManager.LoadState(nullptr);
	AT_PORTABLE_TEST_ASSERT(context, copyA->mValue == 0 && copyB->mValue == 0 && copyA->mStats->mLoads == 2);
	return true;
}

bool ATTestAltirraDeviceNVRAM(ATPortableTestContext& context) {
	Registry registry; ATDeviceManager m;
	const uint8 blob[] = {1,2,3,4}; m.SaveNVRAM("ROM",blob,4); std::array<uint8,6> out; out.fill(0xAA);
	AT_PORTABLE_TEST_ASSERT(context, m.LoadNVRAM("rom",out.data(),out.size()) && out[3] == 4 && out[4] == 0xAA);
	m.SaveNVRAMInt("Rom",-23); sint32 value = 0;
	AT_PORTABLE_TEST_ASSERT(context, !m.LoadNVRAM("ROM",out.data(),4) && m.LoadNVRAMInt("ROM",value) && value == -23);
	m.SaveNVRAM("empty",nullptr,0);
	VDRegistryAppKey key("NVRAM"); m.SaveSettings(key); ATDeviceManager copy; copy.LoadSettings(key);
	AT_PORTABLE_TEST_ASSERT(context, copy.LoadNVRAMInt("rom",value) && value == -23 && copy.LoadNVRAM("EMPTY",nullptr,0));
	AT_PORTABLE_TEST_ASSERT(context, !copy.LoadNVRAMInt("empty",value) && !copy.LoadNVRAM("missing",out.data(),4));
	return true;
}

bool ATTestAltirraDeviceStorage(ATPortableTestContext& context) {
	struct Storage final : IATDeviceStorage {
		void LoadStorage(IATDeviceStorageManager& m) override { ++mLoads; m.LoadNVRAMInt("device-value",mValue); }
		void SaveStorage(IATDeviceStorageManager& m) override { ++mSaves; m.SaveNVRAMInt("device-value",mValue); }
		sint32 mValue = 91; uint32 mLoads = 0, mSaves = 0;
	};
	Registry registry; Storage storage; ATDeviceManager m; m.RegisterDeviceStorage(storage);
	VDRegistryAppKey key("DeviceStorage"); m.SaveSettings(key); storage.mValue = 0; m.LoadSettings(key);
	AT_PORTABLE_TEST_ASSERT(context, storage.mSaves == 1 && storage.mLoads == 1 && storage.mValue == 91);
	m.UnregisterDeviceStorage(storage); m.SaveSettings(key); m.LoadSettings(key);
	AT_PORTABLE_TEST_ASSERT(context, storage.mLoads == 1 && storage.mSaves == 1);
	return true;
}
