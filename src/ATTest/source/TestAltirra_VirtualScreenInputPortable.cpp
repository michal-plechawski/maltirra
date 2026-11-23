// Input, mode switching, resets, and an actual 6502 E: continuation loop.
#include "virtualscreenportablefixture.h"
using namespace VirtualScreenPortable;

bool ATTestAltirraVirtualScreenInput(ATPortableTestContext& context) {
	auto f = std::make_unique<Machine>(); auto& s = *f->screen;
	AT_PORTABLE_TEST_ASSERT(context, s.IsReadyForInput());
	s.PushLine("ABC"); AT_PORTABLE_TEST_ASSERT(context, !s.IsReadyForInput() && f->Row(0).substr(2,3) == "ABC");
	AT_PORTABLE_TEST_ASSERT(context, f->Editor(4) == 0x60 && f->h.mCPU.GetA() == 'A');
	s.PushLine("XYZ"); AT_PORTABLE_TEST_ASSERT(context, f->Input() == std::string("XYZ\x9B") && s.IsReadyForInput());
	s.PushLine(""); AT_PORTABLE_TEST_ASSERT(context, f->Input() == std::string("\x9B") && s.IsReadyForInput());
	uint32 ready = 0;
	s.SetReadyCallback([&] { s.SetReadyCallback({}); s.PushLine("PRINT 2+2"); ++ready; });
	AT_PORTABLE_TEST_ASSERT(context, f->Input() == std::string("PRINT 2+2\x9B") && ready == 1 && s.IsReadyForInput());
	// Screen input joins wrapped physical rows and trims only trailing spaces.
	f->Editor(6,0x7D); f->Print(std::string(38,'X')+"Y Z  ");
	s.ReadScreenAsInput(); AT_PORTABLE_TEST_ASSERT(context, f->Input() == std::string(38,'X')+"Y Z\x9B");
	return true;
}

bool ATTestAltirraVirtualScreenKeyboard(ATPortableTestContext& context) {
	auto f = std::make_unique<Machine>(); auto& s = *f->screen;
	// The translated keystrokes are echoed until RETURN submits the line.
	uint32 i = 0; const std::array<uint8,5> keys {0x3F,0x3E,0x21,0x3D,0x0C}; // A S space G RETURN
	s.SetReadyCallback([&] { if (i<keys.size()) f->Mem(ATKernelSymbols::CH,keys[i++]); });
	AT_PORTABLE_TEST_ASSERT(context, f->Input() == std::string("AS G\x9B") && f->Mem(ATKernelSymbols::CH) == 0xFF);
	s.SetReadyCallback({}); f->Editor(6,0x7D); s.SetShiftControlLockState(false,true);
	AT_PORTABLE_TEST_ASSERT(context, !s.GetShiftLockState() && s.GetControlLockState());
	i=0; s.SetReadyCallback([&] { f->Mem(ATKernelSymbols::CH,i++ ? 0x0C : 0x3F); });
	AT_PORTABLE_TEST_ASSERT(context, f->Input() == std::string("\x01\x9B") && f->Mem(ATKernelSymbols::SHFLOK) == 0x80);
	// Inverse, caps lock, invalid keys and Ctrl+Shift are handled without output.
	s.SetReadyCallback({}); f->Editor(6,0x7D); s.SetShiftControlLockState(false,false); i=0;
	const std::array<uint8,6> extra {0xC0,3,0x27,0x3C,0x3F,0x0C};
	s.SetReadyCallback([&] { if (i<extra.size()) f->Mem(ATKernelSymbols::CH,extra[i++]); });
	const std::string inverse {char(0xC1),char(0x9B)};
	AT_PORTABLE_TEST_ASSERT(context, f->Input() == inverse && f->Mem(ATKernelSymbols::SHFLOK) == 0x40 && f->Mem(ATKernelSymbols::INVFLG) == 0x80);
	s.SetReadyCallback([&] { f->Mem(ATKernelSymbols::CH,0x9A); }); // Ctrl+3: EOF
	AT_PORTABLE_TEST_ASSERT(context, f->Editor(4) == 0x60 && f->h.mCPU.GetY() == kATCIOStat_EndOfFile);
	return true;
}

bool ATTestAltirraVirtualScreenModes(ATPortableTestContext& context) {
	auto f = std::make_unique<Machine>(false); auto& s = *f->screen; std::vector<bool> changes;
	s.SetPassThroughChangedCallback([&] { changes.push_back(s.IsPassThroughEnabled()); });
	AT_PORTABLE_TEST_ASSERT(context, s.IsPassThroughEnabled() && f->Editor(6,'X') == 0 && f->Screen(6,'X') == 0);
	f->Open(); AT_PORTABLE_TEST_ASSERT(context, !s.IsPassThroughEnabled() && changes == std::vector<bool>{false});
	f->Mem(0x3003,0x41); s.CheckForDisplayListReplaced(); s.CheckForDisplayListReplaced();
	AT_PORTABLE_TEST_ASSERT(context, s.IsPassThroughEnabled() && changes == std::vector<bool>({false,true}));
	f->Mem(0x3003,0x42); s.CheckForDisplayListReplaced();
	AT_PORTABLE_TEST_ASSERT(context, !s.IsPassThroughEnabled() && changes.size() == 3);
	f->Mem(ATKernelSymbols::ICAX2,8); AT_PORTABLE_TEST_ASSERT(context, f->Screen(0) == 0 && s.IsPassThroughEnabled() && f->Screen(6,'X') == 0);
	AT_PORTABLE_TEST_ASSERT(context, f->Editor(2) == 0 && f->Screen(2) == 0 && f->Editor(8) == 0x60 && f->Screen(10) == 0x60);
	f->Mem(ATKernelSymbols::ICAX2,0); f->Screen(0); f->Editor(4);
	AT_PORTABLE_TEST_ASSERT(context, !s.IsPassThroughEnabled() && f->Editor(8) == 0 && f->Screen(10) == 0);
	// A display list crossing FFFF must use the 16-bit Atari bus.
	f->Word(ATKernelSymbols::SDLSTL,0xFFFD); f->Mem(0,0x42); f->Mem(1,0); f->Mem(2,0x40); s.CheckForDisplayListReplaced();
	AT_PORTABLE_TEST_ASSERT(context, !s.IsPassThroughEnabled());
	s.Resize(80,30); s.Enable(); AT_PORTABLE_TEST_ASSERT(context, f->Mem(ATKernelSymbols::RMARGN) == 79);
	f->Position(12,7); s.Disable(); AT_PORTABLE_TEST_ASSERT(context, f->Mem(ATKernelSymbols::RMARGN) == 39 && f->Word(ATKernelSymbols::COLCRS) == 0 && f->Mem(ATKernelSymbols::ROWCRS) == 0);
	return true;
}

bool ATTestAltirraVirtualScreenReset(ATPortableTestContext& context) {
	auto f = std::make_unique<Machine>(); auto& s = *f->screen;
	f->Mem(ATKernelSymbols::BRKKEY,0); AT_PORTABLE_TEST_ASSERT(context, f->Editor(4) == 0x60 && f->h.mCPU.GetY() == kATCIOStat_Break);
	AT_PORTABLE_TEST_ASSERT(context, f->Screen(4) == 0x60 && f->h.mCPU.GetY() == kATCIOStat_Break);
	AT_PORTABLE_TEST_ASSERT(context, f->Editor(6,'X') == 0x60 && f->h.mCPU.GetY() == kATCIOStat_Break && f->Mem(ATKernelSymbols::BRKKEY) == 0x80);
	const auto col = f->Word(ATKernelSymbols::COLCRS); s.ToggleSuspend(); f->Editor(6,'X');
	AT_PORTABLE_TEST_ASSERT(context, f->Mem(ATKernelSymbols::SSFLAG) == 0xFF && f->Word(ATKernelSymbols::COLCRS) == col);
	s.ToggleSuspend(); f->Editor(6,'Y'); AT_PORTABLE_TEST_ASSERT(context, f->Mem(ATKernelSymbols::SSFLAG) == 0 && f->Row(0)[2] == 'Y');
	f->Editor(6,0xFD); s.PushLine("stale"); f->Editor(4); s.ToggleSuspend();
	// Reset must not write kernel variables before the OS reinitializes them.
	const auto before = std::vector<uint8>(f->h.mRAM.begin(),f->h.mRAM.begin()+65536);
	s.WarmReset(); uint32 x,y;
	AT_PORTABLE_TEST_ASSERT(context, std::equal(before.begin(),before.end(),f->h.mRAM.begin()) && s.IsReadyForInput() && !s.CheckForBell() && !s.GetCursorInfo(x,y) && s.IsPassThroughEnabled());
	AT_PORTABLE_TEST_ASSERT(context, f->Row(0) == std::string(40,' ') && f->Editor(6,'X') == 0);
	f->Open(); s.PushLine("new"); AT_PORTABLE_TEST_ASSERT(context, f->Input() == std::string("new\x9B"));
	s.ColdReset(); AT_PORTABLE_TEST_ASSERT(context, s.IsReadyForInput() && !s.GetCursorInfo(x,y));
	// Unregistering the pass-through callback during dispatch is supported.
	uint32 calls = 0; s.SetPassThroughChangedCallback([&] { s.SetPassThroughChangedCallback({}); ++calls; });
	f->Open(); AT_PORTABLE_TEST_ASSERT(context, calls == 1);
	return true;
}

bool ATTestAltirraVirtualScreenCPU(ATPortableTestContext& context) {
	auto f = std::make_unique<Machine>(); auto& h = f->h; auto& s = *f->screen;
	ATCPUHookNode *hook = nullptr;
	h.mHooks.SetHook(hook,kATCPUHookMode_Always,0x2200,0,[&](uint16) { return s.OnEditorCIOVector(&h.mCPU,h.mCPU.GetMemory(),4); });
	h.mRAM[0xE4C0] = 0x60;
	// JSR get; STA $4000; JSR get; STA $4001; JMP here.
	h.Load(0x2000,{0x20,0,0x22,0x8D,0,0x40,0x20,0,0x22,0x8D,1,0x40,0x4C,12,0x20});
	h.mCPU.SetPC(0x2000); uint32 waits = 0;
	s.SetReadyCallback([&] { if (++waits == 4) s.PushLine("Q"); });
	const bool ran = h.RunCycles(400); h.mHooks.UnsetHook(hook);
	AT_PORTABLE_TEST_ASSERT(context, ran && waits == 4 && h.mRAM[0x4000] == 'Q' && h.mRAM[0x4001] == 0x9B && h.mCPU.GetS() == 0xFD);
	AT_PORTABLE_TEST_ASSERT(context, h.mCPU.GetPC() >= 0x200C && h.mCPU.GetPC() <= 0x200F && s.IsReadyForInput());
	return true;
}
