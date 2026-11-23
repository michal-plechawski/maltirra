// Native-independent PCLink names/paths plus real file and directory handles.
#include "pclinkportablefixture.h"
using namespace PCLinkPortable;

bool ATTestAltirraPCLinkNames(ATPortableTestContext& context) {
	for (auto s : {L"FILE.TXT",L"noext",L"!CON.TXT",L"$LPT1"}) {
		ATPCLinkFileName a; AT_PORTABLE_TEST_ASSERT(context, a.ParseFromNative(s));
		VDStringW native; a.AppendNative(native); ATPCLinkFileName b;
		AT_PORTABLE_TEST_ASSERT(context, b.ParseFromNative(native.c_str()) && a==b && !a.IsWild());
	}
	for (auto s : {L"",L"!",L"CON",L"LPT1.TXT",L"Long name.txt",L"FILE.LONG",L"A..B",L"A/B",L"ABCDEFGHI",L"!OTHER"}) {
		ATPCLinkFileName a; AT_PORTABLE_TEST_ASSERT(context, !a.ParseFromNative(s));
	}
	auto p=Params(0,0,"F*.T*"); ATPCLinkFileName pattern;
	AT_PORTABLE_TEST_ASSERT(context, pattern.ParseFromNet(p.data()+11) && pattern.IsWild() && pattern.WildMatch(Filename(L"FILE.TXT")) && !pattern.WildMatch(Filename(L"OTHER.TXT")));
	ATPCLinkFileName merged=Filename(L"FILE.TXT"); Name(p,11,"N???.BIN"); ATPCLinkFileName dest;
	AT_PORTABLE_TEST_ASSERT(context, dest.ParseFromNet(p.data()+11)); merged.WildMerge(dest);
	AT_PORTABLE_TEST_ASSERT(context, merged==Filename(L"NILE.BIN"));
	p[11]='/'; AT_PORTABLE_TEST_ASSERT(context, !pattern.ParseFromNet(p.data()+11));
	return true;
}

bool ATTestAltirraPCLinkPaths(ATPortableTestContext& context) {
	struct Case { const char *cwd,*input,*expected; };
	for (auto c : {Case{"","",""},{"","FOO","\\FOO"},{"","FOO.BAR","\\FOO.BAR"},{"","FOO.","\\FOO"},{"",".FOO","\\.FOO"},
		{"","FOOBARBA.Z","\\FOOBARBA.Z"},{"","foo","\\FOO"},{"","FOO\\BAR","\\FOO\\BAR"},{"","FOO>BAR","\\FOO\\BAR"},
		{"\\FOO","BAR","\\FOO\\BAR"},{"\\FOO","\\BAR","\\BAR"},{"\\FOO","\\BAR\\","\\BAR"},{"\\FOO","<",""},
		{"\\FOO","<BAR","\\BAR"},{"\\FOO\\BAR","<BAZ","\\FOO\\BAZ"},{"\\FOO\\BAR","..\\<BAZ","\\BAZ"},
		{"\\FOO\\BAR","<BAZ>","\\FOO\\BAZ"},{"\\FOO","..",""},{"\\FOO","..\\",""}}) {
		VDStringA out; AT_PORTABLE_TEST_ASSERT(context, ATPCLinkDevice::ResolvePathStatic(c.input,c.cwd,out)==1 && out==c.expected);
	}
	for (auto s : {".FOOF","FOOBARBAZ","FOOBARBAZ.FOO",".","FOO\\\\BAR","A/B","A:B","A*","A?","A..B"}) {
		VDStringA out; AT_PORTABLE_TEST_ASSERT(context, ATPCLinkDevice::ResolvePathStatic(s,"",out)==kATCIOStat_FileNameErr);
	}
	for (auto s : {"..","<","..>FOO"}) { VDStringA out; AT_PORTABLE_TEST_ASSERT(context, ATPCLinkDevice::ResolvePathStatic(s,"",out)==kATCIOStat_PathNotFound); }
	return true;
}

bool ATTestAltirraPCLinkDirEntry(ATPortableTestContext& context) {
	static_assert(sizeof(ATPCLinkDirEnt)==23);
	for (auto c : {std::pair<uint32,uint8>{kVDFileAttr_Hidden,ATPCLinkDirEnt::kFlag_Hidden},
		{kVDFileAttr_ReadOnly,ATPCLinkDirEnt::kFlag_Locked},{kVDFileAttr_Archive,ATPCLinkDirEnt::kFlag_Archive},{kVDFileAttr_Directory,ATPCLinkDirEnt::kFlag_Directory}}) {
		ATPCLinkDirEnt e {}; e.SetFlagsFromAttributes(c.first);
		AT_PORTABLE_TEST_ASSERT(context, e.mFlags==(ATPCLinkDirEnt::kFlag_InUse|c.second));
	}
	for (auto pair : {std::pair<uint8,uint8>{1,0x10},{2,0x20},{4,0x40},{8,0x80}}) {
		ATPCLinkDirEnt e {}; const uint8 flag=pair.first==8 ? 0x20 : pair.first; e.mFlags=flag;
		AT_PORTABLE_TEST_ASSERT(context, e.TestAttrFilter(pair.first) && !e.TestAttrFilter(pair.second));
		e.mFlags=0; AT_PORTABLE_TEST_ASSERT(context, !e.TestAttrFilter(pair.first) && e.TestAttrFilter(pair.second));
	}
	for (uint8 year : {uint8(24),uint8(49),uint8(50),uint8(99)}) {
		const uint8 stamp[6] {14,3,year,12,34,56}; ATPCLinkDirEnt e {}; e.SetDate(ATPCLinkDirEnt::DecodeDate(stamp));
		AT_PORTABLE_TEST_ASSERT(context, e.mDay==14 && e.mMonth==3 && e.mYear==year && e.mHour==12 && e.mMin==34 && e.mSec==56);
	}
	return true;
}

bool ATTestAltirraPCLinkFile(ATPortableTestContext& context) {
	Sandbox files; ATPCLinkFileHandle h; uint32 n=99; std::array<uint8,8> b {};
	AT_PORTABLE_TEST_ASSERT(context, h.Read(b.data(),1,n)==kATCIOStat_NotOpen && !n && h.Seek(0)==kATCIOStat_NotOpen);
	AT_PORTABLE_TEST_ASSERT(context, h.OpenFile(files.File(L"FILE.BIN").c_str(),nsVDFile::kWrite|nsVDFile::kCreateAlways,false,true,false)==1 && h.WasCreated());
	AT_PORTABLE_TEST_ASSERT(context, h.Write("ABC",3)==1 && h.GetPosition()==3 && h.GetLength()==3); h.Close();
	AT_PORTABLE_TEST_ASSERT(context, h.OpenFile(files.File(L"FILE.BIN").c_str(),nsVDFile::kRead|nsVDFile::kOpenExisting,true,false,false)==1 && !h.WasCreated());
	AT_PORTABLE_TEST_ASSERT(context, h.Read(b.data(),8,n)==kATCIOStat_TruncRecord && n==3 && b[0]=='A' && b[2]=='C' && b[3]==0 && b[7]==0);
	AT_PORTABLE_TEST_ASSERT(context, h.Seek(4)==kATCIOStat_PointDLen && h.Write("X",1)==kATCIOStat_ReadOnly); h.Close();
	AT_PORTABLE_TEST_ASSERT(context, h.OpenFile(files.File(L"FILE.BIN").c_str(),nsVDFile::kWrite|nsVDFile::kOpenAlways,false,true,true)==1 && h.GetPosition()==3);
	AT_PORTABLE_TEST_ASSERT(context, h.Write("D",1)==1 && h.GetLength()==4 && h.GetPosition()==4); h.Close();
	AT_PORTABLE_TEST_ASSERT(context, files.Read(L"FILE.BIN")== (Bytes{'A','B','C','D'}));
	return true;
}

bool ATTestAltirraPCLinkFileState(ATPortableTestContext& context) {
	Sandbox files; files.Write(L"A.BIN","A"); files.Write(L"B.BIN","B"); ATPCLinkFileHandle h;
	const uint8 date[6] {14,3,24,12,34,56};
	AT_PORTABLE_TEST_ASSERT(context, h.OpenFile(files.File(L"A.BIN").c_str(),nsVDFile::kReadWrite|nsVDFile::kOpenExisting,true,true,false)==1); h.SetTimestamp(date); h.Close();
	{ VDFile b(files.File(L"B.BIN").c_str(),nsVDFile::kWrite|nsVDFile::kOpenExisting); const uint8 other[6] {15,4,25,13,24,16}; b.setLastWriteTime(ATPCLinkDirEnt::DecodeDate(other)); }
	AT_PORTABLE_TEST_ASSERT(context, h.OpenFile(files.File(L"B.BIN").c_str(),nsVDFile::kRead|nsVDFile::kOpenExisting,true,false,false)==1); h.Close();
	{ VDFile b(files.File(L"B.BIN").c_str(),nsVDFile::kRead|nsVDFile::kOpenExisting); ATPCLinkDirEnt e {}; e.SetDate(b.getLastWriteTime()); AT_PORTABLE_TEST_ASSERT(context, e.mDay==15 && e.mMonth==4 && e.mYear==25); }
	AT_PORTABLE_TEST_ASSERT(context, h.OpenFile(files.File(L"MISSING").c_str(),nsVDFile::kRead|nsVDFile::kOpenExisting,true,false,false)==kATCIOStat_FileNotFound && !h.IsOpen());
	AT_PORTABLE_TEST_ASSERT(context, h.OpenFile(files.File(L"LIMIT").c_str(),nsVDFile::kWrite|nsVDFile::kCreateAlways,false,true,false)==1);
	AT_PORTABLE_TEST_ASSERT(context, h.Seek(0xFFFFFE)==1 && h.Write("XY",2)==kATCIOStat_DiskFull && h.GetLength()==0xFFFFFF && h.GetPosition()==0xFFFFFF);
	AT_PORTABLE_TEST_ASSERT(context, h.Seek(0x1000000)==kATCIOStat_PointDLen); h.Close();
	return true;
}

bool ATTestAltirraPCLinkDirHandle(ATPortableTestContext& context) {
	ATPCLinkFileHandle h; ATPCLinkDirEnt a {},b {},d {};
	a.SetFlagsFromAttributes(0); b.SetFlagsFromAttributes(kVDFileAttr_ReadOnly); d.SetFlagsFromAttributes(kVDFileAttr_Directory);
	memcpy(a.mName,Filename(L"A.TXT").mName,11); memcpy(b.mName,Filename(L"B.BIN").mName,11); memcpy(d.mName,Filename(L"DIR").mName,11);
	h.AddDirEnt(b); h.AddDirEnt(a); h.AddDirEnt(d); auto p=Params(0,0,"*.TXT"); ATPCLinkFileName filter; filter.ParseFromNet(p.data()+11);
	h.OpenAsDirectory(Filename(L"MAIN"),filter,0,true); AT_PORTABLE_TEST_ASSERT(context, h.GetLength()==92 && h.GetPosition()==23 && h.IsDir());
	ATPCLinkDirEnt e {}; AT_PORTABLE_TEST_ASSERT(context, h.GetNextDirEnt(e) && !memcmp(e.mName,a.mName,11) && !h.GetNextDirEnt(e));
	AT_PORTABLE_TEST_ASSERT(context, h.Seek(0)==1); Bytes bytes(92); uint32 n;
	AT_PORTABLE_TEST_ASSERT(context, h.Read(bytes.data(),92,n)==1 && n==92 && bytes[0]==0x28 && !memcmp(bytes.data()+6,"MAIN    ",8) && !memcmp(bytes.data()+23+6,"DIR     ",8));
	h.Close(); AT_PORTABLE_TEST_ASSERT(context, !h.IsOpen() && !h.IsDir() && !h.GetLength());
	return true;
}
