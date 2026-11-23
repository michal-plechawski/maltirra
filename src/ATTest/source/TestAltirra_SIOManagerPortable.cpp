// Exercise the original SIO protocol and real chip-facing data paths.
#include <algorithm>
#include <array>
#include <memory>
#include <at/attest/portabletest.h>
#include "siomanagerportablefixture.h"

bool ATTestAltirraSIOCommandFrames(ATPortableTestContext& context) {
	auto f = std::make_unique<SIOPortableFixture>(); SIOTestDevice d; d.InitSIO(&f->mManager);
	f->Command(0x31, 0x52, 0x12, 0x34);
	AT_PORTABLE_TEST_ASSERT(context, d.mCommands.size() == 1);
	auto cmd = d.mCommands.back();
	AT_PORTABLE_TEST_ASSERT(context, cmd.mDevice == 0x31 && cmd.mCommand == 0x52 && cmd.mAUX[0] == 0x12 && cmd.mAUX[1] == 0x34 && cmd.mbStandardRate && !cmd.mbEarlyCmdDeassert);
	f->Command(0x31, 0x52, 0, 0, false);
	AT_PORTABLE_TEST_ASSERT(context, d.mCommands.size() == 1);
	f->Command(0x71, 0x40); f->Command(0x71, 0x40);
	AT_PORTABLE_TEST_ASSERT(context, d.mCommands.back().mPollCount == 1);
	f->Command(0x71, 0x40, 0x4F, 0x4F); f->Command(0x71, 0x40);
	AT_PORTABLE_TEST_ASSERT(context, d.mCommands.back().mPollCount == 0);
	d.mInterface->SetCommandDeassertCheckEnabled(true);
	const auto count = d.mCommands.size(); f->Command(0x71, 0x53, 0, 0, true, true);
	AT_PORTABLE_TEST_ASSERT(context, d.mCommands.size() == count);
	d.mInterface->SetCommandDeassertCheckEnabled(false);
	f->Command(0x71, 0x53, 0, 0, true, true);
	AT_PORTABLE_TEST_ASSERT(context, d.mCommands.back().mbEarlyCmdDeassert);
	return true;
}

bool ATTestAltirraSIOTimedSend(ATPortableTestContext& context) {
	auto f = std::make_unique<SIOPortableFixture>(); SIOTestDevice d; d.InitSIO(&f->mManager); f->ReceiveClock();
	const std::array<uint8, 4> data {0x11, 0xA6, 0xFF, 0x80};
	d.mpCommand = [&](const ATDeviceSIOCommand&) {
		d.mInterface->HandleCommand(data.data(), data.size(), true); return d.kCmdResponse_Start;
	};
	f->Command(0x71);
	auto result = f->Collect(15000);
	std::vector<uint8> expected {'A', 'C', data[0], data[1], data[2], data[3], ATComputeSIOChecksum(data.data(), data.size())};
	AT_PORTABLE_TEST_ASSERT(context, result == expected);
	AT_PORTABLE_TEST_ASSERT(context, d.mAborts == 0);
	vdrefptr<IATObjectState> state; d.mInterface->SaveActiveCommandState(~state);
	AT_PORTABLE_TEST_ASSERT(context, !state);
	return true;
}

bool ATTestAltirraSIOReceive(ATPortableTestContext& context) {
	for (bool bad : {false, true}) {
		auto f = std::make_unique<SIOPortableFixture>(); SIOTestDevice d; d.InitSIO(&f->mManager); f->ReceiveClock();
		d.mpCommand = [&](const ATDeviceSIOCommand&) {
			d.mInterface->BeginCommand(); d.mInterface->ReceiveData(17, 3, true);
			d.mInterface->InsertFence(99); d.mInterface->EndCommand(); return d.kCmdResponse_Start;
		};
		f->Command(0x71);
		const std::array<uint8, 3> bytes {0x10, 0x80, 0xFF};
		for (uint8 c : bytes) f->mManager.PokeyWriteSIO(c, false, 93, 0, false, false);
		f->mManager.PokeyWriteSIO(ATComputeSIOChecksum(bytes.data(), bytes.size()) ^ (bad ? 1 : 0), false, 93, 0, false, false);
		auto reply = f->Collect(6000);
		if (bad) {
			AT_PORTABLE_TEST_ASSERT(context, d.mReceived.empty() && d.mFences.empty() && reply == std::vector<uint8>({'N'}));
		} else {
			AT_PORTABLE_TEST_ASSERT(context, d.mReceiveIds == std::vector<uint32>({17}) && d.mReceived[0] == std::vector<uint8>(bytes.begin(), bytes.end()) && d.mChecksums[0]);
			AT_PORTABLE_TEST_ASSERT(context, d.mFences == std::vector<uint32>({99}) && reply == std::vector<uint8>({'A'}));
		}
	}
	return true;
}

bool ATTestAltirraSIORawLines(ATPortableTestContext& context) {
	auto f = std::make_unique<SIOPortableFixture>(); std::array<SIORawProbe, 3> devices;
	auto& m = f->mManager; for (auto& d : devices) m.AddRawDevice(&d);
	f->mMachine.mChips.mPIA.WriteByte(3, 0x3C);
	auto& a = devices[0]; auto& b = devices[1]; auto& c = devices[2];
	f->mMachine.Motor(true); m.SetReadyState(true); m.PokeySetBreak(true);
	AT_PORTABLE_TEST_ASSERT(context, a.mMotors == std::vector<bool>({true}) && a.mReady == std::vector<bool>({true}) && a.mBreaks == std::vector<bool>({true}));
	m.SetSIOInterrupt(&b, true); m.SetSIOInterrupt(&a, false); // another owner must remain asserted
	auto& pia = f->mMachine.mChips.mPIA;
	AT_PORTABLE_TEST_ASSERT(context, pia.ReadByte(3) & 0x80);
	(void)pia.ReadByte(1);
	m.SetSIOInterrupt(&b, true); // still low: no new edge
	AT_PORTABLE_TEST_ASSERT(context, !(pia.ReadByte(3) & 0x80));
	m.SetSIOInterrupt(&b, false); m.SetSIOInterrupt(&b, true);
	AT_PORTABLE_TEST_ASSERT(context, pia.ReadByte(3) & 0x80);
	(void)pia.ReadByte(1);
	m.SetSIOProceed(&b, true); m.SetSIOProceed(&a, false);
	(void)pia.ReadByte(0); m.SetSIOProceed(&b, true);
	AT_PORTABLE_TEST_ASSERT(context, !(pia.ReadByte(2) & 0x80));
	m.PokeyBeginWriteSIO(0xA5, false, 93); m.PokeyWriteSIO(0xA5, false, 93, 0, true, true); m.PokeySerInReady();
	AT_PORTABLE_TEST_ASSERT(context, c.mBytes == std::vector<uint8>({0xA5}) && c.mBegun == c.mBytes && c.mTruncates == 1 && c.mSendReady == 1);
	for (auto& d : devices) m.RemoveRawDevice(&d);
	return true;
}

bool ATTestAltirraSIORawReentry(ATPortableTestContext& context) {
	auto f = std::make_unique<SIOPortableFixture>(); std::array<SIORawProbe, 4> devices; auto& m = f->mManager;
	m.AddRawDevice(&devices[0]);
	devices[0].mpCommand = [&] { m.AddRawDevice(&devices[1]); };
	m.PokeyBeginCommand(); m.PokeyEndCommand();
	AT_PORTABLE_TEST_ASSERT(context, devices[1].mCommands == std::vector<bool>({false}));
	m.AddRawDevice(&devices[2]); m.AddRawDevice(&devices[3]);
	devices[0].mpCommand = [&] { m.RemoveRawDevice(&devices[1]); m.RemoveRawDevice(&devices[2]); };
	m.PokeyBeginCommand(); m.PokeyEndCommand();
	AT_PORTABLE_TEST_ASSERT(context, devices[3].mCommands == std::vector<bool>({true, false}));
	AT_PORTABLE_TEST_ASSERT(context, devices[1].mCommands == std::vector<bool>({false}) && devices[2].mCommands.empty());
	for (auto& d : devices) m.RemoveRawDevice(&d);
	return true;
}

bool ATTestAltirraSIOClocks(ATPortableTestContext& context) {
	auto f = std::make_unique<SIOPortableFixture>(); std::array<SIORawProbe, 3> devices;
	auto& m = f->mManager; auto& p = f->mMachine.mChips.mPOKEY;
	for (auto& d : devices) m.AddRawDevice(&d);
	p.WriteByte(0x0F, 3); // external input clock
	m.SetExternalClock(&devices[0], 0, 100); AT_PORTABLE_TEST_ASSERT(context, m.GetCyclesPerBitRecv() == 100);
	m.SetExternalClock(&devices[1], 0, 50); AT_PORTABLE_TEST_ASSERT(context, m.GetCyclesPerBitRecv() == 50);
	m.SetExternalClock(&devices[0], 0, 25); AT_PORTABLE_TEST_ASSERT(context, m.GetCyclesPerBitRecv() == 25);
	m.RemoveRawDevice(&devices[0]); AT_PORTABLE_TEST_ASSERT(context, m.GetCyclesPerBitRecv() == 50);
	m.RemoveRawDevice(&devices[1]); AT_PORTABLE_TEST_ASSERT(context, m.GetCyclesPerBitRecv() == 0);
	m.SetBiClockNotifyEnabled(&devices[2], true);
	p.WriteByte(8, 0x28); p.WriteByte(4, 40); p.WriteByte(6, 0); p.WriteByte(0x0F, 0x23); f->Tick(114);
	AT_PORTABLE_TEST_ASSERT(context, !devices[2].mRates.empty() && devices[2].mRates.back() == m.GetCyclesPerBitBiClock());
	const auto count = devices[2].mRates.size(); m.RemoveRawDevice(&devices[2]);
	p.WriteByte(4, 50); f->Tick(114);
	AT_PORTABLE_TEST_ASSERT(context, devices[2].mRates.size() == count);
	return true;
}

bool ATTestAltirraSIOSerialWire(ATPortableTestContext& context) {
	auto f = std::make_unique<SIOPortableFixture>(); SIOTestDevice d; d.InitSIO(&f->mManager); f->ReceiveClock();
	d.mpCommand = [](const ATDeviceSIOCommand&) { return IATDeviceSIO::kCmdResponse_Send_ACK_Complete; };
	auto& p = f->mMachine.mChips.mPOKEY;
	std::array<uint8, 5> frame {0x71, 0x53, 0x12, 0x34, 0}; frame[4] = ATComputeSIOChecksum(frame.data(), 4);
	p.WriteByte(0x0F, 0x23); p.SetCommandLine(true);
	for (uint8 c : frame) { p.WriteByte(0x0D, c); f->Tick(1300); }
	p.WriteByte(0x0F, 0x13); p.SetCommandLine(false);
	AT_PORTABLE_TEST_ASSERT(context, d.mCommands.size() == 1 && d.mCommands[0].mbStandardRate && d.mCommands[0].mAUX[1] == 0x34);
	AT_PORTABLE_TEST_ASSERT(context, f->Collect(6000) == std::vector<uint8>({'A', 'C'}));
	return true;
}
