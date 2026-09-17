//	Altirra - Atari 800/800XL/5200 emulator
//	Copyright (C) 2009-2026 Avery Lee
//
//	This program is free software; you can redistribute it and/or modify
//	it under the terms of the GNU General Public License as published by
//	the Free Software Foundation; either version 2 of the License, or
//	(at your option) any later version.

#include "oshelper.h"

#include <vd2/system/vdstring.h>
#include <at/atcore/enumparseimpl.h>

AT_DEFINE_ENUM_TABLE_BEGIN(ATProcessEfficiencyMode)
	{ ATProcessEfficiencyMode::Default, "default" },
	{ ATProcessEfficiencyMode::Performance, "performance" },
	{ ATProcessEfficiencyMode::Efficiency, "efficiency" },
AT_DEFINE_ENUM_TABLE_END(ATProcessEfficiencyMode, ATProcessEfficiencyMode::Default)

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
