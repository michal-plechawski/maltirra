// Settings, NOTE/POINT, failed-open recovery, native errors and lifecycle/CPU.
#include <cstring>
#include "hostdeviceportablefixture.h"
using namespace HostDevicePortable;

bool ATTestAltirraHostSettings(ATPortableTestContext& context) {
	auto f=std::make_unique<Machine>(); ATDeviceInfo info {}; f->device->GetDeviceInfo(info);
	AT_PORTABLE_TEST_ASSERT(context, info.mpDef && !strcmp(info.mpDef->mpTag,"hostfs") && f->core->Find('H') >= 0 && f->core->Find('D') < 0);
	f->Setting("fakedisk",true); f->files.Write(L"FILE.TXT","x"); Bytes b;
	AT_PORTABLE_TEST_ASSERT(context, f->core->Find('D') >= 0 && f->Open("D:FILE.TXT") == 1 && f->Read(b,1) == 3 && b == Bytes{'x'} && f->Close() == 1);
	ATPropertySet saved; f->device->GetSettings(saved);
	AT_PORTABLE_TEST_ASSERT(context, !saved.GetBool("readonly",true) && saved.GetBool("fakedisk",false));
	f->device->SetSettings(ATPropertySet());
	AT_PORTABLE_TEST_ASSERT(context, f->host->IsReadOnly() && f->host->IsLongNameEncodingEnabled() && f->host->IsLowercaseNamingEnabled() && !*f->host->GetBasePath(0));
	f->device->SetSettings(saved); AT_PORTABLE_TEST_ASSERT(context, !f->host->IsReadOnly() && f->core->Find('D') >= 0);
	f->host->SetLongNameEncodingEnabled(false); AT_PORTABLE_TEST_ASSERT(context, !f->host->IsLongNameEncodingEnabled());
	const VDStringW base(f->host->GetBasePath(0)); f->host->SetBasePath(-1,L"bad"); f->host->SetBasePath(4,L"bad");
	AT_PORTABLE_TEST_ASSERT(context, VDStringW(f->host->GetBasePath(0)) == base && !*f->host->GetBasePath(-1) && !*f->host->GetBasePath(4));
	for (uint8 drive=2; drive<=4; ++drive) {
		f->host->SetBasePath(drive-1,f->files.path.c_str());
		AT_PORTABLE_TEST_ASSERT(context, f->Open(std::string("H")+char('0'+drive)+":FILE.TXT") == 1 && f->Close() == 1);
		AT_PORTABLE_TEST_ASSERT(context, f->Open(std::string("H")+char('5'+drive)+":FILE.TXT") == 1 && f->Close() == 1);
	}
	AT_PORTABLE_TEST_ASSERT(context, f->Open("H5:FILE.TXT") == kATCIOStat_FileNameErr);
	return true;
}

bool ATTestAltirraHostPositions(ATPortableTestContext& context) {
	auto f=std::make_unique<Machine>(); f->files.Write(L"FILE.BIN",std::string(300,'x')); Bytes b;
	AT_PORTABLE_TEST_ASSERT(context, f->Open("H:FILE.BIN") == 1 && f->Read(b,126) == 1 && b.size() == 126);
	constexpr uint16 aux=ATKernelSymbols::ICAX1+0x10;
	AT_PORTABLE_TEST_ASSERT(context, f->Special(0x26) == 1 && f->core->Mem(aux+2) == 1 && f->core->Mem(aux+3) == 0 && f->core->Mem(aux+4) == 1);
	AT_PORTABLE_TEST_ASSERT(context, f->Special(0x27) == 1 && f->core->Mem(aux+2) == 44 && f->core->Mem(aux+3) == 1 && !f->core->Mem(aux+4));
	f->core->Mem(aux+2,0); f->core->Mem(aux+3,0); f->core->Mem(aux+4,125);
	AT_PORTABLE_TEST_ASSERT(context, f->Special(0x25) == kATCIOStat_InvPoint);
	f->core->Mem(aux+2,3); f->core->Mem(aux+4,0);
	AT_PORTABLE_TEST_ASSERT(context, f->Special(0x25) == kATCIOStat_InvPoint && f->Close() == 1);
	for (const char *path : {"H:HOLE.BIN","H6:HOLE.TXT"}) {
		AT_PORTABLE_TEST_ASSERT(context, f->Open(path,8) == 1);
		f->core->Mem(aux+2,0); f->core->Mem(aux+3,0); f->core->Mem(aux+4,4);
		AT_PORTABLE_TEST_ASSERT(context, f->Special(0x25) == 1 && f->Write(Bytes{'Z'}) == 1 && f->Close() == 1);
	}
	AT_PORTABLE_TEST_ASSERT(context, f->files.Read(L"HOLE.BIN") == (Bytes{0,0,0,0,'Z'}) && f->files.Read(L"HOLE.TXT") == (Bytes{0,0,0,0,'Z'}));
	return true;
}

bool ATTestAltirraHostOpenFailures(ATPortableTestContext& context) {
	auto f=std::make_unique<Machine>();
	{ VDFile big(f->files.File(L"BIG.TXT").c_str(),nsVDFile::kWrite|nsVDFile::kCreateAlways); big.seek(0x1000000); big.write("X",1); }
	f->files.Write(L"KEEP.BIN","preserve");
	AT_PORTABLE_TEST_ASSERT(context, f->Open("H6:BIG.TXT",12) == kATCIOStat_FileNotFound);
	AT_PORTABLE_TEST_ASSERT(context, f->Open("H:KEEP.BIN",12) == 1 && f->Write(Bytes{'N','E','W'}) == 1 && f->Close() == 1);
	AT_PORTABLE_TEST_ASSERT(context, f->files.Read(L"KEEP.BIN") == (Bytes{'N','E','W','s','e','r','v','e'}));
	AT_PORTABLE_TEST_ASSERT(context, f->Open("H:ABSENT.TXT") == kATCIOStat_FileNotFound && f->Open("H:KEEP.BIN",3) == kATCIOStat_InvalidCmd);
	AT_PORTABLE_TEST_ASSERT(context, f->Open("H:KEEP.BIN") == 1 && f->Open("H:OTHER.BIN") == kATCIOStat_IOCBInUse && f->Close() == 1);
	AT_PORTABLE_TEST_ASSERT(context, f->Open("H:KEEP.BIN",12) == 1 && f->Open("H:KEEP.BIN",4,2) == kATCIOStat_FileLocked && f->Close() == 1);
	AT_PORTABLE_TEST_ASSERT(context, f->Open("H:KEEP.BIN",4,2) == 1 && f->Close(2) == 1);
	return true;
}

bool ATTestAltirraHostNativeErrors(ATPortableTestContext& context) {
	Sandbox files; uint32 code=0;
	try { VDFile missing(files.File(L"MISSING").c_str(),nsVDFile::kRead|nsVDFile::kOpenExisting); }
	catch (const VDNativeFileError& e) { code=e.GetNativeError(); }
	catch (const MyWin32Error& e) { code=e.GetWin32Error(); }
	AT_PORTABLE_TEST_ASSERT(context, code && ATTranslateHostErrorToSIOError(code) == kATCIOStat_FileNotFound);
	files.Directory(L"EXISTS"); code=0;
	try { VDCreateDirectory(files.File(L"EXISTS").c_str()); }
	catch (const VDNativeFileError& e) { code=e.GetNativeError(); }
	catch (const MyWin32Error& e) { code=e.GetWin32Error(); }
	AT_PORTABLE_TEST_ASSERT(context, code && ATTranslateHostErrorToSIOError(code) == kATCIOStat_FileExists);
	files.Write(L"EXISTS\\CHILD","x"); code=0;
	try { VDRemoveDirectory(files.File(L"EXISTS").c_str()); }
	catch (const VDNativeFileError& e) { code=e.GetNativeError(); }
	catch (const MyWin32Error& e) { code=e.GetWin32Error(); }
	AT_PORTABLE_TEST_ASSERT(context, code && ATTranslateHostErrorToSIOError(code) == kATCIOStat_DirNotEmpty);
	return true;
}

bool ATTestAltirraHostLifecycle(ATPortableTestContext& context) {
	auto f=std::make_unique<Machine>(); f->files.Directory(L"SUB"); f->files.Write(L"FILE.TXT","x");
	f->cio->InitCIO(&f->core->Manager()); uint32 count=0;
	for (uint16 i=0; i<36; i+=3) count+=f->core->Mem(ATKernelSymbols::HATABS+i)=='H';
	AT_PORTABLE_TEST_ASSERT(context, count == 1 && f->Open("H6:FILE.TXT",12) == 1 && f->Write(Bytes{'Y',0x9B}) == 1);
	AT_PORTABLE_TEST_ASSERT(context, f->Special(0x29,"H:SUB") == 1); f->device->WarmReset();
	uint8 b; uint32 n=99;
	AT_PORTABLE_TEST_ASSERT(context, f->Cwd().empty() && f->cio->OnCIOGetBytes(1,1,&b,1,n) == kATCIOStat_NotOpen && !n && f->files.Read(L"FILE.TXT") == (Bytes{'Y','\r','\n'}));
	f->Close(); AT_PORTABLE_TEST_ASSERT(context, f->Open("H:FILE.TXT",12) == 1); f->device->ColdReset();
	{ VDFile unlocked(f->files.File(L"FILE.TXT").c_str(),nsVDFile::kReadWrite|nsVDFile::kDenyAll|nsVDFile::kOpenExisting); }
	f->device->Shutdown(); AT_PORTABLE_TEST_ASSERT(context, !f->core->hook->HasCIODevice('H'));
	f->cio->InitCIO(&f->core->Manager()); f->core->Establish();
	f->Close(); AT_PORTABLE_TEST_ASSERT(context, f->Open("H:FILE.TXT",12) == 1); f->device->Shutdown();
	{ VDFile unlocked(f->files.File(L"FILE.TXT").c_str(),nsVDFile::kReadWrite|nsVDFile::kDenyAll|nsVDFile::kOpenExisting); }
	return true;
}

bool ATTestAltirraHostCPU(ATPortableTestContext& context) {
	auto f=std::make_unique<Machine>(); f->files.Write(L"CPU.BIN","ABC"); f->PrepareOpen("H:CPU.BIN");
	// OPEN, three GETs and EOF, CLOSE, then loop: every entry runs on the real CPU.
	const std::array<uint8,33> code {0x20,0x71,0xD5,
		0x20,0x75,0xD5,0x8D,0,0x40,0x20,0x75,0xD5,0x8D,1,0x40,
		0x20,0x75,0xD5,0x8D,2,0x40,0x20,0x75,0xD5,0x8C,3,0x40,
		0x20,0x73,0xD5,0x4C,30,0x20};
	for (uint16 i=0; i<code.size(); ++i) f->core->Mem(0x2000+i,code[i]);
	f->core->Mem(ATKernelSymbols::ICCOMZ,0); f->core->h.mCPU.SetS(0xFD); f->core->h.mCPU.SetPC(0x2000);
	AT_PORTABLE_TEST_ASSERT(context, f->core->h.RunCycles(1500) && f->core->Mem(0x4000)=='A' && f->core->Mem(0x4001)=='B' && f->core->Mem(0x4002)=='C' && f->core->Mem(0x4003)==kATCIOStat_EndOfFile && f->core->h.mCPU.GetS()==0xFD);
	VDFile unlocked(f->files.File(L"CPU.BIN").c_str(),nsVDFile::kReadWrite|nsVDFile::kDenyAll|nsVDFile::kOpenExisting);
	return true;
}
