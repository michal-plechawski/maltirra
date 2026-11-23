// Screen/editor output, editing, resizing, and logical-line bookkeeping.
#include "virtualscreenportablefixture.h"
using namespace VirtualScreenPortable;

bool ATTestAltirraVirtualScreenOutput(ATPortableTestContext& context) {
	auto f = std::make_unique<Machine>(); auto& s = *f->screen;
	uint32 w,h; const uint8 *p; s.GetScreen(w,h,p); uint32 x,y;
	AT_PORTABLE_TEST_ASSERT(context, w == 40 && h == 24 && std::all_of(p,p+w*h,[](uint8 c) { return c==' '; }) && s.GetTerminalSize() == vdsize32(40,24));
	AT_PORTABLE_TEST_ASSERT(context, s.GetCursorInfo(x,y) && x == 2 && y == 0 && s.CheckForCursorChange() && !s.CheckForCursorChange());
	f->Print("ABCDE");
	AT_PORTABLE_TEST_ASSERT(context, f->Row(0).substr(0,8) == "  ABCDE " && f->Word(ATKernelSymbols::COLCRS) == 7 && f->Mem(ATKernelSymbols::ROWCRS) == 0);
	std::array<uint8,12> selected; selected.fill(0xCC);
	AT_PORTABLE_TEST_ASSERT(context, s.ReadRawText(selected.data(),4,0,3) == 3 && selected[0] == 'C' && selected[1] == 'D' && selected[2] == 'E' && selected[3] == 0xCC);
	AT_PORTABLE_TEST_ASSERT(context, s.ReadRawText(selected.data(),38,0,12) == 2 && s.ReadRawText(nullptr,-1,0,3) == 0 && s.ReadRawText(nullptr,0,-1,3) == 0 && s.ReadRawText(nullptr,40,0,1) == 0 && s.ReadRawText(nullptr,0,24,1) == 0 && s.ReadRawText(nullptr,0,0,0) == 0);
	f->Position(2,0);
	for (uint8 c : {'A','B','C','D','E'}) AT_PORTABLE_TEST_ASSERT(context, f->Screen(4) == 0x60 && f->h.mCPU.GetA() == c && f->h.mCPU.GetY() == 1);
	AT_PORTABLE_TEST_ASSERT(context, f->Word(ATKernelSymbols::COLCRS) == 7);
	f->Position(39,0); f->Screen(4);
	AT_PORTABLE_TEST_ASSERT(context, f->Word(ATKernelSymbols::COLCRS) == 2 && f->Mem(ATKernelSymbols::ROWCRS) == 1);
	f->Position(40,0); AT_PORTABLE_TEST_ASSERT(context, f->Screen(4) == 0x60 && f->h.mCPU.GetY() == kATCIOStat_CursorRange);
	f->Position(0,24); AT_PORTABLE_TEST_ASSERT(context, f->Screen(4) == 0x60 && f->h.mCPU.GetY() == kATCIOStat_CursorRange);
	f->Position(0x100,0); AT_PORTABLE_TEST_ASSERT(context, f->Screen(4) == 0x60 && f->h.mCPU.GetY() == kATCIOStat_CursorRange);
	f->Editor(6,'Z'); AT_PORTABLE_TEST_ASSERT(context, f->Row(0)[39] == 'Z'); // E: clips the full 16-bit coordinate
	return true;
}

bool ATTestAltirraVirtualScreenControls(ATPortableTestContext& context) {
	auto f = std::make_unique<Machine>(); auto& s = *f->screen;
	f->Editor(6,0x1C); AT_PORTABLE_TEST_ASSERT(context, f->Mem(ATKernelSymbols::ROWCRS) == 23);
	f->Editor(6,0x1D); AT_PORTABLE_TEST_ASSERT(context, f->Mem(ATKernelSymbols::ROWCRS) == 0);
	f->Editor(6,0x1E); AT_PORTABLE_TEST_ASSERT(context, f->Word(ATKernelSymbols::COLCRS) == 39);
	f->Editor(6,0x1F); AT_PORTABLE_TEST_ASSERT(context, f->Word(ATKernelSymbols::COLCRS) == 2);
	f->Editor(6,0x1B); AT_PORTABLE_TEST_ASSERT(context, f->Mem(ATKernelSymbols::ESCFLG) == 0x80);
	f->Editor(6,0x9B); AT_PORTABLE_TEST_ASSERT(context, f->Mem(ATKernelSymbols::ESCFLG) == 0x80 && f->Mem(ATKernelSymbols::ROWCRS) == 1);
	f->Editor(6,0x7D); AT_PORTABLE_TEST_ASSERT(context, f->Row(1)[2] == char(0x7D) && f->Mem(ATKernelSymbols::ESCFLG) == 0);
	f->Mem(ATKernelSymbols::DSPFLG,1); f->Editor(6,0x1B);
	AT_PORTABLE_TEST_ASSERT(context, f->Row(1)[3] == 0x1B && f->Mem(ATKernelSymbols::ESCFLG) == 0);
	f->Mem(ATKernelSymbols::DSPFLG,0); f->Screen(6,0x1C);
	AT_PORTABLE_TEST_ASSERT(context, f->Row(1)[4] == 0x1C); // S: writes cursor controls literally
	f->Editor(6,0xFD); AT_PORTABLE_TEST_ASSERT(context, s.CheckForBell() && !s.CheckForBell());
	f->Editor(6,0x7D); AT_PORTABLE_TEST_ASSERT(context, f->Row(1) == std::string(40,' ') && f->Mem(ATKernelSymbols::ROWCRS) == 0);
	f->Editor(6,0x7F); AT_PORTABLE_TEST_ASSERT(context, f->Word(ATKernelSymbols::COLCRS) == 7);
	f->Editor(6,0x9E); f->Position(2,0); f->Editor(6,0x7F);
	AT_PORTABLE_TEST_ASSERT(context, f->Word(ATKernelSymbols::COLCRS) == 15); // cleared tab persists
	f->Position(4,0); f->Editor(6,0x9F); f->Position(2,0); f->Editor(6,0x7F);
	AT_PORTABLE_TEST_ASSERT(context, f->Word(ATKernelSymbols::COLCRS) == 4);
	return true;
}

bool ATTestAltirraVirtualScreenEditing(ATPortableTestContext& context) {
	auto f = std::make_unique<Machine>();
	f->Print("ABCDE"); f->Position(3,0); f->Editor(6,0xFE);
	AT_PORTABLE_TEST_ASSERT(context, f->Row(0).substr(2,5) == "ACDE ");
	f->Editor(6,0xFF); AT_PORTABLE_TEST_ASSERT(context, f->Row(0).substr(2,5) == "A CDE");
	f->Position(7,0); f->Editor(6,0x7E); AT_PORTABLE_TEST_ASSERT(context, f->Row(0).substr(2,5) == "A CD " && f->Word(ATKernelSymbols::COLCRS) == 6);
	f->Editor(6,0x7D); f->Print(std::string(38,'X')); // fills one physical row and extends the logical line
	f->Print("Y"); f->Position(2,0); f->Editor(6,0xFF);
	AT_PORTABLE_TEST_ASSERT(context, f->Row(0).substr(2) == " "+std::string(37,'X') && f->Row(1).substr(2,3) == "XY ");
	f->Editor(6,0xFE); AT_PORTABLE_TEST_ASSERT(context, f->Row(0).substr(2) == std::string(38,'X') && f->Row(1)[2] == 'Y');
	// Inserting at a full physical line must preserve the displaced character.
	f->Editor(6,0x7D); f->Print(std::string(37,'A')+"Z"); f->Position(2,0); f->Editor(6,0x9B);
	f->Position(2,0); f->Editor(6,0xFF);
	AT_PORTABLE_TEST_ASSERT(context, f->Row(1)[2] == 'Z');
	f->Position(2,1); f->Editor(6,0x9C); // delete the whole two-row logical line
	AT_PORTABLE_TEST_ASSERT(context, f->Row(0) == std::string(40,' ') && f->Mem(ATKernelSymbols::ROWCRS) == 0);
	f->Print("first"); f->Editor(6,0x9B); f->Print("second"); f->Position(2,0); f->Editor(6,0x9D);
	AT_PORTABLE_TEST_ASSERT(context, f->Row(0) == std::string(40,' ') && f->Row(1).substr(2,5) == "first" && f->Row(2).substr(2,6) == "second");
	// Compare repeated cross-row edits against an independent flat text model.
	f->Editor(6,0x7D); std::string model;
	for (uint32 i=0; i<120; ++i) model.push_back(char('A'+i%26));
	f->Print(model); uint32 state = 0x31415926;
	for (uint32 i=0; i<64; ++i) {
		state = state*1664525+1013904223; const uint32 at = state % uint32(model.size());
		f->Position(uint16(2+at%38),uint8(at/38));
		if (i&1) { f->Editor(6,0xFE); model.erase(at,1); }
		else { f->Editor(6,0xFF); f->Editor(6,'X'); model.insert(at,1,'X'); }
		std::string actual; for (uint32 y=0; y<5; ++y) actual += f->Row(y).substr(2,38);
		AT_PORTABLE_TEST_ASSERT(context, actual == model+std::string(actual.size()-model.size(),' '));
	}
	return true;
}

bool ATTestAltirraVirtualScreenScroll(ATPortableTestContext& context) {
	auto f = std::make_unique<Machine>();
	for (uint8 y=0; y<24; ++y) { f->Position(2,y); f->Editor(6,uint8('A'+y)); }
	f->Position(39,23); f->Editor(6,'Z');
	AT_PORTABLE_TEST_ASSERT(context, f->Row(0)[2] == 'B' && f->Row(22)[39] == 'Z' && f->Row(23) == std::string(40,' '));
	AT_PORTABLE_TEST_ASSERT(context, f->Mem(ATKernelSymbols::ROWCRS) == 23 && f->Word(ATKernelSymbols::COLCRS) == 2);
	f->Editor(6,0x7D); f->Print(std::string(76,'L')); f->Editor(6,0x9B); // first logical line occupies three rows
	f->Position(2,23); f->Editor(6,0x9B);
	AT_PORTABLE_TEST_ASSERT(context, f->Mem(ATKernelSymbols::ROWCRS) == 21 && f->Row(0) == std::string(40,' '));
	return true;
}

bool ATTestAltirraVirtualScreenResize(ATPortableTestContext& context) {
	auto f = std::make_unique<Machine>(); auto& s = *f->screen; uint32 resized = 0;
	s.SetResizeCallback([&] { ++resized; }); f->Print("preserve"); s.Resize(80,30);
	AT_PORTABLE_TEST_ASSERT(context, resized == 1 && s.GetTerminalSize() == vdsize32(80,30) && f->Row(0).substr(2,8) == "preserve" && f->Row(0).substr(40) == std::string(40,' '));
	f->Editor(6,'!'); AT_PORTABLE_TEST_ASSERT(context, f->Mem(ATKernelSymbols::RMARGN) == 79);
	s.Resize(0,0); s.Resize(40,24); AT_PORTABLE_TEST_ASSERT(context, resized == 2 && s.GetTerminalSize() == vdsize32(40,24));
	s.Resize(0xFFFFFFFF,0xFFFFFFFF); AT_PORTABLE_TEST_ASSERT(context, s.GetTerminalSize() == vdsize32(255,255));
	f->Position(254,254); f->Editor(6,0x1B); // cursor remains at the far corner
	s.Resize(40,24); uint32 x,y; AT_PORTABLE_TEST_ASSERT(context, s.GetCursorInfo(x,y) && x == 39 && y == 23);
	// Resizing with a narrow margin must also be safe before another CIO call.
	s.Resize(255,255); f->Mem(ATKernelSymbols::LMARGN,240); f->Position(240,250); f->Editor(6,'Q');
	s.Resize(40,24); s.ReadScreenAsInput();
	AT_PORTABLE_TEST_ASSERT(context, !s.IsReadyForInput());
	// The callback can unregister and resize recursively without destroying itself.
	const uint32 before = resized;
	s.SetResizeCallback([&] { s.SetResizeCallback({}); s.Resize(60,25); ++resized; });
	s.Resize(80,30); AT_PORTABLE_TEST_ASSERT(context, s.GetTerminalSize() == vdsize32(60,25) && resized == before+1);
	// Many wraps at both size limits, followed by line reconstruction.
	for (uint32 width : {40u,80u,255u}) for (uint32 len : {0u,1u,37u,38u,39u,75u,76u,77u,511u}) {
		s.Resize(width,255); f->Mem(ATKernelSymbols::LMARGN,2); f->Position(2,0); f->Editor(6,0x7D);
		std::string expected; for (uint32 i=0; i<len; ++i) expected.push_back(char('a'+i%26));
		f->Print(expected); s.ReadScreenAsInput();
		AT_PORTABLE_TEST_ASSERT(context, f->Input() == expected+"\x9B");
	}
	return true;
}
