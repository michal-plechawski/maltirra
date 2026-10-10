//	Altirra - Atari 800/800XL/5200 emulator
//	Copyright (C) 2026 Avery Lee
//
//	This program is free software; you can redistribute it and/or modify
//	it under the terms of the GNU General Public License as published by
//	the Free Software Foundation; either version 2 of the License, or
//	(at your option) any later version.
//
//	This program is distributed in the hope that it will be useful,
//	but WITHOUT ANY WARRANTY; without even the implied warranty of
//	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
//	GNU General Public License for more details.
//
//	You should have received a copy of the GNU General Public License along
//	with this program. If not, see <http://www.gnu.org/licenses/>.

#ifndef f_VD2_VDDISPLAY_INTERNAL_PRESENTATIONBUFFER_H
#define f_VD2_VDDISPLAY_INTERNAL_PRESENTATIONBUFFER_H

#include <vd2/Kasumi/pixmaputils.h>

class VDDisplayPresentationBuffer {
public:
	// Creates an owned XRGB8888 snapshot. Conversion from any other source
	// format is rejected unless allowConversion is true.
	bool Update(const VDPixmap& source, bool allowConversion);
	void Clear();

	const VDPixmap& GetPixmap() const { return mBuffer; }

private:
	VDPixmapBuffer mBuffer;
	VDPixmapBuffer mStagingBuffer;
};

#endif
