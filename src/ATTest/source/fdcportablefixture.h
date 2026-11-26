// FDC registers, native production disk images/slot and a deterministic drive clock.
#ifndef f_ATTEST_FDCPORTABLEFIXTURE_H
#define f_ATTEST_FDCPORTABLEFIXTURE_H
#include <array>
#include <algorithm>
#include <memory>
#include <string>
#include <vector>
#include <vd2/system/vdtypes.h>
#include <at/atcore/consoleoutput.h>
#include <at/atcore/crc.h>
#include <at/atcore/media.h>
#include <fdc.h>
#include <trace.h>
#include "diskinterfaceportablefixture.h"
namespace FDCPortable {
	using Bytes=std::vector<uint8>;
	inline Bytes Pattern(size_t n,uint8 seed=0x15) { Bytes b(n); for (size_t i=0; i<n; ++i) b[i]=uint8(seed+i*17); return b; }
	inline Bytes Invert(Bytes b) { for (auto& v:b) v=~v; return b; }
	struct Machine {
		explicit Machine(uint32 size=128,ATFDCEmulator::Type type=ATFDCEmulator::kType_2793,uint32 sides=1,ATScheduler *clock=nullptr)
			: sch(clock ? *clock : ownClock), type(type) {
			sch.SetRate(VDFraction(1000000,1)); ATDiskGeometryInfo g {}; g.mSectorSize=uint16(size); g.mBootSectorCount=size>256 ? 0 : 3;
			g.mTrackCount=40; g.mSideCount=uint8(sides); g.mSectorsPerTrack=18; g.mTotalSectorCount=40*18*sides; g.mbMFM=size>128;
			slot.mDisk.CreateDisk(g); slot.mDisk.SetWriteMode(kATMediaWriteMode_VRW);
			chip.SetOnDrqChange([this](bool v) { drq.push_back(v); if(v) drqTicks.push_back(sch.GetTick()); }); chip.SetOnIrqChange([this](bool v) { irq.push_back(v); });
			chip.SetOnMotorChange([this](bool v) { motor.push_back(v); if (this->type==ATFDCEmulator::kType_1770 || this->type==ATFDCEmulator::kType_1772) chip.SetMotorRunning(v); });
			chip.SetOnHeadLoadChange([this](bool v) { head.push_back(v); }); chip.SetOnIndexChange([this](bool v) { index.push_back({sch.GetTick(),v}); });
			chip.SetOnStep([this](bool inward) { steps.push_back({sch.GetTick(),inward}); if(inward) ++track; else if(track) --track; chip.SetCurrentTrack(2*track,track==0); });
			chip.Init(&sch,300,1,type); chip.SetDiskInterface(&slot.mDisk); chip.SetDiskImage(slot.mDisk.GetDiskImage()); chip.Reset();
			chip.SetCurrentTrack(0,true); chip.SetDensity(size>128); chip.SetMotorRunning(true); chip.SetAutoIndexPulse(true);
			client.Attach(slot.mDisk); client.mpChanging=[this] { chip.SetDiskImage(nullptr); }; client.mpChanged=[this] { chip.SetDiskImage(slot.mDisk.GetDiskImage()); };
		}
		~Machine() { chip.Shutdown(); }
		IATDiskImage& Image() { return *slot.mDisk.GetDiskImage(); }
		void Tick(uint32 n=1) { while(n--) ATSCHEDULER_ADVANCE(&sch); }
		bool WaitIRQ(uint32 limit=3000000) { while(limit-- && !chip.GetIrqStatus()) Tick(); return chip.GetIrqStatus(); }
		bool WaitDRQ(uint32 limit=3000000) { while(limit-- && !chip.GetDrqStatus() && !chip.GetIrqStatus()) Tick(); return chip.GetDrqStatus(); }
		void Select(uint8 sector,uint8 t=0,bool side=false) { track=t; chip.SetCurrentTrack(2*t,t==0); chip.SetSide(side); chip.WriteByte(1,t); chip.WriteByte(2,sector); }
		void Fill(uint32 index,const Bytes& data) { if(!Image().WriteVirtualSector(index,data.data(),uint32(data.size()))) std::abort(); }
		Bytes Stored(uint32 index) { Bytes data(Image().GetSectorSize(index)); Image().ReadVirtualSector(index,data.data(),uint32(data.size())); return data; }
		uint8 Read(uint8 command,Bytes& out,uint32 limit=3000000) {
			out.clear(); chip.WriteByte(0,command);
			while(limit-- && !chip.GetIrqStatus()) { Tick(); if(chip.GetDrqStatus()) out.push_back(chip.ReadByte(3)); }
			if (!chip.GetIrqStatus()) return 0xFF;
			return chip.DebugReadByte(0);
		}
		uint8 Write(uint8 command,const Bytes& input,uint32 limit=3000000) {
			written=0; chip.WriteByte(0,command);
			while(limit-- && !chip.GetIrqStatus()) { Tick(); if(chip.GetDrqStatus()) chip.WriteByte(3,written<input.size() ? input[written++] : 0xFF); }
			if (!chip.GetIrqStatus()) return 0xFF;
			return chip.DebugReadByte(0);
		}
		// Build one real physical track, with selectable FDC flags/weak bytes and
		// optional short boot sectors; other sectors remain independently readable.
		void SpecialTrack(uint32 target,uint8 flags=0xFF,sint16 weak=-1,uint32 physicalSize=0) {
			const auto g=Image().GetGeometry(); const uint32 size=g.mSectorSize,spt=g.mSectorsPerTrack;
			std::vector<ATDiskVirtualSectorInfo> vs(spt); std::vector<ATDiskPhysicalSectorInfo> ps(spt); Bytes data(spt*size);
			for(uint32 i=0; i<spt; ++i) {
				vs[i]={i,1}; auto& p=ps[i]; p.mOffset=i*size; p.mDiskOffset=-1; p.mImageSize=uint16(size); p.mPhysicalSize=uint16(size);
				p.mbMFM=g.mbMFM; p.mRotPos=float(i)/float(spt); p.mFDCStatus=i==target ? flags : 0xFF; p.mWeakDataOffset=i==target ? weak : -1;
				if(i==target && physicalSize) p.mPhysicalSize=p.mImageSize=uint16(physicalSize);
				const auto bytes=Pattern(size,uint8(0x15+i)); std::copy(bytes.begin(),bytes.end(),data.begin()+i*size);
			}
			Image().FormatTrack(0,spt,vs.data(),spt,ps.data(),data.data()); chip.SetDiskImage(&Image());
		}
		ATScheduler ownClock; ATScheduler& sch; DiskInterfaceProbe slot;
		ATFDCEmulator::Type type; uint32 track=0,written=0;
		std::vector<bool> drq,irq,motor,head; std::vector<std::pair<uint32,bool>> steps,index;
		std::vector<uint32> drqTicks;
		ATFDCEmulator chip; DiskInterfaceClientProbe client;
	};
	inline Bytes FormatTokens(bool mfm,uint8 track=0,uint8 sector=1,uint8 sizeCode=0) {
		Bytes b(40,mfm ? 0x4E : 0xFF);
		auto sync=[&] { b.insert(b.end(),mfm ? 12 : 6,0); if(mfm) b.insert(b.end(),3,0xF5); };
		sync(); for(auto c : {uint8(0xFE),track,uint8(0),sector,sizeCode,uint8(0xF7)}) b.push_back(c);
		b.insert(b.end(),mfm ? 22 : 11,mfm ? 0x4E : 0xFF); sync(); b.push_back(0xFB); b.insert(b.end(),128u<<sizeCode,0x5A); b.push_back(0xF7);
		return b;
	}
	struct Text final : ATConsoleOutput { std::string value; void WriteLine(const char *s) override { value+=s; value+='\n'; } };
}
#endif
