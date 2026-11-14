// Covox DAC tests through its production bus, scheduler, renderer, and snapshots.
#include <algorithm>
#include <memory>
#include <string>
#include <at/attest/portabletest.h>
#include <at/atcore/consoleoutput.h>
#include <at/atcore/snapshotimpl.h>
#include <at/atcore/savestate.h>
#include <covox.h>
#include <savestateio.h>
#include <vd2/system/file.h>
#include "covoxportablefixture.h"
#include "maincpuportablefixture.h"

namespace {
	struct Fixture {
		explicit Fixture(bool four = true) {
			mDAC.SetFourChannels(four);
			mDAC.Init(&mHost.mMemory, &mHost.mScheduler, &mHost.mAudio);
		}
		CovoxPortableHost mHost;
		ATCovoxEmulator mDAC;
	};
	struct Status final : ATConsoleOutput {
		void WriteLine(const char *s) override { mText += s; }
		std::string mText;
	};
	std::string Channels(ATCovoxEmulator& dac) {
		Status s; dac.DumpStatus(s); return s.mText;
	}
	bool IsNeutral(const CovoxAudioHost::Output& out) {
		return std::all_of(out.mLeft.begin(), out.mLeft.end(), [](float v) { return v == 0; })
			&& std::all_of(out.mRight.begin(), out.mRight.end(), [](float v) { return v == 0; });
	}
}

bool ATTestAltirraCovoxMemory(ATPortableTestContext& context) {
	auto owner = std::make_unique<Fixture>();
	auto& f = *owner;
	auto& h = f.mHost;
	AT_PORTABLE_TEST_ASSERT(context, h.mAudio.mpSource == &f.mDAC && h.mAudio.mAdds == 1);
	for (uint32 i = 0; i < 4; ++i)
		h.mMemory.WriteByte(0xD600 + i, static_cast<uint8>(0x10 * (i + 1)));
	AT_PORTABLE_TEST_ASSERT(context, Channels(f.mDAC).find("$10 $20 $30 $40") != std::string::npos);
	AT_PORTABLE_TEST_ASSERT(context, h.mRAM[0xD600] == 0x10 && h.mRAM[0xD603] == 0x40);
	h.mMemory.WriteByte(0xD6FC, 0x55); // register aliases repeat every four addresses
	AT_PORTABLE_TEST_ASSERT(context, Channels(f.mDAC).find("$55 $20 $30 $40") != std::string::npos);
	h.mMemory.WriteByte(0xD5FF, 0x99);
	h.mMemory.WriteByte(0xD700, 0x99);
	const std::string before = Channels(f.mDAC);
	AT_PORTABLE_TEST_ASSERT(context, h.mMemory.ReadByte(0xD600) == 0x10);
	AT_PORTABLE_TEST_ASSERT(context, h.mMemory.DebugReadByte(0xD601) == 0x20);
	AT_PORTABLE_TEST_ASSERT(context, Channels(f.mDAC) == before);
	f.mDAC.SetEnabled(false);
	h.mMemory.WriteByte(0xD600, 0x77);
	AT_PORTABLE_TEST_ASSERT(context, Channels(f.mDAC) == before && h.mRAM[0xD600] == 0x77);
	f.mDAC.SetEnabled(true);
	h.mMemory.WriteByte(0xD600, 0x66);
	AT_PORTABLE_TEST_ASSERT(context, Channels(f.mDAC).find("$66 $20 $30 $40") != std::string::npos);

	// Inclusive range spanning two pages, with writes consumed only inside it.
	f.mDAC.SetAddressRange(0xD2FC, 0xD303, false);
	h.mMemory.WriteByte(0xD2FB, 0xA1);
	h.mMemory.WriteByte(0xD304, 0xA2);
	h.mMemory.WriteByte(0xD2FC, 0xA3);
	h.mMemory.WriteByte(0xD303, 0xA4);
	AT_PORTABLE_TEST_ASSERT(context, h.mRAM[0xD2FB] == 0xA1 && h.mRAM[0xD304] == 0xA2);
	AT_PORTABLE_TEST_ASSERT(context, h.mRAM[0xD2FC] == 0xEA && h.mRAM[0xD303] == 0xEA);
	AT_PORTABLE_TEST_ASSERT(context, Channels(f.mDAC).find("$A3 $20 $30 $A4") != std::string::npos);
	h.mMemory.WriteByte(0xD600, 0xBB); // old mapping was removed
	AT_PORTABLE_TEST_ASSERT(context, Channels(f.mDAC).find("$A3 $20 $30 $A4") != std::string::npos);
	f.mDAC.SetAddressRange(0xFFFC, 0xFFFF, false);
	f.mDAC.SetAddressRange(0x10000, 0x10003, false); // invalid ranges retain the active mapping
	f.mDAC.SetAddressRange(0xFFFF, 0xFFF0, false);
	h.mMemory.WriteByte(0xFFFF, 0xCC);
	h.mMemory.WriteByte(0, 0xDD);
	AT_PORTABLE_TEST_ASSERT(context, Channels(f.mDAC).find("$A3 $20 $30 $CC") != std::string::npos);
	f.mDAC.Shutdown();
	f.mDAC.Shutdown();
	AT_PORTABLE_TEST_ASSERT(context, !h.mAudio.mpSource && h.mAudio.mRemoves == 1);
	h.mMemory.WriteByte(0xFFFF, 0x11);
	AT_PORTABLE_TEST_ASSERT(context, h.mRAM[0xFFFF] == 0x11);
	f.mDAC.Init(&h.mMemory, &h.mScheduler, &h.mAudio);
	AT_PORTABLE_TEST_ASSERT(context, h.mAudio.mAdds == 2);
	AT_PORTABLE_TEST_ASSERT(context, Channels(f.mDAC).find("$80 $80 $80 $80") != std::string::npos);

	// Run the production main CPU against the same mapped DAC, without an
	// alternate interpreter or fabricated instruction/cycle callbacks.
	auto machine = std::make_unique<MainCPUPortableFixture>();
	CovoxAudioHost audio;
	ATCovoxEmulator dac;
	dac.SetFourChannels(true);
	dac.Init(&machine->mMemory, &machine->mFastScheduler, &audio);
	machine->Load(0x2000, { 0xA9,0xA0, 0x8D,0x00,0xD6, 0xA9,0x60, 0x8D,0x01,0xD6 });
	const uint32 start = machine->CPUGetCycle();
	AT_PORTABLE_TEST_ASSERT(context, machine->RunCycles(12));
	AT_PORTABLE_TEST_ASSERT(context, Channels(dac).find("$A0 $60 $80 $80") != std::string::npos);
	AT_PORTABLE_TEST_ASSERT(context, machine->mRAM[0xD600] == 0xA0 && machine->mRAM[0xD601] == 0x60);
	AT_PORTABLE_TEST_ASSERT(context, machine->RunCycles(CovoxAudioHost::kSamples * kATCyclesPerSyncSample - 12));
	audio.Render(start);
	AT_PORTABLE_TEST_ASSERT(context, audio.mSubmissions.size() == 2);
	AT_PORTABLE_TEST_ASSERT(context, audio.mSubmissions[0].mEdges.size() == 1 && audio.mSubmissions[1].mEdges.size() == 1);
	AT_PORTABLE_TEST_ASSERT(context, audio.mSubmissions[0].mEdges[0].mTime == start + 11);
	AT_PORTABLE_TEST_ASSERT(context, audio.mSubmissions[1].mEdges[0].mTime == start + 5);
	return true;
}

bool ATTestAltirraCovoxAudio(ATPortableTestContext& context) {
	{
		auto owner = std::make_unique<Fixture>(); auto& f = *owner; auto& h = f.mHost;
		const uint32 start = h.Time(); h.Advance(kATCyclesPerSyncSample);
		f.mDAC.WriteControl(0, 0xA0); f.mDAC.WriteControl(3, 0x90);
		f.mDAC.WriteControl(1, 0x60); f.mDAC.WriteControl(2, 0x70);
		f.mDAC.WriteControl(2, 0x70); // redundant write adds no edge
		AT_PORTABLE_TEST_ASSERT(context, f.mDAC.RequiresStereoMixingNow());
		h.Advance(7 * kATCyclesPerSyncSample);
		const auto out = h.mAudio.Render(start);
		AT_PORTABLE_TEST_ASSERT(context, out.mLeft[1] == 48.0f / 256 && out.mRight[1] == -48.0f / 256);
		for (size_t i = 0; i < out.mLeft.size(); ++i)
			if (i != 1) AT_PORTABLE_TEST_ASSERT(context, out.mLeft[i] == 0 && out.mRight[i] == 0);
		const auto& batches = h.mAudio.mSubmissions;
		AT_PORTABLE_TEST_ASSERT(context, batches.size() == 2 && batches[0].mLabel == "Covox R" && batches[1].mLabel == "Covox L");
		AT_PORTABLE_TEST_ASSERT(context, batches[0].mEdges.size() == 1 && batches[1].mEdges.size() == 1);
		AT_PORTABLE_TEST_ASSERT(context, batches[0].mEdges[0].mDeltaValue == -48 && batches[1].mEdges[0].mDeltaValue == 48);
		AT_PORTABLE_TEST_ASSERT(context, batches[0].mEdges[0].mTime == start + kATCyclesPerSyncSample);
	}
	{
		// A completed mono frame must not leave duplicate right-channel edges to
		// be replayed when stereo mixing becomes necessary in a later frame.
		auto owner = std::make_unique<Fixture>(false); auto& f = *owner; auto& h = f.mHost;
		uint32 start = h.Time(); h.Advance(kATCyclesPerSyncSample); f.mDAC.WriteMono(0xA0);
		h.Advance(7 * kATCyclesPerSyncSample);
		const auto mono = h.mAudio.Render(start, false);
		AT_PORTABLE_TEST_ASSERT(context, mono.mLeft[1] == 0.25f && !f.mDAC.RequiresStereoMixingNow());
		h.mAudio.mSubmissions.clear();
		start = h.Time(); h.Advance(kATCyclesPerSyncSample); f.mDAC.WriteControl(1, 0xB0);
		h.Advance(7 * kATCyclesPerSyncSample);
		const auto stereo = h.mAudio.Render(start);
		AT_PORTABLE_TEST_ASSERT(context, h.mAudio.mSubmissions[0].mEdges.size() == 1);
		AT_PORTABLE_TEST_ASSERT(context, h.mAudio.mSubmissions[0].mEdges[0].mTime == start + kATCyclesPerSyncSample);
		AT_PORTABLE_TEST_ASSERT(context, stereo.mRight[0] == 0 && stereo.mRight[1] == 16.0f / 256);
		AT_PORTABLE_TEST_ASSERT(context, std::all_of(stereo.mLeft.begin(), stereo.mLeft.end(), [](float v) { return v == 0; }));
	}
	{
		// Mono writes update individual DAC latches even if both summed outputs stay unchanged.
		auto owner = std::make_unique<Fixture>(); auto& f = *owner;
		f.mDAC.WriteControl(0, 0x80); f.mDAC.WriteControl(3, 0xC0);
		f.mDAC.WriteControl(1, 0x90); f.mDAC.WriteControl(2, 0xB0);
		f.mDAC.WriteMono(0xA0);
		AT_PORTABLE_TEST_ASSERT(context, Channels(f.mDAC).find("$A0 $A0 $A0 $A0") != std::string::npos);
		const auto prior = Channels(f.mDAC);
		f.mDAC.WarmReset();
		AT_PORTABLE_TEST_ASSERT(context, Channels(f.mDAC) == prior && !f.mDAC.RequiresStereoMixingNow());
		AT_PORTABLE_TEST_ASSERT(context, IsNeutral(f.mHost.mAudio.Render(f.mHost.Time())));
		f.mDAC.ColdReset();
		AT_PORTABLE_TEST_ASSERT(context, Channels(f.mDAC).find("$80 $80 $80 $80") != std::string::npos);
	}
	{
		// Wrap the production scheduler's 32-bit cycle count across a sample boundary.
		auto owner = std::make_unique<Fixture>(false); auto& f = *owner; auto& h = f.mHost;
		h.Advance(0xFFFFFFE4U - h.Time()); const uint32 start = h.Time();
		h.Advance(kATCyclesPerSyncSample); f.mDAC.WriteMono(0xA0);
		AT_PORTABLE_TEST_ASSERT(context, h.Time() == 0);
		h.Advance(7 * kATCyclesPerSyncSample);
		const auto out = h.mAudio.Render(start);
		AT_PORTABLE_TEST_ASSERT(context, out.mLeft[1] == 0.25f && out.mRight[1] == 0.25f);
	}
	return true;
}

bool ATTestAltirraCovoxSnapshot(ATPortableTestContext& context) {
	for (bool four : { false, true }) {
		auto source = std::make_unique<Fixture>(four);
		auto restored = std::make_unique<Fixture>(four);
		uint32 start = source->mHost.Time();
		source->mHost.Advance(kATCyclesPerSyncSample);
		if (four) {
			for (uint8 i = 0; i < 4; ++i) source->mDAC.WriteControl(i, static_cast<uint8>(0x90 + i * 8));
		} else source->mDAC.WriteMono(0xA0);
		source->mHost.Advance(7 * kATCyclesPerSyncSample);
		source->mHost.mAudio.Render(start);
		const auto snapshot = source->mDAC.SaveState();
		VDMemoryBufferStream stream;
		std::unique_ptr<IATSaveStateSerializer> writer(ATCreateSaveStateSerializer());
		writer->Serialize(stream, *snapshot, L"Covox DAC"); stream.Seek(0);
		std::unique_ptr<IATSaveStateDeserializer> reader(ATCreateSaveStateDeserializer());
		vdrefptr<IATSerializable> decoded; reader->Deserialize(stream, ~decoded);
		AT_PORTABLE_TEST_ASSERT(context, !!decoded && &decoded->GetSerializationType() == &snapshot->GetSerializationType());
		restored->mDAC.LoadState(static_cast<IATObjectState *>(decoded.get()));
		AT_PORTABLE_TEST_ASSERT(context, Channels(source->mDAC) == Channels(restored->mDAC));
		// Consume the restoration transition from neutral DACs before comparing
		// later writes against uninterrupted execution.
		restored->mHost.Advance(8 * kATCyclesPerSyncSample);
		restored->mHost.mAudio.Render(start);
		AT_PORTABLE_TEST_ASSERT(context, source->mHost.Time() == restored->mHost.Time());
		start = source->mHost.Time();
		for (Fixture *f : {source.get(), restored.get()}) {
			f->mHost.Advance(kATCyclesPerSyncSample);
			if (four) f->mDAC.WriteControl(2, 0x55); else f->mDAC.WriteMono(0x55);
			f->mHost.Advance(7 * kATCyclesPerSyncSample);
		}
		const auto a = source->mHost.mAudio.Render(start);
		const auto b = restored->mHost.mAudio.Render(start);
		AT_PORTABLE_TEST_ASSERT(context, a.mLeft == b.mLeft && a.mRight == b.mRight);
		const std::string before = Channels(restored->mDAC);
		restored->mDAC.SetFourChannels(!four);
		bool rejected = false;
		try { restored->mDAC.LoadState(snapshot); } catch (const ATInvalidSaveStateException&) { rejected = true; }
		AT_PORTABLE_TEST_ASSERT(context, rejected && Channels(restored->mDAC) == before);
		restored->mDAC.SetFourChannels(four);
		restored->mDAC.LoadState(nullptr);
		AT_PORTABLE_TEST_ASSERT(context, Channels(restored->mDAC).find("$80 $80 $80 $80") != std::string::npos);
	}
	return true;
}
