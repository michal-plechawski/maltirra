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

#ifndef f_AT_LIGHTPEN_H
#define f_AT_LIGHTPEN_H
#include <vd2/system/vdtypes.h>
#include <vd2/system/vectors.h>
#include <at/atcore/enumparse.h>
#include <at/atcore/notifylist.h>
class ATAnticEmulator;
class ATGTIAEmulator;

enum class ATLightPenNoiseMode : uint8 {
	None,
	Low,
	High
};

AT_DECLARE_ENUM_TABLE(ATLightPenNoiseMode);

class ATLightPenPort {
public:
	static const int kHorizOffset;

	ATLightPenPort();

	void Init(ATAnticEmulator& antic, ATGTIAEmulator& gtia);

	ATGTIAEmulator& GetGTIA() const { return *mpGTIA; }

	void SetAdjust(bool pen, vdint2 offset) {
		mAdjust[pen] = offset;
	}

	vdint2 GetAdjust(bool pen) const { return mAdjust[pen]; }

	bool GetImmediateUpdateEnabled() const;
	void SetImmediateUpdateEnabled(bool enable);

	ATLightPenNoiseMode GetNoiseMode() const;
	void SetNoiseMode(ATLightPenNoiseMode mode);

	void ApplyCorrection(int dx, int dy);
	void TriggerCorrection(bool phase, int x, int y);

	void SetIgnorePort34(bool ignore);
	void SetPosition(bool pen, int x, int y);
	void SetPortTriggerState(int index, bool state);

	ATNotifyList<const vdfunction<void(bool, int, int)> *>& OnTriggerCorrectionEvent() {
		return mTriggerCorrectionEvent;
	}

protected:
	void SetAnticPenPosition(int x, int y);

	ATAnticEmulator *mpAntic = nullptr;
	ATGTIAEmulator *mpGTIA = nullptr;
	uint8 mTriggerState = 0;
	uint8 mTriggerStateMask = 0x03;
	vdint2 mAdjust[2] {};
	bool mbLastWasPen = false;
	bool mbOddPhase = false;
	bool mbImmediateUpdate = false;
	int mPenBeamX = 0;
	int mPenBeamY = 0;
	ATLightPenNoiseMode mNoiseMode = ATLightPenNoiseMode::None;

	ATNotifyList<const vdfunction<void(bool, int, int)> *> mTriggerCorrectionEvent;
};

#endif
