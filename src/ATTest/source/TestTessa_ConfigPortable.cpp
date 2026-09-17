// Portable Tessa configuration tests.

#include <at/attest/portabletest.h>
#include <vd2/Tessa/Config.h>

bool ATTestTessaConfig(ATPortableTestContext& context) {
	VDTSetLibraryOverridesEnabled(false);
	AT_PORTABLE_TEST_ASSERT(context, !VDTGetLibraryOverridesEnabled());

	VDTSetLibraryOverridesEnabled(true);
	AT_PORTABLE_TEST_ASSERT(context, VDTGetLibraryOverridesEnabled());

	VDTSetLibraryOverridesEnabled(false);
	AT_PORTABLE_TEST_ASSERT(context, !VDTGetLibraryOverridesEnabled());
	return true;
}
