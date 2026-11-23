// File service exercised through actual PCLink PUT/STATUS/READ SIO exchanges.
#include "pclinkportablefixture.h"
using namespace PCLinkPortable;

bool ATTestAltirraPCLinkProtocol(ATPortableTestContext& context) {
	auto f=std::make_unique<Machine>();
	AT_PORTABLE_TEST_ASSERT(context, f->Function(Params(8))==1 && f->status==(std::array<uint8,4>{0,1,0,0x6F}));
	AT_PORTABLE_TEST_ASSERT(context, f->Transfer(0x3F,0x40,1)==1 && f->output==Bytes{9});
	AT_PORTABLE_TEST_ASSERT(context, f->Transfer(0xBF,0x40,1)==1 && f->output==Bytes{9});
	AT_PORTABLE_TEST_ASSERT(context, f->Transfer(0x77,0x40,1)==kATCIOStat_NAK);
	for (uint8 fn : {5,18,20,255}) AT_PORTABLE_TEST_ASSERT(context, f->Function(Params(fn))==kATCIOStat_NotSupported);
	AT_PORTABLE_TEST_ASSERT(context, f->Function(Params(19))==1 && f->Transfer(0x52,0x40,64)==1 && f->output[0]==0x21 && !memcmp(f->output.data()+14,"PCLink  ",8));
	ATDeviceSIORequest invalid {}; invalid.mDevice=0x6F; invalid.mCommand=0x53; invalid.mMode=0x80; invalid.mLength=4;
	AT_PORTABLE_TEST_ASSERT(context, f->device->OnSerialAccelCommand(invalid)==IATDeviceSIO::kCmdResponse_BypassAccel);
	ATDeviceSIOCommand rate {}; rate.mDevice=0x6F; rate.mCommand=0x53; rate.mCyclesPerBit=29;
	AT_PORTABLE_TEST_ASSERT(context, f->device->OnSerialBeginCommand(rate)==IATDeviceSIO::kCmdResponse_NotHandled);
	return true;
}

bool ATTestAltirraPCLinkTransfer(ATPortableTestContext& context) {
	auto f=std::make_unique<Machine>(); Bytes expected(1001); for (size_t i=0; i<expected.size(); ++i) expected[i]=uint8(i*17);
	const uint8 w=f->Open("DATA.BIN",8);
	AT_PORTABLE_TEST_ASSERT(context, w==1 && (f->output[1] & ATPCLinkDirEnt::kFlag_InUse) && !memcmp(f->output.data()+7,"DATA    BIN",11) && !f->output[4] && !f->output[5] && !f->output[6]);
	AT_PORTABLE_TEST_ASSERT(context, f->Write(w,expected)==1 && f->Value(3,w)==1001 && f->Value(4,w)==1001 && f->Close(w)==1);
	AT_PORTABLE_TEST_ASSERT(context, f->files.Read(L"DATA.BIN")==expected);
	const uint8 h=f->Open("DATA.BIN"); Bytes a,b;
	AT_PORTABLE_TEST_ASSERT(context, h==1 && f->Read(h,257,a)==1 && a==Bytes(expected.begin(),expected.begin()+257));
	AT_PORTABLE_TEST_ASSERT(context, f->Read(h,1000,b)==1 && b==Bytes(expected.begin()+257,expected.end()));
	AT_PORTABLE_TEST_ASSERT(context, f->Read(h,1,b)==kATCIOStat_EndOfFile && b.empty() && f->status[2]==0 && f->status[3]==0 && f->Close(h)==1);
	const uint8 u=f->Open("DATA.BIN",12); auto p=Params(2,u); Number(p,256);
	AT_PORTABLE_TEST_ASSERT(context, f->Function(p)==1 && f->Write(u,Bytes{'X','Y'})==1 && f->Close(u)==1);
	expected[256]='X'; expected[257]='Y'; AT_PORTABLE_TEST_ASSERT(context, f->files.Read(L"DATA.BIN")==expected);
	const uint8 truncated=f->Open("DATA.BIN",8);
	AT_PORTABLE_TEST_ASSERT(context, truncated==1 && !f->output[4] && !f->output[5] && !f->output[6] && f->Value(4,truncated)==0 && f->Close(truncated)==1 && f->files.Read(L"DATA.BIN").empty());
	return true;
}

bool ATTestAltirraPCLinkAppend(ATPortableTestContext& context) {
	auto f=std::make_unique<Machine>(); f->files.Write(L"APPEND","ABC"); const uint8 h=f->Open("APPEND",9);
	AT_PORTABLE_TEST_ASSERT(context, h==1 && f->Value(3,h)==3 && f->Write(h,Bytes{'D','E'})==1 && f->Value(3,h)==5 && f->Value(4,h)==5);
	AT_PORTABLE_TEST_ASSERT(context, f->Close(h)==1 && f->files.Read(L"APPEND")== (Bytes{'A','B','C','D','E'}));
	const uint8 r=f->Open("APPEND"); auto p=Params(2,r); Number(p,6);
	AT_PORTABLE_TEST_ASSERT(context, f->Function(p)==kATCIOStat_PointDLen && f->Value(3,r)==0 && f->Close(r)==1);
	const uint8 w=f->Open("HOLE",8); p=Params(2,w); Number(p,4);
	AT_PORTABLE_TEST_ASSERT(context, f->Function(p)==1 && f->Write(w,Bytes{'Z'})==1 && f->Close(w)==1 && f->files.Read(L"HOLE")== (Bytes{0,0,0,0,'Z'}));
	return true;
}

bool ATTestAltirraPCLinkDirectory(ATPortableTestContext& context) {
	auto f=std::make_unique<Machine>(); f->files.Directory(L"DIR.EXT"); f->files.Write(L"A.TXT","A"); f->files.Write(L"B.BIN","B"); f->files.Write(L"Long file name.txt","hidden from 8.3");
	const uint8 h=f->Open("*.TXT",4,"",10); AT_PORTABLE_TEST_ASSERT(context, h==1 && f->output.size()==24 && f->output[1]==0x28 && !memcmp(f->output.data()+7,"MAIN    ",8));
	AT_PORTABLE_TEST_ASSERT(context, f->Function(Params(6,h))==1 && f->Transfer(0x52,0x40,24)==1 && f->output[0]==1 && !memcmp(f->output.data()+7,"A       TXT",11));
	AT_PORTABLE_TEST_ASSERT(context, f->Function(Params(6,h))==1 && f->Transfer(0x52,0x40,24)==1 && f->output[0]==kATCIOStat_EndOfFile);
	auto p=Params(2,h); Number(p,0); AT_PORTABLE_TEST_ASSERT(context, f->Function(p)==1); Bytes stream;
	AT_PORTABLE_TEST_ASSERT(context, f->Read(h,1000,stream)==1 && stream.size()==92 && !memcmp(stream.data()+23+6,"DIR     EXT",11) && f->Close(h)==1);
	const uint8 d=f->Open("*.*",0x14,"DIR.EXT");
	AT_PORTABLE_TEST_ASSERT(context, d==1 && !memcmp(f->output.data()+7,"DIR     EXT",11) && f->output[2]==1 && f->Close(d)==1);
	return true;
}

bool ATTestAltirraPCLinkNavigation(ATPortableTestContext& context) {
	auto f=std::make_unique<Machine>(); f->files.Directory(L"Mixed"); f->files.Directory(L"Mixed\\Child"); f->files.Write(L"Mixed\\Child\\data.bin","native"); f->files.Directory(L"!CON");
	AT_PORTABLE_TEST_ASSERT(context, f->Function(Params(16,0,"","mixed>child"))==1 && f->Cwd()=="\\MIXED\\CHILD");
	const uint8 h=f->Open("DATA.BIN"); Bytes bytes;
	AT_PORTABLE_TEST_ASSERT(context, h==1 && f->Read(h,20,bytes)==1 && bytes== (Bytes{'n','a','t','i','v','e'}) && f->Close(h)==1);
	AT_PORTABLE_TEST_ASSERT(context, f->Function(Params(16,0,"","<"))==1 && f->Cwd()=="\\MIXED");
	AT_PORTABLE_TEST_ASSERT(context, f->Function(Params(16,0,"",".."))==1 && f->Cwd().empty());
	AT_PORTABLE_TEST_ASSERT(context, f->Function(Params(16,0,"",".."))==kATCIOStat_PathNotFound && f->Cwd().empty());
	AT_PORTABLE_TEST_ASSERT(context, f->Function(Params(16,0,"","CON"))==1 && f->Cwd()=="\\CON");
	f->device->SetBasePath(f->device->GetBasePath()); AT_PORTABLE_TEST_ASSERT(context, f->Cwd()=="\\CON");
	f->device->SetBasePath(f->files.File(L"Mixed").c_str()); AT_PORTABLE_TEST_ASSERT(context, f->Cwd().empty());
	return true;
}

bool ATTestAltirraPCLinkMutation(ATPortableTestContext& context) {
	auto f=std::make_unique<Machine>(); f->files.Write(L"Mixed.txt","payload");
	auto p=Params(11,0,"MIXED.TXT"); Name(p,23,"*.BAK");
	AT_PORTABLE_TEST_ASSERT(context, f->Function(p)==1 && f->files.Read(L"MIXED.BAK")== (Bytes{'p','a','y','l','o','a','d'}));
	f->files.Write(L"lower.txt","x"); AT_PORTABLE_TEST_ASSERT(context, f->Function(Params(12,0,"LOWER.TXT"))==1 && !VDDoesPathExist(f->files.File(L"lower.txt").c_str()));
	AT_PORTABLE_TEST_ASSERT(context, f->Function(Params(12,0,"*.BAK"))==1 && f->Function(Params(12,0,"*.BAK"))==kATCIOStat_FileNotFound);
	AT_PORTABLE_TEST_ASSERT(context, f->Function(Params(14,0,"SUB"))==1 && f->Function(Params(14,0,"SUB"))==kATCIOStat_FileExists);
	f->files.Write(L"SUB\\CHILD","x"); AT_PORTABLE_TEST_ASSERT(context, f->Function(Params(15,0,"SUB"))==kATCIOStat_DirNotEmpty);
	AT_PORTABLE_TEST_ASSERT(context, f->Function(Params(12,0,"CHILD","sub"))==1 && f->Function(Params(15,0,"sub"))==1);
	AT_PORTABLE_TEST_ASSERT(context, f->Function(Params(14,0,"*"))==kATCIOStat_IllegalWild && f->Function(Params(15,0,"MISSING"))==kATCIOStat_PathNotFound);
	return true;
}
