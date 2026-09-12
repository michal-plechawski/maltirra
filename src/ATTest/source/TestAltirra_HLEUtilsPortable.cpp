// Altirra portable HLE disk-I/O register tests

#include <array>

#include <at/attest/portabletest.h>
#include <hleutils.h>
#include <kerneldb.h>

namespace {
	class TestHLEMemory final : public ATCPUEmulatorMemory {
	public:
		TestHLEMemory() {
			const uintptr base = reinterpret_cast<uintptr>(mBytes.data());
			for (uintptr& page : mReadPages)
				page = base;
			for (uintptr& page : mWritePages)
				page = base;
			mpCPUReadPageMap = &mReadPages;
			mpCPUWritePageMap = &mWritePages;
		}

		uint8 CPUReadByte(uint32 address) override { return mBytes[address & 0xffff]; }
		void CPUWriteByte(uint16 address, uint8 value) override { mBytes[address] = value; }
		uint8 CPUExtReadByte(uint16, uint8) override { return 0; }
		sint32 CPUExtReadByteAccel(uint16, uint8, bool) override { return 0; }
		uint8 CPUDebugReadByte(uint16) const override { return 0; }
		uint8 CPUDebugExtReadByte(uint16, uint8) const override { return 0; }
		void CPUExtWriteByte(uint16, uint8, uint8) override {}
		sint32 CPUExtWriteByteAccel(uint16, uint8, uint8, bool) override { return 0; }

		alignas(16) std::array<uint8, 65536> mBytes {};
		PageTable mReadPages {};
		PageTable mWritePages {};
	};
}

bool ATTestAltirraHLEUtils(ATPortableTestContext& context) {
	TestHLEMemory mem;
	ATKernelDatabase kdb(&mem);
	mem.mBytes.fill(0xA5);
	mem.mBytes[ATKernelSymbols::POKMSK] = 0xFF;
	mem.mBytes[ATKernelSymbols::IRQEN] = 0x12;
	mem.mBytes[ATKernelSymbols::AUDCTL] = 0x34;
	auto expected = mem.mBytes;
	for (int i = 0; i < 4; ++i)
		expected[ATKernelSymbols::AUDC1 + i + i] = 0;
	expected[ATKernelSymbols::POKMSK] = 0xC7;
	expected[ATKernelSymbols::IRQEN] = 0xC7;
	expected[ATKernelSymbols::AUDCTL] = 0x28;

	ATClearPokeyTimersOnDiskIo(kdb);
	AT_PORTABLE_TEST_ASSERT(context, mem.mBytes == expected);

	mem.mBytes[ATKernelSymbols::POKMSK] = 0x38;
	mem.mBytes[ATKernelSymbols::IRQEN] = 0x9A;
	mem.mBytes[ATKernelSymbols::AUDCTL] = 0x00;
	for (int i = 0; i < 4; ++i)
		mem.mBytes[ATKernelSymbols::AUDC1 + i + i] = 0xF0;
	expected = mem.mBytes;
	for (int i = 0; i < 4; ++i)
		expected[ATKernelSymbols::AUDC1 + i + i] = 0;
	expected[ATKernelSymbols::POKMSK] = 0;
	expected[ATKernelSymbols::IRQEN] = 0;
	expected[ATKernelSymbols::AUDCTL] = 0x28;

	ATClearPokeyTimersOnDiskIo(kdb);
	AT_PORTABLE_TEST_ASSERT(context, mem.mBytes == expected);
	return true;
}
