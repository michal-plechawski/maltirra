// Exercise the real CIO hook and execute the tape shim on the production CPU.
#include <array>
#include <memory>
#include <vd2/system/error.h>
#include <at/attest/portabletest.h>
#include <at/atcore/cio.h>
#include <at/atcore/ksyms.h>
#include <at/atio/blobimage.h>
#include <hlebasicloader.h>
#include "mmuportablefixture.h"

namespace {
	struct Machine {
		Machine() { h.mHooks.EnableOSHooks(true); h.mMMU.SetBankRegister(0xFF); loader.Init(&h.mCPU); }
		void Word(uint16 a, uint16 v) { h.mMemory.WriteByte(a, uint8(v)); h.mMemory.WriteByte(uint16(a+1), uint8(v>>8)); }
		uint16 Word(uint16 a) { return h.mMemory.ReadByte(a) + 256*h.mMemory.ReadByte(uint16(a+1)); }
		vdrefptr<IATBlobImage> Image(size_t n = 19) {
			std::vector<uint8> data(n); for (size_t i=0; i<n; ++i) data[i] = uint8(i+17);
			vdrefptr<IATBlobImage> image; ATCreateBlobImage(kATImageType_BasicProgram, data.data(), uint32(n), ~image); return image;
		}
		uint8 CIO(uint8 iocb, uint8 cmd, uint16 addr = 0x5000, uint16 len = 32, uint8 mode = 4) {
			h.mCPU.SetX(iocb); h.mMemory.WriteByte(ATKernelSymbols::ICCMD+iocb, cmd);
			Word(ATKernelSymbols::ICBAL+iocb, addr); Word(ATKernelSymbols::ICBLL+iocb, len);
			h.mMemory.WriteByte(ATKernelSymbols::ICAX1+iocb, mode);
			return h.mHooks.OnHookHit(ATKernelSymbols::CIOV);
		}
		MMUPortableFixture h;
		ATHLEBasicLoader loader;
	};
}

bool ATTestAltirraBasicLoaderCommand(ATPortableTestContext& context) {
	auto f = std::make_unique<Machine>(); auto& h = f->h; auto image = f->Image(); f->loader.LoadProgram(image);
	AT_PORTABLE_TEST_ASSERT(context, f->loader.IsLaunchPending());
	AT_PORTABLE_TEST_ASSERT(context, f->CIO(1,kATCIOCmd_GetRecord) == 0 && f->loader.IsLaunchPending());
	AT_PORTABLE_TEST_ASSERT(context, f->CIO(0x10,kATCIOCmd_GetRecord) == 0 && f->loader.IsLaunchPending());
	AT_PORTABLE_TEST_ASSERT(context, f->CIO(0,kATCIOCmd_GetChars) == 0 && f->loader.IsLaunchPending());
	h.mMMU.SetBankRegister(0xFE);
	AT_PORTABLE_TEST_ASSERT(context, f->CIO(0,kATCIOCmd_GetRecord) == 0 && f->loader.IsLaunchPending());
	h.mMMU.SetBankRegister(0xFF);
	AT_PORTABLE_TEST_ASSERT(context, f->CIO(0,kATCIOCmd_GetRecord) == 0x60 && !f->loader.IsLaunchPending());
	const std::array<uint8,8> line {'R','U','N',' ','"','*','"',0x9B};
	for (size_t i=0; i<line.size(); ++i) AT_PORTABLE_TEST_ASSERT(context, h.mMemory.ReadByte(0x5000+i) == line[i]);
	AT_PORTABLE_TEST_ASSERT(context, f->Word(ATKernelSymbols::ICBLL) == 8 && h.mCPU.GetY() == 1 && h.mMemory.ReadByte(ATKernelSymbols::STATUS) == 1);
	AT_PORTABLE_TEST_ASSERT(context, f->CIO(0,kATCIOCmd_GetRecord) == 0);
	f->loader.LoadProgram(image);
	AT_PORTABLE_TEST_ASSERT(context, f->CIO(0,kATCIOCmd_GetRecord,0xFFFF,3) == 0x60);
	AT_PORTABLE_TEST_ASSERT(context, h.mMemory.ReadByte(0) == 'U' && h.mMemory.ReadByte(1) == 'N' && f->Word(ATKernelSymbols::ICBLL) == 3 && h.mCPU.GetY() == kATCIOStat_TruncRecord);
	return true;
}

bool ATTestAltirraBasicLoaderIO(ATPortableTestContext& context) {
	for (uint8 iocb : {uint8(0),uint8(0x10),uint8(0x70)}) {
		auto f = std::make_unique<Machine>(); auto& h = f->h; auto image = f->Image(); f->loader.LoadProgram(image);
		h.mMemory.WriteByte(0x5000,'X'); AT_PORTABLE_TEST_ASSERT(context, f->CIO(iocb,kATCIOCmd_Open) == 0);
		h.mMemory.WriteByte(0x5000,'*');
		AT_PORTABLE_TEST_ASSERT(context, f->CIO(iocb,kATCIOCmd_Open,0x5000,0,8) == 0x60 && h.mCPU.GetY() == kATCIOStat_ReadOnly);
		AT_PORTABLE_TEST_ASSERT(context, f->CIO(iocb,kATCIOCmd_Open) == 0x60 && h.mCPU.GetY() == 1);
		AT_PORTABLE_TEST_ASSERT(context, f->CIO(iocb,kATCIOCmd_Open) == 0x60 && h.mCPU.GetY() == kATCIOStat_IOCBInUse);
		AT_PORTABLE_TEST_ASSERT(context, f->CIO(iocb,kATCIOCmd_PutChars) == 0x60 && h.mCPU.GetY() == kATCIOStat_ReadOnly);
		AT_PORTABLE_TEST_ASSERT(context, f->CIO(iocb,0xFE) == 0x60 && h.mCPU.GetY() == kATCIOStat_NotSupported);
		AT_PORTABLE_TEST_ASSERT(context, f->CIO(iocb,kATCIOCmd_GetChars,0x6000,0) == 0x60 && h.mCPU.GetY() == 1);
		AT_PORTABLE_TEST_ASSERT(context, f->CIO(iocb,kATCIOCmd_GetChars,0x6000,7) == 0x60 && h.mCPU.GetY() == 1);
		AT_PORTABLE_TEST_ASSERT(context, f->CIO(iocb,kATCIOCmd_GetChars,0x6007,100) == 0x60 && h.mCPU.GetY() == kATCIOStat_TruncRecord && f->Word(ATKernelSymbols::ICBLL+iocb) == 12);
		for (uint16 i=0; i<19; ++i) AT_PORTABLE_TEST_ASSERT(context, h.mMemory.ReadByte(0x6000+i) == i+17);
		AT_PORTABLE_TEST_ASSERT(context, f->CIO(iocb,kATCIOCmd_GetChars) == 0x60 && h.mCPU.GetY() == kATCIOStat_EndOfFile && f->Word(ATKernelSymbols::ICBLL+iocb) == 0);
		AT_PORTABLE_TEST_ASSERT(context, f->CIO(iocb,kATCIOCmd_Close) == 0x60 && h.mCPU.GetY() == 1 && h.mMemory.ReadByte(ATKernelSymbols::ICHID+iocb) == 0xFF);
		AT_PORTABLE_TEST_ASSERT(context, h.mHooks.OnHookHit(ATKernelSymbols::CIOV) == 0);
	}
	return true;
}

bool ATTestAltirraBasicLoaderTape(ATPortableTestContext& context) {
	for (bool push : {false,true}) {
		auto f = std::make_unique<Machine>(); auto& h = f->h;
		const uint16 entry = ATKernelSymbols::HATABS+3*11;
		h.mMemory.WriteByte(entry,'C'); f->Word(entry+1,0x1800); f->Word(0x1800,0x2FFF);
		for (uint16 i=2; i<12; ++i) h.mMemory.WriteByte(0x1800+i,uint8(0x80+i));
		h.mMemory.WriteByte(ATKernelSymbols::CH,0xFF);
		f->loader.LoadTape(push); AT_PORTABLE_TEST_ASSERT(context, !f->loader.IsLaunchPending());
		AT_PORTABLE_TEST_ASSERT(context, f->CIO(0,kATCIOCmd_GetRecord) == 0x60 && f->Word(entry+1) == 0x400);
		AT_PORTABLE_TEST_ASSERT(context, h.mMemory.ReadByte(0x5005) == 'C' && h.mMemory.ReadByte(ATKernelSymbols::CH) == (push ? 0x21 : 0xFF));
		for (uint16 i=2; i<12; ++i) AT_PORTABLE_TEST_ASSERT(context, h.mMemory.ReadByte(0x400+i) == 0x80+i);
		AT_PORTABLE_TEST_ASSERT(context, f->Word(0x400) == 0x40B && f->Word(0x41B) == 0x3000);
		// The actual 6502 must set short IRG, restore HATABS, and tail-call OPEN.
		h.mMemory.WriteByte(0x3000,0x60); h.mMemory.WriteByte(0x2100,0x4C); f->Word(0x2101,0x2100);
		h.mCPU.SetS(0xFD); h.mCPU.PushWord(0x20FF); h.mCPU.SetPC(0x40C);
		AT_PORTABLE_TEST_ASSERT(context, h.RunCycles(150));
		AT_PORTABLE_TEST_ASSERT(context, h.mCPU.GetPC() >= 0x2100 && h.mCPU.GetPC() <= 0x2103 && f->Word(entry+1) == 0x1800 && h.mMemory.ReadByte(ATKernelSymbols::ICAX2Z) == 0x80);
		AT_PORTABLE_TEST_ASSERT(context, h.mHooks.OnHookHit(ATKernelSymbols::CIOV) == 0);
	}
	return true;
}

bool ATTestAltirraBasicLoaderLifecycle(ATPortableTestContext& context) {
	auto f = std::make_unique<Machine>(); auto& h = f->h; auto image = f->Image(); auto *raw = image.get();
	f->loader.LoadProgram(image); image = nullptr; f->loader.LoadProgram(raw);
	auto shortImage = f->Image(13); bool threw = false;
	try { f->loader.LoadProgram(shortImage); } catch (const MyError&) { threw = true; }
	AT_PORTABLE_TEST_ASSERT(context, threw && f->loader.IsLaunchPending());
	h.mMemory.WriteByte(0x5000,'*'); AT_PORTABLE_TEST_ASSERT(context, f->CIO(0x70,kATCIOCmd_Open) == 0x60);
	AT_PORTABLE_TEST_ASSERT(context, f->CIO(0x70,kATCIOCmd_GetChars,0x6000,5) == 0x60);
	f->loader.LoadProgram(raw); // resets an open IOCB and partial read
	AT_PORTABLE_TEST_ASSERT(context, f->CIO(0x70,kATCIOCmd_GetChars) == 0);
	AT_PORTABLE_TEST_ASSERT(context, f->CIO(0x70,kATCIOCmd_Open) == 0x60 && f->CIO(0x70,kATCIOCmd_GetChars,0x6000,1) == 0x60 && h.mMemory.ReadByte(0x6000) == 17);
	f->loader.LoadTape(false);
	AT_PORTABLE_TEST_ASSERT(context, f->CIO(0x70,kATCIOCmd_GetChars) == 0 && f->CIO(0,kATCIOCmd_GetRecord) == 0x60);
	f->loader.Shutdown(); f->loader.Shutdown(); f->loader.Init(&h.mCPU);
	AT_PORTABLE_TEST_ASSERT(context, !f->loader.IsLaunchPending() && f->CIO(0,kATCIOCmd_GetRecord) == 0);
	return true;
}
