// Altirra portable PNG utility tests

#include <array>

#include <at/attest/portabletest.h>
#include <common_png.h>

bool ATTestAltirraPNG(ATPortableTestContext& context) {
	static constexpr std::array<uint8, 8> kExpectedSignature {
		137, 80, 78, 71, 13, 10, 26, 10
	};

	for(size_t i = 0; i < kExpectedSignature.size(); ++i)
		AT_PORTABLE_TEST_ASSERT(
			context, nsVDPNG::kPNGSignature[i] == kExpectedSignature[i]);

	AT_PORTABLE_TEST_ASSERT(context, nsVDPNG::PNGPaethPredictor(10, 20, 30) == 10);
	AT_PORTABLE_TEST_ASSERT(context, nsVDPNG::PNGPaethPredictor(30, 10, 20) == 20);
	AT_PORTABLE_TEST_ASSERT(context, nsVDPNG::PNGPaethPredictor(10, 30, 20) == 20);
	AT_PORTABLE_TEST_ASSERT(context, nsVDPNG::PNGPaethPredictor(42, 42, 42) == 42);
	AT_PORTABLE_TEST_ASSERT(context, nsVDPNG::PNGPaethPredictor(0, 255, 0) == 255);
	AT_PORTABLE_TEST_ASSERT(context, nsVDPNG::PNGPaethPredictor(255, 0, 0) == 255);

	return true;
}
