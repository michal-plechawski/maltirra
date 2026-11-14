// Production SlightSID clock, envelope, waveform and filter regression tests.
#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>
#include <at/attest/portabletest.h>
#include "slightsidportablefixture.h"

namespace {
	bool Guarded(const std::vector<float>& v) {
		return v.front() == 12345 && v.back() == 12345
			&& std::all_of(v.begin() + 1, v.end() - 1, [](float x) { return std::isfinite(x); });
	}
	bool Near(float a, float b, float e = 0.00001f) { return std::abs(a - b) < e; }
	std::vector<float> Tone(uint8 waveform, uint16 frequency, uint16 pulse, uint8 volume = 15, uint32 voice = 0) {
		auto f = std::make_unique<SlightSIDPortableFixture>();
		f->Voice(voice, frequency, waveform | 1, pulse);
		f->Write(24, volume);
		f->Advance(11200); f->Mix(400); // drain attack; sustain remains at 255
		f->Advance(64 * kATCyclesPerSyncSample + 9);
		return f->Mix(64);
	}
	std::vector<float> FilterTone(uint8 mode) {
		auto f = std::make_unique<SlightSIDPortableFixture>();
		f->Voice(0, 0x1122, 0x21);
		f->Write(21, 4); f->Write(22, 0x60); f->Write(23, 0x91); f->Write(24, mode | 15);
		f->Advance(11200); f->Mix(400);
		f->Advance(128 * kATCyclesPerSyncSample + 9);
		return f->Mix(128);
	}
}

bool ATTestAltirraSlightSIDTiming(ATPortableTestContext& context) {
	auto f = std::make_unique<SlightSIDPortableFixture>();
	f->Voice(0, 0x1000, 0x21);
	f->Advance(8); f->Mix(0);
	AT_PORTABLE_TEST_ASSERT(context, f->Phase(0) == 4 * 0x1000);
	f->Advance(1); f->Mix(0);
	AT_PORTABLE_TEST_ASSERT(context, f->Phase(0) == 5 * 0x1000);
	f->Advance(5); f->Write(0, 0x20); // flush old frequency before changing it
	f->Advance(4); f->Mix(0);
	AT_PORTABLE_TEST_ASSERT(context, f->Phase(0) == 7 * 0x1000 + 3 * 0x1020);

	// MOS 6581 datasheet, Control Register TEST: phase resets and stays locked
	// until TEST is cleared. https://myoldcomputer.nl/Files/mos_6581_sid.pdf
	const uint8 before = f->mSID.GetEnvelopeValue(0);
	f->Write(4, 0x29);
	AT_PORTABLE_TEST_ASSERT(context, f->Phase(0) == 0);
	f->Advance(900); f->Mix(0);
	AT_PORTABLE_TEST_ASSERT(context, f->Phase(0) == 0 && f->mSID.GetEnvelopeValue(0) > before);
	f->Write(4, 0x21);
	f->Advance(18); f->Mix(0);
	AT_PORTABLE_TEST_ASSERT(context, f->Phase(0) == 10 * 0x1020);

	auto wrapped = std::make_unique<SlightSIDPortableFixture>();
	wrapped->Voice(0, 0xFFFF, 0x21);
	wrapped->Advance(463); wrapped->Mix(0); // 257 SID clocks, 24-bit phase wraps
	AT_PORTABLE_TEST_ASSERT(context, wrapped->Phase(0) == 0x00FEFF);
	wrapped->Advance(0xFFFFFFF0U - wrapped->mHost.Time());
	wrapped->mSID.ColdReset();
	wrapped->Voice(0, 0x1000, 0x21);
	wrapped->Advance(36); wrapped->Mix(0);
	AT_PORTABLE_TEST_ASSERT(context, wrapped->mHost.Time() == 0x14 && wrapped->Phase(0) == 20 * 0x1000);

	// Partitioning real scheduler advances must preserve the 5:9 remainder,
	// all three oscillators, envelopes and rendered samples.
	auto whole = std::make_unique<SlightSIDPortableFixture>();
	auto split = std::make_unique<SlightSIDPortableFixture>();
	for (auto *p : {whole.get(), split.get()}) {
		p->Voice(0, 0x4567, 0x21); p->Voice(1, 0x1234, 0x17); p->Voice(2, 0xFFFF, 0x81);
		p->Write(24, 15);
	}
	constexpr uint32 cycles = 4096;
	whole->Advance(cycles);
	for (uint32 left = cycles; left;) {
		const uint32 n = std::min<uint32>(37, left); left -= n;
		split->Advance(n); split->Mix(0);
	}
	const auto a = whole->Mix(128), b = split->Mix(128);
	AT_PORTABLE_TEST_ASSERT(context, Guarded(a) && Guarded(b) && a == b);
	for (uint32 ch = 0; ch < 3; ++ch)
		AT_PORTABLE_TEST_ASSERT(context, whole->Phase(ch) == split->Phase(ch)
			&& whole->mSID.GetEnvelopeValue(ch) == split->mSID.GetEnvelopeValue(ch));
	return true;
}

bool ATTestAltirraSlightSIDEnvelopes(ATPortableTestContext& context) {
	auto f = std::make_unique<SlightSIDPortableFixture>();
	f->Voice(0, 0x1000, 0x21, 0x800, 0, 0xA0);
	f->Advance(16); f->Mix(0); // eight SID clocks: first rate comparator has not overflowed
	AT_PORTABLE_TEST_ASSERT(context, f->mSID.GetEnvelopeValue(0) == 0);
	f->Advance(2); f->Mix(0); // ten SID clocks in total
	AT_PORTABLE_TEST_ASSERT(context, f->mSID.GetEnvelopeValue(0) == 1);
	f->Advance(4131 - 18); f->Mix(0); // 2295 SID clocks = 255 fastest attack steps
	AT_PORTABLE_TEST_ASSERT(context, f->mSID.GetEnvelopeValue(0) == 255);
	f->Advance(2); f->Mix(0);
	AT_PORTABLE_TEST_ASSERT(context, f->mSID.GetEnvelopeMode(0) == 1);
	f->Advance(2000); f->Mix(0);
	AT_PORTABLE_TEST_ASSERT(context, f->mSID.GetEnvelopeValue(0) == 0xAA);
	f->Advance(1000); f->Mix(0);
	AT_PORTABLE_TEST_ASSERT(context, f->mSID.GetEnvelopeValue(0) == 0xAA);
	f->Write(4, 0x20);
	uint8 previous = 0xAA;
	for (uint32 i = 0; i < 200 && previous; ++i) {
		f->Advance(1008); f->Mix(36);
		const uint8 current = f->mSID.GetEnvelopeValue(0);
		AT_PORTABLE_TEST_ASSERT(context, current <= previous && f->mSID.GetEnvelopeMode(0) == 2);
		previous = current;
	}
	AT_PORTABLE_TEST_ASSERT(context, previous == 0);

	// A reset after unflushed time must not run the newly cleared voices
	// retrospectively over that time and give them spurious envelope levels.
	f->Advance(1000); f->mSID.WarmReset();
	for (uint32 ch = 0; ch < 3; ++ch)
		AT_PORTABLE_TEST_ASSERT(context, f->mSID.GetEnvelopeValue(ch) == 0 && f->Phase(ch) == 0);
	for (uint32 r = 0; r < 25; ++r)
		AT_PORTABLE_TEST_ASSERT(context, f->mSID.GetRegisters()[r] == 0);
	const auto silence = f->Mix(64);
	AT_PORTABLE_TEST_ASSERT(context, Guarded(silence));
	AT_PORTABLE_TEST_ASSERT(context, std::all_of(silence.begin() + 1, silence.end() - 1, [](float v) { return v == 0; }));
	return true;
}

bool ATTestAltirraSlightSIDWaveforms(ATPortableTestContext& context) {
	// Pin the original scalar mixer's gain, not a replacement analog SID model.
	const auto high = Tone(0x40, 0, 0);
	const auto low = Tone(0x40, 0, 0xFFF);
	const auto muted = Tone(0x40, 0, 0, 0);
	AT_PORTABLE_TEST_ASSERT(context, Guarded(high) && Guarded(low) && Guarded(muted));
	for (size_t i = 1; i + 1 < high.size(); ++i) {
		AT_PORTABLE_TEST_ASSERT(context, Near(high[i], 127.0f / (128 * 9)));
		AT_PORTABLE_TEST_ASSERT(context, Near(low[i], -1.0f / 9) && muted[i] == 0);
	}
	// With the pulse window closed the combined pulse+saw waveform remains
	// zero. Saw must not overwrite the logical-AND result of the pulse stage.
	const auto combined = Tone(0x60, 0x0100, 0xFFF);
	AT_PORTABLE_TEST_ASSERT(context, Guarded(combined));
	for (size_t i = 1; i + 1 < combined.size(); ++i)
		AT_PORTABLE_TEST_ASSERT(context, Near(combined[i], -1.0f / 9));
	const auto voice3 = Tone(0x40, 0, 0, 0x8F, 2);
	for (size_t i = 1; i + 1 < voice3.size(); ++i)
		AT_PORTABLE_TEST_ASSERT(context, voice3[i] == 0);

	auto f = std::make_unique<SlightSIDPortableFixture>();
	f->Voice(0, 0xFFFF, 0x81); f->Write(24, 15);
	f->Advance(18000); const auto noise = f->Mix(640);
	f->mSID.WarmReset();
	f->Voice(0, 0xFFFF, 0x81); f->Write(24, 15);
	f->Advance(18000); const auto repeated = f->Mix(640);
	AT_PORTABLE_TEST_ASSERT(context, Guarded(noise) && noise == repeated);
	AT_PORTABLE_TEST_ASSERT(context, std::any_of(noise.begin() + 1, noise.end() - 1, [](float x) { return x != 0; }));
	return true;
}

bool ATTestAltirraSlightSIDFilters(ATPortableTestContext& context) {
	const auto lp = FilterTone(0x10), bp = FilterTone(0x20), hp = FilterTone(0x40), sum = FilterTone(0x70);
	AT_PORTABLE_TEST_ASSERT(context, Guarded(lp) && Guarded(bp) && Guarded(hp) && Guarded(sum));
	for (size_t i = 1; i + 1 < sum.size(); ++i)
		AT_PORTABLE_TEST_ASSERT(context, Near(sum[i], lp[i] + bp[i] + hp[i], 0.0001f));
	AT_PORTABLE_TEST_ASSERT(context, std::any_of(lp.begin() + 1, lp.end() - 1, [](float v) { return std::abs(v) > 0.001f; }));
	const auto quiet = Tone(0x20, 0x1020, 0, 7), loud = Tone(0x20, 0x1020, 0, 14);
	for (size_t i = 1; i + 1 < quiet.size(); ++i)
		AT_PORTABLE_TEST_ASSERT(context, Near(2 * quiet[i], loud[i]));

	auto f = std::make_unique<SlightSIDPortableFixture>();
	f->Voice(0, 0x1020, 0x21); f->Write(24, 15);
	f->Advance(1536 * kATCyclesPerSyncSample + 9);
	const auto full = f->Mix(1536);
	AT_PORTABLE_TEST_ASSERT(context, Guarded(full));
	AT_PORTABLE_TEST_ASSERT(context, Guarded(f->Mix(0)) && Guarded(f->Mix(1)));
	f->Advance(2800);
	const auto scaledOut = f->Mix(100, 0, 0.5f);
	AT_PORTABLE_TEST_ASSERT(context, Guarded(scaledOut));
	for (size_t i = 1; i + 1 < scaledOut.size(); ++i)
		AT_PORTABLE_TEST_ASSERT(context, scaledOut[i] == 0.5f);
	return true;
}
