// Diagnostic formatting against the production main CPU and memory bus.
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include <at/attest/portabletest.h>
#include <consoletext.h>
#include <disasm.h>
#include "maincpuportablefixture.h"

namespace {
	class ConsoleCapture final : public IATUIDebuggerConsoleWindow {
	public:
		void Write(const char *s) override { mText += s; }
		void ShowEnd() override {}
		std::string mText;
	};
	struct ConsoleScope {
		explicit ConsoleScope(ConsoleCapture& sink)
			: mpPrevious(ATConsoleSetTextOutput(&sink))
			, mPreviousBeam(ATConsoleSetBeamPositionProvider({})) {}
		~ConsoleScope() {
			ATConsoleSetBeamPositionProvider(std::move(mPreviousBeam));
			ATConsoleSetTextOutput(mpPrevious);
		}
		IATUIDebuggerConsoleWindow *mpPrevious;
		vdfunction<bool(ATConsoleBeamPosition&)> mPreviousBeam;
	};
	ATCPUHistoryEntry Capture(MainCPUPortableFixture& f) {
		ATCPUHistoryEntry h {};
		h.mPC = f.mCPU.GetInsnPC(); h.mP = f.mCPU.GetP();
		h.mX = f.mCPU.GetX(); h.mY = f.mCPU.GetY();
		h.mbEmulation = f.mCPU.GetEmulationFlag();
		if (f.mCPU.GetCPUMode() == kATCPUMode_65C816) {
			h.mK = f.mCPU.GetK(); h.mB = f.mCPU.GetB(); h.mD = f.mCPU.GetD();
			h.mExt.mXH = f.mCPU.GetXH(); h.mExt.mYH = f.mCPU.GetYH();
		}
		for (uint32 i = 0; i < 4; ++i)
			h.mOpcode[i] = f.mMemory.DebugExtReadByte(static_cast<uint16>(h.mPC + i), h.mK);
		return h;
	}
}

bool ATTestAltirraMainCPUDiagnostics(ATPortableTestContext& context) {
	auto owner = std::make_unique<MainCPUPortableFixture>();
	auto& f = *owner;
	f.Prepare();
	f.Load(0x2000, { 0xAD, 0x34, 0x12 });
	f.mRAM[0x1234] = 0xAB;
	f.EnableBusTrace();
	std::vector<uint32> reads;
	ATDisasmServices services;
	services.mpDebugReadByte = [&](uint32 a) {
		reads.push_back(a);
		return f.mMemory.DebugExtReadByte(static_cast<uint16>(a), static_cast<uint8>(a >> 16));
	};
	VDStringA line;
	auto h = Capture(f);
	ATDisassembleInsn(line, services, nullptr, f.mCPU.GetDisasmMode(), h, true, false, true, true, false);
	AT_PORTABLE_TEST_ASSERT(context, reads.size() == 1 && reads[0] == 0x1234);
	AT_PORTABLE_TEST_ASSERT(context, line.find("LDA") != VDStringA::npos && line.find("1234") != VDStringA::npos);
	AT_PORTABLE_TEST_ASSERT(context, line.find("AB") != VDStringA::npos);
	AT_PORTABLE_TEST_ASSERT(context, f.mBus.empty()); // debugger reads must not execute CPU bus cycles

	f.Prepare(kATCPUMode_65C816, 0x12FFFF, 1, true, AT6502::kFlagM | AT6502::kFlagX);
	f.mRAM[0x12FFFF] = 0xA7; f.mRAM[0x120000] = 0x10; // LDA [$10], operands wrap in PBK
	f.mRAM[0x130000] = 0x99; // distinguish a mistaken linear instruction fetch
	f.mCPU.SetD(0x1200);
	f.Load(0x1210, { 0x56, 0x34, 0x23 });
	f.mRAM[0x233456] = 0xBC;
	reads.clear(); line.clear(); h = Capture(f);
	ATDisassembleInsn(line, services, nullptr, f.mCPU.GetDisasmMode(), h, true, false, true, true, false);
	AT_PORTABLE_TEST_ASSERT(context, h.mOpcode[1] == 0x10);
	AT_PORTABLE_TEST_ASSERT(context, line.find("23:3456") != VDStringA::npos && line.find("BC") != VDStringA::npos);
	AT_PORTABLE_TEST_ASSERT(context, reads.size() == 4 && reads.back() == 0x233456);
	AT_PORTABLE_TEST_ASSERT(context, f.mBus.empty());

#if defined(__APPLE__)
	// The macOS host diagnostic adapter must read this CPU's attached bus.
	// Windows retains its legacy simulator-bound console entry point.
	ConsoleCapture sink;
	ConsoleScope console(sink);
	f.mCPU.SetA(0x42);
	f.mCPU.DumpStatus(false);
	AT_PORTABLE_TEST_ASSERT(context, sink.mText.find("C=0042") != std::string::npos);
	AT_PORTABLE_TEST_ASSERT(context, sink.mText.find("LDA") != std::string::npos);
	AT_PORTABLE_TEST_ASSERT(context, sink.mText.find("23:3456") != std::string::npos);
	AT_PORTABLE_TEST_ASSERT(context, f.mBus.empty());
#endif
	return true;
}
