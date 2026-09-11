// Altirra portable input definition tests

#include <at/attest/portabletest.h>
#include <inputdefs.h>

bool ATTestAltirraInputDefs(ATPortableTestContext& context) {
	for(uint32 value = kATInputControllerType_None;
		value <= kATInputControllerType_LightPenStack;
		++value)
	{
		const bool expected =
			value == kATInputControllerType_5200Controller
			|| value == kATInputControllerType_5200Trackball;

		AT_PORTABLE_TEST_ASSERT(
			context,
			ATInputIs5200ControllerType(
				static_cast<ATInputControllerType>(value)) == expected);
	}

	AT_PORTABLE_TEST_ASSERT(
		context,
		!ATInputIs5200ControllerType(
			static_cast<ATInputControllerType>(UINT32_C(0xFFFFFFFF))));

	return true;
}
