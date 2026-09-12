// Altirra portable CPU memory page-map tests

#include <array>

#include <at/attest/portabletest.h>
#include <cpumemory.h>

namespace {
	class TestCPUMemory final : public ATCPUEmulatorMemory {
	public:
		TestCPUMemory() {
			const uintptr base = reinterpret_cast<uintptr>(mBytes.data());
			for (uintptr& page : mReadPages)
				page = base;
			for (uintptr& page : mWritePages)
				page = base;
			for (uintptr& page : mBankReadPages)
				page = base;
			for (uintptr& page : mBankWritePages)
				page = base;
			for (PageTablePtr& bank : mReadBanks)
				bank = &mBankReadPages;
			for (PageTablePtr& bank : mWriteBanks)
				bank = &mBankWritePages;

			mpCPUReadPageMap = &mReadPages;
			mpCPUWritePageMap = &mWritePages;
			mpCPUReadBankMap = &mReadBanks;
			mpCPUWriteBankMap = &mWriteBanks;
			mpCPUReadAddressPageMap = nullptr;
		}

		uint8 CPUReadByte(uint32 address) override {
			++mCPUReads;
			mLastReadAddress = address;
			return mBytes[address & 0xffff];
		}
		uint8 CPUExtReadByte(uint16 address, uint8 bank) override {
			++mExtReads;
			mLastBank = bank;
			return mBytes[address];
		}
		sint32 CPUExtReadByteAccel(uint16 address, uint8 bank, bool chipOK) override {
			++mAccelReads;
			mLastBank = bank;
			return chipOK ? mBytes[address] : -7;
		}
		uint8 CPUDebugReadByte(uint16 address) const override {
			++mDebugReads;
			return mBytes[address];
		}
		uint8 CPUDebugExtReadByte(uint16 address, uint8 bank) const override {
			++mDebugExtReads;
			mLastDebugBank = bank;
			return mBytes[address];
		}
		void CPUWriteByte(uint16 address, uint8 value) override {
			++mCPUWrites;
			mBytes[address] = value;
		}
		void CPUExtWriteByte(uint16 address, uint8 bank, uint8 value) override {
			++mExtWrites;
			mLastBank = bank;
			mBytes[address] = value;
		}
		sint32 CPUExtWriteByteAccel(uint16 address, uint8 bank, uint8 value, bool chipOK) override {
			++mAccelWrites;
			mLastBank = bank;
			if (chipOK)
				mBytes[address] = value;
			return chipOK ? 3 : -3;
		}

		alignas(16) std::array<uint8, 65536> mBytes {};
		PageTable mReadPages {};
		PageTable mWritePages {};
		PageTable mBankReadPages {};
		PageTable mBankWritePages {};
		BankTable mReadBanks {};
		BankTable mWriteBanks {};
		uint32 mCPUReads = 0;
		uint32 mCPUWrites = 0;
		uint32 mExtReads = 0;
		uint32 mExtWrites = 0;
		uint32 mAccelReads = 0;
		uint32 mAccelWrites = 0;
		uint32 mLastReadAddress = 0;
		uint8 mLastBank = 0;
		mutable uint32 mDebugReads = 0;
		mutable uint32 mDebugExtReads = 0;
		mutable uint8 mLastDebugBank = 0;
	};
}

bool ATTestAltirraCPUMemory(ATPortableTestContext& context) {
	TestCPUMemory mem;
	AT_PORTABLE_TEST_ASSERT(context, !(reinterpret_cast<uintptr>(mem.mBytes.data()) & 1));
	mem.mBytes[0x1234] = 0x42;
	AT_PORTABLE_TEST_ASSERT(context, mem.ReadByte(0x1234) == 0x42);
	AT_PORTABLE_TEST_ASSERT(context, mem.ReadByteAddr16(0x1234) == 0x42);
	AT_PORTABLE_TEST_ASSERT(context, mem.DebugReadByte(0x1234) == 0x42);
	mem.DummyReadByte(0x1234);
	AT_PORTABLE_TEST_ASSERT(context, mem.mCPUReads == 0);
	AT_PORTABLE_TEST_ASSERT(context, mem.mDebugReads == 0);
	mem.WriteByte(0x1234, 0x65);
	AT_PORTABLE_TEST_ASSERT(context, mem.mBytes[0x1234] == 0x65);
	AT_PORTABLE_TEST_ASSERT(context, mem.mCPUWrites == 0);

	mem.mReadPages[0x20] = 1;
	mem.mWritePages[0x20] = 1;
	mem.mBytes[0x2034] = 0xA5;
	AT_PORTABLE_TEST_ASSERT(context, mem.ReadByte(0x12034) == 0xA5);
	AT_PORTABLE_TEST_ASSERT(context, mem.mLastReadAddress == 0x12034);
	AT_PORTABLE_TEST_ASSERT(context, mem.ReadByteAddr16(0x2034) == 0xA5);
	mem.DummyReadByte(0x2034);
	AT_PORTABLE_TEST_ASSERT(context, mem.mCPUReads == 3);
	AT_PORTABLE_TEST_ASSERT(context, mem.DebugReadByte(0x2034) == 0xA5);
	AT_PORTABLE_TEST_ASSERT(context, mem.mDebugReads == 1);
	mem.WriteByte(0x2034, 0xB6);
	AT_PORTABLE_TEST_ASSERT(context, mem.mCPUWrites == 1);
	AT_PORTABLE_TEST_ASSERT(context, mem.mBytes[0x2034] == 0xB6);

	mem.mBytes[0x3456] = 0x31;
	AT_PORTABLE_TEST_ASSERT(context, mem.ExtReadByte(0x3456, 3) == 0x31);
	AT_PORTABLE_TEST_ASSERT(context, mem.ExtReadByteAccel(0x3456, 3, true) == 0x31);
	AT_PORTABLE_TEST_ASSERT(context, mem.DebugExtReadByte(0x3456, 3) == 0x31);
	mem.DummyExtReadByte(0x3456, 3);
	AT_PORTABLE_TEST_ASSERT(context, mem.mExtReads == 0);
	AT_PORTABLE_TEST_ASSERT(context, mem.mAccelReads == 0);
	AT_PORTABLE_TEST_ASSERT(context, mem.mDebugExtReads == 0);
	mem.ExtWriteByte(0x3456, 3, 0x52);
	AT_PORTABLE_TEST_ASSERT(context, mem.ExtWriteByteAccel(0x3456, 3, 0x63, true) == 0);
	AT_PORTABLE_TEST_ASSERT(context, mem.mBytes[0x3456] == 0x63);
	AT_PORTABLE_TEST_ASSERT(context, mem.mExtWrites == 0);
	AT_PORTABLE_TEST_ASSERT(context, mem.mAccelWrites == 0);

	mem.mBankReadPages[0x40] = 1;
	mem.mBankWritePages[0x40] = 1;
	mem.mBytes[0x4012] = 0x74;
	AT_PORTABLE_TEST_ASSERT(context, mem.ExtReadByte(0x4012, 2) == 0x74);
	AT_PORTABLE_TEST_ASSERT(context, mem.mExtReads == 1);
	AT_PORTABLE_TEST_ASSERT(context, mem.mLastBank == 2);
	mem.DummyExtReadByte(0x4012, 2);
	AT_PORTABLE_TEST_ASSERT(context, mem.mExtReads == 2);
	AT_PORTABLE_TEST_ASSERT(context, mem.ExtReadByteAccel(0x4012, 2, true) == 0x74);
	AT_PORTABLE_TEST_ASSERT(context, mem.ExtReadByteAccel(0x4012, 2, false) == -7);
	AT_PORTABLE_TEST_ASSERT(context, mem.mAccelReads == 2);
	AT_PORTABLE_TEST_ASSERT(context, mem.DebugExtReadByte(0x4012, 2) == 0x74);
	AT_PORTABLE_TEST_ASSERT(context, mem.mDebugExtReads == 1);
	AT_PORTABLE_TEST_ASSERT(context, mem.mLastDebugBank == 2);
	mem.ExtWriteByte(0x4012, 2, 0x85);
	AT_PORTABLE_TEST_ASSERT(context, mem.mExtWrites == 1);
	AT_PORTABLE_TEST_ASSERT(context, mem.ExtWriteByteAccel(0x4012, 2, 0x96, true) == 3);
	AT_PORTABLE_TEST_ASSERT(context, mem.ExtWriteByteAccel(0x4012, 2, 0xA7, false) == -3);
	AT_PORTABLE_TEST_ASSERT(context, mem.mAccelWrites == 2);
	AT_PORTABLE_TEST_ASSERT(context, mem.mBytes[0x4012] == 0x96);
	return true;
}
