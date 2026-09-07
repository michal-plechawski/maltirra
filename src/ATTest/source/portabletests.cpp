// Altirra portable test manifest

#include <at/attest/portabletest.h>

bool ATTestSystemBinary(ATPortableTestContext& context);
bool ATTestSystemBitMath(ATPortableTestContext& context);
bool ATTestSystemConstexpr(ATPortableTestContext& context);
bool ATTestSystemFraction(ATPortableTestContext& context);
bool ATTestSystemHash(ATPortableTestContext& context);
bool ATTestSystemHalfFloat(ATPortableTestContext& context);
bool ATTestSystemInt128(ATPortableTestContext& context);
bool ATTestSystemMath(ATPortableTestContext& context);
bool ATTestSystemRefCount(ATPortableTestContext& context);
bool ATTestSystemStrUtil(ATPortableTestContext& context);
bool ATTestSystemTLS(ATPortableTestContext& context);
bool ATTestSystemVDAlloc(ATPortableTestContext& context);
bool ATTestSystemVDFunction(ATPortableTestContext& context);

const ATPortableTestCase *ATGetPortableTests(size_t& count) {
	static const ATPortableTestCase kTests[] = {
		{ "System_Binary", ATTestSystemBinary },
		{ "System_BitMath", ATTestSystemBitMath },
		{ "System_Constexpr", ATTestSystemConstexpr },
		{ "System_Fraction", ATTestSystemFraction },
		{ "System_Hash", ATTestSystemHash },
		{ "System_HalfFloat", ATTestSystemHalfFloat },
		{ "System_Int128", ATTestSystemInt128 },
		{ "System_Math", ATTestSystemMath },
		{ "System_RefCount", ATTestSystemRefCount },
		{ "System_StrUtil", ATTestSystemStrUtil },
		{ "System_TLS", ATTestSystemTLS },
		{ "System_VDAlloc", ATTestSystemVDAlloc },
		{ "System_VDFunction", ATTestSystemVDFunction },
	};

	count = sizeof kTests / sizeof kTests[0];
	return kTests;
}
