//	Altirra - Atari 800/800XL/5200 emulator
//	Copyright (C) 2009-2026 Avery Lee
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

#include <cerrno>

#include <at/atcore/cio.h>
#include "pclinkerror.h"

uint8 ATTranslateHostErrorToSIOError(uint32 errorCode) {
	switch(static_cast<int>(errorCode)) {
		case ENOENT:
			return kATCIOStat_FileNotFound;

		case ENOTDIR:
			return kATCIOStat_PathNotFound;

		case EEXIST:
			return kATCIOStat_FileExists;

		case ENOSPC:
		case EDQUOT:
			return kATCIOStat_DiskFull;

		case ENOTEMPTY:
			return kATCIOStat_DirNotEmpty;

		case EACCES:
		case EPERM:
		case EROFS:
			return kATCIOStat_AccessDenied;

		case EAGAIN:
		case EBUSY:
		case ETXTBSY:
			return kATCIOStat_FileLocked;

		default:
			return kATCIOStat_SystemError;
	}
}
