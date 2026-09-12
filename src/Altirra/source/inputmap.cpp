//	Altirra - Atari 800/800XL/5200 emulator
//	Copyright (C) 2024 Avery Lee
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
//	You should have received a copy of the GNU General Public License along
//	with this program. If not, see <http://www.gnu.org/licenses/>.

#include <algorithm>
#include <climits>
#include <cstdint>
#include <vd2/system/registry.h>
#include "inputmap.h"

namespace {
	void ATEncodeInputMapName(vdfastvector<uint16>& units, const VDStringW& name) {
		for(size_t i = 0; i < name.size(); ++i) {
			const uint32 c = (uint32)name[i];

			if (c >= 0xD800 && c <= 0xDBFF) {
				if (sizeof(wchar_t) == 2 && i + 1 < name.size()) {
					const uint32 next = (uint32)name[i + 1];
					if (next >= 0xDC00 && next <= 0xDFFF) {
						units.push_back((uint16)c);
						units.push_back((uint16)next);
						++i;
						continue;
					}
				}

				units.push_back(0xFFFD);
			} else if (c >= 0xDC00 && c <= 0xDFFF) {
				units.push_back(0xFFFD);
			} else if (c <= 0xFFFF) {
				units.push_back((uint16)c);
			} else if (c <= 0x10FFFF) {
				const uint32 v = c - 0x10000;
				units.push_back((uint16)(0xD800 + (v >> 10)));
				units.push_back((uint16)(0xDC00 + (v & 0x3FF)));
			} else {
				units.push_back(0xFFFD);
			}
		}
	}

	bool ATDecodeInputMapName(VDStringW& name, const uint32 *src, uint32 len) {
		for(uint32 i = 0; i < len; ++i) {
			const uint16 c = (uint16)(src[i >> 1] >> ((i & 1) * 16));

			if (c >= 0xD800 && c <= 0xDBFF) {
				if (++i >= len)
					return false;

				const uint16 next = (uint16)(src[i >> 1] >> ((i & 1) * 16));
				if (next < 0xDC00 || next > 0xDFFF)
					return false;

				if (sizeof(wchar_t) == 2) {
					name.push_back((wchar_t)c);
					name.push_back((wchar_t)next);
				} else {
					name.push_back((wchar_t)(0x10000 + ((uint32)(c - 0xD800) << 10) + (next - 0xDC00)));
				}
			} else if (c >= 0xDC00 && c <= 0xDFFF) {
				return false;
			} else {
				name.push_back((wchar_t)c);
			}
		}

		return true;
	}
}

ATInputMap::ATInputMap()
	: mSpecificInputUnit(-1)
	, mbQuickMap(false)
{
}

ATInputMap::~ATInputMap() {
}

const wchar_t *ATInputMap::GetName() const {
	return mName.c_str();
}

void ATInputMap::SetName(const wchar_t *name) {
	mName = name;
}

bool ATInputMap::UsesPhysicalPort(int portIdx) const {
	for(Controllers::const_iterator it(mControllers.begin()), itEnd(mControllers.end()); it != itEnd; ++it) {
		const Controller& c = *it;

		switch(c.mType) {
			case kATInputControllerType_Joystick:
			case kATInputControllerType_STMouse:
			case kATInputControllerType_5200Controller:
			case kATInputControllerType_LightPen:
			case kATInputControllerType_Tablet:
			case kATInputControllerType_KoalaPad:
			case kATInputControllerType_AmigaMouse:
			case kATInputControllerType_Keypad:
			case kATInputControllerType_Trackball_CX80:
			case kATInputControllerType_5200Trackball:
			case kATInputControllerType_Driving:
			case kATInputControllerType_Keyboard:
			case kATInputControllerType_LightGun:
			case kATInputControllerType_PowerPad:
			case kATInputControllerType_LightPenStack:
				if (c.mIndex == portIdx)
					return true;
				break;

			case kATInputControllerType_Paddle:
				if ((c.mIndex >> 1) == portIdx)
					return true;
				break;
		}
	}

	return false;
}

void ATInputMap::Clear() {
	mControllers.clear();
	mMappings.clear();
	mSpecificInputUnit = -1;
}

uint32 ATInputMap::GetControllerCount() const {
	return (uint32)mControllers.size();
}

bool ATInputMap::HasControllerType(ATInputControllerType type) const {
	return std::find_if(mControllers.begin(), mControllers.end(),
		[=](const Controller& c) { return c.mType == type; }) != mControllers.end();
}

const ATInputMap::Controller& ATInputMap::GetController(uint32 i) const {
	return mControllers[i];
}

uint32 ATInputMap::AddController(ATInputControllerType type, uint32 index) {
	uint32 cindex = (uint32)mControllers.size();
	Controller& c = mControllers.push_back();

	c.mType = type;
	c.mIndex = index;

	return cindex;
}

void ATInputMap::AddControllers(std::initializer_list<Controller> controllers) {
	mControllers.insert(mControllers.end(), controllers.begin(), controllers.end());
}

uint32 ATInputMap::GetMappingCount() const {
	return (uint32)mMappings.size();
}

const ATInputMap::Mapping& ATInputMap::GetMapping(uint32 i) const {
	return mMappings[i];
}

void ATInputMap::AddMapping(uint32 inputCode, uint32 controllerId, uint32 code) {
	Mapping& m = mMappings.push_back();

	m.mInputCode = inputCode;
	m.mControllerId = controllerId;
	m.mCode = code;
}

void ATInputMap::AddMappings(std::initializer_list<Mapping> mappings) {
	mMappings.insert(mMappings.end(), mappings.begin(), mappings.end());
}

bool ATInputMap::Load(VDRegistryKey& key, const char *name) {
	int len = key.getBinaryLength(name);

	if (len < 16 || (len & 3))
		return false;

	vdfastvector<uint32> heap;
	const uint32 heapWords = (uint32)len >> 2;
	heap.resize(heapWords, 0);

	if (!key.getBinary(name, (char *)heap.data(), len))
		return false;

	const uint32 version = heap[0];
	uint32 headerWords = 4;
	if (version == 2) {
		headerWords = 5;
		if (heapWords < headerWords)
			return false;
	} else if (version != 1) {
		return false;
	}

	const uint32 nameLen = heap[1];
	const uint32 ctrlCount = heap[2];
	const uint32 mapCount = heap[3];

	if ((nameLen | ctrlCount | mapCount) & 0xff000000)
		return false;
	const uint32 nameWords = (nameLen + 1) >> 1;
	const uint64 requiredWords = (uint64)headerWords + nameWords + 2ull * ctrlCount + 3ull * mapCount;
	if (requiredWords > heapWords)
		return false;

	const uint32 *src = heap.data() + headerWords;
	VDStringW decodedName;
	if (!ATDecodeInputMapName(decodedName, src, nameLen))
		return false;

	src += nameWords;

	Controllers controllers;
	controllers.resize(ctrlCount);
	for(uint32 i=0; i<ctrlCount; ++i) {
		Controller& c = controllers[i];

		c.mType = (ATInputControllerType)src[0];
		c.mIndex = src[1];
		src += 2;
	}

	Mappings mappings;
	mappings.resize(mapCount);
	for(uint32 i=0; i<mapCount; ++i) {
		Mapping& m = mappings[i];

		m.mInputCode = src[0];
		m.mControllerId = src[1];
		m.mCode = src[2];
		src += 3;
	}

	mName = decodedName;
	mControllers.swap(controllers);
	mMappings.swap(mappings);
	mSpecificInputUnit = headerWords >= 5 ? (sint32)heap[4] : -1;
	return true;
}

void ATInputMap::Save(VDRegistryKey& key, const char *name) {
	vdfastvector<uint16> nameUnits;
	ATEncodeInputMapName(nameUnits, mName);
	if (nameUnits.size() > 0xFFFFFF || mControllers.size() > 0xFFFFFF || mMappings.size() > 0xFFFFFF)
		return;

	vdfastvector<uint32> heap;

	heap.push_back(2);
	heap.push_back((uint32)nameUnits.size());
	heap.push_back((uint32)mControllers.size());
	heap.push_back((uint32)mMappings.size());
	heap.push_back(mSpecificInputUnit);

	const size_t offset = heap.size();
	heap.resize(heap.size() + ((nameUnits.size() + 1) >> 1), 0);

	for(size_t i = 0; i < nameUnits.size(); ++i)
		heap[offset + (i >> 1)] |= (uint32)nameUnits[i] << ((i & 1) * 16);

	for(Controllers::const_iterator it(mControllers.begin()), itEnd(mControllers.end()); it != itEnd; ++it) {
		const Controller& c = *it;

		heap.push_back(c.mType);
		heap.push_back(c.mIndex);
	}

	for(Mappings::const_iterator it(mMappings.begin()), itEnd(mMappings.end()); it != itEnd; ++it) {
		const Mapping& m = *it;

		heap.push_back(m.mInputCode);
		heap.push_back(m.mControllerId);
		heap.push_back(m.mCode);
	}

	if (heap.size() <= INT_MAX / 4)
		key.setBinary(name, (const char *)heap.data(), (int)(heap.size() * 4));
}
