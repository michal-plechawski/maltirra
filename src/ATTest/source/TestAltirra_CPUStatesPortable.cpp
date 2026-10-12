// Altirra portable CPU microstate definition tests

#include <type_traits>

#include <at/attest/portabletest.h>
#include <cpustates.h>
#include <cpuflags.h>

bool ATTestAltirraCPUStates(ATPortableTestContext& context) {
	static_assert(AT6502::kFlagN == 0x80 && AT6502::kFlagV == 0x40);
	static_assert(AT6502::kFlagM == 0x20 && AT6502::kFlagX == 0x10);
	static_assert(AT6502::kFlagB == 0x10 && AT6502::kFlagD == 0x08);
	static_assert(AT6502::kFlagI == 0x04 && AT6502::kFlagZ == 0x02 && AT6502::kFlagC == 0x01);
	using AT6502States::ATCPUState;

	static_assert(std::is_same_v<std::underlying_type_t<ATCPUState>, uint8>);
	static_assert(sizeof(ATCPUState) == 1);

	AT_PORTABLE_TEST_ASSERT(context, AT6502States::kStateNop == 0);
	AT_PORTABLE_TEST_ASSERT(context, AT6502States::kStateReadOpcode == 1);
	AT_PORTABLE_TEST_ASSERT(context, AT6502States::kStateResetBit == 119);
	AT_PORTABLE_TEST_ASSERT(context, AT6502States::kStateReadImmL16 == 134);
	AT_PORTABLE_TEST_ASSERT(context, AT6502States::kState816_SetBankPBR == 240);
	AT_PORTABLE_TEST_ASSERT(context, AT6502States::kStateCount == 241);
	AT_PORTABLE_TEST_ASSERT(context, AT6502States::kStateCount <= 255);

	return true;
}
