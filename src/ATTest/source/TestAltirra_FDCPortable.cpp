#include "fdcportablefixture.h"
using namespace FDCPortable;

bool ATTestAltirraFDCRegisters(ATPortableTestContext& context) {
	auto f=std::make_unique<Machine>(); auto& d=f->chip;
	AT_PORTABLE_TEST_ASSERT(context, !d.GetIrqStatus() && !d.GetDrqStatus() && !(d.DebugReadByte(0)&0x81) && (d.DebugReadByte(0)&4));
	d.WriteByte(5,12); d.WriteByte(6,7); d.WriteByte(7,0x63);
	AT_PORTABLE_TEST_ASSERT(context, d.ReadByte(1)==12 && d.DebugReadByte(2)==7 && d.DebugReadByte(3)==0x63);
	d.WriteByte(0,0xD8); AT_PORTABLE_TEST_ASSERT(context, f->WaitIRQ() && d.GetIrqStatus()); d.DebugReadByte(0);
	AT_PORTABLE_TEST_ASSERT(context, d.GetIrqStatus()); d.ReadByte(0); AT_PORTABLE_TEST_ASSERT(context, !d.GetIrqStatus());
	d.WriteByte(0,0xD0); f->Tick(30); AT_PORTABLE_TEST_ASSERT(context, !d.GetIrqStatus() && !(d.DebugReadByte(0)&1));
	Text output; d.DumpStatus(output); AT_PORTABLE_TEST_ASSERT(context, output.value.find("Track register:   $0C")!=std::string::npos);
	return true;
}

bool ATTestAltirraFDCTypes(ATPortableTestContext& context) {
	for (auto type : {ATFDCEmulator::kType_1771,ATFDCEmulator::kType_2793,ATFDCEmulator::kType_2797,ATFDCEmulator::kType_1770,ATFDCEmulator::kType_1772}) {
		auto f=std::make_unique<Machine>(128,type); f->chip.WriteByte(3,2); f->chip.WriteByte(0,0x18);
		AT_PORTABLE_TEST_ASSERT(context, f->WaitIRQ() && f->track==2 && f->chip.DebugReadByte(1)==2 && f->steps.size()==2 && !(f->chip.DebugReadByte(0)&0x11));
		const uint32 expected=type==ATFDCEmulator::kType_1771 ? 12000 : type==ATFDCEmulator::kType_1772 ? 2000 : 6000;
		AT_PORTABLE_TEST_ASSERT(context, f->steps[1].first-f->steps[0].first==expected);
		f->chip.WriteByte(0,0x08); AT_PORTABLE_TEST_ASSERT(context, f->WaitIRQ() && f->track==0 && f->chip.DebugReadByte(1)==0);
	}
	return true;
}

bool ATTestAltirraFDCTiming(ATPortableTestContext& context) {
	for (bool mfm : {false,true}) for(bool doubled : {false,true}) {
		auto f=std::make_unique<Machine>(mfm ? 256 : 128); f->chip.SetDoubleClock(doubled); f->Fill(3,Pattern(mfm ? 256 : 128)); f->Select(4); Bytes b;
		AT_PORTABLE_TEST_ASSERT(context, !(f->Read(0x80,b)&0x1C) && b.size()==(mfm ? 256 : 128));
		const uint32 period=(mfm ? 32 : 64)/(doubled ? 2 : 1);
		AT_PORTABLE_TEST_ASSERT(context, f->drqTicks.size()==b.size() && f->drqTicks[1]-f->drqTicks[0]==period);
		f->chip.WriteByte(3,2); f->chip.WriteByte(0,0x18); AT_PORTABLE_TEST_ASSERT(context, f->WaitIRQ());
		AT_PORTABLE_TEST_ASSERT(context, f->steps[1].first-f->steps[0].first==(doubled ? 3000 : 6000));
	}
	auto f=std::make_unique<Machine>(); f->Select(4); Bytes b; f->Fill(3,Pattern(128)); f->Read(0x80,b);
	f->chip.SetAccurateTimingEnabled(true); const uint32 start=f->sch.GetTick();
	AT_PORTABLE_TEST_ASSERT(context, !(f->Read(0x80,b)&0x1C) && uint32(f->drqTicks.back()-start)>150000);
	return true;
}

bool ATTestAltirraFDCSensors(ATPortableTestContext& context) {
	auto f=std::make_unique<Machine>(); auto& d=f->chip;
	d.SetDiskImageReady(false); AT_PORTABLE_TEST_ASSERT(context, d.DebugReadByte(0)&0x80);
	d.SetDiskImageReady(std::nullopt); AT_PORTABLE_TEST_ASSERT(context, !(d.DebugReadByte(0)&0x80));
	d.SetDiskImage(nullptr); AT_PORTABLE_TEST_ASSERT(context, d.DebugReadByte(0)&0x80); d.SetDiskImage(&f->Image());
	d.SetDiskChangeStartupHackEnabled(true); f->Tick(27000); AT_PORTABLE_TEST_ASSERT(context, d.DebugReadByte(0)&0x80);
	f->Tick(25000); AT_PORTABLE_TEST_ASSERT(context, !(d.DebugReadByte(0)&0x80));
	d.SetDiskInterface(nullptr); AT_PORTABLE_TEST_ASSERT(context, !(d.DebugReadByte(0)&0x40)); d.SetDiskInterface(&f->slot.mDisk);
	d.SetCurrentTrack(1,false); AT_PORTABLE_TEST_ASSERT(context, !(d.DebugReadByte(0)&4));
	return true;
}

bool ATTestAltirraFDCIndexMotor(ATPortableTestContext& context) {
	auto f=std::make_unique<Machine>(); f->Tick(450000); std::vector<uint32> rising;
	for(auto p:f->index) if(p.second) rising.push_back(p.first);
	AT_PORTABLE_TEST_ASSERT(context, rising.size()>=2 && rising[1]-rising[0]==200000);
	f->chip.SetAutoIndexPulse(false); f->chip.WriteByte(0,0x08); AT_PORTABLE_TEST_ASSERT(context, f->WaitIRQ() && !f->head.empty() && f->head.back());
	for(uint32 i=0;i<15;++i) { f->chip.OnIndexPulse(false); f->chip.OnIndexPulse(true); }
	AT_PORTABLE_TEST_ASSERT(context, !f->head.back());
	auto motor=std::make_unique<Machine>(128,ATFDCEmulator::kType_1770); motor->chip.WriteByte(0,0x58);
	AT_PORTABLE_TEST_ASSERT(context, motor->WaitIRQ() && motor->motor.back() && (motor->chip.DebugReadByte(0)&0x80)); motor->Tick(1800001);
	AT_PORTABLE_TEST_ASSERT(context, !motor->motor.back() && !(motor->chip.DebugReadByte(0)&0x80));
	return true;
}

bool ATTestAltirraFDCForceInterrupt(ATPortableTestContext& context) {
	auto f=std::make_unique<Machine>(); f->Fill(3,Pattern(128)); f->Select(4); f->chip.WriteByte(0,0x80);
	AT_PORTABLE_TEST_ASSERT(context, f->WaitDRQ()); f->chip.ReadByte(3); f->chip.WriteByte(0,0xD8);
	AT_PORTABLE_TEST_ASSERT(context, f->chip.GetIrqStatus() && !(f->chip.DebugReadByte(0)&1) && (f->chip.DebugReadByte(0)&4));
	f->chip.Reset(); f->chip.WriteByte(0,0xD0); f->Tick(30); AT_PORTABLE_TEST_ASSERT(context, !f->chip.GetIrqStatus());
	return true;
}

bool ATTestAltirraFDCReset(ATPortableTestContext& context) {
	auto f=std::make_unique<Machine>(); f->Select(4); f->chip.WriteByte(0,0xA0); AT_PORTABLE_TEST_ASSERT(context, f->WaitDRQ());
	f->chip.Reset(); AT_PORTABLE_TEST_ASSERT(context, !f->chip.GetDrqStatus() && !f->chip.GetIrqStatus() && f->chip.DebugReadByte(1)==0 && f->chip.DebugReadByte(2)==0);
	const auto count=f->drq.size(); f->Tick(100000); AT_PORTABLE_TEST_ASSERT(context, f->drq.size()==count);
	f->Select(4); f->chip.WriteByte(0,0x80); f->chip.Shutdown(); f->Tick(200000); AT_PORTABLE_TEST_ASSERT(context, f->drq.size()==count);
	return true;
}

bool ATTestAltirraFDCTrace(ATPortableTestContext& context) {
	ATTraceContext trace {}; trace.mpCollection=new ATTraceCollection; auto f=std::make_unique<Machine>();
	f->chip.SetTraceContext(nullptr,0,0); f->chip.SetTraceContext(&trace,f->sch.GetTick64(),0.000001);
	f->Fill(3,Pattern(128)); f->Select(4); Bytes bytes; AT_PORTABLE_TEST_ASSERT(context, !(f->Read(0x80,bytes)&0x1C));
	f->chip.SetTraceContext(nullptr,0,0); f->chip.SetTraceContext(nullptr,0,0);
	AT_PORTABLE_TEST_ASSERT(context, trace.mpCollection->GetGroupCount()==1 && trace.mpCollection->GetGroup(0)->GetChannelCount()==2 && trace.mpCollection->GetGroup(0)->GetChannel(0)->GetEventCount()>0);
	return true;
}
