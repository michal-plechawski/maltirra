// The production virtual editor attached to a real CPU and memory bus.
#ifndef f_ATTEST_VIRTUALSCREENPORTABLEFIXTURE_H
#define f_ATTEST_VIRTUALSCREENPORTABLEFIXTURE_H
#include <algorithm>
#include <array>
#include <memory>
#include <string>
#include <at/attest/portabletest.h>
#include <at/atcore/cio.h>
#include <at/atcore/ksyms.h>
#include <virtualscreen.h>
#include "maincpuportablefixture.h"

namespace VirtualScreenPortable {
	struct Machine {
		explicit Machine(bool open = true) {
			std::fill_n(h.mRAM.begin(),65536,0);
			Mem(ATKernelSymbols::LMARGN,2); Mem(ATKernelSymbols::RMARGN,39);
			Word(ATKernelSymbols::COLCRS,2);
			Mem(ATKernelSymbols::BRKKEY,0xFF); Mem(ATKernelSymbols::CH,0xFF); Mem(ATKernelSymbols::SHFLOK,0x40);
			for (uint16 i=0; i<3; ++i) Mem(ATKernelSymbols::TABMAP+i,1);
			Mem(ATKernelSymbols::DMACTL,2); Word(ATKernelSymbols::SDLSTL,0x3000); Word(ATKernelSymbols::SAVMSC,0x4000);
			Mem(0x3003,0x42); Mem(0x3004,0); Mem(0x3005,0x40);
			screen.reset(ATCreateVirtualScreenHandler(*h.mCPU.GetMemory()));
			screen->SetGetCharAddresses(0x2200,0x2210);
			if (open) Open();
		}
		void Mem(uint16 a, uint8 v) { h.mMemory.WriteByte(a,v); }
		uint8 Mem(uint16 a) { return h.mMemory.ReadByte(a); }
		void Word(uint16 a, uint16 v) { Mem(a,uint8(v)); Mem(uint16(a+1),uint8(v>>8)); }
		uint16 Word(uint16 a) { return Mem(a)+256*Mem(uint16(a+1)); }
		uint8 Editor(int offset, uint8 a = 0) { h.mCPU.SetA(a); return screen->OnEditorCIOVector(&h.mCPU,h.mCPU.GetMemory(),offset); }
		uint8 Screen(int offset, uint8 a = 0) { h.mCPU.SetA(a); return screen->OnScreenCIOVector(&h.mCPU,h.mCPU.GetMemory(),offset); }
		void Open() { h.mCPU.SetX(0); Editor(0); Position(2,0); Editor(4); h.mCPU.SetS(0xFD); screen->CheckForDisplayListReplaced(); }
		void Position(uint16 x, uint8 y) { Word(ATKernelSymbols::COLCRS,x); Mem(ATKernelSymbols::ROWCRS,y); }
		void Print(const std::string& text) { for (unsigned char c : text) Editor(6,c); }
		std::string Row(uint32 y) {
			uint32 w,hgt; const uint8 *p; screen->GetScreen(w,hgt,p);
			return y < hgt ? std::string(reinterpret_cast<const char *>(p+y*w),w) : std::string();
		}
		std::string Input(uint32 limit = 1024) {
			std::string s;
			while (limit--) {
				const uint8 stack = h.mCPU.GetS();
				Editor(4);
				if (h.mCPU.GetS() != stack) { h.mCPU.SetS(stack); continue; }
				if (h.mCPU.GetY() != kATCIOStat_Success) return {};
				uint8 c = h.mCPU.GetA(); s.push_back(char(c));
				if (c == 0x9B) return s;
			}
			return {};
		}
		MainCPUPortableFixture h;
		std::unique_ptr<IATVirtualScreenHandler> screen;
	};
}
#endif
