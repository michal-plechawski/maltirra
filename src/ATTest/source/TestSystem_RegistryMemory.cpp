// Altirra portable in-memory registry provider tests

#include <algorithm>
#include <atomic>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

#include <at/attest/portabletest.h>
#include <vd2/system/registrymemory.h>

namespace {
	class VDRegistryMemoryTestKey {
	public:
		VDRegistryMemoryTestKey(VDRegistryProviderMemory& provider, void *key)
			: mProvider(provider)
			, mpKey(key) {
		}

		~VDRegistryMemoryTestKey() {
			Close();
		}

		VDRegistryMemoryTestKey(const VDRegistryMemoryTestKey&) = delete;
		VDRegistryMemoryTestKey& operator=(const VDRegistryMemoryTestKey&) = delete;

		void *Get() const {
			return mpKey;
		}

		void Close() {
			if (mpKey) {
				mProvider.CloseKey(mpKey);
				mpKey = nullptr;
			}
		}

	private:
		VDRegistryProviderMemory& mProvider;
		void *mpKey;
	};

	std::vector<std::string> VDRegistryMemoryEnumKeys(
		VDRegistryProviderMemory& provider,
		void *key)
	{
		std::vector<std::string> names;
		void *enumerator = provider.EnumKeysBegin(key);
		while(const char *name = provider.EnumKeysNext(enumerator))
			names.emplace_back(name);
		provider.EnumKeysClose(enumerator);
		return names;
	}

	std::vector<std::string> VDRegistryMemoryEnumValues(
		VDRegistryProviderMemory& provider,
		void *key)
	{
		std::vector<std::string> names;
		void *enumerator = provider.EnumValuesBegin(key);
		while(const char *name = provider.EnumValuesNext(enumerator))
			names.emplace_back(name);
		provider.EnumValuesClose(enumerator);
		return names;
	}

	bool VDRegistryMemoryContains(
		const std::vector<std::string>& names,
		const char *name)
	{
		return std::find(names.begin(), names.end(), name) != names.end();
	}
}

bool ATTestSystemRegistryMemory(ATPortableTestContext& context) {
	VDRegistryProviderMemory provider;
	void *userRoot = provider.GetUserKey();
	void *machineRoot = provider.GetMachineKey();
	AT_PORTABLE_TEST_ASSERT(context, userRoot != nullptr);
	AT_PORTABLE_TEST_ASSERT(context, machineRoot != nullptr);
	AT_PORTABLE_TEST_ASSERT(context, userRoot != machineRoot);
	AT_PORTABLE_TEST_ASSERT(context,
		provider.CreateKey(userRoot, "Missing", false) == nullptr);

	{
		VDRegistryMemoryTestKey key(
			provider,
			provider.CreateKey(userRoot, "Software\\Altirra\\Tests", true));
		AT_PORTABLE_TEST_ASSERT(context, key.Get() != nullptr);

		VDRegistryMemoryTestKey caseInsensitive(
			provider,
			provider.CreateKey(userRoot, "software\\ALTIRRA\\tests", false));
		AT_PORTABLE_TEST_ASSERT(context, caseInsensitive.Get() == key.Get());

		VDRegistryMemoryTestKey absolute(
			provider,
			provider.CreateKey(key.Get(), "\\\\Software\\Altirra\\Tests", false));
		AT_PORTABLE_TEST_ASSERT(context, absolute.Get() == key.Get());

		VDRegistryMemoryTestKey sameKey(
			provider,
			provider.CreateKey(key.Get(), "", false));
		AT_PORTABLE_TEST_ASSERT(context, sameKey.Get() == key.Get());

		AT_PORTABLE_TEST_ASSERT(context,
			provider.GetType(key.Get(), "MissingValue") == IVDRegistryProvider::kTypeUnknown);
		AT_PORTABLE_TEST_ASSERT(context,
			VDRegistryMemoryEnumValues(provider, key.Get()).empty());

		AT_PORTABLE_TEST_ASSERT(context, provider.SetBool(key.Get(), "Enabled", true));
		bool boolValue = false;
		AT_PORTABLE_TEST_ASSERT(context, provider.GetBool(key.Get(), "enabled", boolValue));
		AT_PORTABLE_TEST_ASSERT(context, boolValue);
		AT_PORTABLE_TEST_ASSERT(context,
			provider.GetType(key.Get(), "ENABLED") == IVDRegistryProvider::kTypeInt);

		AT_PORTABLE_TEST_ASSERT(context, provider.SetInt(key.Get(), "Signed", -1234567));
		int intValue = 0;
		AT_PORTABLE_TEST_ASSERT(context, provider.GetInt(key.Get(), "SIGNED", intValue));
		AT_PORTABLE_TEST_ASSERT(context, intValue == -1234567);
		AT_PORTABLE_TEST_ASSERT(context, !provider.GetBool(key.Get(), "Missing", boolValue));

		AT_PORTABLE_TEST_ASSERT(context, provider.SetInt(key.Get(), nullptr, 73));
		AT_PORTABLE_TEST_ASSERT(context, provider.GetInt(key.Get(), "", intValue));
		AT_PORTABLE_TEST_ASSERT(context, intValue == 73);

		AT_PORTABLE_TEST_ASSERT(context,
			provider.SetString(key.Get(), "Ascii", "portable registry"));
		VDStringA narrowString;
		VDStringW wideString;
		AT_PORTABLE_TEST_ASSERT(context,
			provider.GetString(key.Get(), "ASCII", narrowString));
		AT_PORTABLE_TEST_ASSERT(context, narrowString == "portable registry");
		AT_PORTABLE_TEST_ASSERT(context,
			provider.GetString(key.Get(), "Ascii", wideString));
		AT_PORTABLE_TEST_ASSERT(context, wideString == L"portable registry");

		const wchar_t *unicodeText = L"Za\u017C\u00F3\u0142\u0107 g\u0119\u015Bl\u0105";
		AT_PORTABLE_TEST_ASSERT(context,
			provider.SetString(key.Get(), "Unicode", unicodeText));
		AT_PORTABLE_TEST_ASSERT(context,
			provider.GetString(key.Get(), "unicode", wideString));
		AT_PORTABLE_TEST_ASSERT(context, wideString == unicodeText);

		const char binaryData[] = { '\0', '\x01', '\x7f', (char)0x80, (char)0xff };
		AT_PORTABLE_TEST_ASSERT(context,
			provider.SetBinary(key.Get(), "Blob", binaryData, sizeof binaryData));
		AT_PORTABLE_TEST_ASSERT(context,
			provider.GetType(key.Get(), "blob") == IVDRegistryProvider::kTypeBinary);
		AT_PORTABLE_TEST_ASSERT(context,
			provider.GetBinaryLength(key.Get(), "BLOB") == (int)sizeof binaryData);
		char binaryOutput[sizeof binaryData] = {};
		AT_PORTABLE_TEST_ASSERT(context,
			!provider.GetBinary(key.Get(), "Blob", binaryOutput, sizeof binaryOutput - 1));
		AT_PORTABLE_TEST_ASSERT(context,
			provider.GetBinary(key.Get(), "Blob", binaryOutput, sizeof binaryOutput));
		AT_PORTABLE_TEST_ASSERT(context,
			!memcmp(binaryOutput, binaryData, sizeof binaryData));
		AT_PORTABLE_TEST_ASSERT(context,
			provider.SetBinary(key.Get(), "EmptyBlob", nullptr, 0));
		AT_PORTABLE_TEST_ASSERT(context,
			provider.GetBinaryLength(key.Get(), "EmptyBlob") == 0);
		AT_PORTABLE_TEST_ASSERT(context,
			provider.GetBinary(key.Get(), "EmptyBlob", nullptr, 0));
		AT_PORTABLE_TEST_ASSERT(context,
			!provider.SetBinary(key.Get(), "InvalidBlob", nullptr, 1));
		AT_PORTABLE_TEST_ASSERT(context,
			!provider.SetBinary(key.Get(), "InvalidBlob", binaryData, -1));

		AT_PORTABLE_TEST_ASSERT(context, provider.SetInt(key.Get(), "Mutable", 1));
		AT_PORTABLE_TEST_ASSERT(context,
			provider.SetString(key.Get(), "Mutable", L"changed"));
		AT_PORTABLE_TEST_ASSERT(context, !provider.GetInt(key.Get(), "Mutable", intValue));
		AT_PORTABLE_TEST_ASSERT(context,
			provider.GetString(key.Get(), "Mutable", wideString));
		AT_PORTABLE_TEST_ASSERT(context, wideString == L"changed");
		AT_PORTABLE_TEST_ASSERT(context,
			provider.SetBinary(key.Get(), "Mutable", binaryData, sizeof binaryData));
		AT_PORTABLE_TEST_ASSERT(context,
			!provider.GetString(key.Get(), "Mutable", wideString));
		AT_PORTABLE_TEST_ASSERT(context, provider.SetInt(key.Get(), "Mutable", 99));
		AT_PORTABLE_TEST_ASSERT(context, provider.GetInt(key.Get(), "Mutable", intValue));
		AT_PORTABLE_TEST_ASSERT(context, intValue == 99);

		const std::vector<std::string> valueNames =
			VDRegistryMemoryEnumValues(provider, key.Get());
		AT_PORTABLE_TEST_ASSERT(context, VDRegistryMemoryContains(valueNames, "Enabled"));
		AT_PORTABLE_TEST_ASSERT(context, VDRegistryMemoryContains(valueNames, "Signed"));
		AT_PORTABLE_TEST_ASSERT(context, VDRegistryMemoryContains(valueNames, "Blob"));
		AT_PORTABLE_TEST_ASSERT(context, VDRegistryMemoryContains(valueNames, "Mutable"));
		AT_PORTABLE_TEST_ASSERT(context,
			!VDRegistryMemoryContains(valueNames, "MissingValue"));

		AT_PORTABLE_TEST_ASSERT(context, provider.RemoveValue(key.Get(), "bLoB"));
		AT_PORTABLE_TEST_ASSERT(context, !provider.RemoveValue(key.Get(), "Blob"));
		AT_PORTABLE_TEST_ASSERT(context,
			provider.GetBinaryLength(key.Get(), "Blob") == -1);
	}

	{
		VDRegistryMemoryTestKey first(
			provider, provider.CreateKey(userRoot, "First", true));
		VDRegistryMemoryTestKey second(
			provider, provider.CreateKey(userRoot, "Second", true));
		const std::vector<std::string> keyNames =
			VDRegistryMemoryEnumKeys(provider, userRoot);
		AT_PORTABLE_TEST_ASSERT(context, VDRegistryMemoryContains(keyNames, "Software"));
		AT_PORTABLE_TEST_ASSERT(context, VDRegistryMemoryContains(keyNames, "First"));
		AT_PORTABLE_TEST_ASSERT(context, VDRegistryMemoryContains(keyNames, "Second"));
	}

	{
		VDRegistryMemoryTestKey leaf(
			provider, provider.CreateKey(userRoot, "Parent\\Child", true));
		leaf.Close();
		AT_PORTABLE_TEST_ASSERT(context, !provider.RemoveKey(userRoot, "Parent"));
		AT_PORTABLE_TEST_ASSERT(context,
			provider.RemoveKeyRecursive(userRoot, "PARENT"));
		AT_PORTABLE_TEST_ASSERT(context,
			provider.CreateKey(userRoot, "Parent", false) == nullptr);

		VDRegistryMemoryTestKey simple(
			provider, provider.CreateKey(userRoot, "Simple", true));
		simple.Close();
		AT_PORTABLE_TEST_ASSERT(context, provider.RemoveKey(userRoot, "simple"));
		AT_PORTABLE_TEST_ASSERT(context, !provider.RemoveKey(userRoot, "Simple"));
	}

	{
		VDRegistryMemoryTestKey held(
			provider, provider.CreateKey(userRoot, "Held", true));
		AT_PORTABLE_TEST_ASSERT(context, provider.RemoveKey(userRoot, "Held"));
		AT_PORTABLE_TEST_ASSERT(context,
			!VDRegistryMemoryContains(
				VDRegistryMemoryEnumKeys(provider, userRoot), "Held"));
		AT_PORTABLE_TEST_ASSERT(context,
			provider.CreateKey(userRoot, "Held", false) == nullptr);
		AT_PORTABLE_TEST_ASSERT(context,
			provider.CreateKey(userRoot, "Held", true) == nullptr);
		held.Close();
		AT_PORTABLE_TEST_ASSERT(context,
			provider.CreateKey(userRoot, "Held", false) == nullptr);
	}

	{
		VDRegistryMemoryTestKey leaf(
			provider, provider.CreateKey(userRoot, "Tree\\Branch\\Leaf", true));
		AT_PORTABLE_TEST_ASSERT(context, provider.SetInt(leaf.Get(), "Alive", 1));
		AT_PORTABLE_TEST_ASSERT(context,
			provider.RemoveKeyRecursive(userRoot, "Tree"));
		AT_PORTABLE_TEST_ASSERT(context,
			provider.CreateKey(userRoot, "Tree", false) == nullptr);
		int value = 0;
		AT_PORTABLE_TEST_ASSERT(context, provider.GetInt(leaf.Get(), "Alive", value));
		AT_PORTABLE_TEST_ASSERT(context, value == 1);
		leaf.Close();
		AT_PORTABLE_TEST_ASSERT(context,
			provider.CreateKey(userRoot, "Tree", false) == nullptr);
	}

	{
		VDRegistryMemoryTestKey userKey(
			provider, provider.CreateKey(userRoot, "Independent", true));
		VDRegistryMemoryTestKey machineKey(
			provider, provider.CreateKey(machineRoot, "Independent", true));
		AT_PORTABLE_TEST_ASSERT(context, provider.SetInt(userKey.Get(), "Value", 1));
		AT_PORTABLE_TEST_ASSERT(context, provider.SetInt(machineKey.Get(), "Value", 2));
		int userValue = 0;
		int machineValue = 0;
		AT_PORTABLE_TEST_ASSERT(context,
			provider.GetInt(userKey.Get(), "Value", userValue));
		AT_PORTABLE_TEST_ASSERT(context,
			provider.GetInt(machineKey.Get(), "Value", machineValue));
		AT_PORTABLE_TEST_ASSERT(context, userValue == 1);
		AT_PORTABLE_TEST_ASSERT(context, machineValue == 2);
	}

	{
		VDRegistryMemoryTestKey concurrent(
			provider, provider.CreateKey(userRoot, "Concurrent", true));
		const char *const names[] = {
			"Thread0", "Thread1", "Thread2", "Thread3",
			"Thread4", "Thread5", "Thread6", "Thread7"
		};
		VDSignalPersistent start;
		std::atomic<bool> success { true };
		std::vector<std::thread> threads;
		for(size_t index = 0; index < sizeof names / sizeof names[0]; ++index) {
			threads.emplace_back([&, index] {
				start.wait();
				for(int iteration = 0; iteration < 500; ++iteration) {
					if (!provider.SetInt(concurrent.Get(), names[index], iteration)) {
						success = false;
						break;
					}

					int value = -1;
					if (!provider.GetInt(concurrent.Get(), names[index], value)
						|| value != iteration)
					{
						success = false;
						break;
					}
				}
			});
		}
		start.signal();
		for(auto& thread : threads)
			thread.join();

		AT_PORTABLE_TEST_ASSERT(context, success.load());
		AT_PORTABLE_TEST_ASSERT(context,
			VDRegistryMemoryEnumValues(provider, concurrent.Get()).size()
				== sizeof names / sizeof names[0]);
	}

	AT_PORTABLE_TEST_ASSERT(context,
		provider.RemoveKeyRecursive(userRoot, "Software"));
	AT_PORTABLE_TEST_ASSERT(context, provider.RemoveKey(userRoot, "First"));
	AT_PORTABLE_TEST_ASSERT(context, provider.RemoveKey(userRoot, "Second"));
	AT_PORTABLE_TEST_ASSERT(context, provider.RemoveKey(userRoot, "Independent"));
	AT_PORTABLE_TEST_ASSERT(context, provider.RemoveKey(userRoot, "Concurrent"));
	AT_PORTABLE_TEST_ASSERT(context, provider.RemoveKey(machineRoot, "Independent"));
	AT_PORTABLE_TEST_ASSERT(context, VDRegistryMemoryEnumKeys(provider, userRoot).empty());
	AT_PORTABLE_TEST_ASSERT(context, VDRegistryMemoryEnumKeys(provider, machineRoot).empty());

	return true;
}
