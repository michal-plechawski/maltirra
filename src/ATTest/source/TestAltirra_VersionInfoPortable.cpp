// Altirra portable version metadata tests

#include <string_view>

#include <at/attest/portabletest.h>

// Exercise stable-release preprocessing regardless of the version header used
// by this build. A macro defined as 0 must not select the prerelease channel.
#include <version.h>
#undef AT_VERSION_PRERELEASE
#define AT_VERSION_PRERELEASE 0
#undef AT_VERSION_DEV
#include <versioninfo.h>

bool ATTestAltirraVersionInfo(ATPortableTestContext& context) {
	const std::wstring_view programName(AT_PROGRAM_NAME_STR);
	const std::wstring_view version(AT_VERSION_STR);
	const std::wstring_view platform(AT_PROGRAM_PLATFORM_STR);
	const std::wstring_view fullVersion(AT_FULL_VERSION_STR);
	const std::wstring_view userAgent(AT_HTTP_USER_AGENT);

	AT_PORTABLE_TEST_ASSERT(context, programName == L"Altirra");
	AT_PORTABLE_TEST_ASSERT(context, !version.empty());
	AT_PORTABLE_TEST_ASSERT(context, fullVersion.starts_with(L"Altirra"));
	AT_PORTABLE_TEST_ASSERT(context, fullVersion.find(version) != std::wstring_view::npos);
	AT_PORTABLE_TEST_ASSERT(context, userAgent.starts_with(L"Altirra/"));
	AT_PORTABLE_TEST_ASSERT(context, userAgent.substr(8) == version);

#if defined(VD_CPU_ARM64)
	AT_PORTABLE_TEST_ASSERT(context, platform == L"/ARM64");
#elif defined(VD_CPU_AMD64)
	AT_PORTABLE_TEST_ASSERT(context, platform == L"/x64");
#else
	AT_PORTABLE_TEST_ASSERT(context, platform.empty());
#endif

	AT_PORTABLE_TEST_ASSERT(context, !AT_UPDATE_USE_TEST_CHANNEL);

	return true;
}
