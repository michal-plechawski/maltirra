// VirtualDub/Altirra persistent registry services for macOS

#include <climits>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include <CoreFoundation/CoreFoundation.h>

#include <vd2/system/registry.h>
#include <vd2/system/registrymemory.h>
#include <vd2/system/text.h>
#include <vd2/system/vdstl.h>

namespace {
	const CFStringRef kVDRegistryApplicationID = CFSTR("org.virtualdub.altirra");
	const CFStringRef kVDRegistryUserPreference = CFSTR("RegistryUser");
	const CFStringRef kVDRegistryMachinePreference = CFSTR("RegistryMachine");
	const CFStringRef kVDRegistryKeys = CFSTR("keys");
	const CFStringRef kVDRegistryValues = CFSTR("values");

	template<class T>
	class VDCFRef {
	public:
		explicit VDCFRef(T value = nullptr)
			: mValue(value) {
		}

		~VDCFRef() {
			if (mValue)
				CFRelease(mValue);
		}

		VDCFRef(const VDCFRef&) = delete;
		VDCFRef& operator=(const VDCFRef&) = delete;

		VDCFRef(VDCFRef&& other) noexcept
			: mValue(other.mValue) {
			other.mValue = nullptr;
		}

		VDCFRef& operator=(VDCFRef&& other) noexcept {
			if (this != &other) {
				if (mValue)
					CFRelease(mValue);
				mValue = other.mValue;
				other.mValue = nullptr;
			}
			return *this;
		}

		T get() const {
			return mValue;
		}

		T release() {
			T value = mValue;
			mValue = nullptr;
			return value;
		}

		explicit operator bool() const {
			return mValue != nullptr;
		}

	private:
		T mValue;
	};

	VDCFRef<CFStringRef> VDCFStringFromUTF8(const char *text) {
		if (!text)
			text = "";

		return VDCFRef<CFStringRef>(CFStringCreateWithBytes(
			kCFAllocatorDefault,
			reinterpret_cast<const UInt8 *>(text),
			static_cast<CFIndex>(strlen(text)),
			kCFStringEncodingUTF8,
			false));
	}

	bool VDCFStringToUTF8(CFStringRef text, std::string& result) {
		if (!text)
			return false;

		const CFIndex length = CFStringGetLength(text);
		const CFIndex maximumSize =
			CFStringGetMaximumSizeForEncoding(length, kCFStringEncodingUTF8);
		if (maximumSize < 0)
			return false;

		std::vector<char> buffer(static_cast<size_t>(maximumSize) + 1);
		if (!CFStringGetCString(
			text,
			buffer.data(),
			static_cast<CFIndex>(buffer.size()),
			kCFStringEncodingUTF8))
			return false;

		result = buffer.data();
		return true;
	}

	class VDRegistryProviderMac final : public IVDRegistryProvider {
	public:
		VDRegistryProviderMac()
			: mUserRoot { mMemory.GetUserKey(), false, true, false }
			, mMachineRoot { mMemory.GetMachineKey(), true, true, false } {
			LoadRoot(false);
			LoadRoot(true);
		}

		~VDRegistryProviderMac() {
			std::lock_guard lock(mMutex);
			try {
				PersistRoot(false);
				PersistRoot(true);
			} catch(...) {
			}
		}

		void *GetMachineKey() override {
			return &mMachineRoot;
		}

		void *GetUserKey() override {
			return &mUserRoot;
		}

		void *CreateKey(void *key, const char *path, bool write) override {
			if (!key || !path)
				return nullptr;

			std::lock_guard lock(mMutex);
			Handle *base = static_cast<Handle *>(key);
			if (write && !base->mbWritable)
				return nullptr;

			void *memoryKey = mMemory.CreateKey(base->mpMemoryKey, path, write);
			if (!memoryKey)
				return nullptr;

			try {
				std::unique_ptr<Handle> result(new Handle {
					memoryKey,
					base->mbMachine,
					write && base->mbWritable,
					true
				});
				if (write)
					MarkDirty(base->mbMachine);
				return result.release();
			} catch(...) {
				mMemory.CloseKey(memoryKey);
				throw;
			}
		}

		void CloseKey(void *key) override {
			if (!key)
				return;

			std::lock_guard lock(mMutex);
			Handle *handle = static_cast<Handle *>(key);
			if (!handle->mbOwned)
				return;

			const bool machine = handle->mbMachine;
			mMemory.CloseKey(handle->mpMemoryKey);
			delete handle;
			try {
				PersistRoot(machine);
			} catch(...) {
			}
		}

		bool SetBool(void *key, const char *name, bool value) override {
			std::lock_guard lock(mMutex);
			Handle *handle = WritableHandle(key);
			if (!handle || !mMemory.SetBool(handle->mpMemoryKey, name, value))
				return false;
			return FinishMutation(handle);
		}

		bool SetInt(void *key, const char *name, int value) override {
			std::lock_guard lock(mMutex);
			Handle *handle = WritableHandle(key);
			if (!handle || !mMemory.SetInt(handle->mpMemoryKey, name, value))
				return false;
			return FinishMutation(handle);
		}

		bool SetString(void *key, const char *name, const char *value) override {
			if (!value)
				return false;
			std::lock_guard lock(mMutex);
			Handle *handle = WritableHandle(key);
			if (!handle || !mMemory.SetString(handle->mpMemoryKey, name, value))
				return false;
			return FinishMutation(handle);
		}

		bool SetString(void *key, const char *name, const wchar_t *value) override {
			if (!value)
				return false;
			std::lock_guard lock(mMutex);
			Handle *handle = WritableHandle(key);
			if (!handle || !mMemory.SetString(handle->mpMemoryKey, name, value))
				return false;
			return FinishMutation(handle);
		}

		bool SetBinary(
			void *key,
			const char *name,
			const char *data,
			int length) override {
			std::lock_guard lock(mMutex);
			Handle *handle = WritableHandle(key);
			if (!handle
				|| !mMemory.SetBinary(handle->mpMemoryKey, name, data, length))
				return false;
			return FinishMutation(handle);
		}

		Type GetType(void *key, const char *name) override {
			std::lock_guard lock(mMutex);
			Handle *handle = static_cast<Handle *>(key);
			return handle
				? mMemory.GetType(handle->mpMemoryKey, name)
				: kTypeUnknown;
		}

		bool GetBool(void *key, const char *name, bool& value) override {
			std::lock_guard lock(mMutex);
			Handle *handle = static_cast<Handle *>(key);
			return handle && mMemory.GetBool(handle->mpMemoryKey, name, value);
		}

		bool GetInt(void *key, const char *name, int& value) override {
			std::lock_guard lock(mMutex);
			Handle *handle = static_cast<Handle *>(key);
			return handle && mMemory.GetInt(handle->mpMemoryKey, name, value);
		}

		bool GetString(void *key, const char *name, VDStringA& value) override {
			std::lock_guard lock(mMutex);
			Handle *handle = static_cast<Handle *>(key);
			return handle && mMemory.GetString(handle->mpMemoryKey, name, value);
		}

		bool GetString(void *key, const char *name, VDStringW& value) override {
			std::lock_guard lock(mMutex);
			Handle *handle = static_cast<Handle *>(key);
			return handle && mMemory.GetString(handle->mpMemoryKey, name, value);
		}

		int GetBinaryLength(void *key, const char *name) override {
			std::lock_guard lock(mMutex);
			Handle *handle = static_cast<Handle *>(key);
			return handle ? mMemory.GetBinaryLength(handle->mpMemoryKey, name) : -1;
		}

		bool GetBinary(void *key, const char *name, char *data, int maximumLength) override {
			std::lock_guard lock(mMutex);
			Handle *handle = static_cast<Handle *>(key);
			return handle
				&& mMemory.GetBinary(handle->mpMemoryKey, name, data, maximumLength);
		}

		bool RemoveValue(void *key, const char *name) override {
			std::lock_guard lock(mMutex);
			Handle *handle = WritableHandle(key);
			if (!handle || !mMemory.RemoveValue(handle->mpMemoryKey, name))
				return false;
			return FinishMutation(handle);
		}

		bool RemoveKey(void *key, const char *name) override {
			std::lock_guard lock(mMutex);
			Handle *handle = WritableHandle(key);
			if (!handle || !mMemory.RemoveKey(handle->mpMemoryKey, name))
				return false;
			return FinishMutation(handle);
		}

		bool RemoveKeyRecursive(void *key, const char *name) override {
			std::lock_guard lock(mMutex);
			Handle *handle = WritableHandle(key);
			if (!handle || !mMemory.RemoveKeyRecursive(handle->mpMemoryKey, name))
				return false;
			return FinishMutation(handle);
		}

		void *EnumKeysBegin(void *key) override {
			std::lock_guard lock(mMutex);
			Handle *handle = static_cast<Handle *>(key);
			return handle ? mMemory.EnumKeysBegin(handle->mpMemoryKey) : nullptr;
		}

		const char *EnumKeysNext(void *enumerator) override {
			return enumerator ? mMemory.EnumKeysNext(enumerator) : nullptr;
		}

		void EnumKeysClose(void *enumerator) override {
			if (enumerator)
				mMemory.EnumKeysClose(enumerator);
		}

		void *EnumValuesBegin(void *key) override {
			std::lock_guard lock(mMutex);
			Handle *handle = static_cast<Handle *>(key);
			return handle ? mMemory.EnumValuesBegin(handle->mpMemoryKey) : nullptr;
		}

		const char *EnumValuesNext(void *enumerator) override {
			return enumerator ? mMemory.EnumValuesNext(enumerator) : nullptr;
		}

		void EnumValuesClose(void *enumerator) override {
			if (enumerator)
				mMemory.EnumValuesClose(enumerator);
		}

	private:
		struct Handle {
			void *mpMemoryKey;
			bool mbMachine;
			bool mbWritable;
			bool mbOwned;
		};

		Handle *WritableHandle(void *key) {
			Handle *handle = static_cast<Handle *>(key);
			return handle && handle->mbWritable ? handle : nullptr;
		}

		void MarkDirty(bool machine) {
			(machine ? mbMachineDirty : mbUserDirty) = true;
		}

		bool FinishMutation(Handle *handle) {
			MarkDirty(handle->mbMachine);
			if (handle->mbOwned)
				return true;
			try {
				return PersistRoot(handle->mbMachine);
			} catch(...) {
				return false;
			}
		}

		CFStringRef GetPreferenceKey(bool machine) const {
			return machine ? kVDRegistryMachinePreference : kVDRegistryUserPreference;
		}

		CFStringRef GetPreferenceUser(bool machine) const {
			return machine ? kCFPreferencesAnyUser : kCFPreferencesCurrentUser;
		}

		CFStringRef GetPreferenceHost(bool machine) const {
			return machine ? kCFPreferencesCurrentHost : kCFPreferencesAnyHost;
		}

		void LoadRoot(bool machine) {
			const CFStringRef user = GetPreferenceUser(machine);
			const CFStringRef host = GetPreferenceHost(machine);
			CFPreferencesSynchronize(kVDRegistryApplicationID, user, host);
			VDCFRef<CFPropertyListRef> stored(CFPreferencesCopyValue(
				GetPreferenceKey(machine),
				kVDRegistryApplicationID,
				user,
				host));
			if (!stored || CFGetTypeID(stored.get()) != CFDictionaryGetTypeID())
				return;

			try {
				DeserializeNode(
					machine ? mMemory.GetMachineKey() : mMemory.GetUserKey(),
					static_cast<CFDictionaryRef>(stored.get()));
			} catch(...) {
			}
		}

		bool PersistRoot(bool machine) {
			bool& dirty = machine ? mbMachineDirty : mbUserDirty;
			if (!dirty)
				return true;

			VDCFRef<CFDictionaryRef> tree(SerializeNode(
				machine ? mMemory.GetMachineKey() : mMemory.GetUserKey()));
			if (!tree)
				return false;

			const CFStringRef user = GetPreferenceUser(machine);
			const CFStringRef host = GetPreferenceHost(machine);
			const auto values = static_cast<CFDictionaryRef>(
				CFDictionaryGetValue(tree.get(), kVDRegistryValues));
			const auto keys = static_cast<CFDictionaryRef>(
				CFDictionaryGetValue(tree.get(), kVDRegistryKeys));
			const bool empty = values && keys
				&& !CFDictionaryGetCount(values)
				&& !CFDictionaryGetCount(keys);
			CFPreferencesSetValue(
				GetPreferenceKey(machine),
				empty ? nullptr : tree.get(),
				kVDRegistryApplicationID,
				user,
				host);
			if (!CFPreferencesSynchronize(kVDRegistryApplicationID, user, host))
				return false;

			dirty = false;
			return true;
		}

		CFDictionaryRef SerializeNode(void *key) {
			VDCFRef<CFMutableDictionaryRef> node(CFDictionaryCreateMutable(
				kCFAllocatorDefault,
				0,
				&kCFTypeDictionaryKeyCallBacks,
				&kCFTypeDictionaryValueCallBacks));
			VDCFRef<CFMutableDictionaryRef> values(CFDictionaryCreateMutable(
				kCFAllocatorDefault,
				0,
				&kCFTypeDictionaryKeyCallBacks,
				&kCFTypeDictionaryValueCallBacks));
			VDCFRef<CFMutableDictionaryRef> keys(CFDictionaryCreateMutable(
				kCFAllocatorDefault,
				0,
				&kCFTypeDictionaryKeyCallBacks,
				&kCFTypeDictionaryValueCallBacks));
			if (!node || !values || !keys)
				return nullptr;

			void *valueEnumerator = mMemory.EnumValuesBegin(key);
			while(const char *valueName = mMemory.EnumValuesNext(valueEnumerator)) {
				const std::string name(valueName);
				VDCFRef<CFStringRef> cfName = VDCFStringFromUTF8(name.c_str());
				if (!cfName)
					continue;

				switch(mMemory.GetType(key, name.c_str())) {
					case kTypeInt: {
						int value = 0;
						if (mMemory.GetInt(key, name.c_str(), value)) {
							VDCFRef<CFNumberRef> number(CFNumberCreate(
								kCFAllocatorDefault,
								kCFNumberIntType,
								&value));
							if (number)
								CFDictionarySetValue(values.get(), cfName.get(), number.get());
						}
						break;
					}

					case kTypeString: {
						VDStringW value;
						if (mMemory.GetString(key, name.c_str(), value)) {
							const VDStringA valueUTF8 = VDTextWToU8(value.c_str(), -1);
							VDCFRef<CFStringRef> stringValue =
								VDCFStringFromUTF8(valueUTF8.c_str());
							if (stringValue)
								CFDictionarySetValue(values.get(), cfName.get(), stringValue.get());
						}
						break;
					}

					case kTypeBinary: {
						const int length = mMemory.GetBinaryLength(key, name.c_str());
						if (length >= 0) {
							std::vector<UInt8> data(static_cast<size_t>(length));
							if (mMemory.GetBinary(
								key,
								name.c_str(),
								reinterpret_cast<char *>(data.data()),
								length)) {
								VDCFRef<CFDataRef> binary(CFDataCreate(
									kCFAllocatorDefault,
									data.data(),
									length));
								if (binary)
									CFDictionarySetValue(values.get(), cfName.get(), binary.get());
							}
						}
						break;
					}

					default:
						break;
				}
			}
			mMemory.EnumValuesClose(valueEnumerator);

			void *keyEnumerator = mMemory.EnumKeysBegin(key);
			while(const char *childName = mMemory.EnumKeysNext(keyEnumerator)) {
				const std::string name(childName);
				void *childKey = mMemory.CreateKey(key, name.c_str(), false);
				if (!childKey)
					continue;

				VDCFRef<CFDictionaryRef> child(SerializeNode(childKey));
				mMemory.CloseKey(childKey);
				VDCFRef<CFStringRef> cfName = VDCFStringFromUTF8(name.c_str());
				if (child && cfName)
					CFDictionarySetValue(keys.get(), cfName.get(), child.get());
			}
			mMemory.EnumKeysClose(keyEnumerator);

			CFDictionarySetValue(node.get(), kVDRegistryValues, values.get());
			CFDictionarySetValue(node.get(), kVDRegistryKeys, keys.get());
			return node.release();
		}

		void DeserializeNode(void *key, CFDictionaryRef node) {
			const auto values = static_cast<CFDictionaryRef>(
				CFDictionaryGetValue(node, kVDRegistryValues));
			if (values && CFGetTypeID(values) == CFDictionaryGetTypeID())
				DeserializeValues(key, values);

			const auto keys = static_cast<CFDictionaryRef>(
				CFDictionaryGetValue(node, kVDRegistryKeys));
			if (!keys || CFGetTypeID(keys) != CFDictionaryGetTypeID())
				return;

			const size_t count = static_cast<size_t>(CFDictionaryGetCount(keys));
			std::vector<const void *> names(count);
			std::vector<const void *> children(count);
			CFDictionaryGetKeysAndValues(keys, names.data(), children.data());
			for(size_t index = 0; index < count; ++index) {
				if (CFGetTypeID(names[index]) != CFStringGetTypeID()
					|| CFGetTypeID(children[index]) != CFDictionaryGetTypeID())
					continue;

				std::string name;
				if (!VDCFStringToUTF8(static_cast<CFStringRef>(names[index]), name))
					continue;

				void *childKey = mMemory.CreateKey(key, name.c_str(), true);
				if (!childKey)
					continue;
				DeserializeNode(childKey, static_cast<CFDictionaryRef>(children[index]));
				mMemory.CloseKey(childKey);
			}
		}

		void DeserializeValues(void *key, CFDictionaryRef values) {
			const size_t count = static_cast<size_t>(CFDictionaryGetCount(values));
			std::vector<const void *> names(count);
			std::vector<const void *> storedValues(count);
			CFDictionaryGetKeysAndValues(values, names.data(), storedValues.data());
			for(size_t index = 0; index < count; ++index) {
				if (CFGetTypeID(names[index]) != CFStringGetTypeID())
					continue;

				std::string name;
				if (!VDCFStringToUTF8(static_cast<CFStringRef>(names[index]), name))
					continue;

				CFTypeRef value = storedValues[index];
				const CFTypeID type = CFGetTypeID(value);
				if (type == CFNumberGetTypeID()) {
					int integer = 0;
					if (CFNumberGetValue(
						static_cast<CFNumberRef>(value),
						kCFNumberIntType,
						&integer))
						mMemory.SetInt(key, name.c_str(), integer);
				} else if (type == CFBooleanGetTypeID()) {
					mMemory.SetBool(
						key,
						name.c_str(),
						CFBooleanGetValue(static_cast<CFBooleanRef>(value)));
				} else if (type == CFStringGetTypeID()) {
					std::string stringValue;
					if (VDCFStringToUTF8(static_cast<CFStringRef>(value), stringValue)) {
						const VDStringW wideValue =
							VDTextU8ToW(stringValue.c_str(), static_cast<int>(stringValue.size()));
						mMemory.SetString(key, name.c_str(), wideValue.c_str());
					}
				} else if (type == CFDataGetTypeID()) {
					const auto data = static_cast<CFDataRef>(value);
					const CFIndex length = CFDataGetLength(data);
					if (length <= INT_MAX)
						mMemory.SetBinary(
							key,
							name.c_str(),
							reinterpret_cast<const char *>(CFDataGetBytePtr(data)),
							static_cast<int>(length));
				}
			}
		}

		VDRegistryProviderMemory mMemory;
		Handle mUserRoot;
		Handle mMachineRoot;
		std::mutex mMutex;
		bool mbUserDirty = false;
		bool mbMachineDirty = false;
	};

	VDRegistryProviderMac gVDRegistryProviderMac;
	IVDRegistryProvider *gVDRegistryProvider = &gVDRegistryProviderMac;
}

IVDRegistryProvider *VDGetDefaultRegistryProvider() {
	return &gVDRegistryProviderMac;
}

IVDRegistryProvider *VDGetRegistryProvider() {
	return gVDRegistryProvider;
}

void VDSetRegistryProvider(IVDRegistryProvider *provider) {
	gVDRegistryProvider = provider;
}

VDRegistryKey::VDRegistryKey(const char *keyName, bool global, bool write) {
	IVDRegistryProvider *provider = VDGetRegistryProvider();
	void *rootKey = global ? provider->GetMachineKey() : provider->GetUserKey();
	mKey = provider->CreateKey(rootKey, keyName, write);
}

VDRegistryKey::VDRegistryKey(VDRegistryKey& baseKey, const char *name, bool write) {
	IVDRegistryProvider *provider = VDGetRegistryProvider();
	void *rootKey = baseKey.getRawHandle();
	mKey = rootKey ? provider->CreateKey(rootKey, name, write) : nullptr;
}

VDRegistryKey::VDRegistryKey(VDRegistryKey&& source)
	: mKey(source.mKey) {
	source.mKey = nullptr;
}

VDRegistryKey::~VDRegistryKey() {
	if (mKey)
		VDGetRegistryProvider()->CloseKey(mKey);
}

VDRegistryKey& VDRegistryKey::operator=(VDRegistryKey&& source) {
	if (&source != this) {
		if (mKey)
			VDGetRegistryProvider()->CloseKey(mKey);
		mKey = source.mKey;
		source.mKey = nullptr;
	}
	return *this;
}

bool VDRegistryKey::setBool(const char *name, bool value) const {
	return mKey && VDGetRegistryProvider()->SetBool(mKey, name, value);
}

bool VDRegistryKey::setInt(const char *name, int value) const {
	return mKey && VDGetRegistryProvider()->SetInt(mKey, name, value);
}

bool VDRegistryKey::setString(const char *name, const char *value) const {
	return mKey && VDGetRegistryProvider()->SetString(mKey, name, value);
}

bool VDRegistryKey::setString(const char *name, const wchar_t *value) const {
	return mKey && VDGetRegistryProvider()->SetString(mKey, name, value);
}

bool VDRegistryKey::setBinary(const char *name, const char *data, int length) const {
	return mKey && VDGetRegistryProvider()->SetBinary(mKey, name, data, length);
}

VDRegistryKey::Type VDRegistryKey::getValueType(const char *name) const {
	Type type = kTypeUnknown;
	if (mKey) {
		switch(VDGetRegistryProvider()->GetType(mKey, name)) {
			case IVDRegistryProvider::kTypeInt:
				type = kTypeInt;
				break;
			case IVDRegistryProvider::kTypeString:
				type = kTypeString;
				break;
			case IVDRegistryProvider::kTypeBinary:
				type = kTypeBinary;
				break;
			default:
				break;
		}
	}
	return type;
}

bool VDRegistryKey::getBool(const char *name, bool defaultValue) const {
	bool value;
	return mKey && VDGetRegistryProvider()->GetBool(mKey, name, value)
		? value
		: defaultValue;
}

int VDRegistryKey::getInt(const char *name, int defaultValue) const {
	int value;
	return mKey && VDGetRegistryProvider()->GetInt(mKey, name, value)
		? value
		: defaultValue;
}

int VDRegistryKey::getEnumInt(const char *name, int maximumValue, int defaultValue) const {
	int value = getInt(name, defaultValue);
	if (value < 0 || value >= maximumValue)
		value = defaultValue;
	return value;
}

bool VDRegistryKey::getString(const char *name, VDStringA& value) const {
	return mKey && VDGetRegistryProvider()->GetString(mKey, name, value);
}

bool VDRegistryKey::getString(const char *name, VDStringW& value) const {
	return mKey && VDGetRegistryProvider()->GetString(mKey, name, value);
}

int VDRegistryKey::getBinaryLength(const char *name) const {
	return mKey ? VDGetRegistryProvider()->GetBinaryLength(mKey, name) : -1;
}

bool VDRegistryKey::getBinary(const char *name, char *data, int maximumLength) const {
	return mKey
		&& VDGetRegistryProvider()->GetBinary(mKey, name, data, maximumLength);
}

bool VDRegistryKey::removeValue(const char *name) {
	return mKey && VDGetRegistryProvider()->RemoveValue(mKey, name);
}

bool VDRegistryKey::removeKey(const char *name) {
	return mKey && VDGetRegistryProvider()->RemoveKey(mKey, name);
}

bool VDRegistryKey::removeKeyRecursive(const char *name) {
	return mKey && VDGetRegistryProvider()->RemoveKeyRecursive(mKey, name);
}

VDRegistryValueIterator::VDRegistryValueIterator(const VDRegistryKey& key)
	: mEnumerator(key.getRawHandle()
		? VDGetRegistryProvider()->EnumValuesBegin(key.getRawHandle())
		: nullptr) {
}

VDRegistryValueIterator::~VDRegistryValueIterator() {
	if (mEnumerator)
		VDGetRegistryProvider()->EnumValuesClose(mEnumerator);
}

const char *VDRegistryValueIterator::Next() {
	return mEnumerator ? VDGetRegistryProvider()->EnumValuesNext(mEnumerator) : nullptr;
}

VDRegistryKeyIterator::VDRegistryKeyIterator(const VDRegistryKey& key)
	: mEnumerator(key.getRawHandle()
		? VDGetRegistryProvider()->EnumKeysBegin(key.getRawHandle())
		: nullptr) {
}

VDRegistryKeyIterator::~VDRegistryKeyIterator() {
	if (mEnumerator)
		VDGetRegistryProvider()->EnumKeysClose(mEnumerator);
}

const char *VDRegistryKeyIterator::Next() {
	return mEnumerator ? VDGetRegistryProvider()->EnumKeysNext(mEnumerator) : nullptr;
}

VDString VDRegistryAppKey::s_appbase;

VDRegistryAppKey::VDRegistryAppKey()
	: VDRegistryKey(s_appbase.c_str()) {
}

VDRegistryAppKey::VDRegistryAppKey(const char *key, bool write, bool global)
	: VDRegistryKey((s_appbase + key).c_str(), global, write) {
}

void VDRegistryAppKey::setDefaultKey(const char *applicationName) {
	s_appbase = applicationName;
}

const char *VDRegistryAppKey::getDefaultKey() {
	return s_appbase.c_str();
}

namespace {
	void VDRegistryCopyRecursive(
		IVDRegistryProvider& destinationProvider,
		void *destinationParentKey,
		const char *destinationPath,
		IVDRegistryProvider& sourceProvider,
		void *sourceParentKey,
		const char *sourcePath) {
		void *sourceKey = sourceProvider.CreateKey(sourceParentKey, sourcePath, false);
		if (!sourceKey)
			return;

		void *destinationKey =
			destinationProvider.CreateKey(destinationParentKey, destinationPath, true);
		if (destinationKey) {
			void *valueEnumerator = sourceProvider.EnumValuesBegin(sourceKey);
			if (valueEnumerator) {
				while(const char *valueName = sourceProvider.EnumValuesNext(valueEnumerator)) {
					switch(sourceProvider.GetType(sourceKey, valueName)) {
						case IVDRegistryProvider::kTypeInt: {
							int value = 0;
							if (sourceProvider.GetInt(sourceKey, valueName, value))
								destinationProvider.SetInt(destinationKey, valueName, value);
							break;
						}
						case IVDRegistryProvider::kTypeString: {
							VDStringW value;
							if (sourceProvider.GetString(sourceKey, valueName, value))
								destinationProvider.SetString(
									destinationKey, valueName, value.c_str());
							break;
						}
						case IVDRegistryProvider::kTypeBinary: {
							const int length =
								sourceProvider.GetBinaryLength(sourceKey, valueName);
							if (length >= 0) {
								vdblock<char> data(static_cast<size_t>(length));
								if (sourceProvider.GetBinary(
									sourceKey, valueName, data.data(), length))
									destinationProvider.SetBinary(
										destinationKey, valueName, data.data(), length);
							}
							break;
						}
						default:
							break;
					}
				}
				sourceProvider.EnumValuesClose(valueEnumerator);
			}

			void *keyEnumerator = sourceProvider.EnumKeysBegin(sourceKey);
			if (keyEnumerator) {
				while(const char *childName = sourceProvider.EnumKeysNext(keyEnumerator)) {
					const VDStringA name(childName);
					VDRegistryCopyRecursive(
						destinationProvider,
						destinationKey,
						name.c_str(),
						sourceProvider,
						sourceKey,
						name.c_str());
				}
				sourceProvider.EnumKeysClose(keyEnumerator);
			}

			destinationProvider.CloseKey(destinationKey);
		}

		sourceProvider.CloseKey(sourceKey);
	}
}

void VDRegistryCopy(
	IVDRegistryProvider& destinationProvider,
	const char *destinationPath,
	IVDRegistryProvider& sourceProvider,
	const char *sourcePath) {
	VDRegistryCopyRecursive(
		destinationProvider,
		destinationProvider.GetUserKey(),
		destinationPath,
		sourceProvider,
		sourceProvider.GetUserKey(),
		sourcePath);
}
