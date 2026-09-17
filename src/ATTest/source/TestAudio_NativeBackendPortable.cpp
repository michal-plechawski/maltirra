// Altirra portable native audio backend factory tests

#include <at/attest/portabletest.h>
#include <at/ataudio/internal/audiooutputbackend.h>

bool ATTestAudioNativeBackend(ATPortableTestContext& context) {
	ATAudioApi candidates[kATAudioApiCount] {};
	AT_PORTABLE_TEST_ASSERT(context,
		ATGetNativeAudioApiCandidates(kATAudioApi_Auto, candidates, 0) == 0);

	const uint32 count = ATGetNativeAudioApiCandidates(
		kATAudioApi_Auto, candidates, kATAudioApiCount);
	AT_PORTABLE_TEST_ASSERT(context, count > 0);
	AT_PORTABLE_TEST_ASSERT(context, count <= kATAudioApiCount);

	for(uint32 i = 0; i < count; ++i) {
		AT_PORTABLE_TEST_ASSERT(context, candidates[i] < kATAudioApiCount);
		IVDAudioOutput *output = ATCreateNativeAudioOutput(candidates[i]);
		AT_PORTABLE_TEST_ASSERT(context, output != nullptr);
		delete output;
	}

	return true;
}
