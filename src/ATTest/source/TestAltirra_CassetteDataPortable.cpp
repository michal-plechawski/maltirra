// Production tape decoding, recording, turbo signals and audio; no alternate tape model.
#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <vector>
#include <at/attest/portabletest.h>
#include <vd2/system/vdtypes.h>
#include <at/atcore/ksyms.h>
#include <vd2/system/VDString.h>
#include <vd2/system/file.h>
#include <vd2/system/filesys.h>
#include <vd2/system/process.h>
#include <vd2/system/time.h>
#include "cassetteportablefixture.h"

namespace {
	uint8 Sum(const std::vector<uint8>& data) {
		uint32 sum = 0; for (uint8 c : data) { sum += c; sum = (sum & 255) + (sum >> 8); } return static_cast<uint8>(sum);
	}
	vdrefptr<IATCassetteImage> DataTape(const std::vector<uint8>& data, bool checksum = false, bool bad = false, float leader = 0.1f) {
		vdrefptr<IATCassetteImage> image; ATCreateNewCassetteImage(~image);
		ATCassetteWriteCursor cursor {};
		image->WriteBlankData(cursor, static_cast<uint32>(kATCassetteDataSampleRate * leader), false);
		for (uint8 c : data) image->WriteStdData(cursor, c, 600, false);
		if (checksum) image->WriteStdData(cursor, Sum(data) ^ (bad ? 1 : 0), 600, false);
		image->WriteBlankData(cursor, 2000, false); return image;
	}
	vdrefptr<IATCassetteImage> PulseTape() {
		vdrefptr<IATCassetteImage> image; ATCreateNewCassetteImage(~image); ATCassetteWriteCursor cursor {};
		for (bool bit : {false, true, false, true}) image->WritePulse(cursor, bit, 100, false, false);
		return image;
	}
	struct TapeFile {
		VDStringW mPath;
		TapeFile(IATCassetteImage& image, bool wave) {
			VDStringW name;
			name.sprintf(L"altirra-cassette-%u-%llu", static_cast<unsigned>(VDGetCurrentProcessId()), static_cast<unsigned long long>(VDGetCurrentTick64()));
			name += wave ? L".wav" : L".cas"; mPath = VDGetFullPath(name.c_str());
			VDFileStream file(mPath.c_str(), nsVDFile::kWrite | nsVDFile::kCreateAlways);
			if (wave) ATSaveCassetteImageWAV(file, &image); else ATSaveCassetteImageCAS(file, &image);
		}
		~TapeFile() { VDRemoveFile(mPath.c_str()); }
	};
}

bool ATTestAltirraCassetteBlockRead(ATPortableTestContext& context) {
	auto f = std::make_unique<CassettePortableFixture>(); auto& tape = *f->mTape;
	const std::vector<uint8> bytes {0x55, 0x55, 0xFC, 0x11, 0x22, 0x33, 0x44};
	for (bool bad : {false, true}) {
		auto image = DataTape(bytes, true, bad); tape.Load(image, nullptr, false); tape.Play();
		f->mMemory.WriteByte(ATKernelSymbols::DAUX2, 0); // long IRG: motor switches off after reading
		AT_PORTABLE_TEST_ASSERT(context, tape.ReadBlock(0xFFFE, bytes.size(), &f->mMemory, 2) == (bad ? 0x8F : 1));
		for (size_t i = 0; i < bytes.size(); ++i)
			AT_PORTABLE_TEST_ASSERT(context, f->mMemory.ReadByte(static_cast<uint16>(0xFFFE + i)) == bytes[i]);
		AT_PORTABLE_TEST_ASSERT(context, f->mMemory.ReadByte(ATKernelSymbols::CHKSUM) == (Sum(bytes) ^ (bad ? 1 : 0)));
		AT_PORTABLE_TEST_ASSERT(context, !tape.IsMotorEnabled() && f->mChips.mPOKEY.ReadByte(0x0D) == (Sum(bytes) ^ (bad ? 1 : 0)));
	}
	auto blank = DataTape({}); tape.Load(blank, nullptr, false); tape.Play();
	AT_PORTABLE_TEST_ASSERT(context, tape.ReadBlock(0x2000, 7, &f->mMemory, 0.05f) == 0x8A);
	tape.Stop();
	AT_PORTABLE_TEST_ASSERT(context, tape.ReadBlock(0x2000, 7, &f->mMemory, -1) == 0x8A);
	// Exercise the native file-loading entry point through both CAS and WAV,
	// using the production encoder and decoder rather than a fabricated file.
	auto source = DataTape(bytes, true);
	for (bool wave : {false, true}) {
		TapeFile file(*source, wave); tape.Load(file.mPath.c_str()); tape.Play();
		AT_PORTABLE_TEST_ASSERT(context, tape.IsImagePersistent() && VDStringW(tape.GetPath()) == file.mPath);
		// CAS loading expands short leaders; search the bounded image length.
		AT_PORTABLE_TEST_ASSERT(context, tape.ReadBlock(0x2000, bytes.size(), &f->mMemory, -1) == 1);
		for (size_t i = 0; i < bytes.size(); ++i) AT_PORTABLE_TEST_ASSERT(context, f->mMemory.ReadByte(0x2000 + i) == bytes[i]);
		tape.Unload();
	}
	return true;
}

bool ATTestAltirraCassetteBlockWrite(ATPortableTestContext& context) {
	auto f = std::make_unique<CassettePortableFixture>(); auto& tape = *f->mTape;
	const std::vector<uint8> bytes {0x55, 0x55, 0xFC, 0x00, 0xF1, 0x10, 0x80};
	for (size_t i = 0; i < bytes.size(); ++i) f->mMemory.WriteByte(static_cast<uint16>(0x4000 + i), bytes[i]);
	tape.LoadNew(); f->Motor(true); f->mMemory.WriteByte(ATKernelSymbols::DAUX2, 0x80);
	AT_PORTABLE_TEST_ASSERT(context, f->mSlowScheduler.GetTicksToNextEvent() == 500); // scanlines, not CPU cycles
	AT_PORTABLE_TEST_ASSERT(context, tape.WriteBlock(0x4000, bytes.size(), &f->mMemory) == 1);
	auto expected = DataTape(bytes, true, false, 0.25f);
	const uint32 expectedLength = expected->GetDataLength() - 2000;
	AT_PORTABLE_TEST_ASSERT(context, tape.IsImageDirty() && tape.GetSampleLen() == expectedLength && tape.GetSamplePos() == expectedLength);
	for (uint32 i = 0; i < expectedLength; i += 13)
		AT_PORTABLE_TEST_ASSERT(context, tape.GetImage()->GetBit(i, false) == expected->GetBit(i, false));
	tape.Stop(); tape.Play(); tape.RewindToStart();
	AT_PORTABLE_TEST_ASSERT(context, tape.ReadBlock(0x5000, bytes.size(), &f->mMemory, 2) == 1);
	for (size_t i = 0; i < bytes.size(); ++i) AT_PORTABLE_TEST_ASSERT(context, f->mMemory.ReadByte(0x5000 + i) == bytes[i]);
	// Serial recording is the actual POKEY callback path, separate from block acceleration.
	tape.LoadNew(); f->Motor(true);
	auto& pokey = f->mChips.mPOKEY;
	pokey.WriteByte(8, 0x28); pokey.WriteByte(4, 0xCC); pokey.WriteByte(6, 5); pokey.WriteByte(0x0F, 0x2B);
	AT_PORTABLE_TEST_ASSERT(context, pokey.GetSerialCyclesPerBitSend() == 2982);
	pokey.WriteByte(0x0D, 0xA6); f->Tick(40000);
	auto serialExpected = DataTape({0xA6}, false, false, 0);
	AT_PORTABLE_TEST_ASSERT(context, tape.IsImageDirty() && tape.GetSampleLen() == serialExpected->GetDataLength() - 2000);
	for (uint32 i = 0; i < tape.GetSampleLen(); i += 7)
		AT_PORTABLE_TEST_ASSERT(context, tape.GetImage()->GetBit(i, false) == serialExpected->GetBit(i, false));
	tape.SetPaused(true);
	AT_PORTABLE_TEST_ASSERT(context, !tape.PokeyWriteCassetteData(0x11, 2983));
	return true;
}

bool ATTestAltirraCassetteSerial(ATPortableTestContext& context) {
	auto f = std::make_unique<CassettePortableFixture>();
	const std::vector<uint8> bytes {0x55, 0xA6, 0x39, 0x0D}; auto image = DataTape(bytes);
	f->mTape->Load(image, nullptr, false);
	auto& pokey = f->mChips.mPOKEY;
	// Linked high-clock timers 3/4: (0x05CC+7)*2 = 2982 cycles/bit (~600 baud).
	pokey.WriteByte(8, 0x28); pokey.WriteByte(4, 0xCC); pokey.WriteByte(6, 5); pokey.WriteByte(0x0F, 0x13);
	AT_PORTABLE_TEST_ASSERT(context, pokey.GetSerialCyclesPerBitRecv() == 2982);
	pokey.WriteByte(0x0E, 0x20);
	std::vector<uint8> decoded, received;
	vdfunction<void(uint32,uint32,uint8,bool,uint32)> callback = [&](uint32, uint32, uint8 c, bool framing, uint32 half) {
		if (framing || half != 1491) std::abort(); decoded.push_back(c);
	};
	f->mTape->ByteDecoded.Add(&callback);
	struct Remove { ATCassetteEmulator& t; const decltype(callback) *p; ~Remove() { t.ByteDecoded.Remove(p); } } remove {*f->mTape, &callback};
	f->Motor(true);
	const uint32 maxCycles = (image->GetDataLength() + 700) * kATCassetteCyclesPerDataSample;
	for (uint32 i = 0; i < maxCycles; i += 56) {
		f->Tick(56);
		if (!(pokey.ReadByte(0x0E) & 0x20)) {
			received.push_back(pokey.ReadByte(0x0D)); pokey.WriteByte(0x0E, 0); pokey.WriteByte(0x0E, 0x20);
		}
	}
	AT_PORTABLE_TEST_ASSERT(context, decoded == bytes);
	AT_PORTABLE_TEST_ASSERT(context, received == bytes);
	AT_PORTABLE_TEST_ASSERT(context, f->mChips.mIRQAsserts >= bytes.size());
	return true;
}

bool ATTestAltirraCassetteTurbo(ATPortableTestContext& context) {
	for (auto mode : {kATCassetteTurboMode_ProceedSense, kATCassetteTurboMode_InterruptSense}) {
		auto f = std::make_unique<CassettePortableFixture>(); auto image = PulseTape();
		f->mTape->Load(image, nullptr, false); f->mTape->SetTurboMode(mode); f->Motor(true);
		f->Tick(56 * 50);
		AT_PORTABLE_TEST_ASSERT(context, !f->mSIO.mbProceed && !f->mSIO.mbInterrupt);
		f->Tick(56 * 75);
		AT_PORTABLE_TEST_ASSERT(context, mode == kATCassetteTurboMode_ProceedSense ? f->mSIO.mbProceed : f->mSIO.mbInterrupt);
		AT_PORTABLE_TEST_ASSERT(context, f->mChips.mPIA.ReadByte(mode == kATCassetteTurboMode_ProceedSense ? 2 : 3) & 0x80);
		f->mTape->Stop();
		AT_PORTABLE_TEST_ASSERT(context, !f->mSIO.mbProceed && !f->mSIO.mbInterrupt);
		f->mTape->Play(); f->Tick(56);
		AT_PORTABLE_TEST_ASSERT(context, mode == kATCassetteTurboMode_ProceedSense ? f->mSIO.mbProceed : f->mSIO.mbInterrupt);
	}
	for (auto mode : {kATCassetteTurboMode_TurboD, kATCassetteTurboMode_KSOTurbo2000}) {
		auto f = std::make_unique<CassettePortableFixture>(); auto image = PulseTape();
		f->mTape->Load(image, nullptr, false); f->mTape->SetTurboMode(mode);
		const uint8 mask = mode == kATCassetteTurboMode_TurboD ? 0x10 : 0x80;
		if (mode == kATCassetteTurboMode_KSOTurbo2000) {
			f->mChips.mPIA.WriteByte(2, 0x38); f->mChips.mPIA.WriteByte(0, 0x20);
			f->mChips.mPIA.WriteByte(2, 0x3C); f->mChips.mPIA.WriteByte(0, 0x20); f->mChips.mPIA.WriteByte(0, 0);
		} else f->Motor(true);
		AT_PORTABLE_TEST_ASSERT(context, f->mTape->IsMotorRunning());
		f->Tick(56 * 50); AT_PORTABLE_TEST_ASSERT(context, f->mChips.mPIA.ReadByte(0) & mask);
		f->Tick(56 * 75); AT_PORTABLE_TEST_ASSERT(context, !(f->mChips.mPIA.ReadByte(0) & mask));
		f->mTape->SetPaused(true); AT_PORTABLE_TEST_ASSERT(context, f->mChips.mPIA.ReadByte(0) & mask);
	}
	auto f = std::make_unique<CassettePortableFixture>(); auto& tape = *f->mTape;
	tape.SetTurboMode(kATCassetteTurboMode_CommandControl);
	f->mSIO.Command(true); AT_PORTABLE_TEST_ASSERT(context, tape.IsTurboDecodingEnabled());
	f->mSIO.Command(false); AT_PORTABLE_TEST_ASSERT(context, !tape.IsTurboDecodingEnabled());
	tape.SetTurboMode(kATCassetteTurboMode_DataControl);
	f->mChips.mPOKEY.WriteByte(0x0F, 0x83); AT_PORTABLE_TEST_ASSERT(context, tape.IsTurboDecodingEnabled());
	f->mChips.mPOKEY.WriteByte(0x0F, 3); AT_PORTABLE_TEST_ASSERT(context, !tape.IsTurboDecodingEnabled());
	tape.SetTurboMode(kATCassetteTurboMode_Always); AT_PORTABLE_TEST_ASSERT(context, tape.IsTurboDecodingEnabled());
	tape.SetTurboMode(static_cast<ATCassetteTurboMode>(255)); AT_PORTABLE_TEST_ASSERT(context, tape.GetTurboMode() == kATCassetteTurboMode_None);
	return true;
}

bool ATTestAltirraCassetteAudio(ATPortableTestContext& context) {
	auto f = std::make_unique<CassettePortableFixture>(); auto image = DataTape({0x55,0xA6}, false, false, 0);
	f->mTape->Load(image, nullptr, false); f->mTape->SetLoadDataAsAudioEnable(true);
	std::array<float,128> actual {}, expected {};
	std::array<float,kATAudioMixCount> levels {}; levels[kATAudioMix_Cassette] = 0.25f;
	ATSyncAudioMixInfo mix {}; mix.mStartTime = f->mChips.mVideo.mScheduler.GetTick64(); mix.mCount = actual.size(); mix.mpLeft = actual.data(); mix.mpMixLevels = levels.data();
	f->Motor(true); f->Tick(actual.size() * 28); f->mMixer.mpSource->WriteAudio(mix);
	float *dst = expected.data(); uint32 sample = 0, fraction = 0;
	image->AccumulateAudio(dst, sample, fraction, expected.size(), 0.25f);
	AT_PORTABLE_TEST_ASSERT(context, std::any_of(actual.begin(), actual.end(), [](float x) { return std::abs(x) > 0.01f; }));
	for (size_t i = 0; i < actual.size(); ++i) AT_PORTABLE_TEST_ASSERT(context, std::abs(actual[i] - expected[i]) < 1e-6f);
	f->mTape->SetLoadDataAsAudioEnable(false); actual.fill(0); mix.mStartTime += actual.size() * 28;
	f->Tick(actual.size() * 28); f->mMixer.mpSource->WriteAudio(mix);
	AT_PORTABLE_TEST_ASSERT(context, std::all_of(actual.begin(), actual.end(), [](float x) { return x == 0; }));
	return true;
}

bool ATTestAltirraCassetteDetection(ATPortableTestContext& context) {
	auto f = std::make_unique<CassettePortableFixture>(); auto& tape = *f->mTape;
	AT_PORTABLE_TEST_ASSERT(context, !tape.AutodetectBasicNeeded().has_value());
	auto basic = DataTape({0x55,0x55,0xFC,0,0,0x10,1}, false, false, 3);
	tape.Load(basic, nullptr, false); const auto pos = tape.GetSamplePos();
	AT_PORTABLE_TEST_ASSERT(context, tape.AutodetectBasicNeeded() == std::optional<bool>(true) && tape.GetSamplePos() == pos);
	const uint32 old = tape.OnPreModifyTape(); ATCassetteWriteCursor cursor {}; cursor.mPosition = static_cast<uint32>(kATCassetteDataSampleRate * 3);
	for (uint8 c : {0x55,0x55,0xFC,0,1,0,6}) basic->WriteStdData(cursor, c, 600, false);
	tape.OnPostModifyTape(old);
	AT_PORTABLE_TEST_ASSERT(context, tape.AutodetectBasicNeeded() == std::optional<bool>(false));
	auto unknown = DataTape({0x55,0x55,0,0,0,0,0}, false, false, 3); tape.Load(unknown, nullptr, false);
	AT_PORTABLE_TEST_ASSERT(context, !tape.AutodetectBasicNeeded().has_value());
	return true;
}
