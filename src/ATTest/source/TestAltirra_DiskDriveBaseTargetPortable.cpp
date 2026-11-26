#include <cstring>
#include <cstdio>
#include "diskdrivebaseportablefixture.h"
using namespace DriveBasePortable;

bool ATTestAltirraDriveBaseTargetMemory(ATPortableTestContext& context) {
	auto f=std::make_unique<Target>();
	AT_PORTABLE_TEST_ASSERT(context, !strcmp(f->GetName(),"Device MCU") && f->GetDebugTarget(0)==f.get() && !f->GetDebugTarget(1) && f->GetDisasmMode()==kATDebugDisasmMode_65C02);
	AT_PORTABLE_TEST_ASSERT(context, f->AsInterface(IATDebugTargetHistory::kTypeID)==static_cast<IATDebugTargetHistory *>(f.get()) && !f->AsInterface(0));
	f->WriteByte(0x3000,0x37); AT_PORTABLE_TEST_ASSERT(context, f->ReadByte(0x3000)==0x37 && f->DebugReadByte(0x3000)==0x37);
	struct Registers { uint8 value=0x51; uint32 reads=0,writes=0; } regs;
	ATCoProcReadMemNode read { [](uint32,void *p) -> uint8 { auto& r=*static_cast<Registers *>(p); ++r.reads; return r.value++; }, [](uint32,void *p) -> uint8 { return static_cast<Registers *>(p)->value; }, &regs };
	ATCoProcWriteMemNode write { [](uint32,uint8 v,void *p) { auto& r=*static_cast<Registers *>(p); ++r.writes; r.value=v; }, &regs };
	f->cpu.GetReadMap()[0x40]=(uintptr)&read+1; f->cpu.GetWriteMap()[0x40]=(uintptr)&write+1;
	AT_PORTABLE_TEST_ASSERT(context, f->DebugReadByte(0x4000)==0x51 && !regs.reads && f->ReadByte(0x4000)==0x51 && regs.reads==1 && regs.value==0x52);
	f->WriteByte(0x4000,0x63); AT_PORTABLE_TEST_ASSERT(context, regs.writes==1 && f->DebugReadByte(0x4000)==0x63);
	f->ram[0xFFFF]=0x5A; std::array<uint8,4> b {1,1,1,1}; f->ReadMemory(0xFFFF,b.data(),b.size());
	AT_PORTABLE_TEST_ASSERT(context, b==(std::array<uint8,4>{0x5A,0,0,0})); f->DebugReadMemory(0x10000,b.data(),b.size());
	AT_PORTABLE_TEST_ASSERT(context, b==(std::array<uint8,4>{0,0,0,0})); f->ReadMemory(0,nullptr,0); f->WriteMemory(0,nullptr,0);
	ATCPUExecState state {}; f->GetExecState(state); state.m6502.mPC=0x2200; state.m6502.mA=0x42; state.m6502.mX=0x17; state.m6502.mS=0xFC;
	f->SetExecState(state); state={}; f->GetExecState(state);
	AT_PORTABLE_TEST_ASSERT(context, state.m6502.mPC==0x2200 && state.m6502.mA==0x42 && state.m6502.mX==0x17 && state.m6502.mS==0xFC);
	return true;
}

bool ATTestAltirraDriveBaseTargetHistory(ATPortableTestContext& context) {
	auto f=std::make_unique<Target>(); const uint8 code[] {0xEE,0,2,0x4C,0,0x20}; std::copy(std::begin(code),std::end(code),f->ram.begin()+0x2000);
	f->SetHistoryEnabled(true); AT_PORTABLE_TEST_ASSERT(context, f->GetHistoryEnabled() && f->proxy.history); f->Run(1500000);
	auto range=f->GetHistoryRange(); AT_PORTABLE_TEST_ASSERT(context, range.second>0x20000 && range.second-range.first==0x20000);
	std::array<const ATCPUHistoryEntry *,8> entries {}; const uint32 start=(range.second & ~0x1FFFFu)-2;
	AT_PORTABLE_TEST_ASSERT(context, f->ExtractHistory(entries.data(),start,entries.size())==entries.size());
	AT_PORTABLE_TEST_ASSERT(context, entries[2]<entries[1]);
	for (size_t i=0; i<entries.size(); ++i) {
		AT_PORTABLE_TEST_ASSERT(context, entries[i]->mPC==0x2000 || entries[i]->mPC==0x2003);
		AT_PORTABLE_TEST_ASSERT(context, entries[i]->mOpcode[0]==0xEE || entries[i]->mOpcode[0]==0x4C);
		if (i) AT_PORTABLE_TEST_ASSERT(context, f->ConvertRawTimestamp(entries[i]->mCycle)-f->ConvertRawTimestamp(entries[i-1]->mCycle)==2u*(entries[i]->mCycle-entries[i-1]->mCycle));
	}
	f->SetHistoryEnabled(false); range=f->GetHistoryRange();
	AT_PORTABLE_TEST_ASSERT(context, !f->GetHistoryEnabled() && !f->proxy.history && range.first==range.second && f->ExtractHistory(entries.data(),0,8)==0);
	return true;
}

bool ATTestAltirraDriveBaseTargetTiming(ATPortableTestContext& context) {
	for (auto rates : {std::pair<uint32,uint32>{1000000,500000},{1789773,1000000},{1000000,4000000}}) {
		auto f=std::make_unique<Target>(rates.first,rates.second); const uint64 master=f->master.GetTick64(), drive=f->Drive().GetTick64();
		f->ApplyDisplayCPUClockMultiplier(2);
		AT_PORTABLE_TEST_ASSERT(context, f->GetTimestampFrequency()==rates.second && f->GetDisplayCPUClock()==2.0*rates.second);
		Tick(f->master,1000); AT_PORTABLE_TEST_ASSERT(context, f->GetTimeSkew()==-1000);
		const double expected=1000.0*rates.second/rates.first;
		AT_PORTABLE_TEST_ASSERT(context, std::abs(double(uint32(f->MasterTimeToDriveTime()-uint32(drive)))-expected)<1.1);
		for (sint32 delta : {-200,200,1200}) {
			const auto converted=f->MasterTimeToDriveTime64(master+delta);
			AT_PORTABLE_TEST_ASSERT(context, std::abs(double(sint64(converted-drive))-double(delta)*rates.second/rates.first)<1.1);
		}
		Tick(f->slow,1); // periodic scheduling must synchronize the real CPU too
		// The 6502 stops before the final scheduled cycle. Account for that
		// cycle plus the fractional drive cycle, in units of the master clock.
		const sint32 skew=f->GetTimeSkew(),delta=sint32(f->master.GetTick()-f->DriveTimeToMasterTime());
		const sint32 quantum=sint32(std::ceil(2.0*rates.first/rates.second));
		const bool bounded=skew<=0 && -skew<=quantum && delta>=0 && delta<=quantum && std::abs(skew+delta)<=1;
		if (!bounded)
			printf("Clock %u/%u: skew=%d, mapped delta=%d, master=%u, drive=%u\n",rates.first,rates.second,f->GetTimeSkew(),sint32(f->master.GetTick()-f->DriveTimeToMasterTime()),f->master.GetTick(),f->Drive().GetTick());
		AT_PORTABLE_TEST_ASSERT(context, bounded);
		const double idealMaster=double(uint32(f->Drive().GetTick()-uint32(drive)))*rates.first/rates.second;
		AT_PORTABLE_TEST_ASSERT(context, std::abs(double(uint32(f->DriveTimeToMasterTime()-uint32(master)))-idealMaster)<1.1);
	}
	return true;
}

bool ATTestAltirraDriveBaseTargetSteps(ATPortableTestContext& context) {
	std::vector<bool> results;
	for (uint8 mode=0; mode<3; ++mode) {
		auto f=std::make_unique<Target>(); results.clear();
		const uint8 main[] {0x20,0,0x21,0xA9,0x77,0x4C,3,0x20}; std::copy(std::begin(main),std::end(main),f->ram.begin()+0x2000);
		f->ram[0x2100]=0xEE; f->ram[0x2101]=0; f->ram[0x2102]=2; f->ram[0x2103]=0x60;
		auto done=[&](bool ok) { results.push_back(ok); f->paused=true; };
		if (mode==0) AT_PORTABLE_TEST_ASSERT(context, f->StepInto(done));
		else if (mode==1) AT_PORTABLE_TEST_ASSERT(context, f->StepOver(done));
		else { f->Jump(0x2100); f->cpu.SetS(0xFB); f->ram[0x1FC]=2; f->ram[0x1FD]=0x20; AT_PORTABLE_TEST_ASSERT(context, f->StepOut(done)); }
		f->Run(100);
		AT_PORTABLE_TEST_ASSERT(context, results==std::vector<bool>{true} && f->cpu.GetPC()==(mode==0 ? 0x2100 : 0x2003));
		if (mode) AT_PORTABLE_TEST_ASSERT(context, f->ram[0x200]==0xEB);
	}
	struct Breakpoint final : IATCPUBreakpointHandler {
		bool CheckBreakpoint(uint32 pc) override { address=pc; ++hits; return true; }
		uint32 address=0,hits=0;
	} bp;
	auto f=std::make_unique<Target>(); results.clear();
	f->bp.SetBreakpointHandler(&bp); f->bp.SetBreakpoint(0x2001);
	AT_PORTABLE_TEST_ASSERT(context, f->StepOver([&](bool ok) { results.push_back(ok); f->paused=true; })); f->Run(100);
	AT_PORTABLE_TEST_ASSERT(context, results==std::vector<bool>{false} && bp.hits==1 && bp.address==0x2001 && f->cpu.GetPC()==0x2001);
	return true;
}

bool ATTestAltirraDriveBaseTargetLifecycle(ATPortableTestContext& context) {
	std::vector<bool> results; auto f=std::make_unique<Target>();
	uint32 syncs=f->syncs; f->ScheduleImmediateResume(); Tick(f->master,1);
	AT_PORTABLE_TEST_ASSERT(context, f->syncs==syncs+1); f->StepUpdate(); f->RunUntilSynced();
	AT_PORTABLE_TEST_ASSERT(context, f->syncs==syncs+3);
	auto done=[&](bool ok) { results.push_back(ok); };
	AT_PORTABLE_TEST_ASSERT(context, f->StepInto(done) && f->StepOver(done)); f->Break();
	AT_PORTABLE_TEST_ASSERT(context, results==(std::vector<bool>{false,false}));
	AT_PORTABLE_TEST_ASSERT(context, f->StepOut(done)); f->ResetTargetControl();
	AT_PORTABLE_TEST_ASSERT(context, results.size()==3 && !results.back());
	f->SetHistoryEnabled(true); AT_PORTABLE_TEST_ASSERT(context, f->StepInto(done)); f->ShutdownTargetControl();
	syncs=f->syncs; Tick(f->master,100); Tick(f->slow,100);
	AT_PORTABLE_TEST_ASSERT(context, results.size()==4 && !f->proxy.history && !f->GetHistoryEnabled() && f->syncs==syncs && !f->StepInto(done));
	return true;
}
