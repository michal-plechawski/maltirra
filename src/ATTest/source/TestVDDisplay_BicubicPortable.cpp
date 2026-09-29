// Portable VDDisplay bicubic filter texture tests.

#include <at/attest/portabletest.h>
#include <vd2/VDDisplay/bicubic.h>

bool ATTestVDDisplayBicubic(ATPortableTestContext& context) {
	uint32 defaultTexture[8] = {};
	uint32 explicitTexture[8] = {};
	VDDisplayCreateBicubicTexture(defaultTexture, 8, 4);
	VDDisplayCreateBicubicTexture(explicitTexture, 8, 4, 0.0f, 8.0f);

	for(int i = 0; i < 8; ++i) {
		AT_PORTABLE_TEST_ASSERT(context, defaultTexture[i] == explicitTexture[i]);
		AT_PORTABLE_TEST_ASSERT(context, !(defaultTexture[i] & 0xFF000000));
	}

	AT_PORTABLE_TEST_ASSERT(context, defaultTexture[0] == 0);
	AT_PORTABLE_TEST_ASSERT(context, defaultTexture[1] == 0);
	AT_PORTABLE_TEST_ASSERT(context, defaultTexture[2] == 0);
	AT_PORTABLE_TEST_ASSERT(context, defaultTexture[3] != 0);
	AT_PORTABLE_TEST_ASSERT(context, defaultTexture[4] != 0);
	AT_PORTABLE_TEST_ASSERT(context, defaultTexture[5] == 0);
	AT_PORTABLE_TEST_ASSERT(context, defaultTexture[6] == 0);
	AT_PORTABLE_TEST_ASSERT(context, defaultTexture[7] == 0);

	uint32 center = 0;
	VDDisplayCreateBicubicTexture(&center, 1, 4, 1.5f, 4.0f);
	AT_PORTABLE_TEST_ASSERT(context, center == 0x00BF0080);

	uint32 narrowTexture[4] = { 1, 1, 1, 1 };
	VDDisplayCreateBicubicTexture(narrowTexture, 4, 1);
	for(uint32 value : narrowTexture)
		AT_PORTABLE_TEST_ASSERT(context, value == 0);

	return true;
}
