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

#include <vd2/Kasumi/pixmapops.h>
#include <vd2/VDDisplay/internal/presentationbuffer.h>

namespace {
	bool VDDisplayHasSufficientPitch(ptrdiff_t pitch, uint64 rowBytes, uint64 rows) {
		if (rows <= 1)
			return true;

		const uint64 pitchMagnitude = pitch >= 0
			? (uint64)pitch
			: (uint64)(-(pitch + 1)) + 1;
		return pitchMagnitude >= rowBytes;
	}
}

bool VDDisplayPresentationBuffer::Update(const VDPixmap& source, bool allowConversion) {
	using namespace nsVDPixmap;

	if (!source.data || source.w <= 0 || source.h <= 0
		|| (uint32)source.format >= kPixFormat_Max_Standard
		|| source.format == kPixFormat_Null)
		return false;

	const VDPixmapFormatInfo& formatInfo = VDPixmapGetInfo(source.format);
	if ((formatInfo.auxbufs >= 1 && !source.data2)
		|| (formatInfo.auxbufs >= 2 && !source.data3)
		|| (formatInfo.palsize && !source.palette))
		return false;

	const uint64 mainWidth = ((uint64)source.w + formatInfo.qw - 1) / formatInfo.qw;
	const uint64 mainRows = ((uint64)source.h + formatInfo.qh - 1) / formatInfo.qh;
	if (!VDDisplayHasSufficientPitch(
		source.pitch, mainWidth * formatInfo.qsize, mainRows))
		return false;

	if (formatInfo.auxbufs) {
		const uint64 auxWidth = ((uint64)source.w + (UINT64_C(1) << formatInfo.auxwbits) - 1)
			>> formatInfo.auxwbits;
		const uint64 auxRows = ((uint64)source.h + (UINT64_C(1) << formatInfo.auxhbits) - 1)
			>> formatInfo.auxhbits;
		const uint64 auxRowBytes = auxWidth * formatInfo.auxsize;

		if (!VDDisplayHasSufficientPitch(source.pitch2, auxRowBytes, auxRows)
			|| (formatInfo.auxbufs >= 2
				&& !VDDisplayHasSufficientPitch(source.pitch3, auxRowBytes, auxRows)))
			return false;
	}

	if (source.format != kPixFormat_XRGB8888 && !allowConversion)
		return false;

	if (!VDPixmapIsBltPossible(kPixFormat_XRGB8888, source.format))
		return false;

	const uint64 targetPitch = (((uint64)source.w * sizeof(uint32)) + 15) & ~UINT64_C(15);
	// VDPixmapBuffer stores its allocation size through a 32-bit intermediate
	// and debug builds add guard bytes around it.
	if (targetPitch * (uint64)source.h > UINT32_MAX - 28)
		return false;

	mStagingBuffer.init(source.w, source.h, kPixFormat_XRGB8888);
	if (!VDPixmapBlt(mStagingBuffer, source))
		return false;

	mBuffer.swap(mStagingBuffer);
	return true;
}

void VDDisplayPresentationBuffer::Clear() {
	mBuffer.clear();
	static_cast<VDPixmap&>(mBuffer) = {};
	mStagingBuffer.clear();
	static_cast<VDPixmap&>(mStagingBuffer) = {};
}
