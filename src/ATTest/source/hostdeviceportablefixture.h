// A real H: device, production CIO/CPU, and an isolated native directory tree.
#ifndef f_ATTEST_HOSTDEVICEPORTABLEFIXTURE_H
#define f_ATTEST_HOSTDEVICEPORTABLEFIXTURE_H
#include <algorithm>
#include <array>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>
#include <vd2/system/error.h>
#include <vd2/system/file.h>
#include <vd2/system/filesys.h>
#include <vd2/system/process.h>
#include <vd2/system/refcount.h>
#include <vd2/system/time.h>
#include <vd2/system/VDString.h>
#include <at/atcore/device.h>
#include <at/atcore/devicecio.h>
#include <at/atcore/propertyset.h>
#include <hostdevice.h>
#include <hostdeviceutils.h>
#include <pclinkerror.h>
#include "cioportablefixture.h"

namespace HostDevicePortable {
	using Bytes=std::vector<uint8>;
	inline VDStringW Native(const wchar_t *s) {
		VDStringW path(s); std::replace(path.begin(),path.end(),L'\\',(wchar_t)std::filesystem::path::preferred_separator); return path;
	}
	struct Sandbox {
		Sandbox() {
			static uint32 seq=0; VDStringW name;
			name.sprintf(L"altirra-host-%u-%llu-%u",(unsigned)VDGetCurrentProcessId(),(unsigned long long)VDGetCurrentTick64(),++seq);
			path=VDGetFullPath(name.c_str()); VDCreateDirectory(path.c_str());
		}
		static void RemoveTree(const VDStringW& root) {
			{
				VDDirectoryIterator it(VDMakePath(root.c_str(),L"*").c_str());
				while (it.Next()) {
					auto p=it.GetFullPath();
					if (it.IsDirectory()) RemoveTree(p);
					else { VDFileSetAttributes(p.c_str(),kVDFileAttr_ReadOnly,0); VDRemoveFileEx(p.c_str()); }
				}
			}
			VDRemoveDirectory(root.c_str());
		}
		~Sandbox() { try { RemoveTree(path); } catch (...) {} }
		VDStringW File(const wchar_t *name) const { return VDMakePath(path.c_str(),Native(name).c_str()); }
		void Directory(const wchar_t *name) { VDCreateDirectory(File(name).c_str()); }
		void Write(const wchar_t *name,const Bytes& bytes) {
			VDFile f(File(name).c_str(),nsVDFile::kWrite|nsVDFile::kCreateAlways); if (!bytes.empty()) f.write(bytes.data(),uint32(bytes.size()));
		}
		void Write(const wchar_t *name,const std::string& bytes) { Write(name,Bytes(bytes.begin(),bytes.end())); }
		Bytes Read(const wchar_t *name) const {
			VDFile f(File(name).c_str(),nsVDFile::kRead|nsVDFile::kOpenExisting); Bytes bytes(size_t(f.size()));
			if (!bytes.empty()) f.read(bytes.data(),uint32(bytes.size()));
			return bytes;
		}
		VDStringW path;
	};
	struct Machine {
		Machine() : core(std::make_unique<CIOPortable::Machine>()) {
			ATCreateDeviceHostFS(ATPropertySet(),~device);
			host=vdpoly_cast<IATHostDeviceEmulator *>(device.get()); cio=vdpoly_cast<IATDeviceCIO *>(device.get());
			ATPropertySet p; p.SetBool("readonly",false); p.SetBool("lowercase",false); p.SetBool("encodelfn",true); p.SetString("path1",files.path.c_str()); device->SetSettings(p);
			cio->InitCIO(&core->Manager()); core->Establish();
		}
		~Machine() { device->Shutdown(); }
		void Setting(const char *name,bool value) { ATPropertySet p; device->GetSettings(p); p.SetBool(name,value); device->SetSettings(p); }
		void Filename(const std::string& s,uint16 at=0x5000) { for (size_t i=0; i<s.size(); ++i) core->Mem(uint16(at+i),uint8(s[i])); core->Mem(uint16(at+s.size()),0x9B); }
		void PrepareOpen(const std::string& s,uint8 mode=4,uint8 channel=1) {
			Filename(s); core->Word(ATKernelSymbols::ICBALZ,0x5000); core->Mem(ATKernelSymbols::ICAX1Z,mode); core->Mem(ATKernelSymbols::ICAX2Z,0);
			const uint8 unit=s.size()>1 && s[1]>='1' && s[1]<='9' ? uint8(s[1]-'0') : 1;
			core->Mem(ATKernelSymbols::ICDNOZ,unit); core->h.mCPU.SetX(uint8(channel<<4));
		}
		uint8 Open(const std::string& s,uint8 mode=4,uint8 channel=1) { PrepareOpen(s,mode,channel); return core->Generic(0,channel)==0x60 ? core->h.mCPU.GetY() : 0xFF; }
		uint8 Close(uint8 channel=1) { return core->Generic(1,channel)==0x60 ? core->h.mCPU.GetY() : 0xFF; }
		uint8 Read(Bytes& result,uint16 len,uint8 channel=1) {
			result.clear(); core->Word(ATKernelSymbols::ICBALZ,0x6000); core->Word(ATKernelSymbols::ICBLLZ,len); core->Mem(ATKernelSymbols::ICCOMZ,kATCIOCmd_GetChars);
			core->h.mCPU.SetS(0xFD); core->Mem(0x1FF,0xC0); const uint8 op=core->Generic(2,channel); const uint8 status=core->h.mCPU.GetY();
			if (op!=0x60) return 0xFF;
			if (status<0x80) {
				const uint16 actual=uint16(core->Word(ATKernelSymbols::ICBALZ)-0x6000+1);
				if (actual>1024) return 0xFF;
				for (uint16 i=0; i+1<actual; ++i) result.push_back(core->Mem(0x6000+i));
				result.push_back(core->h.mCPU.GetA());
			}
			return status;
		}
		uint8 Write(const Bytes& bytes,uint8 channel=1) {
			if (bytes.empty()) return 0xFF;
			core->h.mCPU.SetA(bytes[0]); for (size_t i=1; i<bytes.size(); ++i) core->Mem(uint16(0x6000+i),bytes[i]);
			core->Word(ATKernelSymbols::ICBALZ,0x6000); core->Word(ATKernelSymbols::ICBLLZ,uint16(bytes.size())); core->Mem(ATKernelSymbols::ICCOMZ,kATCIOCmd_PutChars);
			core->h.mCPU.SetS(0xFD); core->Mem(0x1FF,0xC0); return core->Generic(3,channel)==0x60 ? core->h.mCPU.GetY() : 0xFF;
		}
		uint8 Special(uint8 command,const std::string& s="H:",uint8 channel=1,uint16 dest=0) {
			Filename(s); core->Word(ATKernelSymbols::ICBALZ,0x5000); core->Word(ATKernelSymbols::ICBLLZ,dest); core->Mem(ATKernelSymbols::ICCOMZ,command);
			core->Mem(ATKernelSymbols::ICDNOZ,s.size()>1 && s[1]>='1' && s[1]<='9' ? uint8(s[1]-'0') : 1); core->Mem(ATKernelSymbols::ICHIDZ,uint8(core->Find(s.empty() ? 'H' : uint8(s[0]))));
			return core->Generic(5,channel)==0x60 ? core->h.mCPU.GetY() : 0xFF;
		}
		std::string Cwd(const std::string& selector="H:") {
			if (Special(0x30,selector,1,0x7000)!=1) return "<error>";
			std::string out; for (uint16 i=0; i<256; ++i) { auto c=core->Mem(0x7000+i); if (!c) return out; out.push_back(char(c)); } return "<unterminated>";
		}
		Sandbox files;
		std::unique_ptr<CIOPortable::Machine> core;
		vdrefptr<IATDevice> device;
		IATHostDeviceEmulator *host=nullptr;
		IATDeviceCIO *cio=nullptr;
	};
}
#endif
