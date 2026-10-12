//	Altirra - Atari 800/800XL emulator
//	Copyright (C) 2008-2010 Avery Lee
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

#ifndef f_AT_VERIFIER_H
#define f_AT_VERIFIER_H

#include "simeventmanager.h"
#include "cpumode.h"

class ATCPUEmulator;
class ATCPUEmulatorMemory;
class ATSimulator;

enum {
	kATVerifierFlag_UndocumentedKernelEntry = 0x01,
	kATVerifierFlag_RecursiveNMI = 0x02,
	kATVerifierFlag_InterruptRegs = 0x04,
	kATVerifierFlag_64KWrap = 0x08,
	kATVerifierFlag_AbnormalDMA = 0x10,
	kATVerifierFlag_AddressZero = 0x20,
	kATVerifierFlag_LoadingOverDisplayList = 0x40,
	kATVerifierFlag_CallingConventionViolations = 0x80,
	kATVerifierFlag_NonCanonicalHardwareAddress = 0x100,
	kATVerifierFlag_StackWrap = 0x200,
	kATVerifierFlag_StackInZP816 = 0x400,
};

// Scalar observations of the running CPU, not an instruction executor.
struct ATCPUVerifierCPUState {
	uint8 mA = 0;
	uint8 mX = 0;
	uint8 mY = 0;
	uint8 mP = 0;
	uint8 mS = 0;
	uint8 mK = 0;
	uint16 mS16 = 0;
	uint16 mPC = 0;
	uint16 mInsnPC = 0;
	bool mbEmulation = true;
	ATCPUMode mMode = kATCPUMode_6502;
};

// Bind these to actual machine observations. CPU reads and machine queries
// stay live across synchronous simulator callbacks; do not cache a whole
// instruction's observations before calling VerifyInsn(). Supply the services
// needed by enabled checks. The observer owns their captured machine objects.
struct ATCPUVerifierConnections {
	vdfunction<ATCPUVerifierCPUState()> mpReadCPU;
	vdfunction<bool(uint16)> mpIsKernelROM;
	vdfunction<uint8(uint16)> mpDebugReadByte;
	vdfunction<uint16(uint16)> mpDebugReadWord;
	vdfunction<uint16()> mpGetDisplayListPtr;
	vdfunction<bool()> mpIsDisplayListEnabled;
};

class ATCPUVerifier {
	ATCPUVerifier(const ATCPUVerifier&) = delete;
	ATCPUVerifier& operator=(const ATCPUVerifier&) = delete;
public:
	ATCPUVerifier();
	~ATCPUVerifier();

	void Init(ATCPUVerifierConnections connections, ATSimulatorEventManager *simevmgr);
	void Init(ATCPUEmulator *cpu, ATCPUEmulatorMemory *mem, ATSimulator *sim, ATSimulatorEventManager *simevmgr);
	void Shutdown();

	uint32 GetFlags() const { return mFlags; }
	void SetFlags(uint32 flags);

	void AddAllowedTarget(uint32 addr);
	void RemoveAllowedTarget(uint32 addr);
	void RemoveAllowedTargets();
	void ResetAllowedTargets();
	void GetAllowedTargets(vdfastvector<uint16>& exceptions);

	void OnReset();
	void OnIRQEntry();
	void OnNMIEntry();
	void VerifyInsn(uint8 opcode, uint16 target);
	void VerifyInsn(const ATCPUEmulator& cpu, uint8 opcode, uint16 target);

private:
	void OnAbnormalDMA();

	void FailStackOverflow();
	void FailStackUnderflow();

	ATCPUVerifierConnections mConnections;
	ATSimulatorEventManager *mpSimEventMgr = nullptr;
	uint32 mEventCallbackId = 0;

	uint32	mFlags = 0;

	bool	mbInNMIRoutine = false;
	uint8	mNMIStackLevel = 0;

	typedef vdfastvector<uint16> Addresses;
	Addresses	mAllowedTargets;

	struct StackRegState {
		uint8	mA;
		uint8	mX;
		uint8	mY;
		uint8	mbActive;
		uint16	mPC;
		uint16	mPad2;
	};

	StackRegState mStackRegState[256] = {};
};

#endif	// f_AT_VERIFIER_H
