// Altirra platform audio backend factory

#ifndef f_AT_ATAUDIO_INTERNAL_AUDIOOUTPUTBACKEND_H
#define f_AT_ATAUDIO_INTERNAL_AUDIOOUTPUTBACKEND_H

#include <at/ataudio/audioout.h>
#include <at/ataudio/audiooutput.h>

uint32 ATGetNativeAudioApiCandidates(
	ATAudioApi selectedApi, ATAudioApi *candidates, uint32 capacity);
IVDAudioOutput *ATCreateNativeAudioOutput(ATAudioApi api);

#endif
