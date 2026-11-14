// Portable production-MMU tests for Axlon and linear high memory.

#include <memory>
#include <vector>

#include <at/attest/portabletest.h>
#include "mmuportablefixture.h"

bool ATTestAltirraMMUAxlon(ATPortableTestContext& context) {
	auto fixture=std::make_unique<MMUPortableFixture>(kATHardwareMode_800,kATMemoryMode_64K);
	fixture->mMMU.SetAxlonMemory(2,false,fixture->mAxlonRAM.data());
	fixture->mMMU.SetAxlonBank(0);
	const uint8 baseValue=fixture->mMemory.CPUReadByte(0x4000);
	AT_PORTABLE_TEST_ASSERT(context, baseValue==fixture->mRAM[0x4000]);
	AT_PORTABLE_TEST_ASSERT(context, baseValue==0x21);

	// The low control mirror is absent when aliasing is disabled, and writes pass through.
	fixture->mMemory.CPUWriteByte(0x0FF0,2);
	AT_PORTABLE_TEST_ASSERT(context, fixture->mMMU.GetAxlonBank()==0);
	AT_PORTABLE_TEST_ASSERT(context, fixture->mRAM[0x0FF0]==2);

	// The 800 kernel is always visible, so its high selector remains decoded.
	fixture->mMemory.CPUWriteByte(0xCFF0,1);
	AT_PORTABLE_TEST_ASSERT(context, fixture->mMMU.GetAxlonBank()==1);
	AT_PORTABLE_TEST_ASSERT(context, fixture->mMemory.CPUReadByte(0x4000)==1);
	AT_PORTABLE_TEST_ASSERT(context, fixture->mMemory.AnticReadByte(0x4000)==1);
	fixture->mMemory.CPUWriteByte(0x4000,0xA1);
	AT_PORTABLE_TEST_ASSERT(context, fixture->mAxlonRAM[0]==0xA1);

	fixture->mMemory.CPUWriteByte(0xCFF0,2);
	AT_PORTABLE_TEST_ASSERT(context, fixture->mMemory.CPUReadByte(0x4000)==2);
	AT_PORTABLE_TEST_ASSERT(context, fixture->mMemory.AnticReadByte(0x4000)==2);
	fixture->mMemory.CPUWriteByte(0xCFF0,3);
	AT_PORTABLE_TEST_ASSERT(context, fixture->mMMU.GetAxlonBank()==3);
	AT_PORTABLE_TEST_ASSERT(context, fixture->mMemory.CPUReadByte(0x4000)==3);

	// Bank bits mask selectors; zero returns to base RAM.
	fixture->mMemory.CPUWriteByte(0xCFF0,4);
	AT_PORTABLE_TEST_ASSERT(context, fixture->mMMU.GetAxlonBank()==0);
	AT_PORTABLE_TEST_ASSERT(context, fixture->mMemory.CPUReadByte(0x4000)==baseValue);

	// Enabling low-address aliasing installs the 0FF0 selector; it also passes the write through.
	fixture->mMMU.SetAxlonMemory(2,true,fixture->mAxlonRAM.data());
	fixture->mMMU.SetAxlonBank(0);
	fixture->mMemory.CPUWriteByte(0x0FF0,3);
	AT_PORTABLE_TEST_ASSERT(context, fixture->mMMU.GetAxlonBank()==3);
	AT_PORTABLE_TEST_ASSERT(context, fixture->mRAM[0x0FF0]==3);
	AT_PORTABLE_TEST_ASSERT(context, fixture->mMemory.CPUReadByte(0x4000)==3);

	// XL PORTB can hide the kernel and its high selector. The low mirror is
	// specifically an 800 feature, even when aliasing is configured.
	fixture->Map(kATHardwareMode_130XE,kATMemoryMode_64K);
	fixture->mMMU.SetAxlonBank(0);
	fixture->mMMU.SetBankRegister(0);
	fixture->mMemory.CPUWriteByte(0xCFF0,2);
	fixture->mMemory.CPUWriteByte(0x0FF0,3);
	AT_PORTABLE_TEST_ASSERT(context, fixture->mMMU.GetAxlonBank()==0 && fixture->mRAM[0xCFF0]==2);
	fixture->mMMU.SetBankRegister(1);
	fixture->mMemory.CPUWriteByte(0xCFF0,1);
	AT_PORTABLE_TEST_ASSERT(context, fixture->mMMU.GetAxlonBank()==1 && fixture->mMemory.CPUReadByte(0x4000)==0xA1);
	for (uint8 bits=1; bits<=8; ++bits) {
		fixture->mMMU.SetAxlonMemory(bits,false,fixture->mAxlonRAM.data());
		fixture->mMMU.SetAxlonBank(0xFF);
		const uint32 bank=(1U<<bits)-1;
		AT_PORTABLE_TEST_ASSERT(context, fixture->mMMU.GetAxlonBank()==bank);
		AT_PORTABLE_TEST_ASSERT(context, fixture->mMemory.CPUReadByte(0x4000)==fixture->mAxlonRAM[(bank-1)*0x4000]);
		AT_PORTABLE_TEST_ASSERT(context, fixture->mMemory.AnticReadByte(0x4000)==fixture->mAxlonRAM[(bank-1)*0x4000]);
	}

	fixture->mMMU.SetAxlonMemory(0,false,nullptr);
	AT_PORTABLE_TEST_ASSERT(context, fixture->mMMU.GetAxlonBank()==0);
	AT_PORTABLE_TEST_ASSERT(context, fixture->mMemory.CPUReadByte(0x4000)==baseValue);
	fixture->mMemory.CPUWriteByte(0xCFF0,1);
	AT_PORTABLE_TEST_ASSERT(context, fixture->mMMU.GetAxlonBank()==0);
	return true;
}

bool ATTestAltirraMMUHighMemory(ATPortableTestContext& context) {
	// Storage is declared before the fixture so MMU shutdown removes each mapping first.
	std::vector<uint8> first(2*0x10000,0xA1);
	std::vector<uint8> replacement(0x10000,0xB2);
	std::vector<uint8> full(255*0x10000,0xD4);
	auto fixture=std::make_unique<MMUPortableFixture>(kATHardwareMode_130XE,kATMemoryMode_128K);
	fixture->mRAM[0x4567]=0x31;
	fixture->mMMU.SetHighMemory(2,first.data());
	AT_PORTABLE_TEST_ASSERT(context, fixture->mMemory.ExtReadByte(0x4567,0)==0x31);
	AT_PORTABLE_TEST_ASSERT(context, fixture->mMemory.ExtReadByte(0x4567,1)==0xA1);
	AT_PORTABLE_TEST_ASSERT(context, fixture->mMemory.ExtReadByte(0x4567,2)==0xA1);
	fixture->mMemory.ExtWriteByte(0x4567,1,0xC1);
	fixture->mMemory.ExtWriteByte(0x4567,2,0xC2);
	AT_PORTABLE_TEST_ASSERT(context, first[0x4567]==0xC1 && first[0x10000+0x4567]==0xC2);

	fixture->mMMU.SetHighMemory(1,replacement.data());
	AT_PORTABLE_TEST_ASSERT(context, fixture->mMemory.ExtReadByte(0x4567,1)==0xB2);
	AT_PORTABLE_TEST_ASSERT(context, fixture->mMemory.ExtReadByte(0x4567,2)==0xFF);
	fixture->mMemory.ExtWriteByte(0x4567,2,0xD2);
	AT_PORTABLE_TEST_ASSERT(context, replacement[0x4567]==0xB2);

	fixture->mMMU.SetHighMemory(255,full.data());
	AT_PORTABLE_TEST_ASSERT(context, fixture->mMemory.ExtReadByte(0xFFFF,255)==0xD4);
	fixture->mMemory.ExtWriteByte(0xFFFF,255,0x22);
	AT_PORTABLE_TEST_ASSERT(context, full.back()==0x22);

	fixture->mMMU.SetHighMemory(0,nullptr);
	AT_PORTABLE_TEST_ASSERT(context, fixture->mMemory.ExtReadByte(0x4567,1)==0xFF);
	fixture->mMemory.ExtWriteByte(0x4567,1,0xE1);
	AT_PORTABLE_TEST_ASSERT(context, replacement[0x4567]==0xB2);
	AT_PORTABLE_TEST_ASSERT(context, fixture->mMemory.ExtReadByte(0x4567,0)==0x31);
	return true;
}
