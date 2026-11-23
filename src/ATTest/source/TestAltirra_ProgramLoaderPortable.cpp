// Real CPU/ROM hooks, segment execution, host services, and production SIO.
#include <memory>
#include <string>
#include <tuple>
#include <vd2/system/error.h>
#include <at/attest/portabletest.h>
#include <at/atcore/ksyms.h>
#include <at/atcore/memoryutils.h>
#include <at/atcore/randomization.h>
#include <at/atio/blobimage.h>
#include <cpuheatmap.h>
#include <hleprogramloader.h>
#include <simeventmanager.h>
#include "mmuportablefixture.h"
#include "siomanagerportablefixture.h"

namespace {
	using Bytes = std::vector<uint8>;
	void Word(Bytes& b, uint16 v) { b.push_back(uint8(v)); b.push_back(uint8(v>>8)); }
	void Segment(Bytes& b, uint16 a, std::initializer_list<uint8> data) {
		Word(b,a); Word(b,uint16(a+data.size()-1)); b.insert(b.end(),data);
	}
	vdrefptr<IATBlobImage> Image(const Bytes& b) {
		vdrefptr<IATBlobImage> image; ATCreateBlobImage(kATImageType_Program,b.data(),uint32(b.size()),~image); return image;
	}
	struct Machine : IATSimulatorCallback {
		Machine() {
			h.mHooks.EnableOSHooks(true); h.mMMU.SetBankRegister(0xFF);
			h.mUpperKernel[0xC0+0xC00] = 0x60; // E4C0: known RTS
			events.AddCallback(this);
			connections.mpGetTick = [&] { return h.mScheduler.GetTick64(); };
			connections.mpIsKernelROMLocation = [&](uint16) { return h.mMMU.IsKernelROMEnabled(); };
			connections.mpRandomizeRawMemory = [&](uint16 a, uint32 n, uint32 seed) {
				randomized.emplace_back(a,n,seed); heat.ResetMemoryRange(a,n); return ATRandomizeMemory(h.mRAM.data()+a,n,seed);
			};
			connections.mpPresetMemoryRange = [&](uint32 a, uint32 n) { heat.PresetMemoryRange(a,n); };
			Init();
		}
		void Init() { loader.Init(&h.mCPU,&events,connections,nullptr); }
		void Word(uint16 a, uint16 v) { h.mMemory.WriteByte(a,uint8(v)); h.mMemory.WriteByte(uint16(a+1),uint8(v>>8)); }
		uint16 Word(uint16 a) { return h.mMemory.ReadByte(a)+256*h.mMemory.ReadByte(uint16(a+1)); }
		uint8 Start(const Bytes& b, bool delay = false) { loader.LoadProgram(nullptr,Image(b),kATHLEProgramLoadMode_Default,delay); return h.mHooks.OnHookHit(ATKernelSymbols::DSKINV); }
		void OnSimulatorEvent(ATSimulatorEvent e) override { seen.push_back(e); }
		MMUPortableFixture h;
		ATSimulatorEventManager events;
		ATCPUHeatMap heat;
		ATHLEProgramLoaderConnections connections;
		std::vector<ATSimulatorEvent> seen;
		std::vector<std::tuple<uint16,uint32,uint32>> randomized;
		ATHLEProgramLoader loader;
	};
	struct Seeds { ATRandomizationSeeds saved = g_ATRandomizationSeeds; ~Seeds() { g_ATRandomizationSeeds = saved; } };
}

bool ATTestAltirraProgramLoaderSegments(ATPortableTestContext& context) {
	auto f = std::make_unique<Machine>(); auto& h = f->h;
	Bytes b {0xFF,0xFF,0xFF,0xFF}; Segment(b,0x3000,{0xA9,0x31,0x60}); Segment(b,0x3010,{0x11,0x22}); Segment(b,ATKernelSymbols::RUNAD,{0x10,0x30});
	f->heat.ResetMemoryRange(0x3000,32);
	AT_PORTABLE_TEST_ASSERT(context, f->Start(b) == 0x4C && h.mCPU.GetPC() == 0x3010);
	AT_PORTABLE_TEST_ASSERT(context, f->Word(0x3010) == 0x2211 && h.mMemory.ReadByte(0x3002) == 0x60 && f->heat.GetMemoryValidity(0x3001) == 0xFF && f->heat.GetMemoryValidity(0x3003) == 0);
	AT_PORTABLE_TEST_ASSERT(context, h.mCPU.GetX() == 0x20 && h.mCPU.GetY() == 3 && (h.mCPU.GetP() & 0xC3) == 3 && h.mMemory.ReadByte(ATKernelSymbols::STATUS) == 1);
	AT_PORTABLE_TEST_ASSERT(context, f->Word(ATKernelSymbols::DOSINI) == 0xE4C0 && h.mMemory.ReadByte(ATKernelSymbols::COLDST) == 0 && (h.mMemory.ReadByte(ATKernelSymbols::BOOT_) & 1));
	AT_PORTABLE_TEST_ASSERT(context, !f->loader.GetCurrentImage() && !f->loader.IsLaunchPending() && f->seen == std::vector<ATSimulatorEvent>({kATSimEvent_EXELoad,kATSimEvent_EXERunSegment}));
	b.clear(); Segment(b,0x3456,{0x60});
	AT_PORTABLE_TEST_ASSERT(context, f->Start(b) == 0x4C && h.mCPU.GetPC() == 0x3456);
	return true;
}

bool ATTestAltirraProgramLoaderCPU(ATPortableTestContext& context) {
	auto f = std::make_unique<Machine>(); auto& h = f->h;
	Bytes b {0xFF,0xFF}; Segment(b,0x3000,{0xA9,0x17,0x8D,0,0x40,0x60}); Segment(b,ATKernelSymbols::INITAD,{0,0x30});
	Segment(b,0x3020,{0xEE,0,0x40,0x60}); Segment(b,ATKernelSymbols::INITAD,{0x20,0x30});
	Segment(b,0x3100,{0xA9,0x42,0x8D,1,0x40,0x4C,5,0x31}); Segment(b,ATKernelSymbols::RUNAD,{0,0x31});
	f->loader.LoadProgram(nullptr,Image(b),kATHLEProgramLoadMode_Default,false);
	// Enter via an actual JSR DSKINV, execute both INITs, and then the RUN code.
	h.mMemory.WriteByte(0x2000,0x20); f->Word(0x2001,ATKernelSymbols::DSKINV);
	h.mMemory.WriteByte(0x2003,0x4C); f->Word(0x2004,0x2003);
	h.mCPU.SetS(0xFD); h.mCPU.SetPC(0x2000);
	AT_PORTABLE_TEST_ASSERT(context, h.RunCycles(1000));
	AT_PORTABLE_TEST_ASSERT(context, h.mMemory.ReadByte(0x4000) == 0x18 && h.mMemory.ReadByte(0x4001) == 0x42 && h.mCPU.GetPC() >= 0x3105 && h.mCPU.GetPC() <= 0x3108);
	AT_PORTABLE_TEST_ASSERT(context, f->seen == std::vector<ATSimulatorEvent>({kATSimEvent_EXELoad,kATSimEvent_EXEInitSegment,kATSimEvent_EXEInitSegment,kATSimEvent_EXERunSegment}));
	AT_PORTABLE_TEST_ASSERT(context, !f->loader.GetCurrentImage() && h.mCPU.GetS() == 0xF9);
	return true;
}

bool ATTestAltirraProgramLoaderValidation(ATPortableTestContext& context) {
	auto f = std::make_unique<Machine>(); Bytes valid; Segment(valid,0x3300,{0x60}); auto original = Image(valid);
	f->loader.LoadProgram(nullptr,original,kATHLEProgramLoadMode_Default,false);
	std::vector<Bytes> bad {{},{0xFE,0xFF,0,0},{0xFA,0xFF,0,0},Bytes(29),Bytes(128),Bytes(128)};
	for (size_t i=3; i<bad.size(); ++i) { bad[i][0]='M'; bad[i][1]='Z'; if (i>3) { bad[i][60]=64; bad[i][64]= i==4 ? 'P' : 'N'; bad[i][65]='E'; } }
	for (const auto& bytes : bad) {
		bool threw = false; try { f->loader.LoadProgram(nullptr,Image(bytes),kATHLEProgramLoadMode_Default,false); } catch (const MyError&) { threw = true; }
		AT_PORTABLE_TEST_ASSERT(context, threw && f->loader.GetCurrentImage() == original && f->loader.IsLaunchPending());
	}
	AT_PORTABLE_TEST_ASSERT(context, f->h.mHooks.OnHookHit(ATKernelSymbols::DSKINV) == 0x4C && f->h.mCPU.GetPC() == 0x3300);
	// Truncated segments retain the original loader's partial-load compatibility.
	Bytes truncated {0,0x30,3,0x30,0x71,0x72};
	AT_PORTABLE_TEST_ASSERT(context, f->Start(truncated) == 0x4C && f->Word(0x3000) == 0x7271);
	// A reversed range terminates loading without reading past the image.
	AT_PORTABLE_TEST_ASSERT(context, f->Start({0xFF,0xFF,0,0x40,0,0x30}) == 0x4C);
	return true;
}

bool ATTestAltirraProgramLoaderLifecycle(ATPortableTestContext& context) {
	auto f = std::make_unique<Machine>(); auto& h = f->h; Bytes b; Segment(b,0x3000,{0x60}); Segment(b,ATKernelSymbols::INITAD,{0,0x30});
	auto image = Image(b); auto *raw = image.get(); f->loader.LoadProgram(nullptr,image,kATHLEProgramLoadMode_Default,false); image = nullptr;
	f->loader.LoadProgram(nullptr,raw,kATHLEProgramLoadMode_Default,false);
	AT_PORTABLE_TEST_ASSERT(context, h.mHooks.OnHookHit(ATKernelSymbols::DSKINV) == 0x60 && f->loader.GetCurrentImage() == raw);
	Bytes next; Segment(next,0x3200,{0x60}); f->loader.LoadProgram(nullptr,Image(next),kATHLEProgramLoadMode_Default,false);
	AT_PORTABLE_TEST_ASSERT(context, h.mHooks.OnHookHit(0x1FE) == 0 && f->loader.IsLaunchPending());
	f->loader.Shutdown(); f->loader.Shutdown(); f->Init();
	AT_PORTABLE_TEST_ASSERT(context, !f->loader.GetCurrentImage() && !f->loader.IsLaunchPending() && h.mHooks.OnHookHit(ATKernelSymbols::DSKINV) == 0 && h.mHooks.OnHookHit(0x1FE) == 0);
	bool threw = false; try { f->loader.LoadProgram(nullptr,Image(next),kATHLEProgramLoadMode_DiskBoot,false); } catch (const MyError&) { threw = true; }
	AT_PORTABLE_TEST_ASSERT(context, threw && !f->loader.GetCurrentImage());
	return true;
}

bool ATTestAltirraProgramLoaderRandomization(ATPortableTestContext& context) {
	Seeds seeds; g_ATRandomizationSeeds.mProgramMemory = 0x12345678; g_ATRandomizationSeeds.mProgramLaunchDelay = 100;
	auto f = std::make_unique<Machine>(); auto& h = f->h;
	f->Word(ATKernelSymbols::MEMLO,0x4000); f->Word(ATKernelSymbols::MEMTOP,0x4100); h.mMemory.WriteByte(0x4100,0x53);
	f->loader.SetRandomizeMemoryOnLoad(true); Bytes b; Segment(b,0x3000,{0x60});
	AT_PORTABLE_TEST_ASSERT(context, f->Start(b,true) == 0x60 && f->loader.GetCurrentImage());
	AT_PORTABLE_TEST_ASSERT(context, f->randomized.size() == 2 && std::get<0>(f->randomized[0]) == 0x80 && std::get<1>(f->randomized[0]) == 0x80 && std::get<1>(f->randomized[1]) == 0x100);
	std::array<uint8,128> expectedZP {}; auto seed = ATRandomizeMemory(expectedZP.data(),expectedZP.size(),0x12345678);
	std::array<uint8,256> expectedRAM {}; ATRandomizeMemory(expectedRAM.data(),expectedRAM.size(),seed);
	for (size_t i=0; i<expectedRAM.size(); ++i) AT_PORTABLE_TEST_ASSERT(context, h.mMemory.ReadByte(0x4000+i) == expectedRAM[i]);
	for (size_t i=0; i<expectedZP.size(); ++i) AT_PORTABLE_TEST_ASSERT(context, h.mMemory.ReadByte(0x80+i) == expectedZP[i]);
	AT_PORTABLE_TEST_ASSERT(context, h.mMemory.ReadByte(0x4100) == 0x53 && f->seen.size() == 1);
	for (int i=0; i<100; ++i) ATSCHEDULER_ADVANCE(&h.mScheduler);
	AT_PORTABLE_TEST_ASSERT(context, h.mHooks.OnHookHit(0x1FE) == 0x4C && h.mCPU.GetPC() == 0x3000 && !f->loader.GetCurrentImage());
	return true;
}

bool ATTestAltirraProgramLoaderSymbols(ATPortableTestContext& context) {
	auto f = std::make_unique<Machine>(); std::vector<std::wstring> paths, scripts; std::vector<uint32> processed, unloaded; uint32 count = 0;
	f->connections.mpIsSymbolLoadingEnabled = [] { return true; };
	f->connections.mpLoadSymbols = [&](const wchar_t *p) -> uint32 { paths.emplace_back(p); ++count; if (count == 2) throw MyError("Missing symbols"); return count; };
	f->connections.mpProcessSymbolDirectives = [&](uint32 id) { processed.push_back(id); };
	f->connections.mpQueueAutoLoadBatchFile = [&](const wchar_t *p) { scripts.emplace_back(p); };
	f->connections.mpUnloadSymbols = [&](uint32 id) { unloaded.push_back(id); };
	f->Init(); Bytes b; Segment(b,0x3000,{0x60});
	f->loader.LoadProgram(L"demo.xex",Image(b),kATHLEProgramLoadMode_Default,false);
	AT_PORTABLE_TEST_ASSERT(context, paths == std::vector<std::wstring>({L"demo.lst",L"demo.lab",L"demo.lbl",L"demo.dbg",L"demo.xex.elf"}));
	AT_PORTABLE_TEST_ASSERT(context, processed == std::vector<uint32>({1,3,4,5}) && scripts == std::vector<std::wstring>{L"demo.atdbg"});
	f->loader.LoadProgram(nullptr,Image(b),kATHLEProgramLoadMode_Default,false);
	AT_PORTABLE_TEST_ASSERT(context, unloaded == processed && paths.size() == 5);
	f->loader.Shutdown(); AT_PORTABLE_TEST_ASSERT(context, unloaded.size() == 4);
	return true;
}

bool ATTestAltirraProgramLoaderDisk(ATPortableTestContext& context) {
	auto f = std::make_unique<SIOPortableFixture>(); ATHLEProgramLoader loader; ATHLEProgramLoaderConnections c;
	loader.Init(&f->mCPU,nullptr,c,&f->mManager); f->mManager.SetDiskSIOAccelEnabled(true);
	Bytes b; Segment(b,0x3000,{0x60}); loader.LoadProgram(nullptr,Image(b),kATHLEProgramLoadMode_DiskBoot,false);
	for (uint16 sector : {uint16(1),uint16(2),uint16(4),uint16(360),uint16(361),uint16(362),uint16(363),uint16(365),uint16(366),uint16(367),uint16(368),uint16(369),uint16(720)}) {
		auto req = f->Request(0x31,0x40,128); req.mAUX[0]=uint8(sector); req.mAUX[1]=uint8(sector>>8);
		AT_PORTABLE_TEST_ASSERT(context, f->mManager.TryAccelRequest(req,false) && f->Mem(ATKernelSymbols::DSTATS) == 1);
		if (sector == 1) AT_PORTABLE_TEST_ASSERT(context, f->Mem(0x4001) == 3 && f->Mem(0x4003) == 0x30);
		if (sector == 366) AT_PORTABLE_TEST_ASSERT(context, f->Mem(0x4000) == 'A' && f->Mem(0x400B) == 0x9B);
		if (sector == 369) AT_PORTABLE_TEST_ASSERT(context, f->Mem(0x4000) == 0xFF && f->Mem(0x4001) == 0xFF && f->Mem(0x407F) == 0x2C);
		if (sector == 720) for (uint16 i=0; i<128; ++i) AT_PORTABLE_TEST_ASSERT(context, f->Mem(0x4000+i) == 0);
	}
	for (uint16 sector : {uint16(0),uint16(721)}) {
		auto req = f->Request(0x31,0x40,128); req.mAUX[0]=uint8(sector); req.mAUX[1]=uint8(sector>>8);
		AT_PORTABLE_TEST_ASSERT(context, f->mManager.TryAccelRequest(req,false) && f->Mem(ATKernelSymbols::DSTATS) == 0x8B);
	}
	AT_PORTABLE_TEST_ASSERT(context, f->mHooks.OnHookHit(0x1FF) == 0x4C && f->mCPU.GetPC() == 0x3000 && !loader.GetCurrentImage());
	loader.Shutdown();
	return true;
}

bool ATTestAltirraProgramLoaderType3(ATPortableTestContext& context) {
	auto f = std::make_unique<SIOPortableFixture>(); ATHLEProgramLoader loader; ATHLEProgramLoaderConnections c;
	loader.Init(&f->mCPU,nullptr,c,&f->mManager); Bytes b; Segment(b,0x3000,{0x60});
	loader.LoadProgram(nullptr,Image(b),kATHLEProgramLoadMode_Type3Poll,false); f->ReceiveClock();
	f->Command(0x4F,0x40); AT_PORTABLE_TEST_ASSERT(context, f->Collect(10000).empty());
	f->Command(0x4F,0x40); auto reply = f->Collect(20000);
	const std::vector<uint8> expected {0x41,0x43,23,0,0x7D,0,0x94};
	AT_PORTABLE_TEST_ASSERT(context, reply == expected);
	f->Command(0x4F,0x40); AT_PORTABLE_TEST_ASSERT(context, f->Collect(10000).empty());
	f->Command(0x4F,0x40,0x4F,0x4F); AT_PORTABLE_TEST_ASSERT(context, f->Collect(10000).empty());
	// First poll after reset is quiet; the next one supplies handler information.
	f->Command(0x4F,0x40); AT_PORTABLE_TEST_ASSERT(context, f->Collect(10000).empty());
	f->Command(0x4F,0x40); AT_PORTABLE_TEST_ASSERT(context, f->Collect(20000) == expected);
	f->mManager.SetOtherSIOAccelEnabled(true); auto req = f->Request(0x7D,0x40,128); req.mCommand=0x26; req.mAUX[0]=req.mAUX[1]=0;
	AT_PORTABLE_TEST_ASSERT(context, f->mManager.TryAccelRequest(req,false) && f->Mem(ATKernelSymbols::DSTATS) == 1 && f->Mem(0x4000) == 0 && f->Mem(0x4001) == 0x11);
	req.mAUX[0]=1; AT_PORTABLE_TEST_ASSERT(context, f->mManager.TryAccelRequest(req,false) && f->Mem(ATKernelSymbols::DSTATS) == 0x8B);
	loader.Shutdown(); AT_PORTABLE_TEST_ASSERT(context, f->mHooks.OnHookHit(0x1FF) == 0);
	return true;
}
