// Acceleration, OS hooks, cassette integration and state restoration of real SIO.
#include <algorithm>
#include <array>
#include <memory>
#include <vd2/system/file.h>
#include <at/atcore/savestate.h>
#include <at/attest/portabletest.h>
#include <savestateio.h>
#include "siomanagerportablefixture.h"

namespace {
	vdrefptr<IATObjectState> RoundTrip(IATObjectState& state) {
		VDMemoryBufferStream stream;
		std::unique_ptr<IATSaveStateSerializer> writer(ATCreateSaveStateSerializer());
		writer->Serialize(stream, state, L"SIO portable state"); stream.Seek(0);
		std::unique_ptr<IATSaveStateDeserializer> reader(ATCreateSaveStateDeserializer());
		vdrefptr<IATSerializable> result; reader->Deserialize(stream, ~result);
		return vdrefptr<IATObjectState>(static_cast<IATObjectState *>(result.get()));
	}
}

bool ATTestAltirraSIOAcceleration(ATPortableTestContext& context) {
	auto f = std::make_unique<SIOPortableFixture>(); SIOTestDevice first, d;
	first.InitSIO(&f->mManager); d.InitSIO(&f->mManager); f->mManager.SetOtherSIOAccelEnabled(true);
	const std::array<uint8, 4> data {0x11, 0xA6, 0xFF, 0x80};
	d.mpAccel = [&](const ATDeviceSIORequest&) { d.mInterface->HandleCommand(data.data(), data.size(), true); return d.kCmdResponse_Start; };
	auto req = f->Request();
	AT_PORTABLE_TEST_ASSERT(context, f->mManager.TryAccelRequest(req, false));
	for (size_t i = 0; i < data.size(); ++i) AT_PORTABLE_TEST_ASSERT(context, f->Mem(req.mAddress + i) == data[i]);
	AT_PORTABLE_TEST_ASSERT(context, f->Mem(ATKernelSymbols::DSTATS) == 1 && f->Mem(ATKernelSymbols::STATUS) == 1 && f->Mem(ATKernelSymbols::TIMFLG) == 1);
	AT_PORTABLE_TEST_ASSERT(context, f->mCPU.GetA() == 0 && f->mCPU.GetX() == 0xFE && f->mCPU.GetY() == 1 && (f->mCPU.GetP() & AT6502::kFlagC));
	AT_PORTABLE_TEST_ASSERT(context, f->Mem(ATKernelSymbols::CHKSUM) == ATComputeSIOChecksum(data.data(), data.size()) && d.mInterface->GetAccelTimeSkew() > 8000);
	req.mLength = 6; AT_PORTABLE_TEST_ASSERT(context, f->mManager.TryAccelRequest(req, false));
	AT_PORTABLE_TEST_ASSERT(context, f->Mem(ATKernelSymbols::DSTATS) == 0x8A && f->Mem(ATKernelSymbols::TIMFLG) == 0);
	req.mLength = 2; AT_PORTABLE_TEST_ASSERT(context, f->mManager.TryAccelRequest(req, false));
	AT_PORTABLE_TEST_ASSERT(context, f->Mem(ATKernelSymbols::DSTATS) == 0x8F);
	for (auto response : {d.kCmdResponse_Fail_NAK, d.kCmdResponse_Send_ACK_Complete}) {
		d.mpAccel = [response](const ATDeviceSIORequest&) { return response; };
		AT_PORTABLE_TEST_ASSERT(context, f->mManager.TryAccelRequest(req, false));
		AT_PORTABLE_TEST_ASSERT(context, f->Mem(ATKernelSymbols::DSTATS) == (response == d.kCmdResponse_Fail_NAK ? 0x8B : 1));
	}
	d.mpAccel = [&](const ATDeviceSIORequest&) { d.mInterface->BeginCommand(); d.mInterface->ReceiveData(7, data.size(), true); d.mInterface->EndCommand(); return d.kCmdResponse_Start; };
	req = f->Request(0x71, 0x80);
	for (size_t i = 0; i < data.size(); ++i) f->Mem(req.mAddress + i, data[i]);
	AT_PORTABLE_TEST_ASSERT(context, f->mManager.TryAccelRequest(req, false));
	AT_PORTABLE_TEST_ASSERT(context, d.mReceived.back() == std::vector<uint8>(data.begin(), data.end()) && d.mChecksums.back() && d.mReceiveIds.back() == 7);
	req.mLength = 2;
	AT_PORTABLE_TEST_ASSERT(context, f->mManager.TryAccelRequest(req, false));
	AT_PORTABLE_TEST_ASSERT(context, !d.mChecksums.back() && d.mReceived.back()[2] == 0 && d.mReceived.back()[3] == 0);
	// The second device in the chain must inherit the disk ID too, so a later
	// override check recognizes that the drive has already been accessed.
	f->mManager.SetDiskSIOAccelEnabled(true); req = f->Request(0x31);
	d.mpAccel = [](const ATDeviceSIORequest&) { return IATDeviceSIO::kCmdResponse_Send_ACK_Complete; };
	AT_PORTABLE_TEST_ASSERT(context, f->mManager.TryAccelRequest(req, false));
	f->mbDetectOverride = true;
	AT_PORTABLE_TEST_ASSERT(context, f->mManager.TryAccelRequest(req, false));
	return true;
}

bool ATTestAltirraSIOAccelGuards(ATPortableTestContext& context) {
	auto f = std::make_unique<SIOPortableFixture>(); SIOTestDevice d; d.InitSIO(&f->mManager);
	f->mManager.SetOtherSIOAccelEnabled(true);
	d.mpAccel = [](const ATDeviceSIORequest&) { return IATDeviceSIO::kCmdResponse_Send_ACK_Complete; };
	auto req = f->Request();
	f->mCPU.SetP(0x24); AT_PORTABLE_TEST_ASSERT(context, !f->mManager.TryAccelRequest(req, false)); f->mCPU.SetP(0x20);
	f->Mem(ATKernelSymbols::BRKKEY, 0); AT_PORTABLE_TEST_ASSERT(context, !f->mManager.TryAccelRequest(req, false)); f->Mem(ATKernelSymbols::BRKKEY, 0xFF);
	req.mMode = 0xC0; AT_PORTABLE_TEST_ASSERT(context, !f->mManager.TryAccelRequest(req, false)); req.mMode = 0x40;
	req.mAddress = ATKernelSymbols::TIMFLG; AT_PORTABLE_TEST_ASSERT(context, !f->mManager.TryAccelRequest(req, false));
	req.mAddress = 0xFFFE; req.mLength = 0x320;
	AT_PORTABLE_TEST_ASSERT(context, !f->mManager.TryAccelRequest(req, false)); // wrapped read still overlaps TIMFLG
	AT_PORTABLE_TEST_ASSERT(context, d.mRequests.empty());
	req = f->Request(); d.mpAccel = [](const ATDeviceSIORequest&) { return IATDeviceSIO::kCmdResponse_BypassAccel; };
	AT_PORTABLE_TEST_ASSERT(context, !f->mManager.TryAccelRequest(req, false));
	f->mbFastBoot = true; d.mpAccel = nullptr;
	req.mCommand = 0x40; req.mAUX[0] = req.mAUX[1] = 0;
	const auto n = d.mRequests.size();
	AT_PORTABLE_TEST_ASSERT(context, !f->mManager.TryAccelRequest(req, false) && d.mRequests.size() == n + 27);
	f->mbDetectOverride = true; f->mManager.SetDiskSIOAccelEnabled(true);
	req = f->Request(0x31);
	AT_PORTABLE_TEST_ASSERT(context, !f->mManager.TryAccelRequest(req, false));
	d.mpCommand = [](const ATDeviceSIOCommand&) { return IATDeviceSIO::kCmdResponse_Send_ACK_Complete; };
	f->Command(0x31); f->Tick(6000);
	d.mpAccel = [](const ATDeviceSIORequest&) { return IATDeviceSIO::kCmdResponse_Send_ACK_Complete; };
	AT_PORTABLE_TEST_ASSERT(context, f->mManager.TryAccelRequest(req, false));
	f->mManager.WarmReset();
	AT_PORTABLE_TEST_ASSERT(context, !f->mManager.TryAccelRequest(req, false));
	f->mbDetectOverride = false; d.mpAccel = nullptr;
	AT_PORTABLE_TEST_ASSERT(context, f->mManager.TryAccelRequest(req, false) && f->Mem(ATKernelSymbols::DSTATS) == 0x8A);
	f->mDiskClients[0] = 2;
	AT_PORTABLE_TEST_ASSERT(context, !f->mManager.TryAccelRequest(req, false));
	f->mDiskClients[0] = 0; f->mDiskEnabled[0] = true;
	AT_PORTABLE_TEST_ASSERT(context, !f->mManager.TryAccelRequest(req, false));
	req.mDevice = 0x4F;
	AT_PORTABLE_TEST_ASSERT(context, f->mManager.TryAccelRequest(req, false));
	f->mbFastBoot = false;
	AT_PORTABLE_TEST_ASSERT(context, !f->mManager.TryAccelRequest(req, false));
	return true;
}

bool ATTestAltirraSIOHooksCassette(ATPortableTestContext& context) {
	auto f = std::make_unique<SIOPortableFixture>(); auto& m = f->mManager; SIOTestDevice d; d.InitSIO(&m);
	m.SetOtherSIOAccelEnabled(true); m.SetSIOPatchEnabled(true);
	d.mpAccel = [](const ATDeviceSIORequest& req) { return req.mDevice == 0x71 ? IATDeviceSIO::kCmdResponse_Send_ACK_Complete : IATDeviceSIO::kCmdResponse_NotHandled; };
	const std::array<uint8, 12> dcb {0x71, 1, 0x53, 0x40, 0, 0x40, 1, 0, 4, 0, 1, 0};
	for (size_t i = 0; i < dcb.size(); ++i) f->Mem(ATKernelSymbols::DDEVIC + i, dcb[i]);
	f->Mem(ATKernelSymbols::CRITIC, 3);
	AT_PORTABLE_TEST_ASSERT(context, f->mHooks.OnHookHit(ATKernelSymbols::SIOV) == 0x60 && f->Mem(ATKernelSymbols::CRITIC) == 0);
	f->Mem(ATKernelSymbols::CRITIC, 7); m.TryAccelPBIRequest(true);
	AT_PORTABLE_TEST_ASSERT(context, f->Mem(ATKernelSymbols::CRITIC) == 7 && (f->mCPU.GetP() & AT6502::kFlagC));
	m.TryAccelPBIRequest(false); AT_PORTABLE_TEST_ASSERT(context, !(f->mCPU.GetP() & AT6502::kFlagC));
	m.SetSIOPatchEnabled(false); AT_PORTABLE_TEST_ASSERT(context, f->mHooks.OnHookHit(ATKernelSymbols::SIOV) == 0);
	f->mbCassettePatch = true;
	const std::array<uint8, 3> bytes {0x55, 0x55, 0xFC}; vdrefptr<IATCassetteImage> image; ATCreateNewCassetteImage(~image);
	ATCassetteWriteCursor cursor {}; image->WriteBlankData(cursor, 4000, false);
	for (uint8 c : bytes) image->WriteStdData(cursor, c, 600, false);
	image->WriteStdData(cursor, ATComputeSIOChecksum(bytes.data(), bytes.size()), 600, false); image->WriteBlankData(cursor, 2000, false);
	auto& tape = *f->mMachine.mTape; tape.Load(image, nullptr, false); tape.Play();
	auto req = f->Request(0x5F, 0x40, bytes.size()); req.mTimeout = 0;
	AT_PORTABLE_TEST_ASSERT(context, m.TryAccelRequest(req, false) && f->Mem(ATKernelSymbols::DSTATS) == 1);
	for (size_t i = 0; i < bytes.size(); ++i) AT_PORTABLE_TEST_ASSERT(context, f->Mem(req.mAddress + i) == bytes[i]);
	AT_PORTABLE_TEST_ASSERT(context, f->mActivity == std::vector<uint32>({1 << 16}));
	tape.LoadNew(); f->mMachine.Motor(true); req.mMode = 0x80; f->Mem(ATKernelSymbols::DAUX2, 0x80);
	AT_PORTABLE_TEST_ASSERT(context, m.TryAccelRequest(req, false) && tape.IsImageDirty() && tape.GetSampleLen() > 0);
	AT_PORTABLE_TEST_ASSERT(context, f->mActivity.size() == 2);
	return true;
}

bool ATTestAltirraSIOSnapshot(ATPortableTestContext& context) {
	auto f = std::make_unique<SIOPortableFixture>(); SIOTestDevice d; d.InitSIO(&f->mManager);
	std::array<uint8, 5> frame {0x71, 0x53, 0, 0, 0}; frame[4] = ATComputeSIOChecksum(frame.data(), 4);
	f->mMachine.mChips.mPOKEY.SetCommandLine(true);
	for (int i = 0; i < 2; ++i) f->mManager.PokeyWriteSIO(frame[i], true, 93, 0, false, false);
	auto saved = f->mManager.SaveState(); AT_PORTABLE_TEST_ASSERT(context, saved);
	saved = RoundTrip(*saved);
	f->mManager.PreLoadState(); f->mManager.LoadState(saved); f->mManager.PostLoadState();
	for (int i = 2; i < 5; ++i) f->mManager.PokeyWriteSIO(frame[i], true, 93, 0, false, false);
	f->mMachine.mChips.mPOKEY.SetCommandLine(false);
	AT_PORTABLE_TEST_ASSERT(context, d.mCommands.size() == 1 && d.mCommands[0].mDevice == 0x71 && !f->mManager.SaveState());
	// Restore an active receive, including partial data, queued replies and fences.
	d.mpCommand = [&](const ATDeviceSIOCommand&) {
		d.mInterface->BeginCommand(); d.mInterface->ReceiveData(9, 3, true);
		d.mInterface->InsertFence(23); d.mInterface->EndCommand(); return d.kCmdResponse_Start;
	};
	f->ReceiveClock(); f->Command(0x71);
	f->mManager.PokeyWriteSIO(0x11, false, 93, 0, false, false);
	d.mInterface->SaveActiveCommandState(~saved); AT_PORTABLE_TEST_ASSERT(context, saved);
	saved = RoundTrip(*saved);
	f->mManager.PreLoadState(); d.mInterface->LoadActiveCommandState(saved); f->mManager.PostLoadState();
	for (uint8 c : {0x22, 0x33, 0x66}) f->mManager.PokeyWriteSIO(c, false, 93, 0, false, false);
	AT_PORTABLE_TEST_ASSERT(context, f->Collect(6000) == std::vector<uint8>({'A'}));
	AT_PORTABLE_TEST_ASSERT(context, d.mReceived == std::vector<std::vector<uint8>>({{0x11, 0x22, 0x33}}) && d.mFences == std::vector<uint32>({23}) && d.mAborts == 1);
	// A partially transmitted response resumes at the saved byte index and delay.
	const std::array<uint8, 4> bytes {0x12, 0x34, 0x56, 0x78};
	d.mpCommand = [&](const ATDeviceSIOCommand&) { d.mInterface->HandleCommand(bytes.data(), bytes.size(), true); return d.kCmdResponse_Start; };
	f->Command(0x71); auto initial = f->Collect(2300);
	d.mInterface->SaveActiveCommandState(~saved); AT_PORTABLE_TEST_ASSERT(context, saved); saved = RoundTrip(*saved);
	auto remaining = f->Collect(10000);
	f->mManager.PreLoadState(); d.mInterface->LoadActiveCommandState(saved); f->mManager.PostLoadState();
	AT_PORTABLE_TEST_ASSERT(context, f->Collect(10000) == remaining && !initial.empty() && !remaining.empty());
	return true;
}

bool ATTestAltirraSIOLifecycle(ATPortableTestContext& context) {
	auto f = std::make_unique<SIOPortableFixture>(); SIOTestDevice d; d.InitSIO(&f->mManager); SIORawProbe raw;
	f->mManager.AddRawDevice(&raw); f->mManager.SetSIOInterrupt(&raw, true); f->mManager.SetSIOProceed(&raw, true);
	f->mManager.SetExternalClock(&raw, 0, 100); f->mManager.SetBiClockNotifyEnabled(&raw, true);
	d.mpCommand = [&](const ATDeviceSIOCommand&) {
		d.mInterface->BeginCommand(); d.mInterface->Delay(1000); d.mInterface->InsertFence(7); d.mInterface->EndCommand(); return d.kCmdResponse_Start;
	};
	f->Command(0x71); f->mManager.Shutdown(); f->mManager.Shutdown();
	AT_PORTABLE_TEST_ASSERT(context, d.mAborts == 1 && d.mFences.empty());
	d.mInterface = nullptr; f->Tick(4000);
	AT_PORTABLE_TEST_ASSERT(context, d.mFences.empty());
	return true;
}
