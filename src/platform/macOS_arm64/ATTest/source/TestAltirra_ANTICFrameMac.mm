// Real ANTIC DMA/decode -> GTIA -> native AppKit display frame contract.
#import <AppKit/AppKit.h>
#include <array>
#include <memory>

#include <at/attest/portabletest.h>
#include <at/atnativeui/uinativewindow.h>
#include <vd2/system/refcount.h>
#include <vd2/system/vdstl.h>
#include <vd2/Kasumi/pixmaputils.h>
#include <vd2/VDDisplay/display_macos.h>
#include <vd2/VDDisplay/displayview_macos.h>
#include <anticportablefixture.h>

namespace {
	struct WindowGuard {
		vdrefptr<ATUINativeWindow>& mWindow;
		bool mbActive = true;
		void Destroy() {
			if (mbActive && mWindow)
				mWindow->Destroy();
			mbActive = false;
		}
		~WindowGuard() { Destroy(); }
	};

	struct ViewDeleter {
		void operator()(__VDGUIHandle *view) const { VDDestroyDisplayViewMac(view); }
	};

	struct DisplayDeleter {
		void operator()(IVDVideoDisplay *display) const { if (display) display->Destroy(); }
	};

	struct GTIADisplayDetachGuard {
		ATGTIAEmulator *mpGTIA = nullptr;
		void Detach() {
			if (mpGTIA) {
				mpGTIA->SetVideoOutput(nullptr);
				mpGTIA = nullptr;
			}
		}
		~GTIADisplayDetachGuard() { Detach(); }
	};

	bool ContainsBusRead(const ANTICPortableFixture& fixture, uint16 address) {
		for(const auto& read : fixture.mBusReads) {
			if (read.mAddress == address)
				return true;
		}
		return false;
	}
}

bool ATTestAltirraANTICFrameMac(ATPortableTestContext& context) {
	auto fixtureStorage = std::make_unique<ANTICPortableFixture>();
	ANTICPortableFixture& fixture = *fixtureStorage;

	@autoreleasepool {
		vdrefptr<ATUINativeWindow> window(new ATUINativeWindow);
		WindowGuard windowGuard { window };
		AT_PORTABLE_TEST_ASSERT(context, window->CreateWindow({ 50, 50, 530, 350 }, L"ANTIC frame"));
		std::unique_ptr<__VDGUIHandle, ViewDeleter> viewHandle(VDCreateDisplayViewMac());
		AT_PORTABLE_TEST_ASSERT(context, viewHandle != nullptr);
		NSView *view = reinterpret_cast<NSView *>(viewHandle.get());
		[view setFrame:NSMakeRect(0, 0, 456, 262)];
		[reinterpret_cast<NSView *>(window->GetClientViewHandle()) addSubview:view];
		std::unique_ptr<IVDVideoDisplay, DisplayDeleter> display(VDCreateVideoDisplayMac(viewHandle.get()));
		AT_PORTABLE_TEST_ASSERT(context, display != nullptr);
		display->SetFilterMode(IVDVideoDisplay::kFilterPoint);
		fixture.AttachVideoDisplay(display.get());
		GTIADisplayDetachGuard displayDetachGuard { &fixture.mGTIA };

		// A real mode-4 text display list. Two character cells select different
		// playfield colors through their glyph nibbles; ANTIC fetches both the
		// screen names and character rows from the fixture's bus-backed RAM.
		constexpr uint16 displayListAddress = 0x2000;
		constexpr uint16 screenAddress = 0x4000;
		constexpr uint16 characterBase = 0x3000;
		fixture.mGTIA.SetArtifactingMode(ATArtifactMode::None);
		fixture.mGTIA.SetPALMode(false);
		fixture.mGTIA.SetForcedBorder(false);
		fixture.mGTIA.WriteByte(0x16, 0xB6); // COLPF0
		fixture.mGTIA.WriteByte(0x17, 0x5A); // COLPF1
		fixture.mGTIA.WriteByte(0x18, 0xD2); // COLPF2
		fixture.mGTIA.WriteByte(0x19, 0x74); // COLPF3
		fixture.mGTIA.WriteByte(0x1A, 0x06); // COLBK
		fixture.WriteANTIC(0x09, characterBase >> 8); // CHBASE
		fixture.WriteANTIC(0x00, 0x22); // normal width + display-list DMA

		for(uint32 row = 0; row < 8; ++row) {
			fixture.mRAM[characterBase + 1 * 8 + row] = 0x66; // PF0/PF1 glyph
			fixture.mRAM[characterBase + 2 * 8 + row] = 0x55; // PF0 glyph
		}
		for(uint32 row = 0; row < 24; ++row) {
			for(uint32 col = 0; col < 40; ++col)
				fixture.mRAM[screenAddress + row * 40 + col] = col < 20 ? 1 : 2;
		}

		std::array<uint8, 30> displayList {};
		displayList[0] = 0x44; // mode 4 + LMS
		displayList[1] = static_cast<uint8>(screenAddress);
		displayList[2] = static_cast<uint8>(screenAddress >> 8);
		for(uint32 i = 0; i < 23; ++i)
			displayList[3 + i] = 0x04; // 23 further 8-line text rows
		displayList[26] = 0x41; // JVB
		displayList[27] = static_cast<uint8>(displayListAddress);
		displayList[28] = static_cast<uint8>(displayListAddress >> 8);
		for(uint32 i = 0; i < displayList.size(); ++i)
			fixture.mRAM[displayListAddress + i] = displayList[i];
		fixture.SetDisplayList(displayListAddress);

		const uint32 expectedPF0 = fixture.mGTIA.GetPlayfieldColor24(0) & 0x00ffffff;
		const uint32 expectedPF1 = fixture.mGTIA.GetPlayfieldColor24(1) & 0x00ffffff;
		const uint32 expectedBackground = fixture.mGTIA.GetBackgroundColor24() & 0x00ffffff;
		AT_PORTABLE_TEST_ASSERT(context, expectedPF0 != expectedPF1);
		AT_PORTABLE_TEST_ASSERT(context, expectedBackground != expectedPF0);

		AT_PORTABLE_TEST_ASSERT(context, fixture.RunToBeam(248, 0));
		AT_PORTABLE_TEST_ASSERT(context, fixture.BeginVideoFrame());
		fixture.Tick(fixture.mAntic.GetScanlineCount() * 114);
		AT_PORTABLE_TEST_ASSERT(context, fixture.mAntic.GetBeamY() == 248);
		AT_PORTABLE_TEST_ASSERT(context, fixture.mAntic.GetBeamX() == 0);
		fixture.mGTIA.UpdateScreen(false, false);

		VDPixmapBuffer rawStorage;
		VDPixmap rawFrame;
		float rawPAR = 0.0f;
		AT_PORTABLE_TEST_ASSERT(context, fixture.mGTIA.GetLastFrameBufferRaw(rawStorage, rawFrame, rawPAR));
		AT_PORTABLE_TEST_ASSERT(context, rawFrame.format == nsVDPixmap::kPixFormat_Pal8);
		AT_PORTABLE_TEST_ASSERT(context, rawFrame.palette != nullptr);
		AT_PORTABLE_TEST_ASSERT(context, (rawFrame.palette[0xB6] & 0x00ffffff) == expectedPF0);
		AT_PORTABLE_TEST_ASSERT(context, (rawFrame.palette[0x5A] & 0x00ffffff) == expectedPF1);
		AT_PORTABLE_TEST_ASSERT(context, ContainsBusRead(fixture, displayListAddress));
		AT_PORTABLE_TEST_ASSERT(context, ContainsBusRead(fixture, displayListAddress + 1));
		AT_PORTABLE_TEST_ASSERT(context, ContainsBusRead(fixture, screenAddress));
		AT_PORTABLE_TEST_ASSERT(context, ContainsBusRead(fixture, characterBase + 1 * 8));

		bool historyHasLMSRow = false;
		const auto *history = fixture.mAntic.GetDLHistory();
		for(uint32 y = 0; y < fixture.mAntic.GetScanlineCount(); ++y) {
			const auto& entry = history[y];
			if (entry.mbValid && (entry.mControl & 0x0f) == 4
				&& entry.mPFAddress >= screenAddress && entry.mPFAddress < screenAddress + 24 * 40) {
				historyHasLMSRow = true;
				break;
			}
		}
		AT_PORTABLE_TEST_ASSERT(context, historyHasLMSRow);

		uint32 indexedPF0 = 0;
		uint32 indexedPF1 = 0;
		for(int y = 0; y < rawFrame.h; ++y) {
			const uint8 *row = rawFrame.GetPixelRow<uint8>(y);
			for(int x = 0; x < rawFrame.w; ++x) {
				indexedPF0 += row[x] == 0xB6;
				indexedPF1 += row[x] == 0x5A;
			}
		}
		AT_PORTABLE_TEST_ASSERT(context, indexedPF0 > 0 && indexedPF1 > 0);

		VDPixmapBuffer capture;
		display->Update(0);
		CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.01, false);
		display->RequestCapture([&](const VDPixmap *px) { if (px) capture.assign(*px); });
		for(int i = 0; i < 100 && !capture.data; ++i)
			CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.01, false);
		const auto output = VDDisplayViewGetOutputInfoMac(viewHandle.get());
		AT_PORTABLE_TEST_ASSERT(context, capture.data != nullptr);
		AT_PORTABLE_TEST_ASSERT(context, capture.w == output.mWidth && capture.h == output.mHeight);
		AT_PORTABLE_TEST_ASSERT(context, capture.format == nsVDPixmap::kPixFormat_XRGB8888);

		uint32 capturedPF0 = 0;
		uint32 capturedPF1 = 0;
		uint32 capturedBackground = 0;
		for(int y = 0; y < capture.h; ++y) {
			const uint32 *row = capture.GetPixelRow<uint32>(y);
			for(int x = 0; x < capture.w; ++x) {
				const uint32 rgb = row[x] & 0x00ffffff;
				capturedPF0 += rgb == expectedPF0;
				capturedPF1 += rgb == expectedPF1;
				capturedBackground += rgb == expectedBackground;
			}
		}
		AT_PORTABLE_TEST_ASSERT(context, capturedPF0 > 0 && capturedPF1 > 0);
		AT_PORTABLE_TEST_ASSERT(context, capturedBackground > 0);

		// Tear down native resources before releasing the fixture owning ANTIC RAM.
		window->SetVisible(false);
		displayDetachGuard.Detach();
		display.reset();
		viewHandle.reset();
		windowGuard.Destroy();
		fixtureStorage.reset();
	}
	return true;
}
