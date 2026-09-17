// Altirra portable CPU microstate definition tests

#include <type_traits>

#include <at/attest/portabletest.h>
#include <cpustates.h>

bool ATTestAltirraCPUStates(ATPortableTestContext& context) {
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
