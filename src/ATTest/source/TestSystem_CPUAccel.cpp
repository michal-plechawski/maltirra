// Altirra portable CPU acceleration state tests

#include <at/attest/portabletest.h>
#include <vd2/system/cpuaccel.h>

namespace {
#if VD_CPU_X86 || VD_CPU_X64
	constexpr long kSupportedExtensionMask = CPUF_SUPPORTS_MASK;
#else
	constexpr long kSupportedExtensionMask = VDCPUF_SUPPORTS_MASK;
#endif

	struct CPUExtensionStateGuard {
		long mFlags;

		~CPUExtensionStateGuard() {
			CPUEnableExtensions(mFlags);
		}
	};
}

bool ATTestSystemCPUAccel(ATPortableTestContext& context) {
	const long originalFlags = CPUGetEnabledExtensions();
	const CPUExtensionStateGuard stateGuard { originalFlags };
	const long detectedFlags = CPUCheckForExtensions();

	AT_PORTABLE_TEST_ASSERT(context, !(detectedFlags & ~kSupportedExtensionMask));
	AT_PORTABLE_TEST_ASSERT(context, CPUEnableExtensions(0) == 0);
	AT_PORTABLE_TEST_ASSERT(context, CPUGetEnabledExtensions() == 0);
	AT_PORTABLE_TEST_ASSERT(context, VDCheckAllExtensionsEnabled(0));

	if (detectedFlags) {
		const long firstFlag = detectedFlags & -detectedFlags;

		AT_PORTABLE_TEST_ASSERT(context, CPUEnableExtensions(firstFlag) == firstFlag);
		AT_PORTABLE_TEST_ASSERT(context, CPUGetEnabledExtensions() == firstFlag);
		AT_PORTABLE_TEST_ASSERT(context, VDCheckAllExtensionsEnabled((uint32)firstFlag));

		const uint32 otherFlags = (uint32)(kSupportedExtensionMask & ~firstFlag);
		if (otherFlags)
			AT_PORTABLE_TEST_ASSERT(context, !VDCheckAllExtensionsEnabled(otherFlags));
	}

#if defined(__APPLE__) && VD_CPU_ARM64
	AT_PORTABLE_TEST_ASSERT(context,
		(detectedFlags & VDCPUF_SUPPORTS_CRYPTO) == VDCPUF_SUPPORTS_CRYPTO);
	AT_PORTABLE_TEST_ASSERT(context,
		(detectedFlags & VDCPUF_SUPPORTS_CRC32) == VDCPUF_SUPPORTS_CRC32);
#endif

	CPUEnableExtensions(detectedFlags);
	VDCPUCleanupExtensions();
	return true;
}
