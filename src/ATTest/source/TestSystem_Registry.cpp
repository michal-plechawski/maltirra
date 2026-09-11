// Altirra portable persistent registry tests

#include <algorithm>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include <at/attest/portabletest.h>
#include <vd2/system/registry.h>
#include <vd2/system/registrymemory.h>
#include <vd2/system/thread.h>
#include <vd2/system/time.h>

namespace {
	class VDPersistentRegistrySandbox {
	public:
		VDPersistentRegistrySandbox() {
			mLeaf.sprintf(
				"PortableTests-%u-%llu",
				static_cast<unsigned>(VDGetCurrentProcessId()),
				static_cast<unsigned long long>(VDGetCurrentTick64()));
			mPath = mLeaf;
			Remove();
		}

		~VDPersistentRegistrySandbox() {
			Remove();
		}

		bool Remove() {
			IVDRegistryProvider *provider = VDGetDefaultRegistryProvider();
			return provider->RemoveKeyRecursive(
				provider->GetUserKey(), mLeaf.c_str());
		}

		VDStringA mLeaf;
		VDStringA mPath;
	};

	class VDRegistryProviderScope {
	public:
		explicit VDRegistryProviderScope(IVDRegistryProvider *provider)
			: mpPrevious(VDGetRegistryProvider()) {
			VDSetRegistryProvider(provider);
		}

		~VDRegistryProviderScope() {
			VDSetRegistryProvider(mpPrevious);
		}

	private:
		IVDRegistryProvider *mpPrevious;
	};

	class VDRegistryAppKeyScope {
	public:
		explicit VDRegistryAppKeyScope(const char *path)
			: mPrevious(VDRegistryAppKey::getDefaultKey()) {
			VDRegistryAppKey::setDefaultKey(path);
		}

		~VDRegistryAppKeyScope() {
			VDRegistryAppKey::setDefaultKey(mPrevious.c_str());
		}

	private:
		VDStringA mPrevious;
	};

	std::vector<std::string> VDEnumerateRegistryKeys(const VDRegistryKey& key) {
		std::vector<std::string> names;
		VDRegistryKeyIterator iterator(key);
		while(const char *name = iterator.Next())
			names.emplace_back(name);
		return names;
	}

	std::vector<std::string> VDEnumerateRegistryValues(const VDRegistryKey& key) {
		std::vector<std::string> names;
		VDRegistryValueIterator iterator(key);
		while(const char *name = iterator.Next())
			names.emplace_back(name);
		return names;
	}

	bool VDContainsRegistryName(
		const std::vector<std::string>& names,
		const char *name) {
		return std::find(names.begin(), names.end(), name) != names.end();
	}
}

bool ATTestSystemRegistry(ATPortableTestContext& context) {
	IVDRegistryProvider *defaultProvider = VDGetDefaultRegistryProvider();
	AT_PORTABLE_TEST_ASSERT(context, defaultProvider != nullptr);
	AT_PORTABLE_TEST_ASSERT(context, VDGetRegistryProvider() == defaultProvider);
	AT_PORTABLE_TEST_ASSERT(context,
		defaultProvider->GetUserKey() != defaultProvider->GetMachineKey());

	VDPersistentRegistrySandbox sandbox;
	{
		VDRegistryKey missing(sandbox.mPath.c_str(), false, false);
		AT_PORTABLE_TEST_ASSERT(context, !missing.isReady());

		VDRegistryKey key(sandbox.mPath.c_str(), false, true);
		AT_PORTABLE_TEST_ASSERT(context, key.isReady());
		AT_PORTABLE_TEST_ASSERT(context, key.setBool("Enabled", true));
		AT_PORTABLE_TEST_ASSERT(context, key.setInt("Signed", -1234567));
		AT_PORTABLE_TEST_ASSERT(context, key.setInt("Enum", 2));
		AT_PORTABLE_TEST_ASSERT(context, key.setString("Narrow", "persistent registry"));
		const wchar_t *unicodeText = L"Za\u017C\u00F3\u0142\u0107 g\u0119\u015Bl\u0105";
		AT_PORTABLE_TEST_ASSERT(context, key.setString("Wide", unicodeText));
		const char binaryData[] = { '\0', '\x01', '\x7f', (char)0x80, (char)0xff };
		AT_PORTABLE_TEST_ASSERT(context,
			key.setBinary("Blob", binaryData, sizeof binaryData));
		AT_PORTABLE_TEST_ASSERT(context, key.setBinary("EmptyBlob", nullptr, 0));

		VDRegistryKey nested(key, "Child\\Grandchild", true);
		AT_PORTABLE_TEST_ASSERT(context, nested.isReady());
		AT_PORTABLE_TEST_ASSERT(context, nested.setInt("NestedValue", 42));

		VDRegistryKey readOnly(sandbox.mPath.c_str(), false, false);
		AT_PORTABLE_TEST_ASSERT(context, readOnly.isReady());
		AT_PORTABLE_TEST_ASSERT(context, readOnly.getBool("enabled", false));
		AT_PORTABLE_TEST_ASSERT(context, readOnly.getInt("SIGNED", 0) == -1234567);
		AT_PORTABLE_TEST_ASSERT(context, readOnly.getInt("Missing", 73) == 73);
		AT_PORTABLE_TEST_ASSERT(context, readOnly.getEnumInt("Enum", 3, 1) == 2);
		AT_PORTABLE_TEST_ASSERT(context, readOnly.getEnumInt("Enum", 2, 1) == 1);
		AT_PORTABLE_TEST_ASSERT(context,
			readOnly.getValueType("Enabled") == VDRegistryKey::kTypeInt);
		AT_PORTABLE_TEST_ASSERT(context,
			readOnly.getValueType("Narrow") == VDRegistryKey::kTypeString);
		AT_PORTABLE_TEST_ASSERT(context,
			readOnly.getValueType("Blob") == VDRegistryKey::kTypeBinary);
		AT_PORTABLE_TEST_ASSERT(context,
			readOnly.getValueType("Missing") == VDRegistryKey::kTypeUnknown);

		VDStringA narrow;
		VDStringW wide;
		AT_PORTABLE_TEST_ASSERT(context, readOnly.getString("narrow", narrow));
		AT_PORTABLE_TEST_ASSERT(context, narrow == "persistent registry");
		AT_PORTABLE_TEST_ASSERT(context, readOnly.getString("WIDE", wide));
		AT_PORTABLE_TEST_ASSERT(context, wide == unicodeText);
		AT_PORTABLE_TEST_ASSERT(context,
			readOnly.getBinaryLength("blob") == (int)sizeof binaryData);
		char binaryOutput[sizeof binaryData] = {};
		AT_PORTABLE_TEST_ASSERT(context,
			!readOnly.getBinary("Blob", binaryOutput, sizeof binaryOutput - 1));
		AT_PORTABLE_TEST_ASSERT(context,
			readOnly.getBinary("Blob", binaryOutput, sizeof binaryOutput));
		AT_PORTABLE_TEST_ASSERT(context,
			!memcmp(binaryOutput, binaryData, sizeof binaryData));
		AT_PORTABLE_TEST_ASSERT(context, readOnly.getBinaryLength("EmptyBlob") == 0);
		AT_PORTABLE_TEST_ASSERT(context,
			readOnly.getBinary("EmptyBlob", nullptr, 0));
		AT_PORTABLE_TEST_ASSERT(context, !readOnly.setInt("Denied", 1));

		const std::vector<std::string> valueNames =
			VDEnumerateRegistryValues(readOnly);
		AT_PORTABLE_TEST_ASSERT(context, VDContainsRegistryName(valueNames, "Enabled"));
		AT_PORTABLE_TEST_ASSERT(context, VDContainsRegistryName(valueNames, "Signed"));
		AT_PORTABLE_TEST_ASSERT(context, VDContainsRegistryName(valueNames, "Blob"));
		const std::vector<std::string> keyNames = VDEnumerateRegistryKeys(readOnly);
		AT_PORTABLE_TEST_ASSERT(context, VDContainsRegistryName(keyNames, "Child"));

		AT_PORTABLE_TEST_ASSERT(context, key.removeValue("Blob"));
		AT_PORTABLE_TEST_ASSERT(context, !key.removeValue("Blob"));
		AT_PORTABLE_TEST_ASSERT(context, !key.removeKey("Child"));
		nested = VDRegistryKey(key, "Temporary", true);
		AT_PORTABLE_TEST_ASSERT(context, nested.isReady());
		VDRegistryKey moved(std::move(nested));
		AT_PORTABLE_TEST_ASSERT(context, !nested.isReady());
		AT_PORTABLE_TEST_ASSERT(context, moved.isReady());
		AT_PORTABLE_TEST_ASSERT(context, key.removeKeyRecursive("Child"));
		AT_PORTABLE_TEST_ASSERT(context, !key.removeKeyRecursive("Child"));
	}

	{
		VDStringA appBase = sandbox.mPath + "\\";
		VDRegistryAppKeyScope appScope(appBase.c_str());
		AT_PORTABLE_TEST_ASSERT(context,
			!strcmp(VDRegistryAppKey::getDefaultKey(), appBase.c_str()));
		VDRegistryAppKey appKey("Application", true, false);
		AT_PORTABLE_TEST_ASSERT(context, appKey.isReady());
		AT_PORTABLE_TEST_ASSERT(context, appKey.setInt("Value", 91));
		VDRegistryAppKey appRead("Application", false, false);
		AT_PORTABLE_TEST_ASSERT(context, appRead.getInt("Value", 0) == 91);
	}

	{
		VDRegistryProviderMemory source;
		void *sourceKey = source.CreateKey(source.GetUserKey(), "Source", true);
		AT_PORTABLE_TEST_ASSERT(context, sourceKey != nullptr);
		AT_PORTABLE_TEST_ASSERT(context, source.SetInt(sourceKey, "Integer", 17));
		AT_PORTABLE_TEST_ASSERT(context,
			source.SetString(sourceKey, "String", L"copied value"));
		void *sourceChild = source.CreateKey(sourceKey, "Nested", true);
		AT_PORTABLE_TEST_ASSERT(context, sourceChild != nullptr);
		AT_PORTABLE_TEST_ASSERT(context, source.SetBool(sourceChild, "Flag", true));
		source.CloseKey(sourceChild);
		source.CloseKey(sourceKey);

		const VDStringA destinationPath = sandbox.mPath + "\\Copied";
		VDRegistryCopy(*defaultProvider, destinationPath.c_str(), source, "Source");
		VDRegistryKey copied(destinationPath.c_str(), false, false);
		AT_PORTABLE_TEST_ASSERT(context, copied.isReady());
		AT_PORTABLE_TEST_ASSERT(context, copied.getInt("Integer", 0) == 17);
		VDStringW copiedString;
		AT_PORTABLE_TEST_ASSERT(context, copied.getString("String", copiedString));
		AT_PORTABLE_TEST_ASSERT(context, copiedString == L"copied value");
		VDRegistryKey copiedNested(copied, "Nested", false);
		AT_PORTABLE_TEST_ASSERT(context, copiedNested.getBool("Flag", false));
	}

	{
		VDRegistryProviderMemory replacement;
		VDRegistryProviderScope providerScope(&replacement);
		VDRegistryKey redirected("Redirected", false, true);
		AT_PORTABLE_TEST_ASSERT(context, redirected.isReady());
		AT_PORTABLE_TEST_ASSERT(context, redirected.setInt("Value", 314));
		AT_PORTABLE_TEST_ASSERT(context, redirected.getInt("Value", 0) == 314);
	}
	AT_PORTABLE_TEST_ASSERT(context, VDGetRegistryProvider() == defaultProvider);

	AT_PORTABLE_TEST_ASSERT(context, sandbox.Remove());
	VDRegistryKey removed(sandbox.mPath.c_str(), false, false);
	AT_PORTABLE_TEST_ASSERT(context, !removed.isReady());
	return true;
}
