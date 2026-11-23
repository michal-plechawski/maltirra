// Permissions, resource limits, lifecycle and actual serial/6502 execution.
#include "pclinkportablefixture.h"
using namespace PCLinkPortable;

bool ATTestAltirraPCLinkAttributes(ATPortableTestContext& context) {
	auto f=std::make_unique<Machine>(); f->files.Write(L"FILE.TXT","x"); auto p=Params(13,0,"FILE.TXT"); p[10]=1;
	AT_PORTABLE_TEST_ASSERT(context, f->Function(p)==1 && (VDFileGetAttributes(f->files.File(L"FILE.TXT").c_str()) & kVDFileAttr_ReadOnly));
	p[10]=0x14; AT_PORTABLE_TEST_ASSERT(context, f->Function(p)==1 && !(VDFileGetAttributes(f->files.File(L"FILE.TXT").c_str()) & kVDFileAttr_ReadOnly) && (VDFileGetAttributes(f->files.File(L"FILE.TXT").c_str()) & kVDFileAttr_Archive));
	for (auto attribute : {std::pair<uint8,uint32>{4,kVDFileAttr_Archive},{2,kVDFileAttr_Hidden}}) {
		p[10]=attribute.first; AT_PORTABLE_TEST_ASSERT(context, f->Function(p)==1 && (VDFileGetAttributes(f->files.File(L"FILE.TXT").c_str()) & attribute.second));
		uint8 d=f->Open("*.*",4,"",10,attribute.first);
		AT_PORTABLE_TEST_ASSERT(context, d==1 && f->Function(Params(6,d))==1 && f->Transfer(0x52,0x40,24)==1 && f->output[0]==1 && !memcmp(f->output.data()+7,"FILE    TXT",11) && f->Close(d)==1);
		p[10]=uint8(attribute.first<<4); AT_PORTABLE_TEST_ASSERT(context, f->Function(p)==1 && !(VDFileGetAttributes(f->files.File(L"FILE.TXT").c_str()) & attribute.second));
		d=f->Open("*.*",4,"",10,attribute.first);
		AT_PORTABLE_TEST_ASSERT(context, d==1 && f->Function(Params(6,d))==1 && f->Transfer(0x52,0x40,24)==1 && f->output[0]==kATCIOStat_EndOfFile && f->Close(d)==1);
	}
	const uint8 update=f->Open("FILE.TXT",12); AT_PORTABLE_TEST_ASSERT(context, update==1 && f->Write(update,Bytes{'Y'})==1 && f->Close(update)==1 && (VDFileGetAttributes(f->files.File(L"FILE.TXT").c_str()) & kVDFileAttr_Archive));
	const uint8 r=f->Open("FILE.TXT"); AT_PORTABLE_TEST_ASSERT(context, r==1 && f->Write(r,Bytes{'Z'})==kATCIOStat_ReadOnly && f->Close(r)==1);
	const uint8 w=f->Open("WRITE.TXT",8); Bytes b; AT_PORTABLE_TEST_ASSERT(context, w==1 && f->Read(w,1,b)==kATCIOStat_WriteOnly && f->Write(w,Bytes{'A'})==1);
	f->device->SetReadOnly(true); AT_PORTABLE_TEST_ASSERT(context, f->Write(w,Bytes{'B'})==kATCIOStat_ReadOnly && f->Close(w)==1 && f->files.Read(L"WRITE.TXT")==Bytes{'A'});
	for (uint8 fn : {11,12,13,14,15}) AT_PORTABLE_TEST_ASSERT(context, f->Function(Params(fn,0,"FILE.TXT"))==kATCIOStat_ReadOnly);
	AT_PORTABLE_TEST_ASSERT(context, !f->Open("FILE.TXT",12) && f->error==kATCIOStat_ReadOnly);
	return true;
}

bool ATTestAltirraPCLinkHandles(ATPortableTestContext& context) {
	auto f=std::make_unique<Machine>(); f->files.Write(L"FILE.TXT","x");
	for (uint8 h=1; h<=15; ++h) AT_PORTABLE_TEST_ASSERT(context, f->Open("FILE.TXT")==h);
	AT_PORTABLE_TEST_ASSERT(context, !f->Open("FILE.TXT") && f->error==kATCIOStat_TooManyFiles);
	AT_PORTABLE_TEST_ASSERT(context, f->Close(7)==1 && f->Open("FILE.TXT")==7);
	for (uint8 h : {uint8(0),uint8(16),uint8(255)}) AT_PORTABLE_TEST_ASSERT(context, f->Function(Params(4,h))==kATCIOStat_InvalidIOCB);
	AT_PORTABLE_TEST_ASSERT(context, f->Close(1)==1 && f->Function(Params(4,1))==kATCIOStat_NotOpen && f->Close(0)==1);
	AT_PORTABLE_TEST_ASSERT(context, f->Function(Params(8))==1 && f->Open("FILE.TXT",12)==1 && !f->Open("FILE.TXT") && f->error==kATCIOStat_FileLocked);
	return true;
}

bool ATTestAltirraPCLinkSettings(ATPortableTestContext& context) {
	auto f=std::make_unique<Machine>(); ATDeviceInfo info {}; f->device->GetDeviceInfo(info);
	AT_PORTABLE_TEST_ASSERT(context, info.mpDef && !strcmp(info.mpDef->mpTag,"pclink"));
	ATPropertySet p; f->device->GetSettings(p); AT_PORTABLE_TEST_ASSERT(context, p.GetBool("write",false));
	p.SetBool("set_timestamps",true); f->device->SetSettings(p);
	auto open=Params(9,0,"STAMP.TXT"); open[8]=8; const uint8 stamp[6] {14,3,24,12,34,56}; std::copy_n(stamp,6,open.begin()+2);
	AT_PORTABLE_TEST_ASSERT(context, f->Function(open)==1 && f->Transfer(0x52,0x40,24)==1); const uint8 h=f->output[0];
	AT_PORTABLE_TEST_ASSERT(context, f->Write(h,Bytes{'S'})==1 && f->Close(h)==1);
	{ VDFile file(f->files.File(L"STAMP.TXT").c_str(),nsVDFile::kRead|nsVDFile::kOpenExisting); ATPCLinkDirEnt e {}; e.SetDate(file.getLastWriteTime()); AT_PORTABLE_TEST_ASSERT(context, e.mDay==14 && e.mMonth==3 && e.mYear==24 && e.mHour==12); }
	struct Output final : ATConsoleOutput { std::string text; void WriteLine(const char *s) override { text+=s; text+='\n'; } } output;
	f->device->DumpStatus(output); AT_PORTABLE_TEST_ASSERT(context, output.text.find("Handle $0f: Not open")!=std::string::npos || output.text.find("Handle $0F: Not open")!=std::string::npos);
	f->device->SetSettings(ATPropertySet()); AT_PORTABLE_TEST_ASSERT(context, f->device->IsReadOnly() && !*f->device->GetBasePath());
	ATDeviceSIOCommand cmd {}; cmd.mDevice=0x6F; cmd.mCommand=0x53; cmd.mbStandardRate=true;
	AT_PORTABLE_TEST_ASSERT(context, f->device->OnSerialBeginCommand(cmd)==IATDeviceSIO::kCmdResponse_NotHandled);
	return true;
}

bool ATTestAltirraPCLinkLifecycle(ATPortableTestContext& context) {
	auto f=std::make_unique<Machine>(); f->files.Write(L"FILE.TXT","x"); f->files.Directory(L"DIR");
	f->device->InitSIO(&f->core->mManager);
	AT_PORTABLE_TEST_ASSERT(context, f->Open("FILE.TXT",12)==1 && f->Function(Params(16,0,"","DIR"))==1);
	f->device->ColdReset(); AT_PORTABLE_TEST_ASSERT(context, f->Cwd().empty() && f->Function(Params(4,1))==kATCIOStat_NotOpen);
	AT_PORTABLE_TEST_ASSERT(context, f->Open("FILE.TXT",12)==1); f->device->Shutdown();
	{ VDFile unlocked(f->files.File(L"FILE.TXT").c_str(),nsVDFile::kReadWrite|nsVDFile::kDenyAll|nsVDFile::kOpenExisting); }
	AT_PORTABLE_TEST_ASSERT(context, f->Transfer(0x53,0x40,4)==0);
	f->device->InitSIO(&f->core->mManager); AT_PORTABLE_TEST_ASSERT(context, f->Open("FILE.TXT",12)==1 && f->Close(1)==1);
	// Short parameter messages must not inherit the previous command's names/path.
	AT_PORTABLE_TEST_ASSERT(context, f->Open("FILE.TXT")==1 && f->Close(1)==1 && f->Function(Bytes{9})==kATCIOStat_FileNameErr);
	return true;
}

bool ATTestAltirraPCLinkSerial(ATPortableTestContext& context) {
	for (bool bad : {false,true}) {
		auto f=std::make_unique<Machine>(); f->Function(Params(8)); f->core->ReceiveClock(); auto p=Params(14,0,"SERIAL");
		f->core->Command(0x6F,0x50,uint8(p.size()));
		AT_PORTABLE_TEST_ASSERT(context, f->core->Collect(4000)==Bytes{'A'});
		for (uint8 c : p) f->core->mManager.PokeyWriteSIO(c,false,93,0,false,false);
		f->core->mManager.PokeyWriteSIO(ATComputeSIOChecksum(p.data(),p.size()) ^ (bad ? 1 : 0),false,93,0,false,false);
		auto reply=f->core->Collect(12000);
		AT_PORTABLE_TEST_ASSERT(context, VDDoesPathExist(f->files.File(L"SERIAL").c_str())==!bad && !reply.empty() && reply.back()==(bad ? 'N' : 'C'));
		f->core->Command(0x6F,0x53); reply=f->core->Collect(15000);
		AT_PORTABLE_TEST_ASSERT(context, reply.size()==7 && reply[0]=='A' && reply[1]=='C' && reply[6]==ATComputeSIOChecksum(reply.data()+2,4));
	}
	return true;
}

bool ATTestAltirraPCLinkCPU(ATPortableTestContext& context) {
	auto f=std::make_unique<Machine>(); f->files.Write(L"CPU.BIN","ABC"); const uint8 h=f->Open("CPU.BIN"); auto p=Params(0,h); Number(p,3);
	AT_PORTABLE_TEST_ASSERT(context, h==1 && f->Function(p)==1);
	f->core->mManager.SetSIOPatchEnabled(true);
	const std::array<uint8,12> dcb {0x6F,1,0x52,0x40,0,0x40,1,0,3,0,0,0};
	for (uint16 i=0; i<dcb.size(); ++i) f->core->Mem(ATKernelSymbols::DDEVIC+i,dcb[i]);
	const std::array<uint8,9> code {0x20,0x59,0xE4,0x8C,3,0x40,0x4C,6,0x20};
	for (uint16 i=0; i<code.size(); ++i) f->core->Mem(0x2000+i,code[i]);
	f->core->mCPU.SetPC(0x2000); f->core->mCPU.SetS(0xFD); f->core->mCPU.SetP(0x20);
	for (uint32 i=0; i<1000; ++i) { f->core->mCPU.GetUnusedCycle(); AT_PORTABLE_TEST_ASSERT(context, !f->core->mCPU.Advance()); f->core->Tick(1); }
	AT_PORTABLE_TEST_ASSERT(context, f->core->Mem(0x4000)=='A' && f->core->Mem(0x4001)=='B' && f->core->Mem(0x4002)=='C' && f->core->Mem(0x4003)==1 && f->core->mCPU.GetS()==0xFD && f->Value(3,h)==3);
	return true;
}
