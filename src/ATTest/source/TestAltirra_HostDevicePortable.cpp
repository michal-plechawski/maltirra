// Atari filename syntax and real host-file binary/text channel contracts.
#include "hostdeviceportablefixture.h"
#include <cstdio>
using namespace HostDevicePortable;

bool ATTestAltirraHostParsing(ATPortableTestContext& context) {
	struct Case { const char *input; const wchar_t *output; };
	for (auto c : {Case{"foo.txt",L"FOO.TXT"}, {"FOO.TOOLONG",L"FOO.TOO"},
		{"FOO>BAR.TXT",L"FOO\\BAR.TXT"}, {"FOO\\BAR.TXT",L"FOO\\BAR.TXT"},
		{"FOO>..>BAR",L"BAR"}, {"FOO>BAR>..",L"FOO"}, {"..>..>BAR",L"BAR"},
		{"FOO>.>BAR",L"FOO\\BAR"}, {"CON.TXT",L"!CON.TXT"}, {"CON>FILE",L"!CON\\FILE"},
		{"FOO>",L"FOO"}, {"FOO>BAR>",L"FOO\\BAR"},
		{"FOO>BAR>.",L"FOO\\BAR"}, {"CON",L"!CON"}, {"CONX.TXT",L"CONX.TXT"}}) {
		VDStringW out;
		const bool valid=ATHostDeviceParseFilename(c.input,true,true,true,false,false,out);
		if (!valid || out != Native(c.output)) printf("Filename case: %s\n",c.input);
		AT_PORTABLE_TEST_ASSERT(context, valid && out == Native(c.output));
	}
	VDStringW out;
	struct Seeded { const wchar_t *base; const char *input; const wchar_t *expected; };
	for (auto c : {Seeded{L"FOO","TEST.TXT",L"FOO\\TEST.TXT"},
		{L"FOO\\BAR","TEST.TXT",L"FOO\\BAR\\TEST.TXT"},
		{L"FOO\\BAR","BAZ>TEST.TXT",L"FOO\\BAR\\BAZ\\TEST.TXT"},
		{L"FOO\\BAR","..\\BAZ>TEST.TXT",L"FOO\\BAZ\\TEST.TXT"},
		{L"FOO\\BAR","..",L"FOO"}, {L"FOO\\BAR","..>..",L""},
		{L"!CON\\BAR","..",L"!CON"},
		{L"FOO\\BAR\\BLAH","..\\..\\BAZ>TEST.TXT",L"FOO\\BAZ\\TEST.TXT"},
		{L"FOO\\BAR\\BLAH","..\\..\\..\\..\\BAZ>TEST.TXT",L"BAZ\\TEST.TXT"}}) {
		out=Native(c.base);
		AT_PORTABLE_TEST_ASSERT(context, ATHostDeviceParseFilename(c.input,true,false,true,false,false,out) && out == Native(c.expected));
	}
	out.clear();
	AT_PORTABLE_TEST_ASSERT(context, ATHostDeviceParseFilename("Foo.TXT",false,false,false,true,false,out) && out == L"foo.txt");
	for (const char *bad : {"ABCDEFGHI.TXT","A/B","A:B","A\"B","A|B","A..B","A.>B","*>B"}) {
		out.clear(); AT_PORTABLE_TEST_ASSERT(context, !ATHostDeviceParseFilename(bad,false,true,true,false,false,out));
	}
	for (auto c : {Case{"Read Me.1.txt",L"Read Me.1.txt"},{"No Extension",L"No Extension"},{"!NAME",L"!!NAME"}}) {
		out.clear(); AT_PORTABLE_TEST_ASSERT(context, ATHostDeviceParseFilename(c.input,false,false,true,false,true,out) && out == c.output);
	}
	for (const char *bad : {"A/B","A:B","A\"B","A<B","A|B"}) {
		out.clear(); AT_PORTABLE_TEST_ASSERT(context, !ATHostDeviceParseFilename(bad,false,false,true,false,true,out));
	}
	out.clear(); AT_PORTABLE_TEST_ASSERT(context, !ATHostDeviceParseFilename("A>B",false,false,false,false,false,out));
	out.clear(); AT_PORTABLE_TEST_ASSERT(context, !ATHostDeviceParseFilename("*.TXT",false,false,true,false,false,out));
	out.clear(); AT_PORTABLE_TEST_ASSERT(context, !ATHostDeviceParseFilename("",false,false,true,false,false,out));
	out.clear(); AT_PORTABLE_TEST_ASSERT(context, ATHostDeviceParseFilename("",true,false,true,false,false,out) && out.empty());
	std::string longName(64,'a'); out.clear();
	AT_PORTABLE_TEST_ASSERT(context, ATHostDeviceParseFilename(longName.c_str(),false,false,true,false,true,out));
	longName+='a'; out.clear(); AT_PORTABLE_TEST_ASSERT(context, !ATHostDeviceParseFilename(longName.c_str(),false,false,true,false,true,out));
	return true;
}

bool ATTestAltirraHostWildcards(ATPortableTestContext& context) {
	struct Case { const wchar_t *source,*pattern,*output; };
	for (auto c : {Case{L"foo.txt",L"bar.txt",L"bar.txt"}, {L"foo.txt",L"*.*",L"foo.txt"},
		{L"foo.txt",L"*.bin",L"foo.bin"}, {L"foo.txt",L"bar.*",L"bar.txt"},
		{L"foo.txt",L"f?x.txt",L"fox.txt"}, {L"foo",L"b*.*",L"boo"}}) {
		VDStringW out; ATHostDeviceMergeWildPath(out,c.source,c.pattern);
		AT_PORTABLE_TEST_ASSERT(context, out == c.output);
	}
	auto f=std::make_unique<Machine>(); f->files.Write(L"FIRST.TXT","one"); f->files.Write(L"OTHER.BIN","two"); Bytes b;
	AT_PORTABLE_TEST_ASSERT(context, f->Open("H:FI???.T*") == 1 && f->Read(b,3) == kATCIOStat_SuccessEOF && b == (Bytes{'o','n','e'}));
	AT_PORTABLE_TEST_ASSERT(context, f->Close() == 1 && f->Special(0x20,"H:*.TXT,*.BAK") == 1);
	AT_PORTABLE_TEST_ASSERT(context, f->Open("H:FIRST.TXT") == kATCIOStat_FileNotFound && f->Open("H:FIRST.BAK") == 1);
	return true;
}

bool ATTestAltirraHostBinary(ATPortableTestContext& context) {
	auto f=std::make_unique<Machine>(); const Bytes data {0,0x9B,0xFF,'A','\r','\n'}; Bytes b;
	AT_PORTABLE_TEST_ASSERT(context, f->Open("H:BINARY.BIN",8) == 1 && f->Write(data) == 1 && f->Close() == 1 && f->files.Read(L"BINARY.BIN") == data);
	AT_PORTABLE_TEST_ASSERT(context, f->Open("H:BINARY.BIN") == 1 && f->Read(b,2) == 1 && b == (Bytes{0,0x9B}));
	AT_PORTABLE_TEST_ASSERT(context, f->Read(b,20) == 3 && b == (Bytes{0xFF,'A','\r','\n'}));
	AT_PORTABLE_TEST_ASSERT(context, f->Read(b,1) == kATCIOStat_EndOfFile && b.empty() && f->Close() == 1);
	AT_PORTABLE_TEST_ASSERT(context, f->Open("H:BINARY.BIN",9) == 1 && f->Write(Bytes{'Z'}) == 1 && f->Close() == 1);
	Bytes appended=data; appended.push_back('Z'); AT_PORTABLE_TEST_ASSERT(context, f->files.Read(L"BINARY.BIN") == appended);
	AT_PORTABLE_TEST_ASSERT(context, f->Open("H:BINARY.BIN",12) == 1 && f->Write(Bytes{'Q'}) == 1 && f->Close() == 1);
	appended[0]='Q'; AT_PORTABLE_TEST_ASSERT(context, f->files.Read(L"BINARY.BIN") == appended);
	for (uint8 ch=0; ch<8; ++ch) AT_PORTABLE_TEST_ASSERT(context, f->Open("H:BINARY.BIN",4,ch) == 1);
	for (uint8 ch=0; ch<8; ++ch) AT_PORTABLE_TEST_ASSERT(context, f->Read(b,1,ch) == 1 && b == Bytes{'Q'} && f->Close(ch) == 1);
	AT_PORTABLE_TEST_ASSERT(context, f->Open("H:BINARY.BIN",8) == 1 && f->Close() == 1 && f->files.Read(L"BINARY.BIN").empty());
	return true;
}

bool ATTestAltirraHostPermissions(ATPortableTestContext& context) {
	auto f=std::make_unique<Machine>(); f->files.Write(L"FILE.TXT","unchanged"); uint32 n=99; uint8 b=0;
	AT_PORTABLE_TEST_ASSERT(context, f->Open("H:FILE.TXT") == 1);
	AT_PORTABLE_TEST_ASSERT(context, f->cio->OnCIOPutBytes(1,1,"X",1,n) == kATCIOStat_ReadOnly && n == 0 && f->Close() == 1);
	AT_PORTABLE_TEST_ASSERT(context, f->Open("H:WRITE.TXT",8) == 1);
	n=99; AT_PORTABLE_TEST_ASSERT(context, f->cio->OnCIOGetBytes(1,1,&b,1,n) == kATCIOStat_WriteOnly && !n);
	AT_PORTABLE_TEST_ASSERT(context, f->Write(Bytes{'A'}) == 1); f->host->SetReadOnly(true);
	n=99; AT_PORTABLE_TEST_ASSERT(context, f->cio->OnCIOPutBytes(1,1,"B",1,n) == kATCIOStat_ReadOnly && !n && f->Close() == 1);
	AT_PORTABLE_TEST_ASSERT(context, f->files.Read(L"WRITE.TXT") == Bytes{'A'} && f->Open("H:FILE.TXT",12) == kATCIOStat_ReadOnly);
	for (uint8 cmd : {0x20,0x21,0x22,0x23,0x24,0x2A,0x2B}) AT_PORTABLE_TEST_ASSERT(context, f->Special(cmd,"H:FILE.TXT,NEW.TXT") == kATCIOStat_ReadOnly);
	AT_PORTABLE_TEST_ASSERT(context, f->files.Read(L"FILE.TXT") == (Bytes{'u','n','c','h','a','n','g','e','d'}));
	f->host->SetReadOnly(false); n=99;
	AT_PORTABLE_TEST_ASSERT(context, f->cio->OnCIOPutBytes(1,1,"X",1,n) == kATCIOStat_NotOpen && !n);
	return true;
}

bool ATTestAltirraHostText(ATPortableTestContext& context) {
	auto f=std::make_unique<Machine>(); f->files.Write(L"TEXT.TXT","A\r\nB\n\rC\rD\nE"); Bytes b;
	AT_PORTABLE_TEST_ASSERT(context, f->Open("H6:TEXT.TXT") == 1 && f->Read(b,100) == 3 && b == (Bytes{'A',0x9B,'B',0x9B,'C',0x9B,'D',0x9B,'E'}) && f->Close() == 1);
	AT_PORTABLE_TEST_ASSERT(context, f->Open("H6:OUT.TXT",8) == 1 && f->Write(Bytes{'X',0x9B,'Y',0x9B}) == 1 && f->Close() == 1);
	AT_PORTABLE_TEST_ASSERT(context, f->files.Read(L"OUT.TXT") == (Bytes{'X','\r','\n','Y','\r','\n'}));
	AT_PORTABLE_TEST_ASSERT(context, f->Open("H6:OUT.TXT",9) == 1 && f->Write(Bytes{'Z'}) == 1 && f->Close() == 1);
	AT_PORTABLE_TEST_ASSERT(context, f->Open("H:OUT.TXT") == 1 && f->Read(b,100) == 3 && b == (Bytes{'X','\r','\n','Y','\r','\n','Z'}) && f->Close() == 1);
	AT_PORTABLE_TEST_ASSERT(context, f->Open("H6:OUT.TXT",12) == 1);
	uint32 n=99; AT_PORTABLE_TEST_ASSERT(context, f->cio->OnCIOGetBytes(1,6,nullptr,0,n) == 1 && !n);
	n=99; AT_PORTABLE_TEST_ASSERT(context, f->cio->OnCIOPutBytes(1,6,nullptr,0,n) == 1 && !n);
	AT_PORTABLE_TEST_ASSERT(context, f->Write(Bytes{'Q'}) == 1 && f->Close() == 1 && f->files.Read(L"OUT.TXT") == (Bytes{'Q','\r','\n','Y','\r','\n','Z'}));
	return true;
}
