#include "fdcportablefixture.h"
using namespace FDCPortable;

bool ATTestAltirraFDCReadFM(ATPortableTestContext& context) {
	for(auto type : {ATFDCEmulator::kType_1771,ATFDCEmulator::kType_2793}) {
		auto f=std::make_unique<Machine>(128,type); const auto pattern=Pattern(128); f->Fill(3,pattern); f->Select(4); Bytes b;
		AT_PORTABLE_TEST_ASSERT(context, !(f->Read(0x80,b)&0x1C) && b==Invert(pattern) && !f->chip.GetDrqStatus());
	}
	return true;
}

bool ATTestAltirraFDCReadMFM(ATPortableTestContext& context) {
	for(uint32 size : {256u,512u,1024u,4096u,8192u}) {
		auto f=std::make_unique<Machine>(size); const auto pattern=Pattern(size); f->Fill(3,pattern); f->Select(4); Bytes b;
		AT_PORTABLE_TEST_ASSERT(context, !(f->Read(0x80,b)&0x1C) && b==Invert(Bytes(pattern.begin(),pattern.begin()+std::min<uint32>(4096,size))));
	}
	return true;
}

bool ATTestAltirraFDCWriteFM(ATPortableTestContext& context) {
	for(auto type : {ATFDCEmulator::kType_1771,ATFDCEmulator::kType_2793}) {
		auto f=std::make_unique<Machine>(128,type); const auto pattern=Pattern(128); f->Select(4);
		AT_PORTABLE_TEST_ASSERT(context, !(f->Write(type==ATFDCEmulator::kType_1771 ? 0xA8 : 0xA0,Invert(pattern))&0x7C) && f->written==128 && f->Stored(3)==pattern && f->Image().IsDirty());
	}
	return true;
}

bool ATTestAltirraFDCWriteMFM(ATPortableTestContext& context) {
	for(uint32 size : {256u,512u,1024u,4096u}) {
		auto f=std::make_unique<Machine>(size); const auto pattern=Pattern(size); f->Select(4);
		AT_PORTABLE_TEST_ASSERT(context, !(f->Write(0xA0,Invert(pattern))&0x7C) && f->written==size && f->Stored(3)==pattern);
	}
	return true;
}

bool ATTestAltirraFDCBootSectors(ATPortableTestContext& context) {
	auto f=std::make_unique<Machine>(256); f->SpecialTrack(0,0xFF,-1,128); Bytes b; f->Select(4); f->Read(0x80,b);
	const auto boot=Pattern(128); Bytes extended=boot; extended.resize(256,0); f->Select(1);
	AT_PORTABLE_TEST_ASSERT(context, !(f->Read(0x80,b)&0x1C) && b==Invert(extended));
	f->chip.SetDDBootSectorMode(ATFDCEmulator::DDBootSectorMode::Swapped); std::rotate(extended.begin(),extended.begin()+128,extended.end());
	AT_PORTABLE_TEST_ASSERT(context, !(f->Read(0x80,b)&0x1C) && b==Invert(extended));
	auto w=std::make_unique<Machine>(256); w->chip.SetDDBootSectorMode(ATFDCEmulator::DDBootSectorMode::Swapped);
	for(uint8 sector=1;sector<=3;++sector) {
		const auto payload=Pattern(128,sector); Bytes physical(128,0); physical.insert(physical.end(),payload.begin(),payload.end()); w->Select(sector);
		AT_PORTABLE_TEST_ASSERT(context, !(w->Write(0xA0,Invert(physical))&0x7C) && w->Stored(sector-1)==payload);
	}
	return true;
}

bool ATTestAltirraFDCSideMapping(ATPortableTestContext& context) {
	using S=ATFDCEmulator::SideMapping;
	for(auto c : {std::pair<S,uint32>{S::Side2Forward,42*18+3},{S::Side2Reversed,77*18+14},{S::Side2ReversedOffByOne,77*18+13},{S::Side2ReversedTracks,77*18+3}}) {
		auto f=std::make_unique<Machine>(128,ATFDCEmulator::kType_2793,2); f->chip.SetSideMapping(c.first,40); auto pattern=Pattern(128); f->Fill(c.second,pattern); f->Select(4,2,true); Bytes b;
		AT_PORTABLE_TEST_ASSERT(context, !(f->Read(0x80,b)&0x1C) && b==Invert(pattern));
		pattern=Pattern(128,0x37); AT_PORTABLE_TEST_ASSERT(context, !(f->Write(0xA0,Invert(pattern))&0x7C) && f->Stored(c.second)==pattern);
	}
	return true;
}

bool ATTestAltirraFDCAddress(ATPortableTestContext& context) {
	for(bool mfm : {false,true}) {
		auto f=std::make_unique<Machine>(mfm ? 256 : 128); f->Select(4,2); Bytes b;
		AT_PORTABLE_TEST_ASSERT(context, !(f->Read(0xC0,b)&0x1C) && b.size()==6 && b[0]==2 && b[1]==0 && b[2]>=1 && b[2]<=18 && b[3]==(mfm ? 1 : 0) && f->chip.DebugReadByte(2)==2);
		Bytes frame; if(mfm) frame={0xA1,0xA1,0xA1}; frame.push_back(0xFE); frame.insert(frame.end(),b.begin(),b.end());
		AT_PORTABLE_TEST_ASSERT(context, ATComputeCRC16(0xFFFF,frame.data(),frame.size())==0);
	}
	return true;
}

bool ATTestAltirraFDCErrorSectors(ATPortableTestContext& context) {
	for(uint8 flags : {uint8(0xF7),uint8(0xDF),uint8(0xEF),uint8(0xE7)}) {
		auto f=std::make_unique<Machine>(); f->SpecialTrack(3,flags); f->Select(4); Bytes b; const uint8 s=f->Read(0x80,b);
		AT_PORTABLE_TEST_ASSERT(context, s!=0xFF && (s&0x38)==(uint8(~flags)&0x38));
		AT_PORTABLE_TEST_ASSERT(context, (flags&0x10) ? b==Invert(Pattern(128,0x18)) : b.empty());
	}
	return true;
}

bool ATTestAltirraFDCWeakSectors(ATPortableTestContext& context) {
	auto f=std::make_unique<Machine>(); f->SpecialTrack(3,0xFF,16); f->Select(4); Bytes a,b; const auto plain=Invert(Pattern(128,0x18));
	AT_PORTABLE_TEST_ASSERT(context, !(f->Read(0x80,a)&0x1C) && !(f->Read(0x80,b)&0x1C) && a!=b && std::equal(plain.begin(),plain.begin()+16,a.begin()) && std::equal(plain.begin(),plain.begin()+16,b.begin()));
	f->chip.Reset(); f->Select(4); AT_PORTABLE_TEST_ASSERT(context, !(f->Read(0x80,b)&0x1C) && a==b);
	return true;
}

bool ATTestAltirraFDCWriteProtect(ATPortableTestContext& context) {
	auto f=std::make_unique<Machine>(); const auto original=Pattern(128),changed=Pattern(128,0x33); f->Fill(3,original); f->Select(4);
	f->slot.mDisk.SetWriteMode(kATMediaWriteMode_RO); AT_PORTABLE_TEST_ASSERT(context, f->chip.DebugReadByte(0)&0x40);
	AT_PORTABLE_TEST_ASSERT(context, (f->Write(0xA0,Invert(changed))&0x40) && f->Stored(3)==original);
	f->slot.mDisk.SetWriteMode(kATMediaWriteMode_VRW); f->chip.SetWriteProtectOverride(true);
	AT_PORTABLE_TEST_ASSERT(context, (f->Write(0xA0,Invert(changed))&0x40) && f->Stored(3)==original);
	f->chip.SetWriteProtectOverride2(kATFDCWPOverride_Invert);
	AT_PORTABLE_TEST_ASSERT(context, !(f->Write(0xA0,Invert(changed))&0x7C) && f->Stored(3)==changed && f->chip.GetWriteProtectOverride2()==kATFDCWPOverride_Invert);
	return true;
}
