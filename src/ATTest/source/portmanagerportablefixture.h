// Production input chips on the shared ANTIC/GTIA scheduler.
#ifndef f_ATTEST_PORTMANAGERPORTABLEFIXTURE_H
#define f_ATTEST_PORTMANAGERPORTABLEFIXTURE_H
#include <at/ataudio/pokey.h>
#include <at/ataudio/pokeytables.h>
#include <lightpen.h>
#include <pia.h>
#include <portmanager.h>
#include "anticportablefixture.h"

class PortManagerPortableFixture final : public IATPokeyEmulatorConnections {
public:
	PortManagerPortableFixture() {
		mPIA.Init(&mVideo.mScheduler); mPIA.ColdReset();
		mPOKEY.Init(this, &mVideo.mScheduler, nullptr, &mTables); mPOKEY.ColdReset();
		mPOKEY.SetImmediatePotUpdateEnabled(false);
		mPOKEY.WriteByte(0x0F, 3);
		mLightPen.Init(mVideo.mAntic, mVideo.mGTIA);
		mPorts.Init(mPIA, mVideo.mGTIA, mPOKEY, mVideo.mAntic, mLightPen);
	}
	~PortManagerPortableFixture() { mPorts.Shutdown(); mPOKEY.ColdReset(); }
	vdrefptr<IATDeviceControllerPort> Port(int index) {
		vdrefptr<IATDeviceControllerPort> p; mPorts.AllocControllerPort(index, ~p); return p;
	}
	uint8 Trigger(int index) { return mVideo.mGTIA.ReadByte(0x10 + index); }
	uint8 Pot(int index) {
		mPOKEY.WriteByte(0x0B, 0);
		mVideo.Tick(230 * 114);
		return mPOKEY.ReadByte(index);
	}
	void PokeyAssertIRQ(bool) override { ++mIRQAsserts; }
	void PokeyNegateIRQ(bool) override { ++mIRQNegates; }
	void PokeyBreak() override {}
	bool PokeyIsInInterrupt() const override { return false; }
	bool PokeyIsKeyPushOK(uint8, bool) const override { return true; }
	uint32 mIRQAsserts = 0, mIRQNegates = 0;
	ANTICPortableFixture mVideo;
	ATPIAEmulator mPIA;
	ATPokeyTables mTables;
	ATPokeyEmulator mPOKEY { false };
	ATLightPenPort mLightPen;
	ATPortManager mPorts;
};
#endif
