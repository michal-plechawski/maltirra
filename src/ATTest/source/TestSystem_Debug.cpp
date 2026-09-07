// Altirra portable low-level diagnostics tests

#include <at/attest/portabletest.h>
#include <vd2/system/debug.h>

namespace {
	class VDTestExternalCallTrap final : public IVDExternalCallTrap {
	public:
		void OnMMXTrap(const wchar_t *, const char *, int) override {
			++mMMXCount;
		}

		void OnFPUTrap(const wchar_t *, const char *, int, uint16) override {
			++mFPUCount;
		}

		void OnSSETrap(const wchar_t *, const char *, int, uint32) override {
			++mSSECount;
		}

		int mMMXCount = 0;
		int mFPUCount = 0;
		int mSSECount = 0;
	};
}

bool ATTestSystemDebug(ATPortableTestContext& context) {
	AT_PORTABLE_TEST_ASSERT(context, !IsMMXState());
	ClearMMXState();
	VDClearEvilCPUStates();

	VDTestExternalCallTrap trap;
	VDSetExternalCallTrap(&trap);
	VDPreCheckExternalCodeCall(__FILE__, __LINE__);
	VDPostCheckExternalCodeCall(L"portable diagnostic test", __FILE__, __LINE__);
	{
		VDSilentExternalCodeBracket bracket;
		(void)bracket;
	}
	{
		VDExternalCodeBracket bracket(L"portable diagnostic test", __FILE__, __LINE__);
		AT_PORTABLE_TEST_ASSERT(context, !bracket);
	}
	{
		const VDExternalCodeBracketLocation location(
			L"portable diagnostic test", __FILE__, __LINE__);
		VDExternalCodeBracket bracket(location);
		AT_PORTABLE_TEST_ASSERT(context, !bracket);
	}
	VDSetExternalCallTrap(nullptr);

	AT_PORTABLE_TEST_ASSERT(context, trap.mMMXCount == 0);
	AT_PORTABLE_TEST_ASSERT(context, trap.mFPUCount == 0);
	AT_PORTABLE_TEST_ASSERT(context, trap.mSSECount == 0);
	VDDebugPrint("%s", "");
	return true;
}
