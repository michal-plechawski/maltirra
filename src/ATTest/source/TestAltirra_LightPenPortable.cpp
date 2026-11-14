// Portable standalone tests for the real ANTIC-backed light pen port.

#include <at/atcore/enumparse.h>
#include <memory>
#include <string>
#include <vd2/system/function.h>
#include <at/attest/portabletest.h>
#include <vd2/system/VDString.h>
#include "portmanagerportablefixture.h"

bool ATTestAltirraLightPen(ATPortableTestContext& context) {
	auto fixture=std::make_unique<PortManagerPortableFixture>();
	auto& pen=fixture->mLightPen;
	auto& antic=fixture->mVideo.mAntic;
	AT_PORTABLE_TEST_ASSERT(context, &pen.GetGTIA()==&fixture->mVideo.mGTIA);

	// Immediate mode latches the requested coordinates as soon as they are set.
	pen.SetImmediateUpdateEnabled(true);
	AT_PORTABLE_TEST_ASSERT(context, pen.GetImmediateUpdateEnabled());
	pen.SetPosition(true,45,20);
	AT_PORTABLE_TEST_ASSERT(context, antic.ReadByte(0x0C)==45 && antic.ReadByte(0x0D)==10);
	pen.SetPosition(false,228,(int)antic.GetScanlineCount()-1);
	AT_PORTABLE_TEST_ASSERT(context, antic.ReadByte(0x0C)==0 && antic.ReadByte(0x0D)==0);
	pen.SetPosition(true,-1,-1);
	AT_PORTABLE_TEST_ASSERT(context, antic.ReadByte(0x0C)==227);
	AT_PORTABLE_TEST_ASSERT(context, antic.ReadByte(0x0D)==((antic.GetScanlineCount()-2)>>1));

	// Deferred mode uses beam timing outside the +/-2 scanline window and keeps odd phase.
	pen.SetImmediateUpdateEnabled(false);
	AT_PORTABLE_TEST_ASSERT(context, !pen.GetImmediateUpdateEnabled());
	pen.SetPosition(true,99,80);
	AT_PORTABLE_TEST_ASSERT(context, fixture->mVideo.RunToBeam(30,20));
	pen.SetPortTriggerState(0,true);
	AT_PORTABLE_TEST_ASSERT(context, antic.ReadByte(0x0C)==53 && antic.ReadByte(0x0D)==15);
	pen.SetPortTriggerState(0,false);

	// A trigger close to the stored pen row restores the exact stored coordinate.
	pen.SetPosition(false,201,51);
	AT_PORTABLE_TEST_ASSERT(context, fixture->mVideo.RunToBeam(50,10));
	pen.SetPortTriggerState(1,true);
	AT_PORTABLE_TEST_ASSERT(context, antic.ReadByte(0x0C)==201 && antic.ReadByte(0x0D)==25);
	pen.SetPortTriggerState(1,false);

	// Ports 3/4 are ignored by default and begin latching when explicitly enabled.
	pen.SetIgnorePort34(true);
	pen.SetPosition(true,40,100);
	AT_PORTABLE_TEST_ASSERT(context, fixture->mVideo.RunToBeam(60,12));
	pen.SetPortTriggerState(2,true);
	AT_PORTABLE_TEST_ASSERT(context, antic.ReadByte(0x0C)==201 && antic.ReadByte(0x0D)==25);
	pen.SetIgnorePort34(false);
	pen.SetPortTriggerState(2,false);
	pen.SetPortTriggerState(2,true);
	AT_PORTABLE_TEST_ASSERT(context, antic.ReadByte(0x0C)==36 && antic.ReadByte(0x0D)==30);
	pen.SetPortTriggerState(2,false);

	// Small calibration adjustments update the selected pen's X/Y offsets.
	pen.SetPosition(false,20,30);
	pen.SetAdjust(false,{2,-3});
	pen.ApplyCorrection(4,5);
	const vdint2 adjust=pen.GetAdjust(false);
	AT_PORTABLE_TEST_ASSERT(context, adjust.x==6 && adjust.y==2);
	pen.SetAdjust(true,{-4,7});
	pen.SetPosition(true,20,30);
	pen.ApplyCorrection(-2,3);
	const vdint2 penAdjust=pen.GetAdjust(true);
	AT_PORTABLE_TEST_ASSERT(context, penAdjust.x==-6 && penAdjust.y==10);

	// Enum text is stable and the correction notification supports safe self-removal.
	pen.SetNoiseMode(ATLightPenNoiseMode::Low);
	AT_PORTABLE_TEST_ASSERT(context, pen.GetNoiseMode()==ATLightPenNoiseMode::Low);
	const auto parsedNoise=ATParseEnum<ATLightPenNoiseMode>(VDStringSpanA("high"));
	AT_PORTABLE_TEST_ASSERT(context, parsedNoise.mValid && parsedNoise.mValue==ATLightPenNoiseMode::High);
	AT_PORTABLE_TEST_ASSERT(context, std::string(ATEnumToString(ATLightPenNoiseMode::None))=="none");
	int correctionCount=0;
	bool correctionPhase=false;
	int correctionX=0,correctionY=0;
	vdfunction<void(bool,int,int)> callback=[&](bool phase,int x,int y) {
		++correctionCount; correctionPhase=phase; correctionX=x; correctionY=y;
		pen.OnTriggerCorrectionEvent().Remove(&callback);
	};
	pen.OnTriggerCorrectionEvent().Add(&callback);
	pen.TriggerCorrection(true,17,-9);
	AT_PORTABLE_TEST_ASSERT(context, correctionCount==1 && correctionPhase && correctionX==17 && correctionY==-9);
	AT_PORTABLE_TEST_ASSERT(context, pen.OnTriggerCorrectionEvent().IsEmpty());
	return true;
}
