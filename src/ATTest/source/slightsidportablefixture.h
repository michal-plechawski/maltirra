// Production SlightSID connected to the actual scheduler and memory manager.
#ifndef f_ATTEST_SLIGHTSIDPORTABLEFIXTURE_H
#define f_ATTEST_SLIGHTSIDPORTABLEFIXTURE_H
#include <array>
#include <cstdio>
#include <string>
#include <vector>
#include <at/atcore/consoleoutput.h>
#include <slightsid.h>
#include "covoxportablefixture.h"

struct SlightSIDPortableFixture {
	SlightSIDPortableFixture() { mSID.Init(&mHost.mMemory, &mHost.mScheduler, &mHost.mAudio); }
	void Write(uint8 r, uint8 v) { mHost.mMemory.WriteByte(0xD500 + r, v); }
	void Voice(uint32 ch, uint16 freq, uint8 control, uint16 pulse = 0x800, uint8 ad = 0, uint8 sr = 0xF0) {
		const uint8 r = static_cast<uint8>(ch * 7);
		Write(r, static_cast<uint8>(freq)); Write(r + 1, static_cast<uint8>(freq >> 8));
		Write(r + 2, static_cast<uint8>(pulse)); Write(r + 3, static_cast<uint8>(pulse >> 8));
		Write(r + 5, ad); Write(r + 6, sr); Write(r + 4, control);
	}
	std::vector<float> Mix(uint32 n, float scale = 1, float initial = 0) {
		std::vector<float> samples(n + 2, initial);
		samples.front() = samples.back() = 12345.0f;
		std::array<float, kATAudioMixCount> levels {};
		levels[kATAudioMix_Other] = scale;
		ATSyncAudioMixInfo info {};
		info.mCount = n; info.mpLeft = samples.data() + 1; info.mpMixLevels = levels.data();
		info.mNumCycles = n * kATCyclesPerSyncSample;
		info.mMixingRate = 1789772.5f / kATCyclesPerSyncSample;
		info.mStartTime = mHost.Time() - info.mNumCycles;
		mSID.WriteAudio(info);
		return samples;
	}
	void Advance(uint32 cycles) { mHost.Advance(cycles); }
	uint32 Phase(uint32 ch) {
		struct Capture final : ATConsoleOutput {
			void WriteLine(const char *s) override { mLines.emplace_back(s); }
			std::vector<std::string> mLines;
		} capture;
		mSID.DumpStatus(capture);
		unsigned index = 0, frequency = 0, phase = 0;
		if (capture.mLines.size() != 4 || std::sscanf(capture.mLines[ch + 1].c_str(), "%u %x %x", &index, &frequency, &phase) != 3)
			std::abort();
		return phase;
	}
	CovoxPortableHost mHost;
	ATSlightSIDEmulator mSID;
};
#endif
