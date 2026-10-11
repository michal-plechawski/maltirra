// Real GTIA frame generation submitted to the native AppKit display backend.
#import <AppKit/AppKit.h>
#include <array>
#include <memory>
#include <vector>

#include <at/atcore/scheduler.h>
#include <at/attest/portabletest.h>
#include <at/atnativeui/uinativewindow.h>
#include <artifacting.h>
#include <irqcontroller.h>
#include <memorymanager.h>
#include <palettegenerator.h>
#include <vbxe.h>
#include <vd2/system/refcount.h>
#include <vd2/system/vdstl.h>
#include <vd2/Kasumi/pixmaputils.h>
#include <vd2/VDDisplay/display_macos.h>
#include <vd2/VDDisplay/displayview_macos.h>
#include <gtia.h>

namespace {
	class GTIAFrameConnections final : public IATGTIAEmulatorConnections {
	public:
		uint32 GTIAGetXClock() override { return mXClock; }
		uint32 GTIAGetTimestamp() const override { return mTimestamp; }
		uint64 GTIAGetTimestamp64() const override { return mTimestamp64; }
		void GTIASetSpeaker(bool) override {}
		void GTIASelectController(uint8, bool) override {}
		void GTIARequestAnticSync(int offset) override {
			mSyncOffsets.push_back(offset);
		}
		uint32 GTIAGetLineEdgeTimingId(uint32 offset) const override {
			const uint32 beamX = mXClock / 2;
			return mTimestamp - beamX + (beamX >= offset ? 114 : 0);
		}

		uint32 mXClock = 0;
		uint32 mTimestamp = 0;
		uint64 mTimestamp64 = 0;
		std::vector<int> mSyncOffsets;
	};

	struct IRQTarget {
		void AssertIRQ(int) {}
		void NegateIRQ() {}
	};

	class VBXEFrameFixture {
	public:
		explicit VBXEFrameFixture(const ATColorParams& colorParams) {
			mMemory->Init();
			mIRQController.Init(&mIRQTarget);
			mScheduler.SetRate(VDFraction(17897725, 10));
			mBaseLayer = mMemory->CreateLayer(kATMemoryPri_BaseRAM, mBaseRAM.data(), 0, 256, false);
			mMemory->EnableLayer(mBaseLayer, true);
			mPaletteGenerator.Generate(colorParams, ATMonitorMode::Color);
			mVBXERAM.resize(512 * 1024);
			mVBXE.SetMemory(mVBXERAM.data());
			mVBXE.SetRegisterBase(0xD6);
			mVBXE.SetDefaultPalette(mPaletteGenerator.mPalette, &mArtifacting);
			mVBXE.Init(&mIRQController, mMemory.get(), &mScheduler);
		}

		~VBXEFrameFixture() {
			mVBXE.Shutdown();
			mMemory->DeleteLayer(mBaseLayer);
		}

		void WriteRegister(uint8 reg, uint8 value) { mVBXE.WriteControl(reg, value); }

		void ConfigureXDL(uint32 overlayAddress, uint8 color) {
			constexpr uint32 xdlAddress = 0x0100;
			for(uint32 i = 0; i < 672; ++i)
				mVBXERAM[overlayAddress + i] = color;
			mVBXERAM[xdlAddress + 0] = 0x62; // bitmap, repeat count + overlay address
			mVBXERAM[xdlAddress + 1] = 0x88; // standard-resolution overlay
			mVBXERAM[xdlAddress + 2] = 238; // 239 display lines, including this one
			mVBXERAM[xdlAddress + 3] = static_cast<uint8>(overlayAddress);
			mVBXERAM[xdlAddress + 4] = static_cast<uint8>(overlayAddress >> 8);
			mVBXERAM[xdlAddress + 5] = static_cast<uint8>(overlayAddress >> 16);
			mVBXERAM[xdlAddress + 6] = 0;
			mVBXERAM[xdlAddress + 7] = 0;
			mVBXERAM[xdlAddress + 8] = 0x11; // normal width, palette bank one
			mVBXERAM[xdlAddress + 9] = 0xFF; // overlay has priority
			WriteRegister(0x41, static_cast<uint8>(xdlAddress));
			WriteRegister(0x42, static_cast<uint8>(xdlAddress >> 8));
			WriteRegister(0x43, static_cast<uint8>(xdlAddress >> 16));
			WriteRegister(0x40, 0x01); // enable XDL fetch/render
		}

		std::unique_ptr<ATMemoryManager> mMemory = std::make_unique<ATMemoryManager>();
		ATScheduler mScheduler;
		ATIRQController mIRQController;
		IRQTarget mIRQTarget;
		ATArtifactingEngine mArtifacting;
		ATColorPaletteGenerator mPaletteGenerator;
		ATVBXEEmulator mVBXE;
		std::vector<uint8> mVBXERAM;
		std::array<uint8, 64 * 1024> mBaseRAM {};
		ATMemoryLayer *mBaseLayer = nullptr;
	};

	struct WindowGuard {
		vdrefptr<ATUINativeWindow>& mWindow;
		~WindowGuard() { if (mWindow) mWindow->Destroy(); }
	};

	struct ViewDeleter {
		void operator()(__VDGUIHandle *view) const { VDDestroyDisplayViewMac(view); }
	};

	struct DisplayDeleter {
		void operator()(IVDVideoDisplay *display) const { if (display) display->Destroy(); }
	};
}

bool ATTestAltirraGTIAFrameMac(ATPortableTestContext& context) {
	@autoreleasepool {
		vdrefptr<ATUINativeWindow> window(new ATUINativeWindow);
		WindowGuard windowGuard { window };
		AT_PORTABLE_TEST_ASSERT(context, window->CreateWindow({ 50, 50, 530, 350 }, L"GTIA frame"));

		std::unique_ptr<__VDGUIHandle, ViewDeleter> viewHandle(VDCreateDisplayViewMac());
		AT_PORTABLE_TEST_ASSERT(context, viewHandle != nullptr);
		NSView *view = reinterpret_cast<NSView *>(viewHandle.get());
		[view setFrame:NSMakeRect(0, 0, 456, 262)];
		[reinterpret_cast<NSView *>(window->GetClientViewHandle()) addSubview:view];
		std::unique_ptr<IVDVideoDisplay, DisplayDeleter> display(VDCreateVideoDisplayMac(viewHandle.get()));
		AT_PORTABLE_TEST_ASSERT(context, display != nullptr);
		display->SetFilterMode(IVDVideoDisplay::kFilterPoint);

		// Drive the production GTIA against a deterministic clock. ANTIC and CPU
		// are deliberately outside this component test; pixel output is not faked.
		GTIAFrameConnections connections;
		auto gtiaStorage = std::make_unique<ATGTIAEmulator>();
		ATGTIAEmulator& gtia = *gtiaStorage;
		gtia.Init(&connections);
		gtia.SetVideoOutput(display.get());
		gtia.SetArtifactingMode(ATArtifactMode::None);
		gtia.SetPALMode(false);
		gtia.SetSECAMMode(false);
		gtia.SetForcedBorder(false);

		gtia.WriteByte(0x17, 0xB6); // COLPF1: non-gray, visible playfield color.
		const uint32 expectedPF1 = gtia.GetPlayfieldColor24(1) & 0x00ffffff;
		const uint32 expectedBackground = gtia.GetBackgroundColor24() & 0x00ffffff;
		AT_PORTABLE_TEST_ASSERT(context, expectedPF1 != expectedBackground);

		std::array<uint8, 78> playfieldBytes;
		playfieldBytes.fill(0x22);
		AT_PORTABLE_TEST_ASSERT(context, gtia.BeginFrame(1, 0, true, false));
		for(int y = 0; y < 262; ++y) {
			connections.mXClock = 0;
			connections.mTimestamp = static_cast<uint32>(y * 114);
			connections.mTimestamp64 = connections.mTimestamp;
			gtia.BeginScanline(y, false);
			if (y == 40)
				gtia.UpdatePlayfield160(36, playfieldBytes.data(), playfieldBytes.size());
			connections.mXClock = 226;
			connections.mTimestamp += 113;
			connections.mTimestamp64 = connections.mTimestamp;
			gtia.EndScanline(0, true);
		}
		gtia.UpdateScreen(false, false);

		VDPixmapBuffer capture;
		display->Update(0);
		CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.01, false);
		display->RequestCapture([&](const VDPixmap *px) { if (px) capture.assign(*px); });
		for(int i = 0; i < 100 && !capture.data; ++i)
			CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.01, false);
		const auto output = VDDisplayViewGetOutputInfoMac(viewHandle.get());
		AT_PORTABLE_TEST_ASSERT(context, capture.data != nullptr);
		AT_PORTABLE_TEST_ASSERT(context, capture.w == output.mWidth && capture.h == output.mHeight);
		AT_PORTABLE_TEST_ASSERT(context, capture.w >= 376 && capture.h >= 240);

		uint32 pfPixels = 0;
		uint32 backgroundPixels = 0;
		for(int y = 0; y < capture.h; ++y) {
			const uint32 *row = capture.GetPixelRow<uint32>(y);
			for(int x = 0; x < capture.w; ++x) {
				const uint32 rgb = row[x] & 0x00ffffff;
				pfPixels += rgb == expectedPF1;
				backgroundPixels += rgb == expectedBackground;
			}
		}
		AT_PORTABLE_TEST_ASSERT(context, pfPixels > 0);
		AT_PORTABLE_TEST_ASSERT(context, backgroundPixels > 0);
		AT_PORTABLE_TEST_ASSERT(context, !connections.mSyncOffsets.empty());

		// Repeat through the real VBXE RGB path. Its XDL and overlay pixels live
		// in emulated VBXE RAM; no intermediate bitmap is substituted here.
		ATColorSettings colorSettings = gtia.GetColorSettings();
		auto vbxeStorage = std::make_unique<VBXEFrameFixture>(colorSettings.mNTSCParams);
		VBXEFrameFixture& vbxe = *vbxeStorage;
		constexpr uint8 overlayColor = 0x2A;
		constexpr uint32 overlayAddress = 0x2000;
		constexpr uint32 xdlAddress = 0x0100;
		vbxe.WriteRegister(0x44, overlayColor);
		vbxe.WriteRegister(0x45, 1);
		vbxe.WriteRegister(0x46, 0xE1);
		vbxe.WriteRegister(0x47, 0x21);
		vbxe.WriteRegister(0x48, 0x41);
		vbxe.ConfigureXDL(overlayAddress, overlayColor);
		gtia.SetVBXE(&vbxe.mVBXE);
		std::array<uint32, 1024> rawVBXEColors {};
		vbxe.mVBXE.GetRawPaletteAndSEL(rawVBXEColors.data());
		auto colorOracle = std::make_unique<ATArtifactingEngine>();
		const auto& matching = vbxe.mPaletteGenerator.mColorMatchingMatrix;
		const auto& tint = vbxe.mPaletteGenerator.mTintColor;
		colorOracle->SetColorParams(colorSettings.mNTSCParams,
			matching ? &*matching : nullptr, tint ? &*tint : nullptr, ATMonitorMode::Color, 0);
		const uint32 expectedOverlay = colorOracle->CorrectSingleColor(
			rawVBXEColors[256 + overlayColor], true, false) & 0x00ffffff;
		struct VBXEDetachGuard {
			ATGTIAEmulator& mGTIA;
			bool mbAttached = true;
			~VBXEDetachGuard() { if (mbAttached) mGTIA.SetVBXE(nullptr); }
		} vbxeDetachGuard { gtia };
		connections.mTimestamp = 262 * 114;
		connections.mTimestamp64 = connections.mTimestamp;
		AT_PORTABLE_TEST_ASSERT(context, gtia.BeginFrame(2, 0, true, false));
		for(int y = 0; y < 262; ++y) {
			connections.mXClock = 0;
			connections.mTimestamp = static_cast<uint32>(262 * 114 + y * 114);
			connections.mTimestamp64 = connections.mTimestamp;
			gtia.BeginScanline(y, false);
			connections.mXClock = 226;
			connections.mTimestamp += 113;
			connections.mTimestamp64 = connections.mTimestamp;
			gtia.EndScanline(0, true);
		}
		gtia.UpdateScreen(false, false);
		VDPixmapBuffer rawFrameStorage;
		VDPixmap rawFrame;
		float rawPAR = 0.0f;
		AT_PORTABLE_TEST_ASSERT(context, gtia.GetLastFrameBufferRaw(rawFrameStorage, rawFrame, rawPAR));
		AT_PORTABLE_TEST_ASSERT(context, rawFrame.format == nsVDPixmap::kPixFormat_XRGB8888);
		uint32 rawOverlayPixels = 0;
		for(int y = 0; y < rawFrame.h; ++y) {
			const uint32 *row = rawFrame.GetPixelRow<uint32>(y);
			for(int x = 0; x < rawFrame.w; ++x)
				rawOverlayPixels += (row[x] & 0x00ffffff) == expectedOverlay;
		}
		AT_PORTABLE_TEST_ASSERT(context, rawOverlayPixels > 0);
		capture.clear();
		display->Update(0);
		CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.01, false);
		display->RequestCapture([&](const VDPixmap *px) { if (px) capture.assign(*px); });
		for(int i = 0; i < 100 && !capture.data; ++i)
			CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.01, false);
		AT_PORTABLE_TEST_ASSERT(context, capture.data != nullptr);
		AT_PORTABLE_TEST_ASSERT(context, capture.format == nsVDPixmap::kPixFormat_XRGB8888);
		uint32 overlayPixels = 0;
		for(int y = 0; y < capture.h; ++y) {
			const uint32 *row = capture.GetPixelRow<uint32>(y);
			for(int x = 0; x < capture.w; ++x)
				overlayPixels += (row[x] & 0x00ffffff) == expectedOverlay;
		}
		AT_PORTABLE_TEST_ASSERT(context, overlayPixels > 0);
		const ATVBXEXDLHistoryEntry *xdlLine = vbxe.mVBXE.GetXDLHistory(9);
		AT_PORTABLE_TEST_ASSERT(context, xdlLine
			&& xdlLine->mOverlayMode == static_cast<uint32>(ATVBXEOverlayMode::SR));
		gtia.SetVBXE(nullptr);
		vbxeDetachGuard.mbAttached = false;
		vbxeStorage.reset();

		// Keep destruction ordered: GTIA unregisters its callbacks before the
		// native display/view/window are released.
		gtia.SetVideoOutput(nullptr);
		gtiaStorage.reset();
		display.reset();
	}
	return true;
}
