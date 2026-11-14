// Production input motion controllers wired to the real PIA, GTIA, POKEY and ANTIC fixture.

#include <memory>

#include <at/attest/portabletest.h>
#include <inputcontroller.h>
#include "portmanagerportablefixture.h"

namespace {
	void ConfigurePIAPort0Inputs(PortManagerPortableFixture& f) {
		f.mPIA.WriteByte(2, 4);
		f.mPIA.WriteByte(3, 4);
	}

	uint8 Port0Pins(PortManagerPortableFixture& f) {
		return f.mPIA.ReadByte(0) & 15;
	}

	bool TestMouseMode(ATPortableTestContext& context, bool amigaMode) {
		auto f = std::make_unique<PortManagerPortableFixture>();
		ConfigurePIAPort0Inputs(*f);
		ATMouseController mouse(amigaMode);
		mouse.Init(&f->mVideo.mScheduler);
		mouse.Attach(f->mPorts, 0);

		// One pixel is 65536 target units; ApplyImpulse scales ds by 32.
		// The calibrated event interval is 256 cycles per pixel at one-pixel error.
		mouse.ApplyImpulse(kATInputTrigger_Right, 2048);
		f->mVideo.Tick(256);
		// Positive X phase 1 is ST=0010, Amiga=0010: PIA inputs are active low.
		AT_PORTABLE_TEST_ASSERT(context, Port0Pins(*f) == 0xD);
		mouse.ApplyImpulse(kATInputTrigger_Right, 2048);
		f->mVideo.Tick(384); // scheduled next pixel step reaches phase 2
		// Phase 2 is ST=0011, Amiga=1010; the differing quadrature table is observable.
		AT_PORTABLE_TEST_ASSERT(context, Port0Pins(*f) == (amigaMode ? 0x5 : 0xC));

		mouse.SetDigitalTrigger(kATInputTrigger_Button0, true);
		AT_PORTABLE_TEST_ASSERT(context, f->Trigger(0) == 0);
		mouse.SetDigitalTrigger(kATInputTrigger_Button0, false);
		AT_PORTABLE_TEST_ASSERT(context, f->Trigger(0) == 1);
		mouse.SetDigitalTrigger(kATInputTrigger_Button0 + 1, true);
		AT_PORTABLE_TEST_ASSERT(context, f->Pot(0) == 1);
		mouse.SetDigitalTrigger(kATInputTrigger_Button0 + 1, false);
		AT_PORTABLE_TEST_ASSERT(context, f->Pot(0) == 228);

		// Two negative pixel impulses unwind phase 2 to phase 0.
		mouse.ApplyImpulse(kATInputTrigger_Left, 4096);
		f->mVideo.Tick(384);
		AT_PORTABLE_TEST_ASSERT(context, Port0Pins(*f) == 15);
		mouse.ApplyImpulse(kATInputTrigger_Left, 2048);
		f->mVideo.Tick(256);
		AT_PORTABLE_TEST_ASSERT(context, Port0Pins(*f) == (amigaMode ? 7 : 14));

		mouse.ApplyImpulse(kATInputTrigger_Right, 2048);
		mouse.Detach();
		f->mVideo.Tick(300);
		return true;
	}

	bool TestMouse(ATPortableTestContext& context) {
		AT_PORTABLE_TEST_ASSERT(context, TestMouseMode(context, false));
		AT_PORTABLE_TEST_ASSERT(context, TestMouseMode(context, true));
		return true;
	}

	bool TestTrackball(ATPortableTestContext& context) {
		auto f = std::make_unique<PortManagerPortableFixture>();
		ConfigurePIAPort0Inputs(*f);
		ATTrackballController trackball;
		trackball.Init(&f->mVideo.mScheduler);
		trackball.Attach(f->mPorts, 0);
		trackball.ApplyImpulse(kATInputTrigger_Right, 2048);
		f->mVideo.Tick(32); // first CX80 poll; each subsequent poll is every 7 cycles
		// CX80 positive X first phase: data bit 1 low, direction bit 0 clear.
		AT_PORTABLE_TEST_ASSERT(context, Port0Pins(*f) == 0xD);
		trackball.ApplyImpulse(kATInputTrigger_Left, 4096);
		f->mVideo.Tick(7); // moves one step back to zero
		AT_PORTABLE_TEST_ASSERT(context, Port0Pins(*f) == 14); // negative direction, phase zero
		f->mVideo.Tick(7); // wraps one step negative; direction bit 0 asserted
		AT_PORTABLE_TEST_ASSERT(context, Port0Pins(*f) == 0xC);
		trackball.SetDigitalTrigger(kATInputTrigger_Button0, true);
		AT_PORTABLE_TEST_ASSERT(context, f->Trigger(0) == 0);
		trackball.SetDigitalTrigger(kATInputTrigger_Button0, false);
		AT_PORTABLE_TEST_ASSERT(context, f->Trigger(0) == 1);
		trackball.Detach();
		f->mVideo.Tick(100);
		return true;
	}

	bool TestDriving(ATPortableTestContext& context) {
		auto f = std::make_unique<PortManagerPortableFixture>();
		ConfigurePIAPort0Inputs(*f);
		ATDrivingController driving;
		driving.Attach(f->mPorts, 0);
		// Rotary state uses a 2-bit Gray sequence {00,01,11,10}; 8192 impulse
		// units turn the top two phase bits once because each unit subtracts 0x20000.
		driving.ApplyImpulse(kATInputTrigger_Right, 8192);
		AT_PORTABLE_TEST_ASSERT(context, Port0Pins(*f) == 0xD);
		driving.ApplyImpulse(kATInputTrigger_Right, 8192);
		AT_PORTABLE_TEST_ASSERT(context, Port0Pins(*f) == 0xC);
		driving.ApplyImpulse(kATInputTrigger_Left, 8192);
		AT_PORTABLE_TEST_ASSERT(context, Port0Pins(*f) == 0xD);
		driving.ApplyImpulse(kATInputTrigger_Left, 8192);
		AT_PORTABLE_TEST_ASSERT(context, Port0Pins(*f) == 15);
		driving.SetDigitalTrigger(kATInputTrigger_Button0, true);
		AT_PORTABLE_TEST_ASSERT(context, f->Trigger(0) == 0);
		driving.SetDigitalTrigger(kATInputTrigger_Button0, false);
		AT_PORTABLE_TEST_ASSERT(context, f->Trigger(0) == 1);
		return true;
	}

	bool TestPaddle(ATPortableTestContext& context) {
		auto f = std::make_unique<PortManagerPortableFixture>();
		ConfigurePIAPort0Inputs(*f);
		ATPaddleController paddle;
		paddle.Attach(f->mPorts, 0);
		paddle.ApplyAnalogInput(kATInputTrigger_Axis0, 0);
		const uint8 center = f->Pot(0);
		paddle.ApplyAnalogInput(kATInputTrigger_Axis0, 65536);
		const uint8 low = f->Pot(0);
		paddle.ApplyAnalogInput(kATInputTrigger_Axis0, -65536);
		const uint8 high = f->Pot(0);
		// Axis0 is normalized [-1,+1], and the model maps its ends to the
		// minimum/maximum paddle resistance observed by a real POTGO scan.
		AT_PORTABLE_TEST_ASSERT(context, low < center && center < high);

		paddle.SetHalf(true);
		paddle.ApplyAnalogInput(kATInputTrigger_Axis0, 0);
		AT_PORTABLE_TEST_ASSERT(context, f->Pot(1) < 228);
		paddle.SetDigitalTrigger(kATInputTrigger_Button0, true);
		AT_PORTABLE_TEST_ASSERT(context, Port0Pins(*f) == 0x7); // second paddle fire pulls right low
		paddle.SetDigitalTrigger(kATInputTrigger_Button0, false);
		AT_PORTABLE_TEST_ASSERT(context, Port0Pins(*f) == 15);

		const uint8 before = f->Pot(1);
		paddle.ApplyImpulse(kATInputTrigger_Right, 2048);
		const uint8 afterRight = f->Pot(1);
		paddle.ApplyImpulse(kATInputTrigger_Left, 2048);
		const uint8 afterLeft = f->Pot(1);
		AT_PORTABLE_TEST_ASSERT(context, afterRight < before);
		AT_PORTABLE_TEST_ASSERT(context, afterLeft > afterRight);
		paddle.Detach();
		AT_PORTABLE_TEST_ASSERT(context, f->Pot(1) == 228);
		return true;
	}
}

bool ATTestAltirraInputMouse(ATPortableTestContext& context) {
	return TestMouse(context);
}

bool ATTestAltirraInputTrackball(ATPortableTestContext& context) {
	return TestTrackball(context);
}

bool ATTestAltirraInputDriving(ATPortableTestContext& context) {
	return TestDriving(context);
}

bool ATTestAltirraInputPaddle(ATPortableTestContext& context) {
	return TestPaddle(context);
}
