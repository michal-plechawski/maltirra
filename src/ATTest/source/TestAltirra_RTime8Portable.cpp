// Altirra portable R-Time 8 tests

#include <memory>

#include <at/attest/portabletest.h>
#include <memorymanager.h>
#include <rtime8.h>

bool ATTestAltirraRTime8(ATPortableTestContext& context) {
	ATRTime8Emulator rtc;
	AT_PORTABLE_TEST_ASSERT(context, rtc.ReadControl(0) == 0);

	// Registers 8-15 are deterministic battery-backed storage. The RTC ignores
	// the upper data nibble and transfers the register in two four-bit phases.
	rtc.WriteControl(0, 8);
	rtc.WriteControl(0, 0xCA);
	rtc.WriteControl(0, 0xDB);
	rtc.WriteControl(0, 8);
	AT_PORTABLE_TEST_ASSERT(context, rtc.DebugReadControl(0) == 0x0A);
	AT_PORTABLE_TEST_ASSERT(context, rtc.DebugReadControl(7) == 0x0A);
	AT_PORTABLE_TEST_ASSERT(context, rtc.ReadControl(0) == 0x0A);
	AT_PORTABLE_TEST_ASSERT(context, rtc.ReadControl(0) == 0x0B);
	AT_PORTABLE_TEST_ASSERT(context, rtc.ReadControl(0) == 0x00);

	rtc.WriteControl(0, 9);
	rtc.Reset();
	AT_PORTABLE_TEST_ASSERT(context, rtc.ReadControl(0) == 0x00);

	auto memory = std::make_unique<ATMemoryManager>();
	memory->Init();
	ATDeviceRTime8 device;
	device.InitMemMap(memory.get());

	uint32 lo = 0;
	uint32 hi = 0;
	AT_PORTABLE_TEST_ASSERT(context, device.GetMappedRange(0, lo, hi));
	AT_PORTABLE_TEST_ASSERT(context, lo == 0xD5B8);
	AT_PORTABLE_TEST_ASSERT(context, hi == 0xD5C0);
	AT_PORTABLE_TEST_ASSERT(context, !device.GetMappedRange(1, lo, hi));
	AT_PORTABLE_TEST_ASSERT(context, memory->ReadByte(0xD5B7) == 0xFF);
	AT_PORTABLE_TEST_ASSERT(context, memory->ReadByte(0xD5C0) == 0xFF);

	memory->WriteByte(0xD5B8, 9);
	memory->WriteByte(0xD5B8, 0xCC);
	memory->WriteByte(0xD5B8, 0xDD);
	memory->WriteByte(0xD5BF, 9);
	AT_PORTABLE_TEST_ASSERT(context, memory->DebugReadByte(0xD5B8) == 0xFC);
	AT_PORTABLE_TEST_ASSERT(context, memory->DebugReadByte(0xD5BF) == 0xFC);
	AT_PORTABLE_TEST_ASSERT(context, memory->ReadByte(0xD5B8) == 0xFC);
	AT_PORTABLE_TEST_ASSERT(context, memory->ReadByte(0xD5BF) == 0xFD);
	AT_PORTABLE_TEST_ASSERT(context, memory->ReadByte(0xD5B8) == 0xF0);

	device.Shutdown();
	return true;
}
