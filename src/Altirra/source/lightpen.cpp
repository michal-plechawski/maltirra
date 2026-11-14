//	Altirra - Atari 800/800XL emulator
//	Copyright (C) 2009 Avery Lee
//
//	This program is free software; you can redistribute it and/or modify
//	it under the terms of the GNU General Public License as published by
//	the Free Software Foundation; either version 2 of the License, or
//	(at your option) any later version.
//
//	This program is distributed in the hope that it will be useful,
//	but WITHOUT ANY WARRANTY; without even the implied warranty of
//	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//	GNU General Public License for more details.
//
//	You should have received a copy of the GNU General Public License
//	along with this program; if not, write to the Free Software
//	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

#include <cstdlib>
#include <at/atcore/enumparseimpl.h>
#include "lightpen.h"
#include "antic.h"
#include "gtia.h"

AT_DEFINE_ENUM_TABLE_BEGIN(ATLightPenNoiseMode)
	{ ATLightPenNoiseMode::None, "none" },
	{ ATLightPenNoiseMode::Low, "low" },
	{ ATLightPenNoiseMode::High, "high" },
AT_DEFINE_ENUM_TABLE_END(ATLightPenNoiseMode, ATLightPenNoiseMode::None)

// This accounts for the discrepancy between the emulator's ANTIC counters
// and the real ANTIC counters, as well as delays in the light pen latch.
constexpr int ATLightPenPort::kHorizOffset = 12;

ATLightPenPort::ATLightPenPort() {
}

void ATLightPenPort::Init(ATAnticEmulator& antic, ATGTIAEmulator& gtia) {
	mpAntic = &antic;
	mpGTIA = &gtia;
}

bool ATLightPenPort::GetImmediateUpdateEnabled() const {
	return mbImmediateUpdate;
}

void ATLightPenPort::SetImmediateUpdateEnabled(bool enable) {
	mbImmediateUpdate = enable;
}

ATLightPenNoiseMode ATLightPenPort::GetNoiseMode() const {
	return mNoiseMode;
}

void ATLightPenPort::SetNoiseMode(ATLightPenNoiseMode mode) {
	mNoiseMode = mode;
}

void ATLightPenPort::ApplyCorrection(int dx, int dy) {
	auto& adjust = mAdjust[mbLastWasPen];

	adjust.x += dx;
	adjust.y += dy;

	if (adjust.x < -114)
		adjust.x += 228 * ((adjust.x - 57) / 228);
	else
		adjust.x -= 228 * ((adjust.x + 57) / 228);
}

void ATLightPenPort::TriggerCorrection(bool phase, int x, int y) {
	mTriggerCorrectionEvent.InvokeAll(phase, x, y);
}

void ATLightPenPort::SetIgnorePort34(bool ignore) {
	mTriggerStateMask = ignore ? 0x03 : 0x0F;
}

void ATLightPenPort::SetPosition(bool pen, int x, int y) {
	mbLastWasPen = pen;

	if (mbImmediateUpdate) {
		SetAnticPenPosition(x, y);
	}

	mbOddPhase = (x & 1) != 0;
	mPenBeamX = x;
	mPenBeamY = y;
}

void ATLightPenPort::SetPortTriggerState(int index, bool state) {
	uint8 newState = mTriggerState;

	if (state)
		newState |= 1 << index;
	else
		newState &= ~(1 << index);

	if (newState == mTriggerState)
		return;

	mTriggerState = newState;

	if ((newState & mTriggerStateMask) && mpAntic && !mbImmediateUpdate) {
		int beamX = mpAntic->GetBeamX() * 2 + kHorizOffset + (mbOddPhase ? 1 : 0);
		int beamY = mpAntic->GetBeamY();

		// if the current timing is close enough, correct to the pen position
		if (abs(beamY - mPenBeamY) <= 2) {
			beamX = mPenBeamX;
			beamY = mPenBeamY;
		}

		SetAnticPenPosition(beamX, beamY);
	}
}

void ATLightPenPort::SetAnticPenPosition(int x, int y) {
	int yn = (int)mpAntic->GetScanlineCount();

	int xc = x;
	int yc = y;

	yc += xc / 228;
	xc %= 228;

	if (xc < 0) {
		xc += 228;
		--yc;
	}

	yc %= yn;

	if (yc < 0)
		yc += yn;

	mpAntic->SetLightPenPosition(xc, yc);
}
