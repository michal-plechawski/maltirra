// Altirra portable R-Verter tests

#include <deque>
#include <cstring>
#include <utility>
#include <vector>

#include <at/atcore/deviceimpl.h>
#include <at/atcore/deviceparent.h>
#include <at/atcore/deviceserial.h>
#include <at/atcore/devicesio.h>
#include <at/atcore/propertyset.h>
#include <at/atcore/scheduler.h>
#include <at/attest/portabletest.h>
#include <rverter.h>

namespace {
	class SchedulingService final : public IATDeviceSchedulingService {
	public:
		explicit SchedulingService(ATScheduler& scheduler) : mScheduler(scheduler) {}
		ATScheduler *GetMachineScheduler() const override { return &mScheduler; }
		ATScheduler *GetSlowScheduler() const override { return &mScheduler; }

		ATScheduler& mScheduler;
	};

	class RVerterDeviceManager final : public IATDeviceManager {
	public:
		explicit RVerterDeviceManager(IATDeviceSchedulingService& schedulingService)
			: mSchedulingService(schedulingService)
		{
		}

		void *GetService(uint32 iid) override {
			return iid == IATDeviceSchedulingService::kTypeID ? &mSchedulingService : nullptr;
		}

		void NotifyDeviceStatusChanged(IATDevice&) override {}

		IATDeviceSchedulingService& mSchedulingService;
	};

	class TestSerialDevice final : public ATDeviceT<IATDeviceSerial> {
	public:
		void GetDeviceInfo(ATDeviceInfo& info) override { info.mpDef = nullptr; }

		void SetOnStatusChange(const vdfunction<void(const ATDeviceSerialStatus&)>& fn) override {
			mOnStatusChange = fn;
		}

		void SetTerminalState(const ATDeviceSerialTerminalState& state) override {
			mTerminalState = state;
			++mTerminalStateChanges;
		}

		ATDeviceSerialStatus GetStatus() override { return mStatus; }

		void SetOnReadReady(vdfunction<void()> fn) override {
			mOnReadReady = std::move(fn);
		}

		bool Read(uint32 baudRate, uint8& c, bool& framingError) override {
			uint32 actualBaudRate = 0;
			if (!Read(actualBaudRate, c))
				return false;

			framingError = false;
			return actualBaudRate == baudRate;
		}

		bool Read(uint32& baudRate, uint8& c) override {
			if (mReads.empty())
				return false;

			baudRate = mReads.front().first;
			c = mReads.front().second;
			mReads.pop_front();
			return true;
		}

		void Write(uint32 baudRate, uint8 c) override {
			mWrites.emplace_back(baudRate, c);
		}

		void FlushBuffers() override {
			mReads.clear();
			mWrites.clear();
		}

		void SetStatus(const ATDeviceSerialStatus& status) {
			mStatus = status;
			if (mOnStatusChange)
				mOnStatusChange(mStatus);
		}

		void QueueRead(uint32 baudRate, uint8 c) {
			mReads.emplace_back(baudRate, c);
			if (mOnReadReady)
				mOnReadReady();
		}

		vdfunction<void(const ATDeviceSerialStatus&)> mOnStatusChange;
		vdfunction<void()> mOnReadReady;
		ATDeviceSerialTerminalState mTerminalState {};
		ATDeviceSerialStatus mStatus {};
		uint32 mTerminalStateChanges = 0;
		std::deque<std::pair<uint32, uint8>> mReads;
		std::vector<std::pair<uint32, uint8>> mWrites;
	};

	class TestSIOManager final : public IATDeviceSIOManager {
	public:
		vdrefptr<IATDeviceSIOInterface> AddDevice(IATDeviceSIO *) override { return nullptr; }
		sint32 GetHighSpeedIndex() const override { return -1; }
		uint32 GetCyclesPerBitRecv() const override { return 0; }
		uint32 GetRecvResetCounter() const override { return 0; }
		uint32 GetCyclesPerBitSend() const override { return 0; }
		uint32 GetCyclesPerBitBiClock() const override { return 0; }

		void AddRawDevice(IATDeviceRawSIO *dev) override { mpRawDevice = dev; }
		void RemoveRawDevice(IATDeviceRawSIO *dev) override {
			if (mpRawDevice == dev)
				mpRawDevice = nullptr;
		}

		void SendRawByte(uint8 byte, uint32 cyclesPerBit, bool, bool, bool) override {
			mReceivedBytes.emplace_back(cyclesPerBit, byte);
		}

		void SetRawInput(bool input) override { mbRawInput = input; }
		bool IsSIOCommandAsserted() const override { return false; }
		bool IsSIOMotorAsserted() const override { return false; }
		bool IsSIOReadyAsserted() const override { return true; }
		bool IsSIOForceBreakAsserted() const override { return false; }

		void SetSIOInterrupt(IATDeviceRawSIO *, bool state) override { mbInterrupt = state; }
		void SetSIOProceed(IATDeviceRawSIO *, bool state) override { mbProceed = state; }
		void SetBiClockNotifyEnabled(IATDeviceRawSIO *, bool enabled) override { mbBiClockNotify = enabled; }
		void SetExternalClock(IATDeviceRawSIO *, uint32 initialOffset, uint32 period) override {
			mExternalClockOffset = initialOffset;
			mExternalClockPeriod = period;
		}

		IATDeviceRawSIO *mpRawDevice = nullptr;
		bool mbRawInput = false;
		bool mbInterrupt = false;
		bool mbProceed = false;
		bool mbBiClockNotify = false;
		uint32 mExternalClockOffset = 0;
		uint32 mExternalClockPeriod = 0;
		std::vector<std::pair<uint32, uint8>> mReceivedBytes;
	};

	void RunNextEvent(ATScheduler& scheduler) {
		scheduler.mNextEventCounter += scheduler.GetTicksToNextEvent();
		scheduler.ProcessNextEvent();
	}
}

bool ATTestAltirraRVerter(ATPortableTestContext& context) {
	ATScheduler scheduler;
	scheduler.SetRate(VDFraction(1790000, 1));
	SchedulingService schedulingService(scheduler);
	RVerterDeviceManager deviceManager(schedulingService);
	TestSIOManager sioManager;

	ATPropertySet settings;
	vdrefptr<IATDevice> device;
	g_ATDeviceDefRVerter.mpFactoryFn(settings, ~device);
	ATDeviceInfo deviceInfo {};
	device->GetDeviceInfo(deviceInfo);
	AT_PORTABLE_TEST_ASSERT(context, deviceInfo.mpDef == &g_ATDeviceDefRVerter);

	device->SetManager(&deviceManager);
	device->Init();
	IATDeviceSIO *deviceSIO = vdpoly_cast<IATDeviceSIO *>(device);
	IATDeviceRawSIO *rawSIO = vdpoly_cast<IATDeviceRawSIO *>(device);
	IATDeviceParent *parent = vdpoly_cast<IATDeviceParent *>(device);
	AT_PORTABLE_TEST_ASSERT(context, deviceSIO && rawSIO && parent);
	deviceSIO->InitSIO(&sioManager);
	AT_PORTABLE_TEST_ASSERT(context, sioManager.mpRawDevice == rawSIO);

	IATDeviceBus *serialBus = parent->GetDeviceBus(0);
	AT_PORTABLE_TEST_ASSERT(context, serialBus != nullptr);
	AT_PORTABLE_TEST_ASSERT(context, parent->GetDeviceBus(1) == nullptr);
	AT_PORTABLE_TEST_ASSERT(context, !strcmp(serialBus->GetBusTag(), "serial"));
	AT_PORTABLE_TEST_ASSERT(context, !strcmp(serialBus->GetSupportedType(0), "serial"));

	vdrefptr<TestSerialDevice> serial(new TestSerialDevice);
	serialBus->AddChildDevice(serial);
	AT_PORTABLE_TEST_ASSERT(context, serial->GetParent() == parent);
	AT_PORTABLE_TEST_ASSERT(context, serial->mTerminalState.mbDataTerminalReady);
	AT_PORTABLE_TEST_ASSERT(context, !serial->mTerminalState.mbRequestToSend);
	RunNextEvent(scheduler);

	ATDeviceSerialStatus connectedStatus {};
	connectedStatus.mbCarrierDetect = true;
	connectedStatus.mbDataSetReady = true;
	serial->SetStatus(connectedStatus);
	AT_PORTABLE_TEST_ASSERT(context, !sioManager.mbProceed && !sioManager.mbInterrupt);
	rawSIO->OnMotorStateChanged(true);
	AT_PORTABLE_TEST_ASSERT(context, sioManager.mbProceed && sioManager.mbInterrupt);
	AT_PORTABLE_TEST_ASSERT(context, serial->mTerminalState.mbRequestToSend);

	rawSIO->OnReceiveByte(0x33, false, 100);
	AT_PORTABLE_TEST_ASSERT(context, serial->mWrites.size() == 1);
	AT_PORTABLE_TEST_ASSERT(context, serial->mWrites[0] == std::make_pair(UINT32_C(17900), uint8(0x33)));
	rawSIO->OnReceiveByte(0x44, false, 0);
	AT_PORTABLE_TEST_ASSERT(context, serial->mWrites.size() == 1);

	serial->QueueRead(9600, 0xA5);
	RunNextEvent(scheduler);
	AT_PORTABLE_TEST_ASSERT(context, sioManager.mReceivedBytes.size() == 1);
	AT_PORTABLE_TEST_ASSERT(context,
		sioManager.mReceivedBytes[0] == std::make_pair(UINT32_C(186), uint8(0xA5)));

	serial->SetStatus({});
	AT_PORTABLE_TEST_ASSERT(context, !sioManager.mbProceed && !sioManager.mbInterrupt);
	rawSIO->OnMotorStateChanged(false);
	AT_PORTABLE_TEST_ASSERT(context, !serial->mTerminalState.mbRequestToSend);
	serial->QueueRead(19200, 0x5A);
	RunNextEvent(scheduler);
	AT_PORTABLE_TEST_ASSERT(context, sioManager.mReceivedBytes.size() == 1);

	device->Shutdown();
	AT_PORTABLE_TEST_ASSERT(context, sioManager.mpRawDevice == nullptr);
	AT_PORTABLE_TEST_ASSERT(context, serial->GetParent() == nullptr);
	device->SetManager(nullptr);

	ATScheduler adapterScheduler;
	adapterScheduler.SetRate(VDFraction(1790000, 1));
	SchedulingService adapterSchedulingService(adapterScheduler);
	RVerterDeviceManager adapterDeviceManager(adapterSchedulingService);
	TestSIOManager adapterSIOManager;
	vdrefptr<IATDevice> adapter;
	g_ATDeviceDefSIOSerialAdapter.mpFactoryFn(settings, ~adapter);
	adapter->SetManager(&adapterDeviceManager);
	adapter->Init();
	IATDeviceSIO *adapterDeviceSIO = vdpoly_cast<IATDeviceSIO *>(adapter);
	IATDeviceRawSIO *adapterRawSIO = vdpoly_cast<IATDeviceRawSIO *>(adapter);
	IATDeviceParent *adapterParent = vdpoly_cast<IATDeviceParent *>(adapter);
	adapterDeviceSIO->InitSIO(&adapterSIOManager);
	vdrefptr<TestSerialDevice> adapterSerial(new TestSerialDevice);
	adapterParent->GetDeviceBus(0)->AddChildDevice(adapterSerial);
	RunNextEvent(adapterScheduler);
	adapterSerial->QueueRead(9600, 0xC3);
	RunNextEvent(adapterScheduler);
	AT_PORTABLE_TEST_ASSERT(context, adapterSIOManager.mReceivedBytes.size() == 1);
	AT_PORTABLE_TEST_ASSERT(context, adapterSIOManager.mReceivedBytes[0].second == 0xC3);
	adapterRawSIO->OnMotorStateChanged(true);
	AT_PORTABLE_TEST_ASSERT(context, !adapterSerial->mTerminalState.mbRequestToSend);

	adapter->Shutdown();
	adapter->SetManager(nullptr);
	return true;
}
