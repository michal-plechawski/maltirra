//	Altirra - Atari 800/800XL/5200 emulator
//	Copyright (C) 2009-2026 Avery Lee
//
//	This program is free software; you can redistribute it and/or modify
//	it under the terms of the GNU General Public License as published by
//	the Free Software Foundation; either version 2 of the License, or
//	(at your option) any later version.

#include "oshelper.h"

#include <vd2/system/binary.h>
#include <vd2/system/filesys.h>
#include <vd2/system/vdstring.h>
#include <at/atcore/enumparseimpl.h>

AT_DEFINE_ENUM_TABLE_BEGIN(ATProcessEfficiencyMode)
	{ ATProcessEfficiencyMode::Default, "default" },
	{ ATProcessEfficiencyMode::Performance, "performance" },
	{ ATProcessEfficiencyMode::Efficiency, "efficiency" },
AT_DEFINE_ENUM_TABLE_END(ATProcessEfficiencyMode, ATProcessEfficiencyMode::Default)

void ATFileSetReadOnlyAttribute(const wchar_t *path, bool readOnly) {
	VDFileSetAttributes(
		path,
		kVDFileAttr_ReadOnly,
		readOnly ? kVDFileAttr_ReadOnly : 0);
}

bool ATDecodeLZPackedResource(const void *srcData, size_t srcSize, vdfastvector<uint8>& data) {
	static constexpr uint32 kMaxDecodedSize = 256U * 1024U * 1024U;

	data.clear();
	if (!srcData || srcSize < 4)
		return false;

	const uint8 *const src = static_cast<const uint8 *>(srcData);
	const uint32 decodedSize = VDReadUnalignedLEU32(src);
	if (decodedSize > kMaxDecodedSize)
		return false;

	data.resize(decodedSize);
	size_t srcOffset = 4;
	size_t dstOffset = 0;

	for(;;) {
		if (srcOffset >= srcSize) {
			data.clear();
			return false;
		}

		const uint8 control = src[srcOffset++];
		if (!control) {
			if (dstOffset == decodedSize)
				return true;

			data.clear();
			return false;
		}

		if (control & 1) {
			if (srcOffset >= srcSize) {
				data.clear();
				return false;
			}

			size_t distance = static_cast<size_t>(src[srcOffset++]) + 1;
			size_t length;

			if (control & 2) {
				if (srcOffset >= srcSize) {
					data.clear();
					return false;
				}

				distance += static_cast<size_t>(control & 0xFC) << 6;
				length = static_cast<size_t>(src[srcOffset++]) + 3;
			} else {
				distance += static_cast<size_t>(control & 0x1C) << 6;
				length = static_cast<size_t>(control >> 5) + 3;
			}

			if (distance > dstOffset || length > decodedSize - dstOffset) {
				data.clear();
				return false;
			}

			for(size_t i = 0; i < length; ++i) {
				data[dstOffset] = data[dstOffset - distance];
				++dstOffset;
			}
		} else {
			const size_t length = control >> 1;
			if (length > srcSize - srcOffset || length > decodedSize - dstOffset) {
				data.clear();
				return false;
			}

			for(size_t i = 0; i < length; ++i)
				data[dstOffset++] = src[srcOffset++];
		}
	}
}

VDStringW ATBuildEscapedCommandLine(vdspan<const wchar_t *> args) {
	VDStringW commandLine;

	for(const wchar_t *arg : args) {
		if (!commandLine.empty())
			commandLine += L' ';

		if (!arg)
			arg = L"";

		bool needsQuotes = !*arg;

		for(const wchar_t *s = arg; *s && !needsQuotes; ++s) {
			switch(*s) {
				case L' ':
				case L'\t':
				case L'\n':
				case L'\v':
				case L'\f':
				case L'\r':
				case L'"':
					needsQuotes = true;
					break;
			}
		}

		if (!needsQuotes) {
			commandLine += arg;
			continue;
		}

		commandLine += L'"';
		uint32 pendingBackslashes = 0;

		for(const wchar_t *s = arg;; ++s) {
			if (*s == L'\\') {
				++pendingBackslashes;
				continue;
			}

			if (*s == L'"') {
				for(uint32 i = 0; i < pendingBackslashes * 2 + 1; ++i)
					commandLine += L'\\';
			} else {
				const uint32 count = *s ? pendingBackslashes : pendingBackslashes * 2;

				for(uint32 i = 0; i < count; ++i)
					commandLine += L'\\';
			}

			pendingBackslashes = 0;

			if (!*s)
				break;

			commandLine += *s;
		}

		commandLine += L'"';
	}

	return commandLine;
}
