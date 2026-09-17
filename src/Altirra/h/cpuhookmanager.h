//	Altirra - Atari 800/800XL/5200 emulator
//	Copyright (C) 2009-2012 Avery Lee
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

//=========================================================================
// CPU Hook manager
//
// CPU hooks are used to intercept events triggered by execution of code
// at a specific address. They are typically used to accelerate specific
// OS functions. A hook is called when the CPU is about to execute an
// instruction at the requested address and has the option of overriding
// the instruction; a value of 0 continues execution, while any other
// value executes that instruction. This is typically 0x60 (RTS) or
// 0x4C (JMP); in the latter case, the CPU core is expected to already have
// had SetPC() called.
//
// Because OS hooks may be sensitive to the specific OS, init and reset
// hooks are provided. Init hooks are called when the OS is starting up,
// while reset hooks are called when the OS is reset. The init hook is
// always called after the reset hook, but there may be some time in
// between; this particularly occurs when no standard OS is running, such
// as the Ultimate1MB BIOS, and prevents OS hooks interfering with the
// boot firmware. However, all hooks are also disabled in this case, so
// it isn't necessary to manually add or remove hooks at known addresses.
//
// CPU hooks are never called when a 65C816 is in native mode or executing
// above bank 0.
//

#ifndef f_AT_CPUHOOKMANAGER_H
#define f_AT_CPUHOOKMANAGER_H

#include <vd2/system/vdstl.h>
#include <vd2/system/linearalloc.h>
#include <vd2/system/function.h>

struct ATCPUHookNode {};
struct ATCPUHookInitNode {};
struct ATCPUHookResetNode {};

typedef vdfunction<uint8(uint16 pc)> ATCPUHookFn;
typedef vdfunction<void()> ATCPUHookResetFn;
typedef vdfunction<void(const uint8 *lowerKernelROM, const uint8 *upperKernelROM)> ATCPUHookInitFn;

enum ATCPUHookMode {
	kATCPUHookMode_Always,
	kATCPUHookMode_KernelROMOnly,
	kATCPUHookMode_MathPackROMOnly
};

class ATCPUHookManager {
	ATCPUHookManager(const ATCPUHookManager&) = delete;
	ATCPUHookManager& operator=(const ATCPUHookManager&) = delete;

	struct HashNode : public ATCPUHookNode {
		HashNode *mpNext;
		uint16 mPC;
		uint8 mMode;
		sint8 mPriority;

		ATCPUHookFn mpHookFn;
	};

	struct InitNode : public ATCPUHookInitNode {
		InitNode *mpNext;
		ATCPUHookInitFn mpHookFn;
	};

	struct ResetNode : public ATCPUHookResetNode {
		ResetNode *mpNext;
		ATCPUHookResetFn mpHookFn;
	};

public:
	ATCPUHookManager();
	~ATCPUHookManager();

	template<typename TCPU, typename TMMU, typename TPBI>
	void Init(TCPU *cpu, TMMU *mmu, TPBI *pbi) {
		mpCPU = cpu;
		mpSetCPUHook = [](void *p, uint16 pc, bool enable) {
			static_cast<TCPU *>(p)->SetHook(pc, enable);
		};

		mpMMU = mmu;
		mpIsKernelROMEnabled = [](const void *p) {
			return static_cast<const TMMU *>(p)->IsKernelROMEnabled();
		};

		mpPBI = pbi;
		mpIsPBIROMOverlayActive = [](const void *p) {
			return static_cast<const TPBI *>(p)->IsROMOverlayActive();
		};
	}
	void Shutdown();

	void EnableOSHooks(bool enabled) { mbOSHooksEnabled = enabled; }

	uint8 OnHookHit(uint16 pc) const;

	// The init hooks are called when it is first safe to register OS hooks,
	// i.e. after the OS has been set up. The routine receives a pointer to
	// the upper kernel ROM ($D800-FFFF).
	void CallInitHooks(const uint8 *lowerKernelROM, const uint8 *upperKernelROM);
	ATCPUHookInitNode *AddInitHook(const ATCPUHookInitFn& fn);
	void RemoveInitHook(ATCPUHookInitNode *hook);

	void CallResetHooks();
	ATCPUHookResetNode *AddResetHook(ATCPUHookResetFn fn);
	void RemoveResetHook(ATCPUHookResetNode *hook);

	template<class T, typename Method>
	ATCPUHookNode *AddHookMethod(ATCPUHookMode mode, uint16 pc, sint8 priority, T *thisPtr, Method method);

	ATCPUHookNode *AddHook(ATCPUHookMode mode, uint16 pc, sint8 priority, const ATCPUHookFn& fn);
	void RemoveHook(ATCPUHookNode *hook);

	template<class T, typename Method>
	void SetHookMethod(ATCPUHookNode *& hook, ATCPUHookMode mode, uint16 pc, sint8 priority, T *thisPtr, Method method) {
		UnsetHook(hook);
		hook = AddHookMethod(mode, pc, priority, thisPtr, method);
	}

	void SetHook(ATCPUHookNode *& hook, ATCPUHookMode mode, uint16 pc, sint8 priority, const ATCPUHookFn& fn) {
		UnsetHook(hook);
		hook = AddHook(mode, pc, priority, fn);
	}

	void UnsetHook(ATCPUHookNode *& hook) {
		if (hook) {
			RemoveHook(hook);
			hook = nullptr;
		}
	}

private:
	void *mpCPU = nullptr;
	void (*mpSetCPUHook)(void *, uint16, bool) = nullptr;
	const void *mpMMU = nullptr;
	bool (*mpIsKernelROMEnabled)(const void *) = nullptr;
	const void *mpPBI = nullptr;
	bool (*mpIsPBIROMOverlayActive)(const void *) = nullptr;
	bool mbOSHooksEnabled = false;

	HashNode *mpFreeList = nullptr;
	InitNode *mpInitChain = nullptr;
	InitNode *mpInitFreeList = nullptr;
	ResetNode *mpResetChain = nullptr;
	ResetNode *mpResetFreeList = nullptr;

	HashNode *mpHashTable[256] {};

	VDLinearAllocator mAllocator;
};

///////////////////////////////////////////////////////////////////////////
template<class T, typename Method>
ATCPUHookNode *ATCPUHookManager::AddHookMethod(ATCPUHookMode mode, uint16 pc, sint8 priority, T *thisPtr, Method method) {
	return AddHook(mode, pc, priority, [=](uint16 pc) { return (thisPtr->*method)(pc); });
}

#endif	// f_AT_CPUHOOKMANAGER_H
