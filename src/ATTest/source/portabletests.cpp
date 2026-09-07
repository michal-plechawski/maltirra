// Altirra portable test manifest

#include <at/attest/portabletest.h>

bool ATTestSystemBinary(ATPortableTestContext& context);
bool ATTestSystemBitMath(ATPortableTestContext& context);
bool ATTestSystemConstexpr(ATPortableTestContext& context);
bool ATTestSystemHalfFloat(ATPortableTestContext& context);
bool ATTestSystemRefCount(ATPortableTestContext& context);
bool ATTestSystemTLS(ATPortableTestContext& context);
bool ATTestSystemVDAlloc(ATPortableTestContext& context);

const ATPortableTestCase *ATGetPortableTests(size_t& count) {
	static const ATPortableTestCase kTests[] = {
		{ "System_Binary", ATTestSystemBinary },
		{ "System_BitMath", ATTestSystemBitMath },
		{ "System_Constexpr", ATTestSystemConstexpr },
		{ "System_HalfFloat", ATTestSystemHalfFloat },
		{ "System_RefCount", ATTestSystemRefCount },
		{ "System_TLS", ATTestSystemTLS },
		{ "System_VDAlloc", ATTestSystemVDAlloc },
	};

	count = sizeof kTests / sizeof kTests[0];
	return kTests;
}
