// Joystick port management through the real PIA/GTIA/POKEY/ANTIC cores.
#include <array>
#include <climits>
#include <memory>
#include <at/attest/portabletest.h>
#include "portmanagerportablefixture.h"

bool ATTestAltirraPortDirections(ATPortableTestContext& context) {
	auto f = std::make_unique<PortManagerPortableFixture>();
	struct RawOutput { uint32 mCalls = 0, mState = 0; } output;
	uint32 changes = 0;
	auto a = f->Port(0), b = f->Port(1), overlapping = f->Port(0), c = f->Port(2), d = f->Port(3);
	f->mPIA.WriteByte(2, 4); f->mPIA.WriteByte(3, 4);
	a->SetDirInput(14); b->SetDirInput(13);
	AT_PORTABLE_TEST_ASSERT(context, f->mPIA.ReadByte(0) == 0xDE);
	overlapping->SetDirInput(11);
	AT_PORTABLE_TEST_ASSERT(context, f->mPIA.ReadByte(0) == 0xDA);
	a->SetEnabled(false);
	AT_PORTABLE_TEST_ASSERT(context, f->mPIA.ReadByte(0) == 0xDB);
	overlapping = nullptr;
	AT_PORTABLE_TEST_ASSERT(context, f->mPIA.ReadByte(0) == 0xDF);
	a->SetEnabled(true); a->SetDirInputBits(15, 1);
	AT_PORTABLE_TEST_ASSERT(context, f->mPIA.ReadByte(0) == 0xDF);
	c->SetDirInput(0); d->SetDirInput(7);
	AT_PORTABLE_TEST_ASSERT(context, f->mPIA.ReadByte(1) == 0xFF);
	f->mPorts.SetPorts34Enabled(true);
	AT_PORTABLE_TEST_ASSERT(context, f->mPIA.ReadByte(1) == 0x70);
	d->SetEnabled(false);
	AT_PORTABLE_TEST_ASSERT(context, f->mPIA.ReadByte(1) == 0xF0);
	f->mPorts.SetPorts34Enabled(false);
	AT_PORTABLE_TEST_ASSERT(context, f->mPIA.ReadByte(1) == 0xFF);
	a->SetOnDirOutputChanged(1, [&] { ++changes; }, true);
	AT_PORTABLE_TEST_ASSERT(context, changes == 1);
	f->mPIA.WriteByte(2, 0); f->mPIA.WriteByte(0, 0x0F);
	AT_PORTABLE_TEST_ASSERT(context, changes == 2 && a->GetCurrentDirOutput() == 0);
	f->mPIA.WriteByte(2, 4); f->mPIA.WriteByte(0, 2);
	AT_PORTABLE_TEST_ASSERT(context, changes == 2);
	f->mPIA.WriteByte(0, 3);
	AT_PORTABLE_TEST_ASSERT(context, changes == 3 && a->GetCurrentDirOutput() == 3);
	a->SetDirOutputMask(0); f->mPIA.WriteByte(0, 0);
	AT_PORTABLE_TEST_ASSERT(context, changes == 3);
	const int raw = f->mPorts.AllocInput();
	f->mPorts.SetInput(raw, 0xFFEF);
	AT_PORTABLE_TEST_ASSERT(context, !(f->mPIA.ReadByte(0) & 0x10));
	f->mPorts.FreeInput(raw); f->mPorts.FreeInput(-1);
	AT_PORTABLE_TEST_ASSERT(context, f->mPorts.GetOutputState() == f->mPIA.GetOutputState());
	const int rawOutput = f->mPorts.AllocOutput([](void *p, uint32 state) {
		auto& out = *static_cast<RawOutput *>(p); ++out.mCalls; out.mState = state;
	}, &output, 0xF0);
	struct OutputScope { ATPortManager& m; int id; ~OutputScope() { m.FreeOutput(id); } } rawScope { f->mPorts, rawOutput };
	f->mPIA.WriteByte(2, 0); f->mPIA.WriteByte(0, 0xF0);
	AT_PORTABLE_TEST_ASSERT(context, output.mCalls == 1 && (output.mState & 0xF0) == 0);
	f->mPorts.ModifyOutputMask(rawOutput, 0x80);
	f->mPIA.WriteByte(2, 4); f->mPIA.WriteByte(0, 0x40);
	AT_PORTABLE_TEST_ASSERT(context, output.mCalls == 1);
	f->mPIA.WriteByte(0, 0xC0);
	AT_PORTABLE_TEST_ASSERT(context, output.mCalls == 2 && (output.mState & 0x80));
	f->mPorts.FreeOutput(-1);
	return true;
}

bool ATTestAltirraPortTriggers(ATPortableTestContext& context) {
	auto f = std::make_unique<PortManagerPortableFixture>();
	auto a = f->Port(0), b = f->Port(0), c = f->Port(2);
	a->SetTriggerDown(true); b->SetTriggerDown(true);
	AT_PORTABLE_TEST_ASSERT(context, f->Trigger(0) == 0);
	a->SetTriggerDown(false);
	AT_PORTABLE_TEST_ASSERT(context, f->Trigger(0) == 0);
	b->SetTriggerDown(false);
	AT_PORTABLE_TEST_ASSERT(context, f->Trigger(0) == 1);
	f->mPorts.SetTriggerOverride(0, true);
	a->SetTriggerDown(true); a->SetTriggerDown(false);
	AT_PORTABLE_TEST_ASSERT(context, f->Trigger(0) == 0);
	f->mPorts.SetTriggerOverride(0, false);
	AT_PORTABLE_TEST_ASSERT(context, f->Trigger(0) == 1);
	c->SetTriggerDown(true);
	AT_PORTABLE_TEST_ASSERT(context, f->Trigger(2) == 1);
	f->mPorts.SetPorts34Enabled(true);
	AT_PORTABLE_TEST_ASSERT(context, f->Trigger(2) == 0);
	f->mPorts.SetPorts34Enabled(false);
	AT_PORTABLE_TEST_ASSERT(context, f->Trigger(2) == 1);
	f->mPorts.SetTriggerOverride(2, true);
	c->SetEnabled(false);
	AT_PORTABLE_TEST_ASSERT(context, f->Trigger(2) == 0);
	f->mPorts.SetTriggerOverride(2, false);
	f->mPorts.ReapplyTriggers();
	AT_PORTABLE_TEST_ASSERT(context, f->Trigger(2) == 1);
	AT_PORTABLE_TEST_ASSERT(context, f->mVideo.RunToBeam(60, 20));
	f->mLightPen.SetPosition(false, 100, 200);
	f->mPorts.SetTriggerOverride(0, true);
	AT_PORTABLE_TEST_ASSERT(context, f->mVideo.mAntic.ReadByte(0x0C) == 52 && f->mVideo.mAntic.ReadByte(0x0D) == 30);
	return true;
}

bool ATTestAltirraPortPots(ATPortableTestContext& context) {
	auto f = std::make_unique<PortManagerPortableFixture>();
	auto a = f->Port(0), b = f->Port(0), c = f->Port(1);
	a->SetPotPosition(false, 40); b->SetPotPosition(false, 80); c->SetPotPosition(false, 100);
	AT_PORTABLE_TEST_ASSERT(context, f->Pot(0) == 40 && f->Pot(2) == 100);
	a->SetEnabled(false);
	AT_PORTABLE_TEST_ASSERT(context, f->Pot(0) == 80);
	a->SetEnabled(true); a->SetPotPosition(false, 120);
	AT_PORTABLE_TEST_ASSERT(context, f->Pot(0) == 80);
	b->ResetPotPosition(false);
	AT_PORTABLE_TEST_ASSERT(context, f->Pot(0) == 120);
	f->mPorts.SetPotOverride(0, 60U << 16);
	AT_PORTABLE_TEST_ASSERT(context, f->Pot(0) == 60);
	f->mPorts.SetPotOverride(0, ATPortManager::kPotGrounded);
	AT_PORTABLE_TEST_ASSERT(context, f->Pot(0) == 228);
	f->mPorts.SetPotOverride(0, ATPortManager::kPotUnconnected);
	b->SetPotHiresPosition(false, 0, true);
	AT_PORTABLE_TEST_ASSERT(context, f->Pot(0) == 228);
	b = nullptr;
	AT_PORTABLE_TEST_ASSERT(context, f->Pot(0) == 120);
	a->SetPotPosition(false, INT_MIN);
	AT_PORTABLE_TEST_ASSERT(context, f->Pot(0) == 1);
	a->SetPotPosition(false, INT_MAX);
	AT_PORTABLE_TEST_ASSERT(context, f->Pot(0) == 228);
	for (int i = 0; i < 50; ++i) {
		auto temporary = f->Port(0); temporary->SetPotPosition(false, 20);
		AT_PORTABLE_TEST_ASSERT(context, f->Pot(0) == 20);
	}
	AT_PORTABLE_TEST_ASSERT(context, f->Pot(0) == 228);
	return true;
}

bool ATTestAltirraPortTiming(ATPortableTestContext& context) {
	auto f = std::make_unique<PortManagerPortableFixture>(); auto port = f->Port(0);
	for (bool pal : {false, true}) {
		f->mVideo.mAntic.SetPALMode(pal);
		AT_PORTABLE_TEST_ASSERT(context, f->mVideo.RunToBeam(60, 20));
		const sint64 frame = static_cast<sint64>(f->mVideo.mAntic.GetScanlineCount()) * 114;
		for (const auto [x, y] : std::array<std::pair<int,int>, 9>{{ {20,60},{21,60},{19,60},{-1,60},{228,60},{-229,-1},{456,900},{INT_MIN,INT_MAX},{INT_MAX,INT_MIN} }}) {
			const sint64 current = static_cast<sint64>(f->mVideo.mAntic.GetBeamY()) * 114 + f->mVideo.mAntic.GetBeamX();
			sint64 expected = (static_cast<sint64>(y) * 114 + x - current) % frame;
			if (expected <= 0) expected += frame;
			AT_PORTABLE_TEST_ASSERT(context, port->GetCyclesToBeamPosition(x, y) == expected);
		}
	}
	return true;
}

bool ATTestAltirraPortLifecycle(ATPortableTestContext& context) {
	vdrefptr<IATDeviceControllerPort> held;
	{
		auto f = std::make_unique<PortManagerPortableFixture>();
		held = f->Port(0); held->SetDirInput(0); held->SetTriggerDown(true); held->SetPotPosition(false, 40);
		f->mPorts.SetPotNoiseEnabled(true);
		AT_PORTABLE_TEST_ASSERT(context, held->IsPotNoiseEnabled());
		auto token = std::make_shared<int>(7);
		std::weak_ptr<int> weakToken = token;
		auto cycle = f->Port(1);
		cycle->SetOnDirOutputChanged(1, [keep = cycle, token] { (void)keep; (void)token; }, false);
		token.reset(); cycle = nullptr;
		f->mPorts.Shutdown();
		AT_PORTABLE_TEST_ASSERT(context, weakToken.expired());
		f->mPIA.WriteByte(2, 4);
		AT_PORTABLE_TEST_ASSERT(context, f->mPIA.ReadByte(0) == 0xFF && f->Trigger(0) == 1);
		AT_PORTABLE_TEST_ASSERT(context, held->GetCurrentDirOutput() == 15 && !held->IsPotNoiseEnabled());
		f->mPorts.Init(f->mPIA, f->mVideo.mGTIA, f->mPOKEY, f->mVideo.mAntic, f->mLightPen);
		auto fresh = f->Port(0); fresh->SetTriggerDown(true); fresh->SetTriggerDown(false);
		AT_PORTABLE_TEST_ASSERT(context, f->Trigger(0) == 1 && f->Pot(0) == 228);
	}
	// The interface contract neutralizes ports that outlive the manager/chips.
	held->SetEnabled(false); held->SetEnabled(true); held->SetDirInput(0); held->SetTriggerDown(false);
	held->SetPotPosition(false, 10); held->ResetPotPosition(true);
	bool notified = false;
	held->SetOnDirOutputChanged(15, [&] { notified = true; }, true);
	AT_PORTABLE_TEST_ASSERT(context, !notified);
	AT_PORTABLE_TEST_ASSERT(context, held->GetCurrentDirOutput() == 15 && !held->IsPotNoiseEnabled());
	AT_PORTABLE_TEST_ASSERT(context, held->GetCyclesToBeamPosition(0, 0) == 1);
	return true;
}
