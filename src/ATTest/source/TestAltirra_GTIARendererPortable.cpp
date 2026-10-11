// Portable pixel contracts for the production GTIA scanline renderer.
#include <algorithm>
#include <array>
#include <cstring>
#include <cstdio>
#include <memory>
#include <at/atcore/snapshotimpl.h>
#include <at/attest/portabletest.h>
#include <vd2/system/file.h>
#include <gtiarenderer.h>
#include <savestateio.h>

namespace {
	constexpr uint8 kColors[9] { 0x12, 0x34, 0x56, 0x78, 0x92, 0xa4, 0xb6, 0xc8, 0x26 };
	constexpr uint8 kPF[5] { 0, 1, 2, 4, 8 };
	constexpr uint8 kMode10Color[16] { 0, 1, 2, 3, 4, 5, 6, 7, 8, 8, 8, 8, 4, 5, 6, 7 };
	struct Line {
		alignas(16) std::array<uint8, 480> pixels;
		alignas(16) std::array<uint8, 240> merge;
		alignas(16) std::array<uint8, 240> antic;
		Line() {
			pixels.fill(0xed);
			merge.fill(0);
			for(int x = 0; x < 240; ++x) {
				const uint8 nibble = (x / 2) & 15;
				antic[x] = x & 1 ? nibble & 3 : nibble >> 2;
			}
		}
	};
	void Configure(ATGTIARenderer& renderer, uint8 prior, bool secam = false) {
		for(uint8 i = 0; i < 9; ++i)
			renderer.SetRegisterImmediate(0x12 + i, kColors[i]);
		renderer.SetRegisterImmediate(0x1b, prior);
		renderer.SetSECAMMode(secam);
	}
	uint8 ExpectedPixel(const Line& line, int mode, bool secam, int x, int half) {
		if (mode == 0) {
			for(int i = 0; i < 5; ++i)
				if (line.merge[x] == kPF[i])
					return kColors[i ? i + 3 : 8];
		}
		if (mode == 1) {
			const uint8 base = kColors[line.merge[x] ? 6 : 8];
			return line.antic[x] & (half ? 1 : 2) ? (base & 0xf0) | (kColors[5] & 15) : base;
		}
		const int pair = (mode == 3 ? x - 1 : x) & ~1;
		const uint8 nibble = line.antic[pair] * 4 + line.antic[pair + 1];
		if (mode == 2)
			return kColors[8] | (secam ? nibble & 0xfe : nibble);
		if (mode == 3)
			return kColors[kMode10Color[nibble]];
		const uint8 chroma = nibble << 4;
		return chroma ? kColors[8] | chroma : kColors[8] & 0xf0;
	}
	bool TestSpans(ATPortableTestContext& context) {
		constexpr uint8 kPrior[5] { 0, 0, 0x40, 0x80, 0xc0 };
		for(int mode = 0; mode < 5; ++mode) {
			for(bool secam : { false, true }) {
				for(int start = 34; start < 66; ++start) {
					for(int width = 0; width <= 34; ++width) {
						Line fast, general;
						for(int x = 0; x < 240; ++x) {
							// GTIA UpdatePlayfield320 always supplies PF2 for hires
							// data. Background occurs only on a blank AN bus.
							fast.merge[x] = mode == 0 ? kPF[x % 5] : mode == 1 ? (fast.antic[x] || (x & 1) ? 4 : 0) : mode == 3 ? 4 : 0;
							general.merge[x] = fast.merge[x];
						}
						ATGTIARenderer accelerated, reference;
						Configure(accelerated, kPrior[mode], secam);
						Configure(reference, kPrior[mode], secam);
						accelerated.BeginScanline(fast.pixels.data(), fast.merge.data(), fast.antic.data(), mode == 1);
						reference.BeginScanline(general.pixels.data(), general.merge.data(), general.antic.data(), mode == 1);
						accelerated.RenderScanline(start, true, false, false);
						reference.RenderScanline(start, true, true, true);
						accelerated.RenderScanline(start + width, true, false, false);
						reference.RenderScanline(start + width, true, true, true);
						if (fast.pixels != general.pixels) {
							static char detail[160];
							std::snprintf(detail, sizeof detail, "fast/general mismatch: mode=%d SECAM=%d start=%d width=%d",
								mode, int(secam), start, width);
							context.mpFile = __FILE__;
							context.mLine = __LINE__;
							context.mpExpression = detail;
							return false;
						}
						for(int x = 34; x < start + width; ++x)
							for(int half = 0; half < 2; ++half)
								AT_PORTABLE_TEST_ASSERT(context, fast.pixels[2*x + half] == ExpectedPixel(fast, mode, secam, x, half));
						for(size_t x = 2 * (start + width); x < fast.pixels.size(); ++x)
							AT_PORTABLE_TEST_ASSERT(context, fast.pixels[x] == 0xed);
					}
				}
			}
		}
		return true;
	}
	bool TestPriorities(ATPortableTestContext& context) {
		uint8 tables[32][256];
		ATInitGTIAPriorityTables(tables);
		constexpr uint8 kPalette[24] {
			0x12, 0x34, 0x56, 0x78, 0x92, 0xa4, 0xb6, 0xc8, 0x26, 0,
			0x36, 0x7e, 0x92, 0xb6, 0xb6, 0xb6, 0xb4, 0xb6,
			0xf6, 0xfe, 0xfe, 0xde, 0xf8, 0xfe
		};
		for(int prior = 0; prior < 32; ++prior) {
			Line line;
			ATGTIARenderer renderer;
			Configure(renderer, (prior & 15) | (prior & 16 ? 32 : 0));
			for(int offset = 0; offset < 256; offset += 128) {
				for(int x = 34; x < 162; ++x)
					line.merge[x] = offset + x - 34;
				renderer.BeginScanline(line.pixels.data(), line.merge.data(), line.antic.data(), false);
				renderer.RenderScanline(162, true, true, true);
				for(int x = 34; x < 162; ++x) {
					AT_PORTABLE_TEST_ASSERT(context, line.pixels[x*2] == kPalette[tables[prior][line.merge[x]]]);
					AT_PORTABLE_TEST_ASSERT(context, line.pixels[x*2 + 1] == line.pixels[x*2]);
				}
				renderer.EndScanline();
			}
		}
		return true;
	}
	bool TestStateAndTiming(ATPortableTestContext& context) {
		for(bool hires : { false, true }) {
			for(uint8 initial : { uint8(0), uint8(0x40), uint8(0x80), uint8(0xc0) }) {
				for(uint8 target : { uint8(0), uint8(0x40), uint8(0x80), uint8(0xc0) }) {
					for(int checkpoint = 40; checkpoint < 46; ++checkpoint) {
						Line original, resumed, whole;
						original.merge.fill(4);
						resumed.merge.fill(4);
						whole.merge.fill(4);
						ATGTIARenderer a, b, c;
						Configure(a, initial);
						Configure(b, initial);
						Configure(c, initial);
						for(ATGTIARenderer *r : { &a, &c }) {
							// Intentionally insert out of order, with equal-clock writes.
							r->AddRegisterChange(89, 0x17, 0x64);
							r->AddRegisterChange(39, 0x1a, 0x38);
							r->AddRegisterChange(80, 0x1a, 0x42);
							r->AddRegisterChange(40, 0x1b, target);
							r->AddRegisterChange(89, 0x17, 0x86);
						}
						a.BeginScanline(original.pixels.data(), original.merge.data(), original.antic.data(), hires);
						b.BeginScanline(resumed.pixels.data(), resumed.merge.data(), resumed.antic.data(), hires);
						c.BeginScanline(whole.pixels.data(), whole.merge.data(), whole.antic.data(), hires);
						a.RenderScanline(checkpoint, true, true, true);
						vdrefptr<IATSerializable> state;
						a.SaveState(~state);
						VDMemoryBufferStream stream;
						std::unique_ptr<IATSaveStateSerializer> writer(ATCreateSaveStateSerializer());
						writer->Serialize(stream, *state, L"GTIA renderer");
						stream.Seek(0);
						std::unique_ptr<IATSaveStateDeserializer> reader(ATCreateSaveStateDeserializer());
						reader->Deserialize(stream, ~state);
						// The parent GTIA restores architectural colors separately.
						b.SetRegisterImmediate(0x1a, 0x38);
						b.LoadState(state);
						a.RenderScanline(222, true, true, true);
						b.RenderScanline(222, true, true, true);
						c.RenderScanline(222, true, true, true);
						AT_PORTABLE_TEST_ASSERT(context, original.pixels == whole.pixels);
						AT_PORTABLE_TEST_ASSERT(context, !memcmp(original.pixels.data() + checkpoint*2,
							resumed.pixels.data() + checkpoint*2, (222 - checkpoint)*2));
						// An exhausted queue also must be safe to save.
						a.SaveState(~state);
						a.EndScanline();
						b.EndScanline();
						AT_PORTABLE_TEST_ASSERT(context, original.pixels[444] == 0x42);
						AT_PORTABLE_TEST_ASSERT(context, original.pixels[455] == 0x42);
						AT_PORTABLE_TEST_ASSERT(context, original.pixels[456] == 0xed);
					}
				}
			}
		}
		return true;
	}
	bool TestSenseAndReset(ATPortableTestContext& context) {
		Line line;
		ATGTIARenderer renderer;
		Configure(renderer, 0);
		renderer.BeginScanline(line.pixels.data(), line.merge.data(), line.antic.data(), false);
		for(int x = 0; x < 456; ++x)
			line.pixels[x] = uint8(x);
		std::array<uint8, 600> weights;
		for(size_t i = 0; i < weights.size(); ++i)
			weights[i] = uint8(i % 7);
		for(const auto range : { std::array<int, 2>{ -20, 460 }, { 0, 90 }, { 68, 444 },
			{ 430, 480 }, { -20, 20 }, { 444, 460 }, { 80, 80 }, { 90, 80 } }) {
			uint32 expected = 0;
			for(int x = std::max(68, range[0]); x < std::min(444, range[1]); ++x) {
				const uint32 luma = (line.pixels[x] & 15) + (line.pixels[x] & 0xf0 ? 2 : 0);
				expected += luma * luma * weights[x - range[0]];
			}
			AT_PORTABLE_TEST_ASSERT(context, renderer.SenseScanline(range[0], range[1], weights.data()) == expected * 289);
		}
		renderer.EndScanline();
		AT_PORTABLE_TEST_ASSERT(context, renderer.SenseScanline(-20, 460, weights.data()) == 0);
		// Reset must also update the selected priority table.
		renderer.SetRegisterImmediate(0x1b, 0x0f);
		renderer.ResetState();
		line.merge.fill(ATGTIA::P0 | ATGTIA::PF0);
		renderer.BeginScanline(line.pixels.data(), line.merge.data(), line.antic.data(), false);
		renderer.RenderScanline(40, true, true, true);
		AT_PORTABLE_TEST_ASSERT(context, line.pixels[68] == (kColors[0] | kColors[4]));
		renderer.ColdReset();
		renderer.BeginScanline(line.pixels.data(), line.merge.data(), line.antic.data(), false);
		renderer.RenderScanline(40, true, true, true);
		AT_PORTABLE_TEST_ASSERT(context, line.pixels[68] == 0);
		renderer.SetAnalysisMode(true);
		renderer.BeginScanline(line.pixels.data(), line.merge.data(), line.antic.data(), false);
		renderer.RenderScanline(40, true, true, true);
		AT_PORTABLE_TEST_ASSERT(context, line.pixels[68] == kATAnalysisColorTable[ATGTIA::kColorPF0P0]);
		// Mode 10's delayed AN pair must not read before the scanline at x=0.
		for(bool full : { false, true }) {
			for(int end = 1; end <= 3; ++end) {
				Configure(renderer, 0x80);
				renderer.SetAnalysisMode(false);
				renderer.SetVBlank(false);
				line.merge.fill(4);
				line.antic[0] = line.antic[1] = 3;
				renderer.BeginScanline(line.pixels.data(), line.merge.data(), line.antic.data(), false);
				line.pixels.fill(0xed);
				renderer.RenderScanline(end, true, full, full);
				AT_PORTABLE_TEST_ASSERT(context, line.pixels[0] == kColors[0]);
				for(int x = 1; x < end; ++x)
					AT_PORTABLE_TEST_ASSERT(context, line.pixels[2*x] == kColors[7]);
				for(size_t x = 2 * end; x < line.pixels.size(); ++x)
					AT_PORTABLE_TEST_ASSERT(context, line.pixels[x] == 0xed);
			}
		}
		// CTIA suppresses both the active and queued GTIA mode selection.
		Configure(renderer, 0xc0);
		renderer.AddRegisterChange(40, 0x1b, 0x80);
		renderer.SetCTIAMode();
		renderer.SetVBlank(true);
		line.merge.fill(4);
		renderer.BeginScanline(line.pixels.data(), line.merge.data(), line.antic.data(), false);
		renderer.RenderScanline(50, true, false, false);
		AT_PORTABLE_TEST_ASSERT(context, line.pixels[68] == kColors[6] && line.pixels[98] == kColors[6]);
		return true;
	}
}

bool ATTestAltirraGTIARenderer(ATPortableTestContext& context) {
	return TestSpans(context) && TestPriorities(context) && TestStateAndTiming(context) && TestSenseAndReset(context);
}
