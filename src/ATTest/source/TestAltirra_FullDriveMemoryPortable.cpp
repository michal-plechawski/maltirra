#include "fullDrivePortableFixture.h"
#include <cstdio>
using namespace FullDrivePortable;

bool ATTestAltirraFullDriveBaseMemory(ATPortableTestContext& context) {
	for(Type type : {Drive::kDeviceType_810,Drive::kDeviceType_1050,Drive::kDeviceType_USDoubler}) {
		auto f=std::make_unique<Machine>(type); auto& d=*f->device; d.WriteByte(0x80,0x37); d.WriteByte(0x180,0x68);
		AT_PORTABLE_TEST_ASSERT(context, d.DebugReadByte(0x80)==(f->is1050 ? 0x68 : 0x37) && d.DebugReadByte(0x180)==0x68 && d.DebugReadByte(0x2080)==(f->is1050 ? 0x68 : 0x37) && d.DebugReadByte(0x2180)==0x68);
		if(type==Drive::kDeviceType_USDoubler) { d.WriteByte(0x200,0x71); AT_PORTABLE_TEST_ASSERT(context, d.DebugReadByte(0x200)==0x71 && d.DebugReadByte(0x2200)==0x71); }
		else if(f->is1050) { d.WriteByte(0x20,0x52); AT_PORTABLE_TEST_ASSERT(context, d.ReadByte(0x120)==0x52); }
		else { d.WriteByte(0x190,0x52); AT_PORTABLE_TEST_ASSERT(context, d.ReadByte(0x110)==0x52 && d.ReadByte(0x590)==0x52); }
	}
	return true;
}

bool ATTestAltirraFullDriveExpandedRAM(ATPortableTestContext& context) {
	for(Type type : {Drive::kDeviceType_TOMS1050,Drive::kDeviceType_1050Duplicator,Drive::kDeviceType_Speedy1050,Drive::kDeviceType_ISPlate,Drive::kDeviceType_Tygrys1050,Drive::kDeviceType_810Turbo}) {
		auto f=std::make_unique<Machine>(type); const uint16 start=type==Drive::kDeviceType_TOMS1050 || type==Drive::kDeviceType_1050Duplicator ? 0x2000 : type==Drive::kDeviceType_Tygrys1050 || type==Drive::kDeviceType_810Turbo ? 0x800 : 0x8000;
		f->device->WriteByte(start,0x37); f->device->WriteByte(start+0x7FF,0x68); AT_PORTABLE_TEST_ASSERT(context, f->device->ReadByte(start)==0x37 && f->device->ReadByte(start+0x7FF)==0x68);
		if(type==Drive::kDeviceType_ISPlate) { f->device->WriteByte(0xC000,0x79); AT_PORTABLE_TEST_ASSERT(context, f->device->ReadByte(0x8000)==0x37 && f->device->ReadByte(0xC000)==0x79); }
	}
	return true;
}

bool ATTestAltirraFullDriveHappyBanks(ATPortableTestContext& context) {
	for(Type type : {Drive::kDeviceType_Happy810,Drive::kDeviceType_Happy1050}) {
		auto f=std::make_unique<Machine>(type); Bytes rom(8192,0x37); std::fill(rom.begin()+4096,rom.end(),0x68); f->Reload(rom); const uint16 base=f->is1050 ? 0xF400 : 0x1400,control=f->is1050 ? 0xFFF8 : 0x1FF8;
		AT_PORTABLE_TEST_ASSERT(context, f->device->DebugReadByte(base)==0x68); f->device->DebugReadByte(control); AT_PORTABLE_TEST_ASSERT(context, f->device->DebugReadByte(base)==0x68);
		f->device->ReadByte(control); AT_PORTABLE_TEST_ASSERT(context, f->device->DebugReadByte(base)==0x37); f->device->WriteByte(control+1,0); AT_PORTABLE_TEST_ASSERT(context, f->device->DebugReadByte(base)==0x68);
		f->device->WriteByte(base,0x12); AT_PORTABLE_TEST_ASSERT(context, f->device->DebugReadByte(base)==0x68);
	}
	return true;
}

bool ATTestAltirraFullDriveArchiverBanks(ATPortableTestContext& context) {
	for(Type type : {Drive::kDeviceType_810Archiver,Drive::kDeviceType_SuperArchiver,Drive::kDeviceType_SuperArchiver_BitWriter}) {
		auto f=std::make_unique<Machine>(type); Bytes rom(4096,0x37); std::fill(rom.begin()+2048,rom.end(),0x68); f->Reload(rom); const uint16 base=f->is1050 ? 0x1800 : 0x800; const uint8 mask=f->is1050 ? 4 : 8;
		AT_PORTABLE_TEST_ASSERT(context, f->device->ReadByte(base)==0x68); f->device->WriteByte(f->RIOT(),0); f->device->WriteByte(f->RIOT()+1,mask); AT_PORTABLE_TEST_ASSERT(context, f->device->ReadByte(base)==0x37);
		if(f->is1050) { f->device->WriteByte(0x1805,0x71); AT_PORTABLE_TEST_ASSERT(context, f->device->ReadByte(0x1005)==0x71 && f->device->ReadByte(0x1805)==0x37); }
	}
	return true;
}

bool ATTestAltirraFullDriveTurboBanks(ATPortableTestContext& context) {
	for(Type type : {Drive::kDeviceType_1050Turbo,Drive::kDeviceType_1050TurboII}) {
		auto f=std::make_unique<Machine>(type); Bytes rom(8192); for(uint32 b=0;b<4;++b) std::fill_n(rom.begin()+b*2048,2048,uint8(0x40+b));
		for(uint32 b=0;b<4;++b) { rom[b*2048]=type==Drive::kDeviceType_1050Turbo ? 1 : 0x30; rom[b*2048+1]=type==Drive::kDeviceType_1050Turbo ? 0 : 0x20; }
		f->Reload(rom); AT_PORTABLE_TEST_ASSERT(context, f->device->DebugReadByte(0x1802)==0x43); f->device->DebugReadByte(0x1000); AT_PORTABLE_TEST_ASSERT(context, f->device->DebugReadByte(0x1802)==0x43);
		f->device->ReadByte(0x1000); AT_PORTABLE_TEST_ASSERT(context, f->device->DebugReadByte(0x1802)==0x41); f->device->ReadByte(0x1001); AT_PORTABLE_TEST_ASSERT(context, f->device->DebugReadByte(0x1802)==0x40);
	}
	return true;
}

bool ATTestAltirraFullDriveHappyMirrors(ATPortableTestContext& context) {
	auto f=std::make_unique<Machine>(Drive::kDeviceType_Happy1050); auto& d=*f->device;
	d.WriteByte(0x8000,0x37); AT_PORTABLE_TEST_ASSERT(context, d.DebugReadByte(0xA000)==0x37);
	d.WriteByte(0x8F20,0x68); d.WriteByte(0x9F20,0x52); AT_PORTABLE_TEST_ASSERT(context, d.DebugReadByte(0xAF20)==0x68 && d.DebugReadByte(0xBF20)==0x52);
	d.WriteByte(0xAF21,0x79); AT_PORTABLE_TEST_ASSERT(context, d.DebugReadByte(0x8F21)==0x79);
	d.WriteByte(0x20,0x52); const auto wp=f->FDC().GetWriteProtectOverride2(); AT_PORTABLE_TEST_ASSERT(context, d.DebugReadByte(0x4020)==0x52 && f->FDC().GetWriteProtectOverride2()==wp);
	d.ReadByte(0x4020); AT_PORTABLE_TEST_ASSERT(context, f->FDC().GetWriteProtectOverride2()==kATFDCWPOverride_Invert); d.DebugReadByte(0x9800); AT_PORTABLE_TEST_ASSERT(context, f->FDC().GetWriteProtectOverride2()==kATFDCWPOverride_Invert);
	d.ReadByte(0x9800); AT_PORTABLE_TEST_ASSERT(context, f->FDC().GetWriteProtectOverride2()==kATFDCWPOverride_None);
	return true;
}

bool ATTestAltirraFullDriveBitWriterRAM(ATPortableTestContext& context) {
	auto f=std::make_unique<Machine>(Drive::kDeviceType_SuperArchiver_BitWriter); f->BitWriterSetup(); f->BitWriterByte(0x37); f->BitWriterNext(); f->BitWriterByte(0x68);
	f->PIA(1,0x1F); f->PIA(1,0x1D); f->PIA(2,0x38); f->PIA(0,0); f->PIA(2,0x3C);
	AT_PORTABLE_TEST_ASSERT(context, f->device->ReadByte(0x200)==0x37); f->BitWriterNext(); AT_PORTABLE_TEST_ASSERT(context, f->device->ReadByte(0x200)==0x68);
	return true;
}

bool ATTestAltirraFullDriveBitWriterTrack(ATPortableTestContext& context) {
	auto f=std::make_unique<Machine>(Drive::kDeviceType_SuperArchiver_BitWriter); f->Fill(183,Bytes(128,0x37));
	auto writeTrack=[&](IATDiskImage& input) {
		constexpr uint32 bits=52088; Bytes raw(bits/8+4); ATDiskRenderRawTrack(raw.data(),bits,nullptr,input,10,false,false,true); raw.resize(bits/8);
		f->BitWriterSetup();
		for(uint8 v:raw) { f->BitWriterNext(); uint8 reversed=0; for(uint32 i=0;i<8;++i) reversed=uint8((reversed<<1)|((v>>i)&1)); f->BitWriterByte(reversed); }
		f->PIA(0,0xFF); f->PIA(1,0x1F); f->PIA(1,0x1D); f->PIA(1,0x15); f->Run(uint32(std::ceil(raw.size()*32.0*1789772.5/1000000.0))+1); f->PIA(1,0x1D);
	};
	writeTrack(f->Image());
	ATDiskVirtualSectorInfo info {}; f->Image().GetVirtualSectorInfo(183,info);
	AT_PORTABLE_TEST_ASSERT(context, info.mNumPhysSectors>0 && f->Stored(183)==Bytes(128,0x37) && f->Image().IsDirty());
	// A 1024-byte data field straddling the index needs a full encoded sector
	// beyond the end of the circular track while the parser scans the splice.
	std::array<ATDiskVirtualSectorInfo,18> virtuals {}; virtuals[3]={0,1}; ATDiskPhysicalSectorInfo physical {};
	physical.mImageSize=physical.mPhysicalSize=1024; physical.mFDCStatus=0xFF; physical.mWeakDataOffset=-1; physical.mDiskOffset=-1; physical.mRotPos=0.99f;
	Bytes payload(1024); for(uint32 i=0;i<1024;++i) payload[i]=uint8(i*17+9);
	ATDiskGeometryInfo geometry {}; geometry.mTotalSectorCount=720; geometry.mSectorSize=1024; geometry.mSectorsPerTrack=18; geometry.mTrackCount=40; geometry.mSideCount=1;
	vdrefptr<IATDiskImage> input; ATCreateDiskImage(geometry,~input); input->FormatTrack(180,18,virtuals.data(),1,&physical,payload.data()); writeTrack(*input); f->Image().GetVirtualSectorInfo(183,info);
	Bytes actual(1024); if(info.mNumPhysSectors) f->Image().ReadPhysicalSector(info.mStartPhysSector,actual.data(),1024);
	if(info.mNumPhysSectors!=1 || actual!=payload) {
		const auto mismatch=std::mismatch(actual.begin(),actual.end(),payload.begin()); ATDiskPhysicalSectorInfo parsed {}; if(info.mNumPhysSectors) f->Image().GetPhysicalSectorInfo(info.mStartPhysSector,parsed);
		std::printf("BitWriter splice: count=%u, size=%u, status=$%02X, first mismatch=%zu\n",info.mNumPhysSectors,parsed.mPhysicalSize,parsed.mFDCStatus,size_t(mismatch.first-actual.begin()));
	}
	AT_PORTABLE_TEST_ASSERT(context, info.mNumPhysSectors==1 && actual==payload);
	return true;
}
