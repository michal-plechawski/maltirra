// Altirra portable audio output mixer tests

#include <memory>

#include <at/attest/portabletest.h>
#include <at/ataudio/audiooutput.h>

bool ATTestAudioOutput(ATPortableTestContext& context) {
	std::unique_ptr<IATAudioOutput> output { ATCreateAudioOutput() };
	AT_PORTABLE_TEST_ASSERT(context, output != nullptr);

	AT_PORTABLE_TEST_ASSERT(context, !output->GetMute());
	output->SetMute(true);
	AT_PORTABLE_TEST_ASSERT(context, output->GetMute());

	output->SetVolume(0.375f);
	AT_PORTABLE_TEST_ASSERT(context, output->GetVolume() == 0.375f);
	output->SetMixLevel(kATAudioMix_Drive, 0.625f);
	AT_PORTABLE_TEST_ASSERT(context,
		output->GetMixLevel(kATAudioMix_Drive) == 0.625f);

	output->SetLatency(1);
	AT_PORTABLE_TEST_ASSERT(context, output->GetLatency() == 10);
	output->SetLatency(1000);
	AT_PORTABLE_TEST_ASSERT(context, output->GetLatency() == 500);
	output->SetExtraBuffer(1);
	AT_PORTABLE_TEST_ASSERT(context, output->GetExtraBuffer() == 10);
	output->SetExtraBuffer(1000);
	AT_PORTABLE_TEST_ASSERT(context, output->GetExtraBuffer() == 500);

	output->SetApi(kATAudioApi_Auto);
	AT_PORTABLE_TEST_ASSERT(context, output->GetApi() == kATAudioApi_Auto);
	output->SetFiltersEnabled(false);
	output->SetFiltersEnabled(true);
	output->Pause();
	output->Resume();

	(void)output->AsMixer();
	(void)output->GetPool();
	return true;
}
