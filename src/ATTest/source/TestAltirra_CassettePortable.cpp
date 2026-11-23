// Tests for the original cassette implementation through portable chip/image services.
#include <memory>
#include <at/attest/portabletest.h>
#include "cassetteportablefixture.h"

namespace {
	vdrefptr<IATCassetteImage> BlankTape(uint32 samples = 1000) {
		vdrefptr<IATCassetteImage> image; ATCreateNewCassetteImage(~image);
		ATCassetteWriteCursor cursor {}; image->WriteBlankData(cursor, samples, false); return image;
	}
}

bool ATTestAltirraCassetteTransport(ATPortableTestContext& context) {
	auto f = std::make_unique<CassettePortableFixture>();
	auto& tape = *f->mTape;
	AT_PORTABLE_TEST_ASSERT(context, tape.IsPlayEnabled() && !tape.IsLoaded() && !tape.IsMotorRunning());
	auto image = BlankTape(); tape.Load(image, L"memory.cas", true); f->Motor(true);
	AT_PORTABLE_TEST_ASSERT(context, tape.IsMotorRunning() && tape.IsImagePersistent());
	f->Tick(5600);
	AT_PORTABLE_TEST_ASSERT(context, tape.GetSamplePos() > 95 && tape.GetSamplePos() < 106);
	tape.SetPaused(true); const auto pos = tape.GetSamplePos(); f->Tick(560);
	AT_PORTABLE_TEST_ASSERT(context, tape.GetSamplePos() == pos && !tape.IsMotorRunning());
	tape.SetPaused(false); f->Tick(560);
	AT_PORTABLE_TEST_ASSERT(context, tape.GetSamplePos() > pos);
	tape.SeekToBitPos(9000);
	AT_PORTABLE_TEST_ASSERT(context, tape.GetSamplePos() == 1000);
	tape.SeekToTime(-1); AT_PORTABLE_TEST_ASSERT(context, tape.GetSamplePos() == 0);
	tape.SeekToBitPos(10); tape.SkipForward(-1);
	AT_PORTABLE_TEST_ASSERT(context, tape.GetSamplePos() == 0);
	image = nullptr; // Only the cassette now owns the image; loading it again must retain it first.
	auto *retainedImage = tape.GetImage(); tape.Load(retainedImage, nullptr, false);
	AT_PORTABLE_TEST_ASSERT(context, tape.GetImage() == retainedImage && tape.GetSampleLen() == 1000);
	tape.Stop(); f->Motor(false);
	AT_PORTABLE_TEST_ASSERT(context, tape.IsStopped() && !tape.IsMotorRunning());
	tape.Unload(); AT_PORTABLE_TEST_ASSERT(context, !tape.IsLoaded() && tape.GetSampleLen() == 0);
	return true;
}

bool ATTestAltirraCassetteLifecycle(ATPortableTestContext& context) {
	auto f = std::make_unique<CassettePortableFixture>();
	auto image = BlankTape(); f->mTape->Load(image, nullptr, false); f->Motor(true);
	f->mTape->SeekToBitPos(4); // queued position notification and a scheduled play callback
	f->mChips.mPOKEY.SetCassette(nullptr); f->mTape.reset();
	f->mDeferred.FlushPendingEvents(); f->Tick(1000);
	AT_PORTABLE_TEST_ASSERT(context, !f->mMixer.mpSource && f->mSIO.mDevices.empty());
	f->Attach(); f->mTape->LoadNew(); f->Motor(true);
	f->mTape->Shutdown(); f->mTape->Shutdown();
	f->mDeferred.FlushPendingEvents(); f->Tick(114000);
	AT_PORTABLE_TEST_ASSERT(context, f->mMixer.mAdds == f->mMixer.mRemoves && f->mSIO.mDevices.empty());
	// Shutdown removes queued events without losing notifications that can be
	// reattached to a manager during reinitialization.
	ATDeferredEvent event; uint32 calls = 0;
	vdfunction<void()> callback = [&] { ++calls; };
	event.Init(&f->mDeferred); event += &callback; event.NotifyDeferred(); event.Shutdown();
	f->mDeferred.FlushPendingEvents(); AT_PORTABLE_TEST_ASSERT(context, calls == 0);
	event.Init(&f->mDeferred); f->mDeferred.FlushPendingEvents();
	AT_PORTABLE_TEST_ASSERT(context, calls == 1);
	event -= &callback; event.Shutdown();
	return true;
}
