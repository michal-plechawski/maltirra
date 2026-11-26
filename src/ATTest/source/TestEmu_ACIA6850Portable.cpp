#include <vector>
#include <at/atcore/scheduler.h>
#include <at/atemulation/acia6850.h>
#include <at/attest/portabletest.h>
namespace {
	struct Serial {
		Serial() { clock.SetRate(VDFraction(1000000,1)); chip.Init(&clock); chip.SetTransmitFn([this](uint8 b,uint32 rate) { bytes.push_back(b); rates.push_back(rate); }); chip.SetInterruptFn([this](bool v) { irq.push_back(v); }); chip.SetMasterClockPeriod(208); chip.Reset(); }
		~Serial() { chip.Shutdown(); }
		void Tick(uint32 n) { while(n--) ATSCHEDULER_ADVANCE(&clock); }
		ATScheduler clock; ATACIA6850Emulator chip; std::vector<uint8> bytes; std::vector<uint32> rates; std::vector<bool> irq;
	};
}

bool ATTestEmuACIA6850Reset(ATPortableTestContext& context) {
	Serial f; f.chip.SetCTS(true); f.chip.SetDCD(true); f.chip.Reset();
	AT_PORTABLE_TEST_ASSERT(context, (f.chip.ReadByte(0)&0x8F)==0x0E); f.chip.WriteByte(0,0x15); f.Tick(2000);
	AT_PORTABLE_TEST_ASSERT(context, f.bytes.empty() && (f.chip.ReadByte(0)&2));
	return true;
}

bool ATTestEmuACIA6850Transmit(ATPortableTestContext& context) {
	Serial f; f.chip.WriteByte(0,0x15); f.chip.WriteByte(1,0xA6); f.chip.WriteByte(1,0x5B);
	AT_PORTABLE_TEST_ASSERT(context, !(f.chip.ReadByte(0)&2)); f.Tick(519); AT_PORTABLE_TEST_ASSERT(context, f.bytes.empty()); f.Tick(1);
	AT_PORTABLE_TEST_ASSERT(context, f.bytes==std::vector<uint8>{0xA6} && (f.chip.ReadByte(0)&2)); f.Tick(520);
	AT_PORTABLE_TEST_ASSERT(context, f.bytes==(std::vector<uint8>{0xA6,0x5B}) && f.rates==(std::vector<uint32>{52,52}));
	return true;
}

bool ATTestEmuACIA6850ResetPending(ATPortableTestContext& context) {
	Serial f; f.chip.WriteByte(0,0x15); f.chip.WriteByte(1,0xA6); f.chip.WriteByte(1,0x5B); f.Tick(100); f.chip.Reset(); f.chip.WriteByte(0,0x15); f.Tick(2000);
	AT_PORTABLE_TEST_ASSERT(context, f.bytes.empty() && (f.chip.ReadByte(0)&2)); f.chip.WriteByte(1,0x37); f.Tick(520);
	AT_PORTABLE_TEST_ASSERT(context, f.bytes==std::vector<uint8>{0x37});
	return true;
}

bool ATTestEmuACIA6850Receive(ATPortableTestContext& context) {
	Serial f; f.chip.WriteByte(0,0x15); f.chip.ReceiveByte(0xE3,52);
	AT_PORTABLE_TEST_ASSERT(context, (f.chip.ReadByte(0)&0x11)==1 && f.chip.DebugReadByte(1)==0xE3 && (f.chip.ReadByte(0)&1)); f.chip.ReadByte(1); AT_PORTABLE_TEST_ASSERT(context, !(f.chip.ReadByte(0)&1));
	f.chip.ReceiveByte(0x67,20); AT_PORTABLE_TEST_ASSERT(context, f.chip.ReadByte(0)&0x10); f.chip.ReceiveByte(0x68,52); AT_PORTABLE_TEST_ASSERT(context, !(f.chip.ReadByte(0)&0x10));
	return true;
}

bool ATTestEmuACIA6850Rates(ATPortableTestContext& context) {
	for(uint32 mode=0;mode<3;++mode) {
		Serial f; f.chip.SetMasterClockPeriod(mode==1 ? 1664 : 208); f.chip.WriteByte(0,mode==0 ? 0x15 : mode==1 ? 0x14 : 0x16); f.chip.WriteByte(1,0x37); f.Tick(3000);
		AT_PORTABLE_TEST_ASSERT(context, f.bytes==std::vector<uint8>{0x37} && f.rates==std::vector<uint32>{mode==0 ? 52u : mode==1 ? 26u : 208u});
	}
	return true;
}

bool ATTestEmuACIA6850IRQ(ATPortableTestContext& context) {
	Serial f; f.chip.WriteByte(0,0x95); f.chip.ReceiveByte(0xA6,52); AT_PORTABLE_TEST_ASSERT(context, (f.chip.ReadByte(0)&0x81)==0x81 && f.irq.back());
	f.chip.DebugReadByte(1); AT_PORTABLE_TEST_ASSERT(context, f.irq.back()); f.chip.ReadByte(1); AT_PORTABLE_TEST_ASSERT(context, !f.irq.back() && !(f.chip.ReadByte(0)&0x80));
	f.chip.WriteByte(0,0x35); AT_PORTABLE_TEST_ASSERT(context, f.irq.back()); f.chip.WriteByte(1,0x37); f.chip.WriteByte(1,0x68); AT_PORTABLE_TEST_ASSERT(context, !f.irq.back()); f.Tick(520); AT_PORTABLE_TEST_ASSERT(context, f.irq.back());
	return true;
}
