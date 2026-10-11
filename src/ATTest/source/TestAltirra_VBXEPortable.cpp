// Portable behavioral tests for the production VBXE emulator core.

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

#include <at/atcore/scheduler.h>
#include <at/atcore/savestate.h>
#include <at/atcore/snapshot.h>
#include <at/attest/portabletest.h>
#include <artifacting.h>
#include <irqcontroller.h>
#include <memorymanager.h>
#include <palettegenerator.h>
#include <vbxe.h>
#include <vbxestate.h>
#include <vd2/system/refcount.h>

namespace {
	struct IRQTarget {
		void AssertIRQ(int cycleOffset) {
			mAssertOffsets.push_back(cycleOffset);
		}

		void NegateIRQ() {
			++mNegateCount;
		}

		std::vector<int> mAssertOffsets;
		uint32 mNegateCount = 0;
	};

	ATColorParams MakeColorParams() {
		ATColorParams params {};
		params.mHueStart = -57.0f;
		params.mHueRange = 27.1f * 15.0f;
		params.mBrightness = -0.04f;
		params.mContrast = 1.04f;
		params.mSaturation = 0.20f;
		params.mGammaCorrect = 1.0f;
		params.mIntensityScale = 1.0f;
		params.mArtifactHue = 252.0f;
		params.mArtifactSat = 1.15f;
		params.mArtifactSharpness = 0.50f;
		params.mRedScale = 1.0f;
		params.mGrnScale = 1.0f;
		params.mBluScale = 1.0f;
		params.mLumaRampMode = kATLumaRampMode_XL;
		params.mColorMatchingMode = ATColorMatchingMode::SRGB;
		return params;
	}

	class VBXEFixture {
	public:
		VBXEFixture() {
			memory->Init();
			irqController.Init(&irqTarget);
			scheduler.SetRate(VDFraction(17897725, 10));

			baseLayer = memory->CreateLayer(kATMemoryPri_BaseRAM, baseRAM.data(), 0, 256, false);
			memory->SetLayerName(baseLayer, "VBXE portable test base RAM");
			memory->EnableLayer(baseLayer, true);

			paletteGenerator.Generate(MakeColorParams(), ATMonitorMode::Color);
			emulator.SetMemory(vbxeRAM.data());
			emulator.SetRegisterBase(0xD6);
			emulator.SetDefaultPalette(paletteGenerator.mPalette, &artifacting);
			emulator.Init(&irqController, memory.get(), &scheduler);
		}

		~VBXEFixture() {
			emulator.Shutdown();
			memory->DeleteLayer(baseLayer);
		}

		void WriteRegister(uint8 reg, uint8 value) {
			memory->CPUWriteByte(static_cast<uint16>(0xD600 + reg), value);
		}

		uint8 ReadRegister(uint8 reg) {
			return memory->CPUReadByte(static_cast<uint16>(0xD600 + reg));
		}

		void AdvanceSchedulerToNextEvent() {
			scheduler.mNextEventCounter += scheduler.GetTicksToNextEvent();
			scheduler.ProcessNextEvent();
		}

		std::unique_ptr<ATMemoryManager> memory = std::make_unique<ATMemoryManager>();
		ATScheduler scheduler;
		ATIRQController irqController;
		IRQTarget irqTarget;
		ATArtifactingEngine artifacting;
		ATColorPaletteGenerator paletteGenerator;
		ATVBXEEmulator emulator;
		alignas(16) std::array<uint8, 512 * 1024> vbxeRAM {};
		alignas(16) std::array<uint8, 64 * 1024> baseRAM {};
		ATMemoryLayer *baseLayer = nullptr;
	};

	uint8 EncodePaletteComponent(uint8 value) {
		return (value & 0xFE) | (value >> 7);
	}

	void WritePaletteEntry(VBXEFixture& fixture, uint8 bank, uint8 index, uint32 rgb) {
		fixture.WriteRegister(0x44, index);
		fixture.WriteRegister(0x45, bank);
		fixture.WriteRegister(0x46, EncodePaletteComponent(static_cast<uint8>(rgb >> 16)));
		fixture.WriteRegister(0x47, EncodePaletteComponent(static_cast<uint8>(rgb >> 8)));
		fixture.WriteRegister(0x48, EncodePaletteComponent(static_cast<uint8>(rgb)));
	}

	void ConfigureXDL(VBXEFixture& fixture, uint8 mode, uint32 overlayAddress) {
		constexpr uint32 xdlAddress = 0x0100;
		fixture.vbxeRAM[xdlAddress + 0] = mode == 0 ? 0x41 : 0x42; // text / bitmap + OVADR
		fixture.vbxeRAM[xdlAddress + 1] = mode == 0 ? 0x89 : (mode == 1 ? 0x88 : 0x98);
		fixture.vbxeRAM[xdlAddress + 2] = static_cast<uint8>(overlayAddress);
		fixture.vbxeRAM[xdlAddress + 3] = static_cast<uint8>(overlayAddress >> 8);
		fixture.vbxeRAM[xdlAddress + 4] = static_cast<uint8>(overlayAddress >> 16);
		fixture.vbxeRAM[xdlAddress + 5] = 0; // overlay row step
		fixture.vbxeRAM[xdlAddress + 6] = 0;
		if (mode == 0)
			fixture.vbxeRAM[xdlAddress + 7] = 8; // character base at $4000
		fixture.vbxeRAM[xdlAddress + (mode == 0 ? 8 : 7)] = 0x11; // normal width, overlay palette 1
		fixture.vbxeRAM[xdlAddress + (mode == 0 ? 9 : 8)] = 0xFF; // overlay priority

		fixture.WriteRegister(0x41, static_cast<uint8>(xdlAddress));
		fixture.WriteRegister(0x42, static_cast<uint8>(xdlAddress >> 8));
		fixture.WriteRegister(0x43, static_cast<uint8>(xdlAddress >> 16));
		fixture.WriteRegister(0x40, 0x01); // enable XDL fetch/render
	}

	uint32 RenderXDLFrame(VBXEFixture& fixture, bool& canariesIntact) {
		constexpr uint32 kGuardWords = 16;
		constexpr uint32 kLineWords = 912;
		constexpr uint32 kCanary = 0xDEADBEEF;
		std::array<uint32, kLineWords + 2 * kGuardWords> guardedLine;
		guardedLine.fill(kCanary);
		std::array<uint8, 456> mergeBuffer {};
		std::array<uint8, 456> anticBuffer {};
		uint32 *line = guardedLine.data() + kGuardWords;

		fixture.emulator.BeginFrame(0, false);
		fixture.emulator.BeginScanline(0, line, mergeBuffer.data(), anticBuffer.data(), false);
		fixture.emulator.RenderScanline(222, false); // GTIA's production VBXE span limit
		fixture.emulator.EndScanline();
		fixture.emulator.EndFrame();

		canariesIntact = true;
		for(uint32 i = 0; i < kGuardWords; ++i)
			canariesIntact &= guardedLine[i] == kCanary && guardedLine[kGuardWords + kLineWords + i] == kCanary;

		// Return one representative pixel inside the normal-width overlay window.
		return line[48 * 4];
	}
}

bool ATTestAltirraVBXEMemory(ATPortableTestContext& context) {
	auto storage = std::make_unique<VBXEFixture>();
	VBXEFixture& fixture = *storage;

	// MEMAC A maps a 4 KiB bank into $4000. Both MEMAC windows overlap here;
	// A must win over B, and its CPU/ANTIC access bits are independent.
	fixture.vbxeRAM[0x3000] = 0xA3;
	fixture.vbxeRAM[0x4000] = 0xB4;
	fixture.baseRAM[0x4000] = 0x19;
	fixture.WriteRegister(0x5D, 0xC1); // MEMAC B, CPU + ANTIC, bank 1 ($4000 VBXE RAM)
	fixture.WriteRegister(0x5F, 0x83); // MEMAC A, bank 3
	fixture.WriteRegister(0x5E, 0x4C); // 4 KiB, both access modes, mapped at $4000

	AT_PORTABLE_TEST_ASSERT(context, fixture.memory->CPUReadByte(0x4000) == 0xA3);
	AT_PORTABLE_TEST_ASSERT(context, fixture.memory->AnticReadByte(0x4000) == 0xA3);
	fixture.memory->CPUWriteByte(0x4000, 0x5A);
	AT_PORTABLE_TEST_ASSERT(context, fixture.vbxeRAM[0x3000] == 0x5A);
	AT_PORTABLE_TEST_ASSERT(context, fixture.vbxeRAM[0x4000] == 0xB4);

	// Disable A. B should now be visible at the same CPU and ANTIC address.
	fixture.WriteRegister(0x5E, 0x40); // retain the map address, disable MEMAC A
	AT_PORTABLE_TEST_ASSERT(context, fixture.memory->CPUReadByte(0x4000) == 0xB4);
	AT_PORTABLE_TEST_ASSERT(context, fixture.memory->AnticReadByte(0x4000) == 0xB4);

	// Re-enable A for ANTIC only: CPU falls through to base RAM, ANTIC sees VBXE.
	fixture.WriteRegister(0x5D, 0x41); // leave MEMAC B visible only to ANTIC
	fixture.WriteRegister(0x5F, 0x83);
	fixture.WriteRegister(0x5E, 0x44);
	AT_PORTABLE_TEST_ASSERT(context, fixture.memory->CPUReadByte(0x4000) == 0x19);
	AT_PORTABLE_TEST_ASSERT(context, fixture.memory->AnticReadByte(0x4000) == 0x5A);
	fixture.memory->CPUWriteByte(0x4000, 0x2E);
	AT_PORTABLE_TEST_ASSERT(context, fixture.baseRAM[0x4000] == 0x2E);
	AT_PORTABLE_TEST_ASSERT(context, fixture.vbxeRAM[0x3000] == 0x5A);

	// A 32 KiB window starting at page $F0 must truncate at $FFFF rather than
	// wrapping to $0000. The selected RAM bank itself wraps only at 512 KiB.
	fixture.WriteRegister(0x5D, 0x00); // disable B
	fixture.WriteRegister(0x5F, 0xBF);
	fixture.WriteRegister(0x5E, 0xFF); // 32 KiB, CPU + ANTIC, starts at $F000
	fixture.vbxeRAM[0x38000] = 0x71;
	fixture.vbxeRAM[0x38FFF] = 0x72;
	AT_PORTABLE_TEST_ASSERT(context, fixture.memory->CPUReadByte(0xEFFF) == 0x00);
	AT_PORTABLE_TEST_ASSERT(context, fixture.memory->CPUReadByte(0xF000) == 0x71);
	AT_PORTABLE_TEST_ASSERT(context, fixture.memory->CPUReadByte(0xFFFF) == 0x72);
	AT_PORTABLE_TEST_ASSERT(context, fixture.memory->CPUReadByte(0x0000) == 0x00);

	return true;
}

bool ATTestAltirraVBXEBlitter(ATPortableTestContext& context) {
	auto storage = std::make_unique<VBXEFixture>();
	VBXEFixture& fixture = *storage;

	// A two-row, three-byte mode-0 copy across the 512 KiB source wrap.
	constexpr uint32 bcbAddress = 0x0200;
	constexpr uint32 destinationAddress = 0x1800;
	fixture.vbxeRAM[0x7FFFE] = 0x34;
	fixture.vbxeRAM[0x7FFFF] = 0x56;
	fixture.vbxeRAM[0x00000] = 0x78;
	fixture.vbxeRAM[0x00001] = 0x9A;
	fixture.vbxeRAM[0x00002] = 0xBC;
	fixture.vbxeRAM[0x00003] = 0xDE;
	fixture.vbxeRAM[bcbAddress + 0] = 0xFE; // source $7FFFE; fetch wraps at 512 KiB
	fixture.vbxeRAM[bcbAddress + 1] = 0xFF;
	fixture.vbxeRAM[bcbAddress + 2] = 0x07;
	fixture.vbxeRAM[bcbAddress + 3] = 0x03; // next source row starts three bytes later
	fixture.vbxeRAM[bcbAddress + 5] = 0x01; // source X step
	fixture.vbxeRAM[bcbAddress + 6] = static_cast<uint8>(destinationAddress);
	fixture.vbxeRAM[bcbAddress + 7] = static_cast<uint8>(destinationAddress >> 8);
	fixture.vbxeRAM[bcbAddress + 8] = static_cast<uint8>(destinationAddress >> 16);
	fixture.vbxeRAM[bcbAddress + 9] = 0x03; // destination Y step
	fixture.vbxeRAM[bcbAddress + 11] = 0x01; // destination X step
	fixture.vbxeRAM[bcbAddress + 12] = 0x02; // width - 1 = 3 bytes
	fixture.vbxeRAM[bcbAddress + 13] = 0x00;
	fixture.vbxeRAM[bcbAddress + 14] = 0x01; // height - 1 = 2 rows
	fixture.vbxeRAM[bcbAddress + 15] = 0xFF; // copy source byte
	fixture.vbxeRAM[bcbAddress + 16] = 0x00;
	fixture.vbxeRAM[bcbAddress + 17] = 0x00; // collision mask
	fixture.vbxeRAM[bcbAddress + 18] = 0x00; // 1x1 zoom
	fixture.vbxeRAM[bcbAddress + 19] = 0x00; // no pattern
	fixture.vbxeRAM[bcbAddress + 20] = 0x00; // mode 0, terminate list

	fixture.WriteRegister(0x54, 0x01); // enable blitter completion IRQ
	fixture.WriteRegister(0x50, static_cast<uint8>(bcbAddress));
	fixture.WriteRegister(0x51, static_cast<uint8>(bcbAddress >> 8));
	fixture.WriteRegister(0x52, static_cast<uint8>(bcbAddress >> 16));

	std::array<uint32, 912> renderedLine {};
	std::array<uint8, 456> mergeBuffer {};
	std::array<uint8, 456> anticBuffer {};
	fixture.emulator.BeginFrame(0, false);
	fixture.emulator.BeginScanline(0, renderedLine.data(), mergeBuffer.data(), anticBuffer.data(), false);
	fixture.WriteRegister(0x53, 0x01);

	AT_PORTABLE_TEST_ASSERT(context, fixture.vbxeRAM[destinationAddress + 0] == 0x34);
	AT_PORTABLE_TEST_ASSERT(context, fixture.vbxeRAM[destinationAddress + 1] == 0x56);
	AT_PORTABLE_TEST_ASSERT(context, fixture.vbxeRAM[destinationAddress + 2] == 0x78);
	AT_PORTABLE_TEST_ASSERT(context, fixture.vbxeRAM[destinationAddress + 3] == 0x9A);
	AT_PORTABLE_TEST_ASSERT(context, fixture.vbxeRAM[destinationAddress + 4] == 0xBC);
	AT_PORTABLE_TEST_ASSERT(context, fixture.vbxeRAM[destinationAddress + 5] == 0xDE);
	AT_PORTABLE_TEST_ASSERT(context, (fixture.ReadRegister(0x53) & 0x02) != 0);
	AT_PORTABLE_TEST_ASSERT(context, fixture.ReadRegister(0x54) == 0);
	AT_PORTABLE_TEST_ASSERT(context, fixture.irqTarget.mAssertOffsets.empty());

	// The completion callback is dispatched by the real scheduler at the scanline
	// deadline; it then asserts the real VBXE source on the real IRQ controller.
	fixture.AdvanceSchedulerToNextEvent();
	AT_PORTABLE_TEST_ASSERT(context, fixture.ReadRegister(0x53) == 0);
	AT_PORTABLE_TEST_ASSERT(context, fixture.ReadRegister(0x54) == 1);
	AT_PORTABLE_TEST_ASSERT(context, fixture.irqTarget.mAssertOffsets.size() == 1);
	AT_PORTABLE_TEST_ASSERT(context, fixture.irqTarget.mAssertOffsets[0] == 0);

	fixture.WriteRegister(0x54, 0x01); // acknowledge while retaining IRQ enable
	AT_PORTABLE_TEST_ASSERT(context, fixture.ReadRegister(0x54) == 0);
	AT_PORTABLE_TEST_ASSERT(context, fixture.irqTarget.mNegateCount == 1);
	AT_PORTABLE_TEST_ASSERT(context, fixture.irqTarget.mAssertOffsets[0] == 0);

	fixture.emulator.EndScanline();
	fixture.emulator.EndFrame();

	// Reset must release an asserted VBXE line from the shared IRQ controller.
	fixture.emulator.BeginFrame(0, false);
	fixture.emulator.BeginScanline(0, renderedLine.data(), mergeBuffer.data(), anticBuffer.data(), false);
	fixture.WriteRegister(0x53, 0x01);
	fixture.AdvanceSchedulerToNextEvent();
	AT_PORTABLE_TEST_ASSERT(context, fixture.ReadRegister(0x54) == 1);
	fixture.emulator.WarmReset();
	AT_PORTABLE_TEST_ASSERT(context, fixture.irqTarget.mNegateCount == 2);
	fixture.emulator.EndScanline();
	fixture.emulator.EndFrame();

	// Shutdown has the same ownership obligation for a live IRQ source.
	fixture.emulator.BeginFrame(0, false);
	fixture.emulator.BeginScanline(0, renderedLine.data(), mergeBuffer.data(), anticBuffer.data(), false);
	fixture.WriteRegister(0x54, 0x01);
	fixture.WriteRegister(0x53, 0x01);
	fixture.AdvanceSchedulerToNextEvent();
	AT_PORTABLE_TEST_ASSERT(context, fixture.ReadRegister(0x54) == 1);
	fixture.emulator.Shutdown();
	AT_PORTABLE_TEST_ASSERT(context, fixture.irqTarget.mNegateCount == 3);
	return true;
}

bool ATTestAltirraVBXEVideo(ATPortableTestContext& context) {
	auto storage = std::make_unique<VBXEFixture>();
	VBXEFixture& fixture = *storage;

	// The production GTIA palette generator feeds bank zero. Program a distinct
	// bank-one color through the real mapped VBXE palette registers.
	constexpr uint8 backgroundColor = 0x2A;
	constexpr uint32 xdlAddress = 0x0100;
	constexpr uint32 overlayAddress = 0x2000;
	constexpr uint8 overlayColor = 0x2A;
	fixture.emulator.SetRegisterImmediate(0x1A, backgroundColor);
	const uint32 programmedColor = 0xE02040;
	WritePaletteEntry(fixture, 1, 0x02, programmedColor);
	WritePaletteEntry(fixture, 1, 0x0A, programmedColor);
	WritePaletteEntry(fixture, 1, overlayColor, programmedColor);

	std::array<uint32, 1024> rawPalette {};
	uint32 selection = fixture.emulator.GetRawPaletteAndSEL(rawPalette.data());
	AT_PORTABLE_TEST_ASSERT(context, selection == 4 + overlayColor + 1);
	AT_PORTABLE_TEST_ASSERT(context, rawPalette[0] == fixture.paletteGenerator.mPalette[0]);
	AT_PORTABLE_TEST_ASSERT(context, rawPalette[backgroundColor] == fixture.paletteGenerator.mPalette[backgroundColor]);
	AT_PORTABLE_TEST_ASSERT(context, rawPalette[256 + overlayColor] != rawPalette[overlayColor]);
	AT_PORTABLE_TEST_ASSERT(context, rawPalette[256 + overlayColor] == 0xE12040);
	AT_PORTABLE_TEST_ASSERT(context, rawPalette[256 + 0x02] == rawPalette[256 + overlayColor]);
	AT_PORTABLE_TEST_ASSERT(context, rawPalette[256 + 0x0A] == rawPalette[256 + overlayColor]);

	fixture.vbxeRAM[overlayAddress] = overlayColor;
	fixture.vbxeRAM[overlayAddress + 1] = overlayColor;
	fixture.vbxeRAM[overlayAddress + 2] = overlayColor;
	fixture.vbxeRAM[overlayAddress + 3] = overlayColor;
	fixture.vbxeRAM[0x4000 + (0x10 << 3)] = 0xFF; // text glyph row: all foreground
	for(uint32 i = 0; i < 320; i += 2) {
		fixture.vbxeRAM[overlayAddress + i] = 0x10; // character code
		fixture.vbxeRAM[overlayAddress + i + 1] = overlayColor; // palette index
	}

	// Save the real emulated RAM and VBXE registers, corrupt both, then restore.
	ConfigureXDL(fixture, 1, overlayAddress);
	const uint32 savedPaletteColor = rawPalette[256 + overlayColor];
	vdrefptr<IATObjectState> savedState = fixture.emulator.SaveState();
	AT_PORTABLE_TEST_ASSERT(context, savedState != nullptr);
	fixture.vbxeRAM[overlayAddress] = 0;
	WritePaletteEntry(fixture, 1, overlayColor, 0x204080);
	fixture.emulator.LoadState(savedState);
	AT_PORTABLE_TEST_ASSERT(context, fixture.vbxeRAM[overlayAddress] == 0x10);
	selection = fixture.emulator.GetRawPaletteAndSEL(rawPalette.data());
	AT_PORTABLE_TEST_ASSERT(context, selection == 4 + overlayColor + 1);
	AT_PORTABLE_TEST_ASSERT(context, rawPalette[256 + overlayColor] == savedPaletteColor);

	const uint32 restoredOverlayColor = rawPalette[256 + overlayColor];
	for(uint8 mode = 1; mode <= 2; ++mode) {
		// SR consumes 8-bit overlay pixels; HR consumes 4-bit nibbles.
		const uint8 pixelByte = mode == 1 ? overlayColor : 0xAA;
		for(uint32 i = 0; i < 672; ++i)
			fixture.vbxeRAM[overlayAddress + i] = pixelByte;
		ConfigureXDL(fixture, mode, overlayAddress);
		bool canariesIntact = false;
		const uint32 pixel = RenderXDLFrame(fixture, canariesIntact);
		AT_PORTABLE_TEST_ASSERT(context, canariesIntact);
		AT_PORTABLE_TEST_ASSERT(context, (pixel & 0xFFFFFF) == (restoredOverlayColor & 0xFFFFFF));
	}

	// 80-column text uses real character/attribute fetches from VBXE RAM and the
	// real character generator bank selected by the XDL.
	for(uint32 i = 0; i < 320; i += 2) {
		fixture.vbxeRAM[overlayAddress + i] = 0x10;
		fixture.vbxeRAM[overlayAddress + i + 1] = overlayColor;
	}
	fixture.vbxeRAM[0x4000 + (0x10 << 3)] = 0xFF;
	ConfigureXDL(fixture, 0, overlayAddress);
	bool canariesIntact = false;
	const uint32 textPixel = RenderXDLFrame(fixture, canariesIntact);
	AT_PORTABLE_TEST_ASSERT(context, canariesIntact);
	AT_PORTABLE_TEST_ASSERT(context, (textPixel & 0xFFFFFF) == (restoredOverlayColor & 0xFFFFFF));

	// Character and attribute fetches can straddle the end of 512 KiB RAM.
	constexpr uint32 wrappedTextAddress = 0x7FFFF;
	for (uint32 i = 0; i < 320; i += 2) {
		fixture.vbxeRAM[(wrappedTextAddress + i) & 0x7FFFF] = 0x10;
		fixture.vbxeRAM[(wrappedTextAddress + i + 1) & 0x7FFFF] = overlayColor;
	}
	ConfigureXDL(fixture, 0, wrappedTextAddress);
	const uint32 wrappedTextPixel = RenderXDLFrame(fixture, canariesIntact);
	AT_PORTABLE_TEST_ASSERT(context, canariesIntact);
	AT_PORTABLE_TEST_ASSERT(context, (wrappedTextPixel & 0xFFFFFF) == (restoredOverlayColor & 0xFFFFFF));

	// The GTIA/background render path remains operational alongside the XDL
	// overlays and fills the full 912-word scanline including right blanking.
	fixture.WriteRegister(0x40, 0x00);
	fixture.emulator.SetRegisterImmediate(0x1A, backgroundColor);
	std::array<uint32, 912> blankLine {};
	std::array<uint8, 456> mergeBuffer {};
	std::array<uint8, 456> anticBuffer {};
	fixture.emulator.BeginFrame(0, false);
	fixture.emulator.BeginScanline(0, blankLine.data(), mergeBuffer.data(), anticBuffer.data(), false);
	fixture.emulator.RenderScanline(222, false);
	fixture.emulator.EndScanline();
	fixture.emulator.EndFrame();
	// Snapshot palette components have the hardware RAMDAC's seven-bit precision;
	// compare to the restored architectural palette, not the initial 8-bit input.
	AT_PORTABLE_TEST_ASSERT(context, (blankLine[0] & 0xFFFFFF) == rawPalette[backgroundColor]);
	AT_PORTABLE_TEST_ASSERT(context, (blankLine[887] & 0xFFFFFF) == rawPalette[backgroundColor]);
	AT_PORTABLE_TEST_ASSERT(context, (blankLine[888] & 0xFFFFFF) == rawPalette[backgroundColor]);
	AT_PORTABLE_TEST_ASSERT(context, (blankLine[911] & 0xFFFFFF) == rawPalette[backgroundColor]);

	// Snapshot only the still-pending portion of the register queue after the
	// first write has already been consumed by a partial scanline render.
	fixture.emulator.BeginFrame(0, false);
	fixture.emulator.BeginScanline(0, blankLine.data(), mergeBuffer.data(), anticBuffer.data(), false);
	fixture.emulator.AddRegisterChange(10, 0x1A, 0x44);
	fixture.emulator.AddRegisterChange(90, 0x1A, 0x66);
	fixture.emulator.RenderScanline(20, false);
	vdrefptr<IATObjectState> queueState = fixture.emulator.SaveState();
	AT_PORTABLE_TEST_ASSERT(context, queueState != nullptr);
	const auto& savedQueueState = atser_cast<const ATSaveStateVbxe&>(*queueState);
	AT_PORTABLE_TEST_ASSERT(context, savedQueueState.mGtiaRegisterChanges.size() == 3);
	AT_PORTABLE_TEST_ASSERT(context, savedQueueState.mGtiaRegisterChanges[0] == 90);
	AT_PORTABLE_TEST_ASSERT(context, savedQueueState.mGtiaRegisterChanges[1] == 0x1A);
	AT_PORTABLE_TEST_ASSERT(context, savedQueueState.mGtiaRegisterChanges[2] == 0x66);
	fixture.emulator.EndScanline();
	fixture.emulator.EndFrame();

	return true;
}
