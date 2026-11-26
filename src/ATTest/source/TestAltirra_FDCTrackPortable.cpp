#include "fdcportablefixture.h"
#include "mmuportablefixture.h"
using namespace FDCPortable;

bool ATTestAltirraFDCReadLostData(ATPortableTestContext& context) {
	auto f=std::make_unique<Machine>(); f->Fill(3,Pattern(128)); f->Select(4); f->chip.WriteByte(0,0x80);
	AT_PORTABLE_TEST_ASSERT(context, f->WaitDRQ()); f->Tick(130); Bytes b;
	for(uint32 n=0;n<100000 && !f->chip.GetIrqStatus();++n) { if(f->chip.GetDrqStatus()) b.push_back(f->chip.ReadByte(3)); f->Tick(); }
	AT_PORTABLE_TEST_ASSERT(context, f->chip.GetIrqStatus() && (f->chip.DebugReadByte(0)&4) && b.size()==126 && b.front()==Invert(Pattern(128))[2]);
	// A DRQ consumer may acknowledge the byte synchronously from the callback.
	Bytes direct; auto r=std::make_unique<Machine>(); r->Fill(3,Pattern(128)); r->Select(4);
	r->chip.SetOnDrqChange([&](bool v) { if(v) direct.push_back(r->chip.ReadByte(3)); }); r->chip.WriteByte(0,0x80);
	AT_PORTABLE_TEST_ASSERT(context, r->WaitIRQ() && direct==Invert(Pattern(128)) && !(r->chip.DebugReadByte(0)&6));
	return true;
}

bool ATTestAltirraFDCWriteLostData(ATPortableTestContext& context) {
	auto f=std::make_unique<Machine>(); const auto original=Pattern(128); f->Fill(3,original); f->Select(4); f->chip.WriteByte(0,0xA0);
	AT_PORTABLE_TEST_ASSERT(context, f->WaitDRQ() && f->WaitIRQ() && (f->chip.DebugReadByte(0)&4) && f->Stored(3)==original);
	auto w=std::make_unique<Machine>(); w->Select(4); w->chip.WriteByte(0,0xA0); AT_PORTABLE_TEST_ASSERT(context, w->WaitDRQ()); w->chip.WriteByte(3,0x5A);
	AT_PORTABLE_TEST_ASSERT(context, w->WaitDRQ()); w->Tick(65);
	for(uint32 n=0;n<100000 && !w->chip.GetIrqStatus();++n) { if(w->chip.GetDrqStatus()) w->chip.WriteByte(3,0x5A); w->Tick(); }
	const auto stored=w->Stored(3);
	AT_PORTABLE_TEST_ASSERT(context, w->chip.GetIrqStatus() && (w->chip.DebugReadByte(0)&4) && stored[0]==0xA5 && stored[1]==0xFF && stored.back()==0xA5);
	return true;
}

bool ATTestAltirraFDCReadTrack(ATPortableTestContext& context) {
	for(bool mfm : {false,true}) {
		Bytes b; auto f=std::make_unique<Machine>(mfm ? 256 : 128); f->Fill(3,Bytes(mfm ? 256 : 128,0xA5)); f->Select(4);
		uint8 status;
		if(mfm) {
			f->chip.SetOnDrqChange([&](bool v) { if(v) b.push_back(f->chip.ReadByte(3)); }); f->chip.WriteByte(0,0xE0);
			AT_PORTABLE_TEST_ASSERT(context, f->WaitIRQ()); status=f->chip.DebugReadByte(0);
		} else status=f->Read(0xE0,b);
		const Bytes run(64,0x5A);
		AT_PORTABLE_TEST_ASSERT(context, !(status&0x1C) && b.size()>(mfm ? 6000 : 3000) && b.size()<(mfm ? 6600 : 3300) && std::search(b.begin(),b.end(),run.begin(),run.end())!=b.end() && std::find(b.begin(),b.end(),0xFE)!=b.end());
	}
	return true;
}

bool ATTestAltirraFDCWriteTrack(ATPortableTestContext& context) {
	for(bool mfm : {false,true}) {
		auto f=std::make_unique<Machine>(); f->chip.SetDensity(mfm); f->Select(1);
		AT_PORTABLE_TEST_ASSERT(context, !(f->Write(0xF0,FormatTokens(mfm,0,1,mfm ? 1 : 0))&0x7C));
		const uint32 size=mfm ? 256 : 128; ATDiskVirtualSectorInfo v {}; f->Image().GetVirtualSectorInfo(0,v);
		AT_PORTABLE_TEST_ASSERT(context, f->Image().GetGeometry().mSectorSize==size && v.mNumPhysSectors==1 && f->Stored(0)==Bytes(128,0xA5));
		Bytes b; AT_PORTABLE_TEST_ASSERT(context, !(f->Read(0x80,b)&0x1C) && b==Bytes(size,0x5A));
	}
	auto ro=std::make_unique<Machine>(); ro->slot.mDisk.SetWriteMode(kATMediaWriteMode_RO);
	AT_PORTABLE_TEST_ASSERT(context, ro->Write(0xF0,FormatTokens(false))&0x40);
	return true;
}

bool ATTestAltirraFDCPersistence(ATPortableTestContext& context) {
	DiskInterfaceSandbox sandbox; const auto path=sandbox.File(L"fdc.atr"); auto f=std::make_unique<Machine>(); const auto bytes=Pattern(128); f->Select(4);
	AT_PORTABLE_TEST_ASSERT(context, !(f->Write(0xA0,Invert(bytes))&0x7C)); f->Image().Save(path.c_str(),kATDiskImageFormat_ATR);
	vdrefptr<IATDiskImage> loaded; ATLoadDiskImage(path.c_str(),~loaded); Bytes b(128); loaded->ReadVirtualSector(3,b.data(),128);
	AT_PORTABLE_TEST_ASSERT(context, loaded->GetVirtualSectorCount()==720 && b==bytes);
	return true;
}

bool ATTestAltirraFDCCPU(ATPortableTestContext& context) {
	auto host=std::make_unique<MMUPortableFixture>(); auto f=std::make_unique<Machine>(128,ATFDCEmulator::kType_2793,1,&host->mScheduler);
	f->Fill(3,Pattern(128)); ATMemoryHandlerTable handlers {}; handlers.mpThis=&f->chip;
	handlers.mpDebugReadHandler=[](void *p,uint32 a)->sint32 { return static_cast<ATFDCEmulator *>(p)->DebugReadByte(a&3); };
	handlers.mpReadHandler=[](void *p,uint32 a)->sint32 { return static_cast<ATFDCEmulator *>(p)->ReadByte(a&3); };
	handlers.mpWriteHandler=[](void *p,uint32 a,uint8 v) { static_cast<ATFDCEmulator *>(p)->WriteByte(a&3,v); return true; };
	auto *layer=host->mMemory.CreateLayer(kATMemoryPri_Hardware,handlers,0xD5,1); host->mMemory.EnableLayer(layer,true);
	// Poll DRQ, copy one sector through real 6502 instructions, then wait for BUSY.
	const uint8 program[]={0xA9,4,0x8D,2,0xD5,0xA9,0x80,0x8D,0,0xD5,0xA2,0,
		0xAD,0,0xD5,0x29,2,0xF0,0xF9,0xAD,3,0xD5,0x9D,0,0x40,0xE8,0xE0,0x80,0xD0,0xEE,
		0xAD,0,0xD5,0x29,1,0xD0,0xF9,0x4C,0x25,0x20};
	std::copy(std::begin(program),std::end(program),host->mRAM.begin()+0x2000);
	const bool ran=host->RunCycles(20000); host->mMemory.DeleteLayer(layer);
	const auto expected=Invert(Pattern(128));
	AT_PORTABLE_TEST_ASSERT(context, ran && host->mCPU.GetX()==0x80 && std::equal(expected.begin(),expected.end(),host->mRAM.begin()+0x4000) && !(f->chip.DebugReadByte(0)&7));
	return true;
}
