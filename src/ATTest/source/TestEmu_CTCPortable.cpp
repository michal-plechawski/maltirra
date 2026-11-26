#include <vector>
#include <at/atemulation/ctc.h>
#include <at/attest/portabletest.h>
namespace {
	struct Timer {
		Timer() { clock.SetRate(VDFraction(4000000,1)); chip.Init(&clock); chip.SetInterruptFn([this](sint32 v) { irq.push_back(v); }); chip.Reset(); Tick(1); irq.clear(); }
		~Timer() { chip.Shutdown(); }
		void Tick(uint32 n) { while(n--) ATSCHEDULER_ADVANCE(&clock); }
		void Pulse(uint8 ch) { chip.SetInputState(ch,true); chip.SetInputState(ch,false); }
		ATScheduler clock; ATCTCEmulator chip; std::vector<sint32> irq;
	};
}
bool ATTestEmuCTCTimer(ATPortableTestContext& context) {
	for(bool slow:{false,true}) { Timer f; uint32 pulses=0; f.chip.SetUnderflowFn(0,[&] { ++pulses; }); f.chip.WriteByte(0,slow ? 0x27 : 7); f.chip.WriteByte(0,3); const uint32 period=3*(slow ? 256 : 16); f.Tick(period-1); AT_PORTABLE_TEST_ASSERT(context, !pulses && f.chip.ReadByte(0)==1); f.Tick(1); AT_PORTABLE_TEST_ASSERT(context, pulses==1 && f.chip.ReadByte(0)==3); f.Tick(period*2); AT_PORTABLE_TEST_ASSERT(context, pulses==3 && f.irq.empty()); }
	return true;
}
bool ATTestEmuCTCExternalTrigger(ATPortableTestContext& context) {
	Timer f; f.chip.WriteByte(0,0xA0); f.chip.WriteByte(0,0x9F); f.chip.WriteByte(0,3); f.Tick(1000); AT_PORTABLE_TEST_ASSERT(context, f.irq.empty()); f.chip.SetInputState(0,true); f.Tick(47); AT_PORTABLE_TEST_ASSERT(context, f.irq.empty()); f.Tick(1); AT_PORTABLE_TEST_ASSERT(context, f.irq==std::vector<sint32>{0xA0}); return true;
}
bool ATTestEmuCTCCounter(ATPortableTestContext& context) {
	for(bool enabled:{false,true}) { Timer f; uint32 pulses=0; f.chip.SetUnderflowFn(1,[&] { ++pulses; }); f.chip.WriteByte(0,0xA0); f.chip.WriteByte(1,enabled ? 0xD7 : 0x57); f.chip.WriteByte(1,3); f.Pulse(1); AT_PORTABLE_TEST_ASSERT(context, f.chip.ReadByte(1)==2 && !pulses && f.irq.empty()); f.Pulse(1); f.Pulse(1); AT_PORTABLE_TEST_ASSERT(context, f.chip.ReadByte(1)==3 && pulses==1 && f.irq==(enabled ? std::vector<sint32>{0xA2} : std::vector<sint32>{})); }
	return true;
}
bool ATTestEmuCTCZeroConstant(ATPortableTestContext& context) {
	Timer f; f.chip.WriteByte(2,0x57); f.chip.WriteByte(2,0); f.Pulse(2); AT_PORTABLE_TEST_ASSERT(context, f.chip.ReadByte(2)==255); for(uint32 i=0;i<255;++i) f.Pulse(2); AT_PORTABLE_TEST_ASSERT(context, f.chip.ReadByte(2)==0 && f.irq.empty()); return true;
}
bool ATTestEmuCTCCascade(ATPortableTestContext& context) {
	Timer f; f.chip.SetUnderflowFn(2,[&] { f.Pulse(3); }); f.chip.WriteByte(0,0xA0); f.chip.WriteByte(3,0xD7); f.chip.WriteByte(3,3); f.chip.WriteByte(2,7); f.chip.WriteByte(2,2); f.Tick(95); AT_PORTABLE_TEST_ASSERT(context, f.irq.empty()); f.Tick(1); AT_PORTABLE_TEST_ASSERT(context, f.irq==std::vector<sint32>{0xA6}); return true;
}
bool ATTestEmuCTCPriority(ATPortableTestContext& context) {
	Timer f; f.chip.WriteByte(0,0xA0); for(uint8 ch:{0,1}) { f.chip.WriteByte(ch,0xD7); f.chip.WriteByte(ch,1); } f.Pulse(0); f.Pulse(1); AT_PORTABLE_TEST_ASSERT(context, f.irq==std::vector<sint32>{0xA0}); f.chip.AcknowledgeInterrupt(); f.Tick(1); AT_PORTABLE_TEST_ASSERT(context, f.irq==(std::vector<sint32>{0xA0,0xA2})); f.chip.AcknowledgeInterrupt(); f.Tick(100); AT_PORTABLE_TEST_ASSERT(context, f.irq.size()==2);
	// Higher-priority counters and timers can interrupt a lower-priority service.
	for(bool timer:{false,true}) { Timer nested; nested.chip.WriteByte(0,0xA0); nested.chip.WriteByte(1,0xD7); nested.chip.WriteByte(1,1); nested.Pulse(1);
		nested.chip.WriteByte(0,timer ? 0x87 : 0xD7); nested.chip.WriteByte(0,timer ? 2 : 1);
		if(timer) { nested.Tick(16); nested.chip.WriteByte(2,0xD7); nested.chip.WriteByte(2,1); nested.Pulse(2); nested.Tick(16); } else nested.Pulse(0);
		AT_PORTABLE_TEST_ASSERT(context, nested.irq==(std::vector<sint32>{0xA2,0xA0})); nested.chip.AcknowledgeInterrupt(); nested.chip.WriteByte(0,3); nested.chip.AcknowledgeInterrupt(); nested.Tick(1);
		if(timer) { AT_PORTABLE_TEST_ASSERT(context, nested.irq==(std::vector<sint32>{0xA2,0xA0,0xA4})); nested.chip.AcknowledgeInterrupt(); } nested.Tick(100); AT_PORTABLE_TEST_ASSERT(context, nested.irq.size()==(timer ? 3u : 2u));
	} return true;
}
bool ATTestEmuCTCResetPending(ATPortableTestContext& context) {
	Timer f; uint32 pulses=0; f.chip.SetUnderflowFn(0,[&] { ++pulses; }); f.chip.WriteByte(0,0x87); f.chip.WriteByte(0,3); f.Tick(20); f.chip.Reset(); f.Tick(500); AT_PORTABLE_TEST_ASSERT(context, f.irq==std::vector<sint32>{-1} && !pulses && f.chip.ReadByte(0)==0); f.chip.WriteByte(0,7); f.chip.WriteByte(0,2); f.Tick(32); AT_PORTABLE_TEST_ASSERT(context, pulses==1 && f.irq.size()==1); return true;
}
bool ATTestEmuCTCEdges(ATPortableTestContext& context) {
	Timer f; f.chip.WriteByte(0,0xC7); f.chip.WriteByte(0,2); f.chip.SetInputState(0,true); AT_PORTABLE_TEST_ASSERT(context, f.chip.ReadByte(0)==2); f.chip.SetInputState(0,false); AT_PORTABLE_TEST_ASSERT(context, f.chip.ReadByte(0)==1 && f.irq.empty()); f.Pulse(0); AT_PORTABLE_TEST_ASSERT(context, f.irq==std::vector<sint32>{0}); return true;
}
