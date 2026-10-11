// Altirra - Atari 800/800XL emulator
// Copyright (C) 2008 Avery Lee
// SPDX-License-Identifier: GPL-2.0-or-later
#include <artifactingparams.h>

ATArtifactingParams ATArtifactingParams::GetDefault() {
	ATArtifactingParams params = {};
	params.mScanlineIntensity = 0.75f;
	params.mbEnableBloom = false;
	params.mbBloomScanlineCompensation = true;
	params.mBloomRadius = 0.86f;
	params.mBloomDirectIntensity = 0.80f;
	params.mBloomIndirectIntensity = 0.40f;
	params.mSDRIntensity = 200.0f;
	params.mHDRIntensity = 350.0f;
	params.mbUseSystemSDR = false;
	params.mbUseSystemSDRAsHDR = false;

	return params;
}
