// Altirra portable constant and hardware trait tests

#include <array>
#include <cstring>
#include <iterator>

#include <at/attest/portabletest.h>
#include <constants.h>
#include <vd2/system/VDString.h>

namespace {
	template<typename T>
	struct EnumEntry {
		T mValue;
		const char *mpName;
	};

	template<typename T, size_t N>
	bool CheckEnumTable(
		const std::array<EnumEntry<T>, N>& entries,
		T defaultValue)
	{
		for(const auto& entry : entries) {
			if (strcmp(ATEnumToString(entry.mValue), entry.mpName))
				return false;

			const auto parsed = ATParseEnum<T>(VDStringSpanA(entry.mpName));

			if (!parsed.mValid || parsed.mValue != entry.mValue)
				return false;
		}

		const auto invalid = ATParseEnum<T>(VDStringSpanA("not-a-valid-value"));

		return !invalid.mValid
			&& invalid.mValue == defaultValue
			&& !*ATEnumToString(
				ATGetEnumLookupTable<T>(), UINT32_C(0xFFFFFFFF));
	}
}

bool ATTestAltirraConstants(ATPortableTestContext& context) {
	static constexpr std::array kProgramLoadModes {
		EnumEntry { kATHLEProgramLoadMode_Default, "default" },
		EnumEntry { kATHLEProgramLoadMode_Type3Poll, "type3poll" },
		EnumEntry { kATHLEProgramLoadMode_Deferred, "deferred" },
		EnumEntry { kATHLEProgramLoadMode_DiskBoot, "diskboot" },
	};

	static constexpr std::array kMemoryModes {
		EnumEntry { kATMemoryMode_48K, "48k" },
		EnumEntry { kATMemoryMode_52K, "52k" },
		EnumEntry { kATMemoryMode_64K, "64k" },
		EnumEntry { kATMemoryMode_128K, "128k" },
		EnumEntry { kATMemoryMode_320K, "320k" },
		EnumEntry { kATMemoryMode_576K, "576k" },
		EnumEntry { kATMemoryMode_1088K, "1088k" },
		EnumEntry { kATMemoryMode_16K, "16k" },
		EnumEntry { kATMemoryMode_8K, "8k" },
		EnumEntry { kATMemoryMode_24K, "24k" },
		EnumEntry { kATMemoryMode_32K, "32k" },
		EnumEntry { kATMemoryMode_40K, "40k" },
		EnumEntry { kATMemoryMode_320K_Compy, "320kcompy" },
		EnumEntry { kATMemoryMode_576K_Compy, "576kcompy" },
		EnumEntry { kATMemoryMode_256K, "256k" },
	};

	static constexpr std::array kHardwareModes {
		EnumEntry { kATHardwareMode_800, "800" },
		EnumEntry { kATHardwareMode_800XL, "800xl" },
		EnumEntry { kATHardwareMode_5200, "5200" },
		EnumEntry { kATHardwareMode_XEGS, "XEGS" },
		EnumEntry { kATHardwareMode_1200XL, "1200xl" },
		EnumEntry { kATHardwareMode_130XE, "130xe" },
		EnumEntry { kATHardwareMode_1400XL, "1400xl" },
	};

	static constexpr std::array kVideoStandards {
		EnumEntry { kATVideoStandard_NTSC, "ntsc" },
		EnumEntry { kATVideoStandard_PAL, "pal" },
		EnumEntry { kATVideoStandard_SECAM, "secam" },
		EnumEntry { kATVideoStandard_PAL60, "pal60" },
		EnumEntry { kATVideoStandard_NTSC50, "ntsc50" },
	};

	AT_PORTABLE_TEST_ASSERT(
		context,
		CheckEnumTable(kProgramLoadModes, kATHLEProgramLoadMode_Default));
	AT_PORTABLE_TEST_ASSERT(
		context,
		CheckEnumTable(kMemoryModes, kATMemoryMode_320K));
	AT_PORTABLE_TEST_ASSERT(
		context,
		CheckEnumTable(kHardwareModes, kATHardwareMode_800XL));
	AT_PORTABLE_TEST_ASSERT(
		context,
		CheckEnumTable(kVideoStandards, kATVideoStandard_NTSC));

	const auto mixedCaseHardwareMode =
		ATParseEnum<ATHardwareMode>(VDStringSpanA("XeGs"));
	AT_PORTABLE_TEST_ASSERT(context, mixedCaseHardwareMode.mValid);
	AT_PORTABLE_TEST_ASSERT(
		context,
		mixedCaseHardwareMode.mValue == kATHardwareMode_XEGS);

	struct ExpectedHardwareTraits {
		bool mbRunsXLOS;
		bool mbHasPort34;
		bool mbFloatingDataBus;
		bool mbInternalBASIC;
		bool mbSupportsPBI;
		bool mbHasSIO12V;
		bool mbHas9VACPower;
	};

	static constexpr ExpectedHardwareTraits kExpectedHardwareTraits[] {
		{ false, true,  true,  false, false, true,  true  }, // 800
		{ true,  false, false, true,  true,  false, false }, // 800XL
		{ false, true,  true,  false, false, false, true  }, // 5200
		{ true,  false, true,  true,  true,  false, false }, // XEGS
		{ true,  false, false, false, true,  false, true  }, // 1200XL
		{ true,  false, true,  true,  true,  false, false }, // 130XE
		{ true,  false, false, true,  true,  false, false }, // 1400XL
	};

	for(size_t i = 0; i < std::size(kExpectedHardwareTraits); ++i) {
		const auto& actual = kATHardwareModeTraits[i];
		const auto& expected = kExpectedHardwareTraits[i];

		AT_PORTABLE_TEST_ASSERT(context, actual.mbRunsXLOS == expected.mbRunsXLOS);
		AT_PORTABLE_TEST_ASSERT(context, actual.mbHasPort34 == expected.mbHasPort34);
		AT_PORTABLE_TEST_ASSERT(context, actual.mbFloatingDataBus == expected.mbFloatingDataBus);
		AT_PORTABLE_TEST_ASSERT(context, actual.mbInternalBASIC == expected.mbInternalBASIC);
		AT_PORTABLE_TEST_ASSERT(context, actual.mbSupportsPBI == expected.mbSupportsPBI);
		AT_PORTABLE_TEST_ASSERT(context, actual.mbHasSIO12V == expected.mbHasSIO12V);
		AT_PORTABLE_TEST_ASSERT(context, actual.mbHas9VACPower == expected.mbHas9VACPower);
	}

	AT_PORTABLE_TEST_ASSERT(context, kATROMImageCount == 16);
	AT_PORTABLE_TEST_ASSERT(context, kATKernelModeCount == 5);
	AT_PORTABLE_TEST_ASSERT(context, kATStorageId_UnitMask == 0x00FF);
	AT_PORTABLE_TEST_ASSERT(context, kATStorageId_TypeMask == 0xFF00);
	AT_PORTABLE_TEST_ASSERT(context, kATStorageId_Disk == 0x0100);
	AT_PORTABLE_TEST_ASSERT(context, kATStorageId_Cartridge == 0x0200);
	AT_PORTABLE_TEST_ASSERT(context, kATStorageId_Tape == 0x0300);
	AT_PORTABLE_TEST_ASSERT(context, kATStorageId_Firmware == 0x0400);
	AT_PORTABLE_TEST_ASSERT(context, kATStorageIdTypeShift == 8);
	AT_PORTABLE_TEST_ASSERT(context, kATStorageTypeMask_All == 0x7);
	AT_PORTABLE_TEST_ASSERT(context, kATMemoryClearModeCount == 5);

	return true;
}
