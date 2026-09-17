// Altirra Windows native audio backend factory

#include <stdafx.h>
#include <at/ataudio/internal/audiooutputbackend.h>

uint32 ATGetNativeAudioApiCandidates(
	ATAudioApi selectedApi, ATAudioApi *candidates, uint32 capacity) {
	if (!capacity)
		return 0;

	if (selectedApi == kATAudioApi_Auto) {
		candidates[0] = kATAudioApi_WASAPI;
		if (capacity >= 2) {
			candidates[1] = kATAudioApi_WaveOut;
			return 2;
		}

		return 1;
	}

	if (selectedApi < 0 || selectedApi >= kATAudioApiCount)
		selectedApi = kATAudioApi_WaveOut;

	candidates[0] = selectedApi;
	return 1;
}

IVDAudioOutput *ATCreateNativeAudioOutput(ATAudioApi api) {
	switch(api) {
		case kATAudioApi_WASAPI:
			return VDCreateAudioOutputWASAPIW32();

		case kATAudioApi_XAudio2:
			return VDCreateAudioOutputXAudio2W32();

		case kATAudioApi_DirectSound:
			return VDCreateAudioOutputDirectSoundW32();

		default:
			return VDCreateAudioOutputWaveOutW32();
	}
}
