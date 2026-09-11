// Altirra portable cartridge image tests

#include <array>
#include <cstring>
#include <cwchar>
#include <vector>

#include <at/attest/portabletest.h>
#include <at/atio/cartridgeimage.h>
#include <vd2/system/binary.h>
#include <vd2/system/file.h>
#include <vd2/system/filesys.h>
#include <vd2/system/thread.h>
#include <vd2/system/time.h>

#if VD_CPU_X86 || VD_CPU_X64 || VD_CPU_ARM64
uint32 ATComputeCartridgeImageByteSum32(const uint8 *src, size_t len);
#endif

namespace {
	class ATCartridgeImageTestSandbox {
	public:
		explicit ATCartridgeImageTestSandbox(const VDStringW& basePath)
			: mBasePath(basePath)
			, mImagePath(VDMakePath(basePath.c_str(), L"saved.car")) {
		}

		~ATCartridgeImageTestSandbox() {
			VDRemoveFile(mImagePath.c_str());

			try {
				VDRemoveDirectory(mBasePath.c_str());
			} catch(...) {
			}
		}

		VDStringW mBasePath;
		VDStringW mImagePath;
	};

	uint32 ATComputeByteSumReference(const uint8 *src, size_t len) {
		uint32 sum = 0;

		while(len--)
			sum += *src++;

		return sum;
	}

	bool ATCheckAcceleratedByteSum(ATPortableTestContext& context) {
#if VD_CPU_X86 || VD_CPU_X64 || VD_CPU_ARM64
		static constexpr std::array<size_t, 17> kLengths {
			0, 1, 2, 15, 16, 17, 31, 63, 64, 65,
			1023, 1024, 1025, 4095, 4096, 65535, 65536
		};
		std::vector<uint8> data(65536 + 32);

		uint32 state = UINT32_C(0xC001D00D);
		for(uint8& value : data) {
			state = state * UINT32_C(1664525) + UINT32_C(1013904223);
			value = static_cast<uint8>(state >> 24);
		}

		const std::vector<uint8> original = data;
		for(size_t offset = 0; offset < 32; ++offset) {
			for(size_t len : kLengths) {
				AT_PORTABLE_TEST_ASSERT(
					context,
					ATComputeCartridgeImageByteSum32(data.data() + offset, len)
						== ATComputeByteSumReference(data.data() + offset, len));
			}
		}
		AT_PORTABLE_TEST_ASSERT(context, data == original);

		// The .CAR checksum is explicitly modulo 2^32. Exercise the wrap
		// independently of alignment and SIMD lane reduction.
		std::vector<uint8> wrappingData(17 * 1024 * 1024 + 3, UINT8_C(0xFF));
		AT_PORTABLE_TEST_ASSERT(
			context,
			ATComputeCartridgeImageByteSum32(
				wrappingData.data() + 1, wrappingData.size() - 2)
				== ATComputeByteSumReference(
					wrappingData.data() + 1, wrappingData.size() - 2));
#endif

		return true;
	}

	bool ATCheckCreateAndRawLoad(ATPortableTestContext& context) {
		vdrefptr<IATCartridgeImage> created;
		ATCreateCartridgeImage(kATCartridgeMode_8K, ~created);

		AT_PORTABLE_TEST_ASSERT(context, created != nullptr);
		AT_PORTABLE_TEST_ASSERT(context, created->GetImageType() == kATImageType_Cartridge);
		AT_PORTABLE_TEST_ASSERT(context, created->GetMode() == kATCartridgeMode_8K);
		AT_PORTABLE_TEST_ASSERT(context, created->GetImageSize() == 8192);
		AT_PORTABLE_TEST_ASSERT(context, created->GetPath() == nullptr);
		AT_PORTABLE_TEST_ASSERT(context, !created->IsDirty());
		AT_PORTABLE_TEST_ASSERT(context, !created->GetImageFileCRC().has_value());
		AT_PORTABLE_TEST_ASSERT(context, !created->GetImageFileSHA256().has_value());

		const uint8 *createdData = static_cast<const uint8 *>(created->GetBuffer());
		for(size_t i = 0; i < 8192; ++i)
			AT_PORTABLE_TEST_ASSERT(context, createdData[i] == UINT8_C(0xFF));

		const uint64 initialChecksum = created->GetChecksum();
		static_cast<uint8 *>(created->GetBuffer())[1234] = UINT8_C(0x42);
		created->SetDirty();
		AT_PORTABLE_TEST_ASSERT(context, created->IsDirty());
		AT_PORTABLE_TEST_ASSERT(context, created->GetChecksum() != initialChecksum);
		created->SetClean();
		AT_PORTABLE_TEST_ASSERT(context, !created->IsDirty());

		std::vector<uint8> rawImage(2048);
		for(size_t i = 0; i < rawImage.size(); ++i)
			rawImage[i] = static_cast<uint8>((i * 37 + 11) & 0xFF);

		VDMemoryStream rawStream(rawImage.data(), static_cast<uint32>(rawImage.size()));
		ATCartLoadContext loadContext;
		vdrefptr<IATCartridgeImage> loaded;
		AT_PORTABLE_TEST_ASSERT(
			context,
			ATLoadCartridgeImage(
				L"memory.rom", rawStream, &loadContext, ~loaded));
		AT_PORTABLE_TEST_ASSERT(context, loaded->GetMode() == kATCartridgeMode_8K);
		AT_PORTABLE_TEST_ASSERT(context, loaded->GetImageSize() == 8192);
		AT_PORTABLE_TEST_ASSERT(context, !wcscmp(loaded->GetPath(), L"memory.rom"));
		AT_PORTABLE_TEST_ASSERT(context, loadContext.mCartSize == 2048);
		AT_PORTABLE_TEST_ASSERT(context, loadContext.mCartMapper == kATCartridgeMode_8K);
		AT_PORTABLE_TEST_ASSERT(context, loadContext.mLoadStatus == kATCartLoadStatus_Ok);
		AT_PORTABLE_TEST_ASSERT(context, loaded->GetImageFileCRC().has_value());
		AT_PORTABLE_TEST_ASSERT(context, loaded->GetImageFileSHA256().has_value());
		AT_PORTABLE_TEST_ASSERT(context, !loaded->IsDirty());

		const uint8 *loadedData = static_cast<const uint8 *>(loaded->GetBuffer());
		for(size_t copy = 0; copy < 4; ++copy) {
			AT_PORTABLE_TEST_ASSERT(
				context,
				!memcmp(loadedData + copy * rawImage.size(),
					rawImage.data(), rawImage.size()));
		}

		VDMemoryStream twoKStream(rawImage.data(), static_cast<uint32>(rawImage.size()));
		ATCartLoadContext twoKContext;
		twoKContext.mCartMapper = kATCartridgeMode_2K;
		vdrefptr<IATCartridgeImage> twoKImage;
		AT_PORTABLE_TEST_ASSERT(
			context,
			ATLoadCartridgeImage(
				nullptr, twoKStream, &twoKContext, ~twoKImage));
		AT_PORTABLE_TEST_ASSERT(context, twoKImage->GetMode() == kATCartridgeMode_2K);
		AT_PORTABLE_TEST_ASSERT(context, twoKImage->GetImageSize() == 2048);
		const uint8 *twoKData = static_cast<const uint8 *>(twoKImage->GetBuffer());
		for(size_t i = 0; i < 6144; ++i)
			AT_PORTABLE_TEST_ASSERT(context, twoKData[i] == UINT8_C(0xFF));
		AT_PORTABLE_TEST_ASSERT(
			context,
			!memcmp(twoKData + 6144, rawImage.data(), rawImage.size()));

		return true;
	}

	bool ATCheckCARLoadAndSave(ATPortableTestContext& context) {
		constexpr size_t kPayloadSize = 65536;
		std::vector<uint8> carImage(16 + kPayloadSize);
		memcpy(carImage.data(), "CART", 4);
		VDWriteUnalignedBEU32(carImage.data() + 4, 13);

		for(size_t i = 0; i < kPayloadSize; ++i)
			carImage[16 + i] = static_cast<uint8>((i * 29 + (i >> 8)) & 0xFF);

		const uint32 payloadSum = ATComputeByteSumReference(
			carImage.data() + 16, kPayloadSize);
		VDWriteUnalignedBEU32(carImage.data() + 8, payloadSum);

		VDMemoryStream carStream(carImage.data(), static_cast<uint32>(carImage.size()));
		ATCartLoadContext loadContext;
		vdrefptr<IATCartridgeImage> loaded;
		AT_PORTABLE_TEST_ASSERT(
			context,
			ATLoadCartridgeImage(L"memory.car", carStream, &loadContext, ~loaded));
		AT_PORTABLE_TEST_ASSERT(context, loaded->GetMode() == kATCartridgeMode_XEGS_64K);
		AT_PORTABLE_TEST_ASSERT(context, loaded->GetImageSize() == kPayloadSize);
		AT_PORTABLE_TEST_ASSERT(context, loadContext.mCartSize == kPayloadSize);
		AT_PORTABLE_TEST_ASSERT(
			context,
			!memcmp(loaded->GetBuffer(), carImage.data() + 16, kPayloadSize));
		AT_PORTABLE_TEST_ASSERT(context, loaded->GetImageFileCRC().has_value());
		AT_PORTABLE_TEST_ASSERT(context, loaded->GetImageFileSHA256().has_value());

		std::vector<uint8> corruptImage = carImage;
		VDWriteUnalignedBEU32(corruptImage.data() + 8, payloadSum + 1);
		VDMemoryStream corruptStream(
			corruptImage.data(), static_cast<uint32>(corruptImage.size()));
		vdrefptr<IATCartridgeImage> checksumFallback;
		AT_PORTABLE_TEST_ASSERT(
			context,
			ATLoadCartridgeImage(
				L"corrupt.car", corruptStream, nullptr, ~checksumFallback));
		// Preserve the existing loader contract: a bad header checksum makes
		// the input fall back to raw image detection rather than rejecting it.
		AT_PORTABLE_TEST_ASSERT(
			context,
			checksumFallback->GetMode() == kATCartridgeMode_XEGS_64K);

		VDMemoryStream ignoredStream(
			corruptImage.data(), static_cast<uint32>(corruptImage.size()));
		ATCartLoadContext ignoredContext;
		ignoredContext.mbIgnoreChecksum = true;
		vdrefptr<IATCartridgeImage> ignored;
		AT_PORTABLE_TEST_ASSERT(
			context,
			ATLoadCartridgeImage(
				L"corrupt.car", ignoredStream, &ignoredContext, ~ignored));
		AT_PORTABLE_TEST_ASSERT(context, ignored->GetMode() == kATCartridgeMode_XEGS_64K);

		static uint32 sequence = 0;
		VDStringW baseName;
		baseName.sprintf(
			L"altirra-cartridge-test-%u-%llu-%u",
			static_cast<unsigned>(VDGetCurrentProcessId()),
			static_cast<unsigned long long>(VDGetCurrentTick64()),
			static_cast<unsigned>(++sequence));
		ATCartridgeImageTestSandbox sandbox(baseName);
		VDCreateDirectory(sandbox.mBasePath.c_str());

		static_cast<uint8 *>(loaded->GetBuffer())[17] ^= UINT8_C(0x5A);
		loaded->SetDirty();
		ATSaveCartridgeImage(loaded, sandbox.mImagePath.c_str(), true);
		AT_PORTABLE_TEST_ASSERT(context, !loaded->IsDirty());

		vdrefptr<IATCartridgeImage> saved;
		ATLoadCartridgeImage(sandbox.mImagePath.c_str(), ~saved);
		AT_PORTABLE_TEST_ASSERT(context, saved->GetMode() == loaded->GetMode());
		AT_PORTABLE_TEST_ASSERT(context, saved->GetImageSize() == loaded->GetImageSize());
		AT_PORTABLE_TEST_ASSERT(
			context,
			!memcmp(saved->GetBuffer(), loaded->GetBuffer(), loaded->GetImageSize()));

		return true;
	}
}

bool ATTestIOCartridgeImage(ATPortableTestContext& context) {
	return ATCheckAcceleratedByteSum(context)
		&& ATCheckCreateAndRawLoad(context)
		&& ATCheckCARLoadAndSave(context);
}
