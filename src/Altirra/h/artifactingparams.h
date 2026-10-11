// Altirra - Atari 800/800XL emulator
// Copyright (C) 2008 Avery Lee
// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef f_AT_ARTIFACTINGPARAMS_H
#define f_AT_ARTIFACTINGPARAMS_H

#include <vd2/system/vdtypes.h>

struct ATArtifactingParams {
	// Intensity ratio of darkest point between scanlines to the brightest portion of
	// scanlines, in gamma space.
	float mScanlineIntensity;

	// Horizontal view angle in degrees (0-179).
	float mDistortionViewAngleX;

	// Ratio of vertical distortion to horizontal distortion.
	float mDistortionYRatio;

	bool mbEnableBloom;
	bool mbBloomScanlineCompensation;
	float mBloomRadius;
	float mBloomDirectIntensity;
	float mBloomIndirectIntensity;

	bool mbEnableHDR;
	float mSDRIntensity;
	float mHDRIntensity;
	bool mbUseSystemSDR;
	bool mbUseSystemSDRAsHDR;

	static ATArtifactingParams GetDefault();
};

#endif
