// Real directory enumeration, drive CWDs, DOS mutations, LFN and Unicode aliases.
#include "hostdeviceportablefixture.h"
using namespace HostDevicePortable;

bool ATTestAltirraHostDirectory(ATPortableTestContext& context) {
	auto f=std::make_unique<Machine>(); f->files.Write(L"FILE.TXT",std::string(126,'x')); f->files.Write(L"NODOT",""); f->files.Directory(L"SUBDIR");
	AT_PORTABLE_TEST_ASSERT(context, f->Special(0x23,"H:FILE.TXT") == 1);
	for (uint8 mode : {6,7}) {
		AT_PORTABLE_TEST_ASSERT(context, f->Open("H:",mode) == 1); Bytes bytes;
		AT_PORTABLE_TEST_ASSERT(context, f->Read(bytes,1000) == 1 && bytes.size() == 3*18+17);
		bool file=false,dir=false,nodot=false;
		for (size_t i=0; i<54; i+=18) {
			std::string name(bytes.begin()+i+2,bytes.begin()+i+10);
			AT_PORTABLE_TEST_ASSERT(context, bytes[i+17] == 0x9B);
			if (name == "FILE    ") file=bytes[i]=='*' && bytes[i+14]=='0' && bytes[i+15]=='0' && bytes[i+16]=='2';
			if (name == "SUBDIR  ") dir=bytes[i+1]==':';
			if (name == "NODOT   ") nodot=true;
		}
		AT_PORTABLE_TEST_ASSERT(context, file && dir && nodot && std::string(bytes.begin()+54,bytes.end()-1) == "999 FREE SECTORS");
		AT_PORTABLE_TEST_ASSERT(context, f->Read(bytes,1) == kATCIOStat_EndOfFile && f->Close() == 1);
	}
	AT_PORTABLE_TEST_ASSERT(context, f->Open("H:*.TXT",6) == 1); Bytes filtered;
	AT_PORTABLE_TEST_ASSERT(context, f->Read(filtered,100) == 1 && filtered.size() == 35 && filtered[0]=='*');
	return true;
}

bool ATTestAltirraHostNavigation(ATPortableTestContext& context) {
	auto f=std::make_unique<Machine>(); f->files.Directory(L"FIRST"); f->files.Directory(L"FIRST\\NEST"); f->files.Directory(L"OTHER"); f->files.Write(L"FILE.TXT","x");
	f->host->SetBasePath(1,f->files.path.c_str());
	AT_PORTABLE_TEST_ASSERT(context, f->Special(0x29,"H:FIRST") == 1 && f->Cwd() == ">FIRST");
	AT_PORTABLE_TEST_ASSERT(context, f->Special(0x2C,"H:NEST") == 1 && f->Cwd() == ">FIRST>NEST");
	AT_PORTABLE_TEST_ASSERT(context, f->Special(0x29,"H2:OTHER") == 1 && f->Cwd("H2:") == ">OTHER" && f->Cwd("H1:") == ">FIRST>NEST");
	AT_PORTABLE_TEST_ASSERT(context, f->Cwd("H7:") == ">OTHER" && f->Cwd("H6:") == ">FIRST>NEST");
	AT_PORTABLE_TEST_ASSERT(context, f->Special(0x29,"H:<") == 1 && f->Cwd() == ">FIRST");
	AT_PORTABLE_TEST_ASSERT(context, f->Special(0x29,"H:..") == 1 && f->Cwd().empty());
	AT_PORTABLE_TEST_ASSERT(context, f->Special(0x29,"H:FILE.TXT") == kATCIOStat_PathNotFound && f->Cwd().empty());
	AT_PORTABLE_TEST_ASSERT(context, f->Special(0x29,"H:>FIRST\\NEST") == 1 && f->Special(0x29,"H:<<<") == 1 && f->Cwd().empty());
	AT_PORTABLE_TEST_ASSERT(context, f->Special(0x29,"H:>FIRST>NEST") == 1);
	// Applying an unchanged base path preserves CWD, including aliasing its storage.
	f->host->SetBasePath(0,f->host->GetBasePath(0)); AT_PORTABLE_TEST_ASSERT(context, f->Cwd() == ">FIRST>NEST");
	f->host->SetBasePath(0,f->files.File(L"OTHER").c_str()); AT_PORTABLE_TEST_ASSERT(context, f->Cwd().empty());
	return true;
}

bool ATTestAltirraHostMutation(ATPortableTestContext& context) {
	auto f=std::make_unique<Machine>(); f->files.Write(L"ONE.TXT","one"); f->files.Write(L"TWO.BIN","two");
	AT_PORTABLE_TEST_ASSERT(context, f->Special(0x20,"H:ONE.TXT,NEW.TXT") == 1 && f->files.Read(L"NEW.TXT") == (Bytes{'o','n','e'}));
	AT_PORTABLE_TEST_ASSERT(context, f->Special(0x20,"H:NEW.TXT,TWO.BIN") == kATCIOStat_FileExists);
	AT_PORTABLE_TEST_ASSERT(context, f->Special(0x23,"H:NEW.TXT") == 1 && (VDFileGetAttributes(f->files.File(L"NEW.TXT").c_str()) & kVDFileAttr_ReadOnly));
	AT_PORTABLE_TEST_ASSERT(context, f->Special(0x20,"H:NEW.TXT,NO.TXT") == kATCIOStat_FileLocked && f->Special(0x24,"H:NEW.TXT") == 1);
	AT_PORTABLE_TEST_ASSERT(context, f->Special(0x21,"H:*.TXT") == 1 && !VDDoesPathExist(f->files.File(L"NEW.TXT").c_str()));
	AT_PORTABLE_TEST_ASSERT(context, f->Special(0x21,"H:*.TXT") == kATCIOStat_FileNotFound);
	AT_PORTABLE_TEST_ASSERT(context, f->Special(0x22,"H:DIR") == 1 && f->Special(0x22,"H:DIR") == kATCIOStat_FileExists);
	f->files.Write(L"DIR\\CHILD","x"); AT_PORTABLE_TEST_ASSERT(context, f->Special(0x2B,"H:DIR") == kATCIOStat_DirNotEmpty);
	AT_PORTABLE_TEST_ASSERT(context, f->Special(0x21,"H:DIR>CHILD") == 1 && f->Special(0x2B,"H:DIR") == 1);
	AT_PORTABLE_TEST_ASSERT(context, f->Special(0x2A,"H:EMPTY") == 1 && f->Special(0x21,"H:EMPTY") == 1);
	AT_PORTABLE_TEST_ASSERT(context, f->Special(0x2B,"H:ABSENT") == kATCIOStat_PathNotFound && f->Special(0xFF) == kATCIOStat_NotSupported);
	return true;
}

bool ATTestAltirraHostLongNames(ATPortableTestContext& context) {
	auto f=std::make_unique<Machine>(); f->Setting("longfilenames",true); f->files.Directory(L"Mixed Folder"); f->files.Write(L"Mixed Folder\\Read Me.1.txt","payload"); Bytes b;
	AT_PORTABLE_TEST_ASSERT(context, f->Open("H:mixed folder>read me.1.TXT") == 1 && f->Read(b,100) == 3 && b == (Bytes{'p','a','y','l','o','a','d'}) && f->Close() == 1);
	AT_PORTABLE_TEST_ASSERT(context, f->Open("H:No Extension",8) == 1 && f->Write(Bytes{'N'}) == 1 && f->Close() == 1);
	AT_PORTABLE_TEST_ASSERT(context, f->Open("H:No Extension") == 1 && f->Read(b,100) == 3 && b == Bytes{'N'} && f->Close() == 1);
	AT_PORTABLE_TEST_ASSERT(context, f->Special(0x20,"H:No Extension,New Mixed Name.txt") == 1 && f->files.Read(L"New Mixed Name.txt") == Bytes{'N'});
	for (const char *bad : {"H:bad/name","H:bad:name","H:bad\"name","H:bad<name"}) AT_PORTABLE_TEST_ASSERT(context, f->Open(bad,8) == kATCIOStat_FileNameErr);
	AT_PORTABLE_TEST_ASSERT(context, f->Special(0x29,"H:mixed folder") == 1 && f->Cwd() == ">Mixed Folder");
	return true;
}

bool ATTestAltirraHostAliases(ATPortableTestContext& context) {
	auto f=std::make_unique<Machine>(); const wchar_t *native=L"Za\u017C\u00F3\u0142\u0107-\U0001F3AE.txt";
	f->files.Write(native,"unicode"); VDStringA alias; ATHostDeviceEncodeName(alias,native,true,false); Bytes b;
	AT_PORTABLE_TEST_ASSERT(context, f->Open(std::string("H:")+alias.c_str()) == 1 && f->Read(b,100) == 3 && b == (Bytes{'u','n','i','c','o','d','e'}) && f->Close() == 1);
	f->files.Directory(L"Long directory name"); f->files.Write(L"Long directory name\\DATA.BIN","nested"); VDStringA dir;
	ATHostDeviceEncodeName(dir,L"Long directory name",true,false);
	// 8.3 directory aliases include an empty extension; Atari syntax omits its dot.
	std::string component(dir.c_str()); if (!component.empty() && component.back()=='.') component.pop_back();
	AT_PORTABLE_TEST_ASSERT(context, f->Open(std::string("H:")+component+">DATA.BIN") == 1 && f->Read(b,100) == 3 && b == (Bytes{'n','e','s','t','e','d'}) && f->Close() == 1);
	AT_PORTABLE_TEST_ASSERT(context, f->Open("H:CON.TXT",8) == 1 && f->Write(Bytes{'C'}) == 1 && f->Close() == 1 && f->files.Read(L"!CON.TXT") == Bytes{'C'});
	AT_PORTABLE_TEST_ASSERT(context, f->Open("H:CON.TXT") == 1 && f->Read(b,1) == 3 && b == Bytes{'C'} && f->Close() == 1);
	f->host->SetLowercaseNamingEnabled(true);
	AT_PORTABLE_TEST_ASSERT(context, f->Open("H:LOWER.TXT",8) == 1 && f->Close() == 1 && VDDoesPathExist(f->files.File(L"lower.txt").c_str()));
	return true;
}
