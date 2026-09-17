// Altirra portable disk timing profile tests

#include <array>
#include <iterator>

#include <at/attest/portabletest.h>
#include <diskprofile.h>

namespace {
	struct ExpectedProfile {
		ATDiskEmulationMode mMode;
		uint8 mHighSpeedIndex;
		bool mbNotReady;
		bool mbCmdHighSpeed;
		bool mbCmdFrameHighSpeed;
		bool mbPERCOM;
		bool mbFormatSkewed;
		bool mbGetHighSpeedIndex;
		bool mbHalfTracks;
		bool mbRetry1050;
		bool mbReverseForwardSeeks;
		bool mbWaitLongSectors;
		bool mbWritePercomChangesDensity;
		bool mbBufferTrackReads;
		bool mbBufferTrackReadErrors;
		bool mbBufferSector1;
		bool mbRequireCommandDeassert;
		bool mbCommandTruncation;
	};

	constexpr ExpectedProfile kExpectedProfiles[] {
		{ kATDiskEmulationMode_Generic,          16, true,  true,  false, true,  true,  false, false, false, false, false, false, false, false, false, false, false },
		{ kATDiskEmulationMode_FastestPossible,  0, true,  true,  true,  true,  true,  true,  false, false, false, false, false, false, false, false, false, false },
		{ kATDiskEmulationMode_810,              0, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false },
		{ kATDiskEmulationMode_1050,             0, true,  false, false, false, false, false, true,  true,  true,  true,  false, false, false, false, false, false },
		{ kATDiskEmulationMode_XF551,           16, false, true,  false, true,  false, false, false, true,  true,  true,  true,  false, false, false, true,  false },
		{ kATDiskEmulationMode_USDoubler,       10, true,  false, true,  true,  true,  true,  true,  true,  true,  true,  false, false, false, false, false, false },
		{ kATDiskEmulationMode_Speedy1050,       9, true,  false, true,  true,  false, true,  true,  true,  true,  true,  true,  true,  false, false, false, false },
		{ kATDiskEmulationMode_IndusGT,          0, true,  true,  false, true,  false, false, true,  true,  false, true,  true,  false, false, false, false, true  },
		{ kATDiskEmulationMode_Happy1050,       10, true,  false, true,  true,  false, true,  true,  true,  true,  true,  false, true,  true,  true,  false, false },
		{ kATDiskEmulationMode_1050Turbo,        6, true,  false, false, true,  false, false, true,  true,  true,  true,  true,  false, false, false, false, false },
		{ kATDiskEmulationMode_Generic57600,     8, true,  true,  true,  true,  true,  true,  false, false, false, false, false, false, false, false, false, false },
		{ kATDiskEmulationMode_Happy810,         0, false, false, false, false, false, false, false, false, false, false, false, true,  false, false, false, false },
	};
}

bool ATTestAltirraDiskProfile(ATPortableTestContext& context) {
	static_assert(std::size(kExpectedProfiles) == kATDiskEmulationModeCount);

	for(const ExpectedProfile& expected : kExpectedProfiles) {
		const ATDiskProfile& profile = ATGetDiskProfile(expected.mMode);
		AT_PORTABLE_TEST_ASSERT(context, &profile == &ATGetDiskProfile(expected.mMode));
		AT_PORTABLE_TEST_ASSERT(context, profile.mHighSpeedIndex == expected.mHighSpeedIndex);
		AT_PORTABLE_TEST_ASSERT(context, profile.mbSupportedNotReady == expected.mbNotReady);
		AT_PORTABLE_TEST_ASSERT(context, profile.mbSupportedCmdHighSpeed == expected.mbCmdHighSpeed);
		AT_PORTABLE_TEST_ASSERT(context, profile.mbSupportedCmdFrameHighSpeed == expected.mbCmdFrameHighSpeed);
		AT_PORTABLE_TEST_ASSERT(context, profile.mbSupportedCmdPERCOM == expected.mbPERCOM);
		AT_PORTABLE_TEST_ASSERT(context, profile.mbSupportedCmdFormatSkewed == expected.mbFormatSkewed);
		AT_PORTABLE_TEST_ASSERT(context, !profile.mbSupportedCmdFormatBoot);
		AT_PORTABLE_TEST_ASSERT(context, profile.mbSupportedCmdGetHighSpeedIndex == expected.mbGetHighSpeedIndex);
		AT_PORTABLE_TEST_ASSERT(context, profile.mbSeekHalfTracks == expected.mbHalfTracks);
		AT_PORTABLE_TEST_ASSERT(context, profile.mbRetryMode1050 == expected.mbRetry1050);
		AT_PORTABLE_TEST_ASSERT(context, profile.mbReverseOnForwardSeeks == expected.mbReverseForwardSeeks);
		AT_PORTABLE_TEST_ASSERT(context, profile.mbWaitForLongSectors == expected.mbWaitLongSectors);
		AT_PORTABLE_TEST_ASSERT(context, profile.mbWritePercomChangesDensity == expected.mbWritePercomChangesDensity);
		AT_PORTABLE_TEST_ASSERT(context, profile.mbBufferTrackReads == expected.mbBufferTrackReads);
		AT_PORTABLE_TEST_ASSERT(context, profile.mbBufferTrackReadErrors == expected.mbBufferTrackReadErrors);
		AT_PORTABLE_TEST_ASSERT(context, profile.mbBufferSector1 == expected.mbBufferSector1);
		AT_PORTABLE_TEST_ASSERT(context, profile.mbRequireCommandDeassertCheck == expected.mbRequireCommandDeassert);
		AT_PORTABLE_TEST_ASSERT(context, profile.mbSupportCommandTruncation == expected.mbCommandTruncation);

		AT_PORTABLE_TEST_ASSERT(context, profile.mCyclesPerSIOByte > 0);
		AT_PORTABLE_TEST_ASSERT(context, profile.mCyclesPerSIOBit > 0);
		AT_PORTABLE_TEST_ASSERT(context, profile.mCyclesPerTrackStep > 0);
		AT_PORTABLE_TEST_ASSERT(context, profile.mCyclesForHeadSettle > 0);
		AT_PORTABLE_TEST_ASSERT(context, profile.mCyclesToMotorOff > 0);
		AT_PORTABLE_TEST_ASSERT(context,
			profile.mCyclesToFDCCommand > profile.mCyclesToACKSent);

		const uint32 rpm = expected.mMode == kATDiskEmulationMode_XF551 ? 300 : 288;
		const uint32 expectedRotation = (uint32)(0.5 + (60.0 / rpm) * (7159090.0 / 4.0));
		AT_PORTABLE_TEST_ASSERT(context, profile.mCyclesPerDiskRotation == expectedRotation);

		if (profile.mbSupportedCmdFrameHighSpeed) {
			const uint32 divisor = profile.mHighSpeedIndex * 2 + 14;
			AT_PORTABLE_TEST_ASSERT(context, profile.mHighSpeedCmdFrameRateLo <= divisor);
			AT_PORTABLE_TEST_ASSERT(context, profile.mHighSpeedCmdFrameRateHi >= divisor);
		} else {
			AT_PORTABLE_TEST_ASSERT(context, profile.mHighSpeedCmdFrameRateLo == 0);
			AT_PORTABLE_TEST_ASSERT(context, profile.mHighSpeedCmdFrameRateHi == 0);
		}
	}

	const ATDiskProfile& synchromesh = ATGetDiskProfileIndusGTSynchromesh();
	AT_PORTABLE_TEST_ASSERT(context, synchromesh.mbSupportedCmdFormatBoot);
	AT_PORTABLE_TEST_ASSERT(context, synchromesh.mbSupportCommandTruncation);
	AT_PORTABLE_TEST_ASSERT(context, synchromesh.mHighSpeedIndex == 10);
	AT_PORTABLE_TEST_ASSERT(context, synchromesh.mCyclesPerSIOByteHighSpeed == 520);
	AT_PORTABLE_TEST_ASSERT(context, synchromesh.mCyclesPerSIOBitHighSpeed == 47);
	AT_PORTABLE_TEST_ASSERT(context, synchromesh.mCyclesPerSIOBitHighSpeedF == 47.0f);

	const ATDiskProfile& superSynchromesh = ATGetDiskProfileIndusGTSuperSynchromesh();
	AT_PORTABLE_TEST_ASSERT(context, superSynchromesh.mbSupportedCmdFormatBoot);
	AT_PORTABLE_TEST_ASSERT(context, superSynchromesh.mbSupportCommandTruncation);
	AT_PORTABLE_TEST_ASSERT(context, superSynchromesh.mHighSpeedIndex == 6);
	AT_PORTABLE_TEST_ASSERT(context, superSynchromesh.mCyclesPerSIOByteHighSpeed == 268);
	AT_PORTABLE_TEST_ASSERT(context, superSynchromesh.mCyclesPerSIOBitHighSpeed == 26);
	AT_PORTABLE_TEST_ASSERT(context, superSynchromesh.mCyclesPerSIOBitHighSpeedF == 26.0f);
	AT_PORTABLE_TEST_ASSERT(context, superSynchromesh.mCyclesCEToDataFrameHighSpeed == 571);
	AT_PORTABLE_TEST_ASSERT(context, superSynchromesh.mCyclesCEToDataFrameHighSpeedPBDiv256 == 3780);

	return true;
}
