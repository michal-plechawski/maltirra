// Native sandbox and production CPU/POKEY/SIO backing the PCLink wire protocol.
#ifndef f_ATTEST_PCLINKPORTABLEFIXTURE_H
#define f_ATTEST_PCLINKPORTABLEFIXTURE_H
#include <at/atcore/consoleoutput.h>
#include <pclink.h>
#include "hostdeviceportablefixture.h"
#include "siomanagerportablefixture.h"
namespace PCLinkPortable {
	using HostDevicePortable::Bytes;
	using HostDevicePortable::Sandbox;
	inline void Name(Bytes& p,size_t offset,const std::string& name) {
		std::fill(p.begin()+offset,p.begin()+offset+11,' ');
		auto dot=name.find('.'); auto base=dot==std::string::npos ? name : name.substr(0,dot);
		std::copy_n(base.begin(),std::min<size_t>(8,base.size()),p.begin()+offset);
		if (dot!=std::string::npos) { auto ext=name.substr(dot+1); std::copy_n(ext.begin(),std::min<size_t>(3,ext.size()),p.begin()+offset+8); }
	}
	inline Bytes Params(uint8 fn,uint8 handle=0,const std::string& name="",const std::string& path="") {
		Bytes p(100,0); p[0]=fn; p[1]=handle; Name(p,11,name); Name(p,23,"");
		std::copy_n(path.begin(),std::min<size_t>(64,path.size()),p.begin()+35); return p;
	}
	inline void Number(Bytes& p,uint32 value) { p[2]=uint8(value); p[3]=uint8(value>>8); p[4]=uint8(value>>16); }
	inline ATPCLinkFileName Filename(const wchar_t *s) { ATPCLinkFileName n {}; n.ParseFromNative(s); return n; }
	struct Machine {
		Machine() : core(std::make_unique<SIOPortableFixture>()), device(new ATPCLinkDevice) {
			device->SetBasePath(files.path.c_str()); device->SetReadOnly(false); device->InitSIO(&core->mManager);
			core->mManager.SetOtherSIOAccelEnabled(true);
		}
		~Machine() { device->Shutdown(); }
		uint8 Transfer(uint8 command,uint8 mode,uint16 length,uint8 aux=0,const Bytes& input={}) {
			for (size_t i=0; i<input.size(); ++i) core->Mem(uint16(0x4000+i),input[i]);
			auto req=core->Request(0x6F,mode,length); req.mCommand=command; req.mAUX[0]=aux; req.mAUX[1]=0;
			core->mCPU.SetP(0x20);
			if (!core->mManager.TryAccelRequest(req,false)) return 0;
			output.clear(); if (mode==0x40) for (uint16 i=0; i<length; ++i) output.push_back(core->Mem(0x4000+i));
			return core->Mem(ATKernelSymbols::DSTATS);
		}
		std::array<uint8,4> Status() {
			if (Transfer(0x53,0x40,4)!=1) return {0,0xFF,0,0};
			std::array<uint8,4> s; std::copy_n(output.begin(),4,s.begin()); return s;
		}
		uint8 Function(const Bytes& params) {
			if (Transfer(0x50,0x80,uint16(params.size()),uint8(params.size()),params)!=1) return error=0;
			status=Status(); return error=status[1];
		}
		uint8 Open(const std::string& name,uint8 mode=4,const std::string& path="",uint8 function=9,uint8 filter=0) {
			auto p=Params(function,0,name,path); p[8]=mode; p[9]=filter;
			if (Function(p)!=1 || Transfer(0x52,0x40,24)!=1) return 0;
			return output[0];
		}
		uint8 Close(uint8 h) { return Function(Params(7,h)); }
		uint8 Read(uint8 h,uint16 length,Bytes& bytes) {
			bytes.clear(); auto p=Params(0,h); Number(p,length); uint8 s=Function(p); if (s!=1) return s;
			const uint16 n=uint16(status[2]+256u*status[3]);
			if (Transfer(0x52,0x40,n)!=1) return 0;
			bytes=output; return Status()[1];
		}
		uint8 Write(uint8 h,const Bytes& bytes) {
			auto p=Params(1,h); Number(p,uint32(bytes.size())); uint8 s=Function(p); if (s!=1) return s;
			if (Transfer(0x52,0x80,uint16(bytes.size()),0,bytes)!=1) return 0;
			return Status()[1];
		}
		uint32 Value(uint8 fn,uint8 h) {
			if (Function(Params(fn,h))!=1 || Transfer(0x52,0x40,3)!=1) return 0xFFFFFFFF;
			return output[0]+(uint32(output[1])<<8)+(uint32(output[2])<<16);
		}
		std::string Cwd() {
			if (Function(Params(17))!=1 || Transfer(0x52,0x40,64)!=1) return "<error>";
			return std::string(output.begin(),std::find(output.begin(),output.end(),0));
		}
		Sandbox files;
		std::unique_ptr<SIOPortableFixture> core;
		vdrefptr<ATPCLinkDevice> device;
		Bytes output;
		std::array<uint8,4> status {};
		uint8 error=0;
	};
}
#endif
