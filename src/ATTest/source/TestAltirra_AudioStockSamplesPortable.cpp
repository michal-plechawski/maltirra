// Altirra portable stock audio sample tests

#include <array>
#include <cmath>

#include <at/attest/portabletest.h>
#include <at/ataudio/audiosamplepool.h>
#include <audiostocksamples.h>
#include "../../ATAudio/h/at/ataudio/audiosamplebuffer.h"

namespace {
	struct ExpectedSample {
		ATAudioSampleId mId;
		uint32 mSampleCount;
		float mBaseVolume;
		sint16 mFirstSample;
		sint16 mLastSample;
	};
}

bool ATTestAltirraAudioStockSamples(ATPortableTestContext& context) {
	static constexpr std::array kExpectedSamples {
		ExpectedSample { kATAudioSampleId_DiskRotation, 32012, 0.05f, 604, 4 },
		ExpectedSample { kATAudioSampleId_DiskStep1, 1934, 0.4f, -6, -148 },
		ExpectedSample { kATAudioSampleId_DiskStep2, 889, 0.8f, 2238, -2002 },
		ExpectedSample { kATAudioSampleId_DiskStep2H, 444, 0.8f, 2238, -2123 },
		ExpectedSample { kATAudioSampleId_DiskStep3, 5717, 0.4f, 1717, -166 },
		ExpectedSample { kATAudioSampleId_SpeakerStep, 2560, 1.0f, 84, 0 },
		ExpectedSample { kATAudioSampleId_1030Relay, 6949, 1.0f, -35, 44 },
		ExpectedSample { kATAudioSampleId_Printer1029Pin, 506, 0.2f, -5375, -2 },
		ExpectedSample { kATAudioSampleId_Printer1029Platen, 11717, 0.1f, 1782, -5398 },
		ExpectedSample { kATAudioSampleId_Printer1029Retract, 21650, 0.1f, 25589, -1 },
		ExpectedSample { kATAudioSampleId_Printer1029Home, 5968, 0.2f, 0, -2 },
		ExpectedSample { kATAudioSampleId_Printer1025Feed, 5717, 0.05f, 7712, 0 },
	};

	ATAudioSamplePool pool;
	ATAudioRegisterStockSamples(pool);

	AT_PORTABLE_TEST_ASSERT(context, pool.GetStockSample(kATAudioSampleId_None) == nullptr);

	for(const ExpectedSample& expected : kExpectedSamples) {
		const ATAudioSampleBuffer *const sample = pool.GetStockSample(expected.mId);
		AT_PORTABLE_TEST_ASSERT(context, sample != nullptr);
		AT_PORTABLE_TEST_ASSERT(context, sample->mSampleCount == expected.mSampleCount);
		AT_PORTABLE_TEST_ASSERT(context, sample->mSamplingRate.mUnit == ATAudioSamplingRateUnit::Hz);
		AT_PORTABLE_TEST_ASSERT(context, sample->mSamplingRate.mValue == 63920.8f);
		AT_PORTABLE_TEST_ASSERT(context,
			std::abs(sample->mVolume - expected.mBaseVolume * (1.0f / 32767.0f)) < 1e-9f);

		const sint16 *const data = sample->GetOneShotSampleStart();
		AT_PORTABLE_TEST_ASSERT(context, data[0] == expected.mFirstSample);
		AT_PORTABLE_TEST_ASSERT(context, data[sample->mSampleCount - 1] == expected.mLastSample);
	}

	AT_PORTABLE_TEST_ASSERT(context,
		pool.GetStockSample(static_cast<ATAudioSampleId>(kExpectedSamples.size() + 1)) == nullptr);
	return true;
}
