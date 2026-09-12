// Altirra portable kernel register adapter tests

#include <array>

#include <at/attest/portabletest.h>
#include <kerneldb.h>

namespace {
	class TestKernelMemory final : public ATCPUEmulatorMemory {
	public:
		TestKernelMemory() {
			const uintptr base = reinterpret_cast<uintptr>(mBytes.data());
			for (uintptr& page : mReadPages)
				page = base;
			for (uintptr& page : mWritePages)
				page = base;
			mpCPUReadPageMap = &mReadPages;
			mpCPUWritePageMap = &mWritePages;
		}

		uint8 CPUReadByte(uint32 address) override {
			++mVirtualReads;
			return mBytes[address & 0xffff];
		}
		void CPUWriteByte(uint16 address, uint8 value) override {
			++mVirtualWrites;
			mBytes[address] = value;
		}
		uint8 CPUExtReadByte(uint16, uint8) override { return 0; }
		sint32 CPUExtReadByteAccel(uint16, uint8, bool) override { return 0; }
		uint8 CPUDebugReadByte(uint16) const override { return 0; }
		uint8 CPUDebugExtReadByte(uint16, uint8) const override { return 0; }
		void CPUExtWriteByte(uint16, uint8, uint8) override {}
		sint32 CPUExtWriteByteAccel(uint16, uint8, uint8, bool) override { return 0; }

		alignas(16) std::array<uint8, 65536> mBytes {};
		PageTable mReadPages {};
		PageTable mWritePages {};
		uint32 mVirtualReads = 0;
		uint32 mVirtualWrites = 0;
	};
}

bool ATTestAltirraKernelDB(ATPortableTestContext& context) {
	TestKernelMemory mem;
	ATKernelDatabase kdb(&mem);
	AT_PORTABLE_TEST_ASSERT(context, sizeof kdb == sizeof(void *));
	AT_PORTABLE_TEST_ASSERT(context, kdb.mAdapter.mpMem == &mem);

	kdb.POKMSK = 0xF3;
	AT_PORTABLE_TEST_ASSERT(context, mem.mBytes[ATKernelSymbols::POKMSK] == 0xF3);
	AT_PORTABLE_TEST_ASSERT(context, uint8(kdb.POKMSK) == 0xF3);
	AT_PORTABLE_TEST_ASSERT(context, (kdb.POKMSK &= 0xC7) == 0xC3);
	AT_PORTABLE_TEST_ASSERT(context, (kdb.POKMSK |= 0x08) == 0xCB);
	AT_PORTABLE_TEST_ASSERT(context, mem.mBytes[ATKernelSymbols::POKMSK] == 0xCB);

	kdb.DOSVEC = 0x12FF;
	AT_PORTABLE_TEST_ASSERT(context, mem.mBytes[ATKernelSymbols::DOSVEC] == 0xFF);
	AT_PORTABLE_TEST_ASSERT(context, mem.mBytes[ATKernelSymbols::DOSVEC + 1] == 0x12);
	AT_PORTABLE_TEST_ASSERT(context, uint16(kdb.DOSVEC) == 0x12FF);
	AT_PORTABLE_TEST_ASSERT(context, ++kdb.DOSVEC == 0x1300);
	AT_PORTABLE_TEST_ASSERT(context, --kdb.DOSVEC == 0x12FF);
	kdb.DOSVEC.Lo() = 0x34;
	kdb.DOSVEC.Hi() = 0x56;
	AT_PORTABLE_TEST_ASSERT(context, uint16(kdb.DOSVEC) == 0x5634);

	kdb.BRKKEY = 0xFF;
	AT_PORTABLE_TEST_ASSERT(context, ++kdb.BRKKEY == 0x00);
	AT_PORTABLE_TEST_ASSERT(context, --kdb.BRKKEY == 0xFF);
	AT_PORTABLE_TEST_ASSERT(context, (kdb.BRKKEY -= 2) == 0xFD);
	kdb.AUDC1[2] = 0x77;
	AT_PORTABLE_TEST_ASSERT(context, mem.mBytes[ATKernelSymbols::AUDC1 + 2] == 0x77);
	AT_PORTABLE_TEST_ASSERT(context, uint8(kdb.AUDC1[2]) == 0x77);

	ATByteVAdapter variable(&mem, 0x3000);
	variable.w16(0xABCD);
	AT_PORTABLE_TEST_ASSERT(context, variable.r16() == 0xABCD);
	AT_PORTABLE_TEST_ASSERT(context, (variable &= 0xF0) == 0xC0);
	AT_PORTABLE_TEST_ASSERT(context, (variable |= 0x05) == 0xC5);
	AT_PORTABLE_TEST_ASSERT(context, mem.mBytes[0x3000] == 0xC5);
	AT_PORTABLE_TEST_ASSERT(context, mem.mBytes[0x3001] == 0xAB);

	ATKernelDatabase5200 kdb5200(&mem);
	AT_PORTABLE_TEST_ASSERT(context, sizeof kdb5200 == sizeof(void *));
	kdb5200.POKMSK = 0x41;
	AT_PORTABLE_TEST_ASSERT(context, mem.mBytes[ATKernelSymbols5200::POKMSK] == 0x41);
	kdb5200.RTCLOK = 0xBEEF;
	AT_PORTABLE_TEST_ASSERT(context, mem.mBytes[ATKernelSymbols5200::RTCLOK] == 0xEF);
	AT_PORTABLE_TEST_ASSERT(context, mem.mBytes[ATKernelSymbols5200::RTCLOK + 1] == 0xBE);

	mem.mReadPages[ATKernelSymbols::POKMSK >> 8] = 1;
	mem.mWritePages[ATKernelSymbols::POKMSK >> 8] = 1;
	kdb.POKMSK = 0x92;
	AT_PORTABLE_TEST_ASSERT(context, mem.mVirtualWrites == 1);
	AT_PORTABLE_TEST_ASSERT(context, uint8(kdb.POKMSK) == 0x92);
	AT_PORTABLE_TEST_ASSERT(context, mem.mVirtualReads == 1);
	return true;
}
