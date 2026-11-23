// Registration, rollback, callbacks and production service wiring.
#include "devicemanagerportablefixture.h"
#include "maincpuportablefixture.h"
#include <cartridge.h>
#include <cartridgeport.h>
using namespace DeviceManagerPortable;

bool ATTestAltirraDeviceRegistration(ATPortableTestContext& context) {
	Fixture f; Service service; auto& m = f.mManager; m.RegisterService(&service);
	AT_PORTABLE_TEST_ASSERT(context, m.GetService<IATDeviceStorageManager>() == &m && m.GetDeviceCount() == 0);
	auto empty = m.GetDevices(false,false,false).begin(); empty += 0; empty -= 0;
	AT_PORTABLE_TEST_ASSERT(context, std::distance(empty,m.GetDevices(false,false,false).end()) == 0);
	AT_PORTABLE_TEST_ASSERT(context, m.GetDeviceDefinition("probe") == &kProbe && !m.GetDeviceDefinition("unknown"));
	auto configure = +[](VDGUIHandle, ATPropertySet& props) { props.SetBool("configured", true); return true; };
	m.AddDeviceConfigurer("probe",configure); ATPropertySet configured;
	AT_PORTABLE_TEST_ASSERT(context, m.GetDeviceConfigureFn("probe")(nullptr,configured) && configured.GetBool("configured") && !m.GetDeviceConfigureFn("missing"));
	AT_PORTABLE_TEST_ASSERT(context, !m.AddDevice("unknown", ATPropertySet()));
	auto *a = f.Add("probe", false, 17); auto stats = a->mStats;
	AT_PORTABLE_TEST_ASSERT(context, a->HasManager() && a->GetProbeService() == &service);
	AT_PORTABLE_TEST_ASSERT(context, stats->mCalls == std::vector<std::string>({"init","cold","peripheral","computer"}));
	AT_PORTABLE_TEST_ASSERT(context, m.GetDeviceCount() == 1 && m.GetChangeCounter() == 1 && m.GetDeviceByIndex(0) == a && !m.GetDeviceByIndex(1));
	AT_PORTABLE_TEST_ASSERT(context, m.GetInterface<Port>()->Marker() == 17);
	struct Command final : vdrefcounted<IATDeviceXCmd> {
		bool IsSupported(IATDevice *d, int bus) const override { return d && bus < 0 && d->AsInterface(Port::kTypeID); }
		ATDeviceXCmdInfo GetInfo() const override { ATDeviceXCmdInfo info; info.mDisplayName = L"Increment value"; return info; }
		void Invoke(ATDeviceManager& manager, IATDevice *d, int) override { ++static_cast<Device *>(d)->mValue; manager.IncrementChangeCounter(); }
	};
	vdrefptr<Command> command {new Command}; m.RegisterExtendedCommand(*command); command = nullptr;
	auto commands = m.GetExtendedCommandsForDevice(a,-1);
	AT_PORTABLE_TEST_ASSERT(context, commands.size() == 1 && m.GetExtendedCommandsForDevice(a,0).empty());
	AT_PORTABLE_TEST_ASSERT(context, commands[0]->GetInfo().mDisplayName == L"Increment value");
	commands[0]->Invoke(m,a,-1); AT_PORTABLE_TEST_ASSERT(context, a->mValue == 18 && m.GetChangeCounter() == 2);
	const auto begin = m.GetInterfaces<Port>(false, false, false).cbegin();
	AT_PORTABLE_TEST_ASSERT(context, (0 + begin)->Marker() == 18 && begin[0]->Marker() == 18);
	auto *b = f.Add("probe", false, 23);
	AT_PORTABLE_TEST_ASSERT(context, m.GetDeviceByTag("probe", 1) == b);
	AT_PORTABLE_TEST_ASSERT(context, std::distance(m.GetDevices(false,false,false).cbegin(), m.GetDevices(false,false,false).cend()) == 2);
	vdrefptr<Device> hidden {new Device(kHidden)}, internal {new Device(kInternal)};
	m.AddDevice(hidden); m.AddDevice(internal);
	AT_PORTABLE_TEST_ASSERT(context, std::distance(m.GetDevices(false,true,true).begin(), m.GetDevices(false,true,true).end()) == 2);
	AT_PORTABLE_TEST_ASSERT(context, !m.GetDeviceByTag("hidden", 0, true) && !m.GetDeviceByTag("internal", 0, false, true));
	m.RemoveAllDevices(false);
	AT_PORTABLE_TEST_ASSERT(context, m.GetDeviceCount() == 1 && m.GetDeviceByIndex(0) == internal && stats->mShutdown == 1 && stats->mDestroyed == 1);
	m.RemoveAllDevices(true); m.UnregisterService(&service);
	AT_PORTABLE_TEST_ASSERT(context, !m.GetService<Service>() && internal->HasManager() == false);
	return true;
}

bool ATTestAltirraDeviceRollback(ATPortableTestContext& context) {
	for (uint32 phase = 1; phase <= 4; ++phase) {
		Fixture f; auto& m = f.mManager; vdrefptr<Device> d {new Device};
		d->mSettings.SetUint32("fail", phase); bool sawSelf = false;
		d->mpInit = [&] { auto ports = m.GetInterfaces<Port>(false,false,false); sawSelf = ports.begin() != ports.end() && *ports.begin() == static_cast<Port *>(d.get()); if (phase == 1) f.Add("hub"); };
		bool threw = false; try { m.AddDevice(d); } catch (const MyError&) { threw = true; }
		AT_PORTABLE_TEST_ASSERT(context, threw && sawSelf && !d->HasManager() && d->mStats->mShutdown == 1);
		AT_PORTABLE_TEST_ASSERT(context, m.GetDeviceCount() == (phase == 1 ? 1 : 0) && !m.GetDeviceByTag("probe"));
		AT_PORTABLE_TEST_ASSERT(context, m.GetInterface<Port>() == (phase == 1 ? static_cast<Port *>(m.GetDeviceByTag("hub")->AsInterface(Port::kTypeID)) : nullptr));
		auto remaining = m.GetInterfaces<Port>(false,false,false);
		AT_PORTABLE_TEST_ASSERT(context, std::distance(remaining.begin(),remaining.end()) == (phase == 1 ? 1 : 0));
	}
	// An init callback may populate the interface cache before insertion.
	Fixture f; f.mManager.AddInitCallback([&](IATDevice&) { (void)f.mManager.GetInterfaces<Port>(false,false,false); });
	auto *d = f.Add(); auto ports = f.mManager.GetInterfaces<Port>(false,false,false);
	AT_PORTABLE_TEST_ASSERT(context, std::distance(ports.begin(),ports.end()) == 1 && *ports.begin() == static_cast<Port *>(d));
	vdrefptr<Device> unplug {new Device}; unplug->mpInit = [&] { f.mManager.RemoveDevice(unplug); };
	f.mManager.AddDevice(unplug);
	AT_PORTABLE_TEST_ASSERT(context, !unplug->HasManager() && unplug->mStats->mShutdown == 1 && f.mManager.GetDeviceCount() == 1);
	Fixture early; vdrefptr<Device> rejected {new Device};
	early.mManager.AddInitCallback([](IATDevice&) { throw MyError("Init callback failed"); });
	bool threw = false; try { early.mManager.AddDevice(rejected); } catch (const MyError&) { threw = true; }
	AT_PORTABLE_TEST_ASSERT(context, threw && !rejected->HasManager() && rejected->mStats->mShutdown == 1 && early.mManager.GetDeviceCount() == 0);
	Fixture self; std::shared_ptr<Stats> selfStats;
	self.mManager.AddInitCallback([&](IATDevice& p) {
		auto& device = static_cast<Device&>(p); selfStats = device.mStats;
		device.mpInit = [&, p = &device] { self.mManager.RemoveDevice(p); };
	});
	AT_PORTABLE_TEST_ASSERT(context, !self.mManager.AddDevice("probe",ATPropertySet()) && selfStats->mDestroyed == 1 && selfStats->mShutdown == 1);
	// Manager destruction releases all owned devices, even without explicit removal.
	std::shared_ptr<Stats> stats;
	{ ATDeviceManager manager; vdrefptr<Device> owned {new Device}; stats = owned->mStats; manager.AddDevice(owned); }
	AT_PORTABLE_TEST_ASSERT(context, stats->mShutdown == 1 && stats->mDestroyed == 1);
	return true;
}

bool ATTestAltirraDeviceCallbacks(ATPortableTestContext& context) {
	Fixture f; Observer a(f.mManager), b(f.mManager);
	a.mpAdded = [&] { f.mManager.RemoveDeviceChangeCallback(Port::kTypeID, &a); f.mManager.RemoveDeviceChangeCallback(Port::kTypeID, &b); };
	auto *d = f.Add(); AT_PORTABLE_TEST_ASSERT(context, a.mEvents == std::vector<std::string>{"added"} && b.mEvents.empty());
	f.mManager.RemoveDevice(d); AT_PORTABLE_TEST_ASSERT(context, a.mEvents.size() == 1 && b.mEvents.empty());
	Observer c(f.mManager); d = f.Add();
	uint32 status = 0; vdfunction<void(IATDevice&)> cb = [&](IATDevice& changed) { if (&changed == d) ++status; };
	f.mManager.AddDeviceStatusCallback(&cb); d->StatusChanged(); f.mManager.RemoveDeviceStatusCallback(&cb); d->StatusChanged();
	AT_PORTABLE_TEST_ASSERT(context, status == 1);
	f.mManager.RemoveDevice(d);
	AT_PORTABLE_TEST_ASSERT(context, c.mEvents == std::vector<std::string>({"added","removing","removed"}));
	auto *removed = f.Add("probe",false,91); auto stats = removed->mStats; uint32 valueAfterRemoval = 0;
	vdfunction<void(IATDevice&)> remove = [&](IATDevice& changed) { f.mManager.RemoveDevice(&changed); };
	vdfunction<void(IATDevice&)> inspect = [&](IATDevice& changed) { valueAfterRemoval = static_cast<Device&>(changed).mValue; };
	f.mManager.AddDeviceStatusCallback(&remove); f.mManager.AddDeviceStatusCallback(&inspect);
	removed->StatusChanged();
	f.mManager.RemoveDeviceStatusCallback(&remove); f.mManager.RemoveDeviceStatusCallback(&inspect);
	AT_PORTABLE_TEST_ASSERT(context, valueAfterRemoval == 91 && stats->mDestroyed == 1 && f.mManager.GetDeviceCount() == 0);
	return true;
}

bool ATTestAltirraDeviceServices(ATPortableTestContext& context) {
	struct Scheduling final : IATDeviceSchedulingService {
		Scheduling(ATScheduler& a, ATScheduler& b) : mFast(a), mSlow(b) {}
		ATScheduler *GetMachineScheduler() const override { return &mFast; }
		ATScheduler *GetSlowScheduler() const override { return &mSlow; }
		ATScheduler& mFast; ATScheduler& mSlow;
	};
	auto machine = std::make_unique<MainCPUPortableFixture>(); auto& h = *machine;
	ATCartridgePort port; Scheduling scheduling(h.mFastScheduler, h.mSlowScheduler);
	ATDeviceManager manager; manager.RegisterService(&h.mMemory); manager.RegisterService<IATDeviceSchedulingService>(&scheduling);
	manager.AddInitCallback([&](IATDevice& d) { if (auto *cart = vdpoly_cast<IATDeviceCartridge *>(&d)) cart->InitCartridge(&port); });
	vdrefptr<ATCartridgeEmulator> cart {new ATCartridgeEmulator(kATMemoryPri_Cartridge1, kATCartridgePriority_Default, false)};
	manager.AddDevice(cart); cart->LoadNewCartridge(kATCartridgeMode_XEGS_32K);
	auto *rom = static_cast<uint8 *>(cart->GetImage()->GetBuffer()); for (uint32 i=0; i<32768; ++i) rom[i] = static_cast<uint8>(i >> 12);
	AT_PORTABLE_TEST_ASSERT(context, h.mMemory.ReadByte(0x8000) == 0 && h.mMemory.ReadByte(0xA000) == 6 && port.IsLeftMapped());
	h.mMemory.WriteByte(0xD500, 2);
	AT_PORTABLE_TEST_ASSERT(context, h.mMemory.ReadByte(0x8000) == 4);
	manager.RemoveAllDevices(true);
	AT_PORTABLE_TEST_ASSERT(context, h.mMemory.ReadByte(0x8000) == 0xEA && !port.IsLeftMapped());
	return true;
}

bool ATTestAltirraDeviceReconfigure(ATPortableTestContext& context) {
	Fixture f; auto *hub = f.Add("hub"); auto *d = f.Add("probe", true, 7); hub->mBuses[1].AddChildDevice(d);
	ATPropertySet props; props.SetUint32("value", 19); const uint32 before = f.mManager.GetChangeCounter();
	f.mManager.ReconfigureDevice(*d, props);
	AT_PORTABLE_TEST_ASSERT(context, d->mValue == 19 && f.mManager.GetChangeCounter() == before + 1);
	auto old = d->mStats; props.SetBool("restart", true); props.SetUint32("value", 29);
	f.mManager.ReconfigureDevice(*d, props);
	auto *replacement = static_cast<Device *>(f.mManager.GetDeviceByTag("probe"));
	AT_PORTABLE_TEST_ASSERT(context, replacement && replacement->mValue == 29 && replacement->GetParent() == hub && replacement->GetParentBusIndex() == 1);
	AT_PORTABLE_TEST_ASSERT(context, old->mDestroyed == 1 && old->mShutdown == 1);
	return true;
}

bool ATTestAltirraDeviceSweep(ATPortableTestContext& context) {
	Fixture f; auto *root = f.Add("hub"); auto *child = f.Add("hub", true); root->mBuses[0].AddChildDevice(child);
	auto *leaf = f.Add("probe", true); child->mBuses[0].AddChildDevice(leaf); auto *orphan = f.Add("probe", true);
	vdfastvector<IATDevice *> garbage; f.mManager.MarkAndSweep(nullptr, 0, garbage);
	AT_PORTABLE_TEST_ASSERT(context, garbage.size() == 1 && garbage[0] == orphan);
	garbage.clear(); IATDevice *excluded = child; f.mManager.MarkAndSweep(&excluded, 1, garbage);
	AT_PORTABLE_TEST_ASSERT(context, garbage.size() == 2 && std::find(garbage.begin(),garbage.end(),leaf) != garbage.end());
	return true;
}
