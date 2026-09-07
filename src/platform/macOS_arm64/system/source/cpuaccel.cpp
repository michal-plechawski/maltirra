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

#include <vd2/system/cpuaccel.h>

long g_lCPUExtensionsEnabled = 0;

long CPUCheckForExtensions() {
	long flags = 0;

#if defined(__ARM_FEATURE_CRYPTO) || defined(__ARM_FEATURE_AES) || defined(__ARM_FEATURE_SHA2)
	flags |= VDCPUF_SUPPORTS_CRYPTO;
#endif

#if defined(__ARM_FEATURE_CRC32)
	flags |= VDCPUF_SUPPORTS_CRC32;
#endif

	return flags;
}

long CPUEnableExtensions(long enableFlags) {
	g_lCPUExtensionsEnabled = enableFlags;
	return g_lCPUExtensionsEnabled;
}

void VDCPUCleanupExtensions() {
}
