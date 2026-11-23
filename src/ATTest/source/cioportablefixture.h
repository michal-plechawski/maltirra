// Production CPU/MMU/PIA and CIO manager; only the attached device is a probe.
#ifndef f_ATTEST_CIOPORTABLEFIXTURE_H
#define f_ATTEST_CIOPORTABLEFIXTURE_H
#include <array>
#include <cstring>
#include <memory>
#include <string>
#include <vector>
#include <at/attest/portabletest.h>
#include <at/atcore/cio.h>
#include <at/atcore/devicecio.h>
#include <at/atcore/ksyms.h>
#include <cpumemory.h>
#include <hleciohook.h>
#include <virtualscreen.h>
#include "mmuportablefixture.h"

namespace CIOPortable {
	struct Device final : IATDeviceCIO {
		void InitCIO(IATDeviceCIOManager *p) override { manager = p; p->AddCIODevice(this); }
		void GetCIODevices(IATDeviceCIODeviceList& list) const override { for (const auto& n : names) list.AddDevice(n.first,n.second); }
		sint32 OnCIOOpen(int ch, uint8 unit, uint8 a, uint8 b, const uint8 *name) override {
			++opens; channel=ch; deviceNo=unit; aux={a,b,0,0,0,0}; filename=reinterpret_cast<const char *>(name);
			return onOpen ? onOpen() : openStatus;
		}
		sint32 OnCIOClose(int ch, uint8 unit) override { ++closes; channel=ch; deviceNo=unit; return onClose ? onClose() : 1; }
		sint32 OnCIOGetBytes(int ch, uint8 unit, void *p, uint32 n, uint32& actual) override {
			++gets; channel=ch; deviceNo=unit; requests.push_back(n);
			if (onGet) return onGet(p,n,actual);
			actual=std::min<uint32>(n,uint32(input.size()-readIndex));
			if (actual) memcpy(p,input.data()+readIndex,actual);
			readIndex+=actual; return actual ? 1 : kATCIOStat_EndOfFile;
		}
		sint32 OnCIOPutBytes(int ch, uint8 unit, const void *p, uint32 n, uint32& actual) override {
			++puts; channel=ch; deviceNo=unit; requests.push_back(n);
			actual=n; const sint32 r=onPut ? onPut(p,n,actual) : 1;
			if (actual && actual<=n) output.insert(output.end(),static_cast<const uint8 *>(p),static_cast<const uint8 *>(p)+actual);
			return r;
		}
		sint32 OnCIOGetStatus(int ch, uint8 unit, uint8 status[4]) override {
			++statuses; channel=ch; deviceNo=unit;
			for (uint32 i=0; i<4; ++i) status[i]=uint8(0x41+i);
			return onStatus ? onStatus() : 1;
		}
		sint32 OnCIOSpecial(int ch, uint8 unit, uint8 cmd, uint16 addr, uint16 len, uint8 a[6]) override {
			++specials; channel=ch; deviceNo=unit; command=cmd; address=addr; length=len; std::copy_n(a,6,aux.begin());
			for (uint8 i=0; i<6; ++i) a[i]=uint8(a[i]+0x10);
			return 1;
		}
		void OnCIOAbortAsync() override { ++aborts; if (onAbort) onAbort(); }
		IATDeviceCIOManager *manager = nullptr;
		std::vector<std::pair<uint8,uint32>> names;
		std::vector<uint8> input,output;
		std::vector<uint32> requests;
		std::string filename;
		std::array<uint8,6> aux {};
		uint32 opens=0,closes=0,gets=0,puts=0,statuses=0,specials=0,aborts=0;
		uint32 readIndex=0;
		int channel=0;
		uint8 deviceNo=0,command=0;
		uint16 address=0,length=0;
		sint32 openStatus=1;
		vdfunction<sint32()> onOpen,onClose,onStatus;
		vdfunction<void()> onAbort;
		vdfunction<sint32(void *,uint32,uint32&)> onGet;
		vdfunction<sint32(const void *,uint32,uint32&)> onPut;
	};
	struct Machine {
		Machine() : hook(nullptr,ATDestroyHLECIOHook) {
			std::fill_n(h.mRAM.begin(),65536,0);
			h.mLowerKernel.fill(0xEA); h.mUpperKernel.fill(0xEA);
			h.mUpperKernel[0xCC0]=0x60; // E4C0: known RTS for asynchronous polling
			h.mHooks.EnableOSHooks(true); h.mMMU.SetBankRegister(0xFF);
			for (uint16 i=0; i<8; ++i) Mem(ATKernelSymbols::ICHID+(i<<4),0xFF);
			Mem(ATKernelSymbols::HATABS,'E'); Word(ATKernelSymbols::HATABS+1,0xE400);
			Mem(ATKernelSymbols::BRKKEY,0xFF);
			ATHLECIOConnections c;
			c.mpGetVirtualScreenHandler=[&] { return screen.get(); };
			c.mpIsCassetteSIOPatchEnabled=[&] { return cassettePatch; };
			hook.reset(ATCreateHLECIOHook(&h.mCPU,std::move(c),&h.mMemory));
			hook->ReinitHooks(0xD5);
		}
		void Mem(uint16 a, uint8 v) { h.mMemory.WriteByte(a,v); }
		uint8 Mem(uint16 a) { return h.mMemory.ReadByte(a); }
		void Word(uint16 a, uint16 v) { Mem(a,uint8(v)); Mem(uint16(a+1),uint8(v>>8)); }
		uint16 Word(uint16 a) { return Mem(a)+256*Mem(uint16(a+1)); }
		IATDeviceCIOManager& Manager() { return *hook->AsDeviceCIOManager(); }
		Device& Add(uint8 name='H', uint32 mask=~UINT32_C(0)) {
			auto d=std::make_unique<Device>(); d->names.emplace_back(name,mask); auto& result=*d;
			devices.push_back(std::move(d)); result.InitCIO(&Manager()); return result;
		}
		uint8 Generic(uint8 function, uint8 channel=1) { h.mCPU.SetX(uint8(channel<<4)); return h.mHooks.OnHookHit(uint16((page<<8)+0x71+2*function)); }
		uint8 Poll() { return h.mHooks.OnHookHit(uint16((page<<8)+0x7F)); }
		uint8 Open(const std::string& path="H:TEST", uint8 channel=1, uint8 unit=1, uint8 a=12, uint8 b=0) {
			for (size_t i=0; i<path.size(); ++i) Mem(uint16(0x5000+i),uint8(path[i])); Mem(uint16(0x5000+path.size()),0x9B);
			Word(ATKernelSymbols::ICBALZ,0x5000); Mem(ATKernelSymbols::ICDNOZ,unit);
			Mem(ATKernelSymbols::ICAX1Z,a); Mem(ATKernelSymbols::ICAX2Z,b);
			return Generic(0,channel);
		}
		void Establish() { h.mCPU.SetX(0x10); h.mHooks.OnHookHit(ATKernelSymbols::CIOV); }
		int Find(uint8 name) { for (int i=0; i<36; i+=3) if (Mem(ATKernelSymbols::HATABS+i)==name) return i; return -1; }
		void ROMWord(uint16 vector, uint16 target) {
			auto& r=h.mUpperKernel; const uint32 i=vector-0xD800;
			r[i]=uint8(target-1); r[i+1]=uint8((target-1)>>8);
		}
		void ROMInit() { h.mHooks.CallInitHooks(h.mLowerKernel.data(),h.mUpperKernel.data()); }
		void Page(uint8 p) { page=p; hook->ReinitHooks(p); }
		MMUPortableFixture h;
		std::vector<std::unique_ptr<Device>> devices;
		std::unique_ptr<IATVirtualScreenHandler> screen;
		bool cassettePatch=false;
		uint8 page=0xD5;
		std::unique_ptr<IATHLECIOHook,void(*)(IATHLECIOHook *)> hook;
	};
}
#endif
