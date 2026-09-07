//	VirtualDub - Video processing and capture application
//	System library component
//	Copyright (C) 1998-2004 Avery Lee, All Rights Reserved.
//
//	Beginning with 1.6.0, the VirtualDub system library is licensed
//	differently than the remainder of VirtualDub.  This particular file is
//	thus licensed as follows (the "zlib" license):
//
//	This software is provided 'as-is', without any express or implied
//	warranty.  In no event will the authors be held liable for any
//	damages arising from the use of this software.
//
//	Permission is granted to anyone to use this software for any purpose,
//	including commercial applications, and to alter it and redistribute it
//	freely, subject to the following restrictions:
//
//	1.	The origin of this software must not be misrepresented; you must
//		not claim that you wrote the original software. If you use this
//		software in a product, an acknowledgment in the product
//		documentation would be appreciated but is not required.
//	2.	Altered source versions must be plainly marked as such, and must
//		not be misrepresented as being the original software.
//	3.	This notice may not be removed or altered from any source
//		distribution.

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <new>
#include <utility>

#include <mach/mach.h>
#include <mach/mach_vm.h>
#include <unistd.h>

#include <vd2/system/memory.h>

namespace {
	bool IsPowerOfTwo(size_t value) {
		return value && !(value & (value - 1));
	}

	bool VDRegionHasProtection(const void *p0, size_t bytes, vm_prot_t requiredProtection) {
		if (!bytes)
			return true;

		if (!p0)
			return false;

		const uintptr startAddress = (uintptr)p0;
		if (bytes - 1 > std::numeric_limits<uintptr>::max() - startAddress)
			return false;

		const uintptr lastAddress = startAddress + bytes - 1;
		uintptr currentAddress = startAddress;

		for(;;) {
			mach_vm_address_t regionAddress = (mach_vm_address_t)currentAddress;
			mach_vm_size_t regionSize = 0;
			vm_region_basic_info_data_64_t info {};
			mach_msg_type_number_t infoCount = VM_REGION_BASIC_INFO_COUNT_64;
			mach_port_t objectName = MACH_PORT_NULL;

			const kern_return_t result = mach_vm_region(
				mach_task_self(),
				&regionAddress,
				&regionSize,
				VM_REGION_BASIC_INFO_64,
				reinterpret_cast<vm_region_info_t>(&info),
				&infoCount,
				&objectName);

			if (objectName != MACH_PORT_NULL)
				mach_port_deallocate(mach_task_self(), objectName);

			if (result != KERN_SUCCESS
				|| regionAddress > currentAddress
				|| (info.protection & requiredProtection) != requiredProtection)
				return false;

			if (regionSize > std::numeric_limits<mach_vm_address_t>::max() - regionAddress)
				return false;

			const mach_vm_address_t regionLimit = regionAddress + regionSize;
			if (regionLimit <= currentAddress)
				return false;

			if (regionLimit - 1 >= lastAddress)
				return true;

			currentAddress = (uintptr)regionLimit;
		}
	}
}

void *VDAlignedMallocThrow(size_t n, unsigned alignment) {
	void *p = VDAlignedMalloc(n, alignment);

	if (!p)
		throw std::bad_alloc();

	return p;
}

void *VDAlignedMalloc(size_t n, unsigned alignment) vdnoexcept {
	if (!IsPowerOfTwo(alignment))
		return nullptr;

	const size_t nativeAlignment = std::max<size_t>(alignment, sizeof(void *));
	void *p = nullptr;

	if (posix_memalign(&p, nativeAlignment, n))
		return nullptr;

	return p;
}

void VDAlignedFree(void *p) vdnoexcept {
	free(p);
}

void *VDAlignedVirtualAlloc(size_t n) {
	if (!n)
		return nullptr;

	const long pageSize = sysconf(_SC_PAGESIZE);
	if (pageSize <= 0)
		return nullptr;

	return VDAlignedMalloc(n, (unsigned)pageSize);
}

void VDAlignedVirtualFree(void *p) {
	VDAlignedFree(p);
}

void __cdecl VDSwapMemoryScalar(void *p0, void *p1, size_t bytes) {
	uint8 *dst0 = static_cast<uint8 *>(p0);
	uint8 *dst1 = static_cast<uint8 *>(p1);

	while(bytes--) {
		const uint8 value = *dst0;
		*dst0++ = *dst1;
		*dst1++ = value;
	}
}

void (__cdecl *VDSwapMemory)(void *p0, void *p1, size_t bytes) = VDSwapMemoryScalar;

void VDInvertMemory(void *p, unsigned bytes) {
	uint8 *dst = static_cast<uint8 *>(p);

	while(bytes--) {
		*dst = (uint8)~*dst;
		++dst;
	}
}

bool VDIsValidReadRegion(const void *p, size_t bytes) {
	return VDRegionHasProtection(p, bytes, VM_PROT_READ);
}

bool VDIsValidWriteRegion(void *p, size_t bytes) {
	return VDRegionHasProtection(p, bytes, VM_PROT_WRITE);
}

bool VDCompareRect(void *dst, ptrdiff_t dstpitch, const void *src, ptrdiff_t srcpitch, size_t w, size_t h) {
	if (!w || !h)
		return false;

	do {
		if (memcmp(dst, src, w))
			return true;

		dst = static_cast<char *>(dst) + dstpitch;
		src = static_cast<const char *>(src) + srcpitch;
	} while(--h);

	return false;
}

const void *VDMemCheck8(const void *src, uint8 value, size_t count) {
	const uint8 *src8 = static_cast<const uint8 *>(src);

	while(count--) {
		if (*src8 != value)
			return src8;

		++src8;
	}

	return nullptr;
}

void VDMemset8(void *dst, uint8 value, size_t count) {
	if (count)
		memset(dst, value, count);
}

void VDMemset16(void *dst, uint16 value, size_t count) {
	uint8 *dst8 = static_cast<uint8 *>(dst);

	while(count--) {
		memcpy(dst8, &value, sizeof value);
		dst8 += sizeof value;
	}
}

void VDMemset24(void *dst, uint32 value, size_t count) {
	uint8 *dst8 = static_cast<uint8 *>(dst);
	const uint8 pattern[] = {
		(uint8)value,
		(uint8)(value >> 8),
		(uint8)(value >> 16),
	};

	while(count--) {
		memcpy(dst8, pattern, sizeof pattern);
		dst8 += sizeof pattern;
	}
}

void VDMemset32(void *dst, uint32 value, size_t count) {
	uint8 *dst8 = static_cast<uint8 *>(dst);

	while(count--) {
		memcpy(dst8, &value, sizeof value);
		dst8 += sizeof value;
	}
}

void VDMemset64(void *dst, uint64 value, size_t count) {
	uint8 *dst8 = static_cast<uint8 *>(dst);

	while(count--) {
		memcpy(dst8, &value, sizeof value);
		dst8 += sizeof value;
	}
}

void VDMemset128(void *dst, const void *src, size_t count) {
	if (!count)
		return;

	uint8 pattern[16];
	memcpy(pattern, src, sizeof pattern);

	uint8 *dst8 = static_cast<uint8 *>(dst);
	while(count--) {
		memcpy(dst8, pattern, sizeof pattern);
		dst8 += sizeof pattern;
	}
}

void VDMemsetPointer(void *dst, const void *value, size_t count) {
	uint8 *dst8 = static_cast<uint8 *>(dst);

	while(count--) {
		memcpy(dst8, &value, sizeof value);
		dst8 += sizeof value;
	}
}

void VDMemset8Rect(void *dst, ptrdiff_t pitch, uint8 value, size_t w, size_t h) {
	while(w && h--) {
		VDMemset8(dst, value, w);
		dst = static_cast<char *>(dst) + pitch;
	}
}

void VDMemset16Rect(void *dst, ptrdiff_t pitch, uint16 value, size_t w, size_t h) {
	while(w && h--) {
		VDMemset16(dst, value, w);
		dst = static_cast<char *>(dst) + pitch;
	}
}

void VDMemset24Rect(void *dst, ptrdiff_t pitch, uint32 value, size_t w, size_t h) {
	while(w && h--) {
		VDMemset24(dst, value, w);
		dst = static_cast<char *>(dst) + pitch;
	}
}

void VDMemset32Rect(void *dst, ptrdiff_t pitch, uint32 value, size_t w, size_t h) {
	while(w && h--) {
		VDMemset32(dst, value, w);
		dst = static_cast<char *>(dst) + pitch;
	}
}

void VDFastMemcpyPartial(void *dst, const void *src, size_t bytes) {
	memcpy(dst, src, bytes);
}

void VDFastMemcpyFinish() {
}

void VDFastMemcpyAutodetect() {
}

void VDMemcpyRect(void *dst, ptrdiff_t dststride, const void *src, ptrdiff_t srcstride, size_t w, size_t h) {
	if (!w || !h)
		return;

	if (w == (size_t)srcstride && w == (size_t)dststride) {
		VDFastMemcpyPartial(dst, src, w * h);
	} else {
		char *dst8 = static_cast<char *>(dst);
		const char *src8 = static_cast<const char *>(src);

		do {
			VDFastMemcpyPartial(dst8, src8, w);
			dst8 += dststride;
			src8 += srcstride;
		} while(--h);
	}

	VDFastMemcpyFinish();
}

bool VDMemcpyGuarded(void *dst, const void *src, size_t bytes) {
	if (!bytes)
		return true;

	if (!VDIsValidReadRegion(src, bytes) || !VDIsValidWriteRegion(dst, bytes))
		return false;

	memcpy(dst, src, bytes);
	return true;
}
