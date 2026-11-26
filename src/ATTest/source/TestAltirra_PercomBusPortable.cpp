#include "percomportablefixture.h"
using namespace PercomPortable;
namespace {
	void Print(Machine& f,uint8 value) { f.device->WriteByte(9,0); f.device->WriteByte(8,0xFF); f.device->WriteByte(9,4); f.device->WriteByte(8,uint8(~value)); f.device->WriteByte(11,0x34); f.device->WriteByte(11,0x3C); }
	std::wstring Printed(Machine& f) { auto& p=f.services.printers.GetOutput(0); return std::wstring(p.GetTextPointer(0),p.GetLength()); }
}

bool ATTestAltirraPercomSerial(ATPortableTestContext& context) {
	for(auto type:variants) { Config c; c.type=type; auto f=std::make_unique<Machine>(c); f->host->ReceiveClock(); Code code; code.Write(f->ACIA(),0x15); const uint8 loop=uint8(code.bytes.size()); code.Emit({0xB6,uint8(f->ACIA()>>8),uint8(f->ACIA()),0x85,1,0x27,0xF9,0xB6,uint8(f->ACIA()>>8),uint8(f->ACIA()+1),0xB7,uint8(f->ACIA()>>8),uint8(f->ACIA()+1)}); const uint8 branch=uint8(code.bytes.size()); code.Emit({0x20,uint8(loop-branch-2)}); f->Program(code.bytes); f->Run(1000);
		f->host->mManager.PokeyWriteSIO(0xA6,false,93,f->host->mMachine.mChips.mVideo.mScheduler.GetTick64(),false,false); AT_PORTABLE_TEST_ASSERT(context, f->host->Collect(4000)==Bytes{0xA6});
		f->host->mManager.PokeyWriteSIO(0x67,false,200,0,false,false); AT_PORTABLE_TEST_ASSERT(context, f->device->ReadByte(f->ACIA())&0x10);
	} return true;
}

bool ATTestAltirraPercomCTS(ATPortableTestContext& context) {
	for(auto type:variants) { Config c; c.type=type; auto f=std::make_unique<Machine>(c); auto& p=f->host->mMachine.mChips.mPOKEY; p.SetCommandLine(true); f->Run(100); AT_PORTABLE_TEST_ASSERT(context, f->device->ReadByte(f->ACIA())&8); p.SetCommandLine(false); f->Run(200); AT_PORTABLE_TEST_ASSERT(context, f->device->ReadByte(f->ACIA())&8); f->Run(700); AT_PORTABLE_TEST_ASSERT(context, !(f->device->ReadByte(f->ACIA())&8)); p.SetCommandLine(true); f->Run(100); f->device->PeripheralColdReset(); f->Run(100); AT_PORTABLE_TEST_ASSERT(context, f->device->ReadByte(f->ACIA())&8); }
	return true;
}

bool ATTestAltirraPercomFIRQ(ATPortableTestContext& context) {
	for(auto type:variants) { Config c; c.type=type; c.size=256; auto f=std::make_unique<Machine>(c); Bytes payload(256); for(uint32 i=0;i<256;++i) payload[i]=uint8(i*13+7); f->Fill(183,payload); Code code;
		code.Emit({0x10,0xCE,uint8((f->RAM()+0x3F0)>>8),0xF0,0x8E,uint8(f->RAM()>>8),0x80,0xC6,0,0x1C,0xBF}); f->EmitSelect(code,0,true); code.Write(f->FDC()+1,uint8(~10)); code.Write(f->FDC()+2,uint8(~4)); code.Write(f->FDC(),uint8(~0x80)); code.Emit({0x8C,uint8((f->RAM()+0x180)>>8),0x80,0x26,0xFB}); code.Halt();
		f->MakeROM(code.bytes); f->Handler(0xFCA0,{0xB6,uint8(f->FDC()>>8),uint8(f->FDC()+3),0xA7,0x80,0x5C,0x3B},0xFFF6); f->Run(100000); Bytes actual(256); f->device->ReadMemory(f->RAM()+0x80,actual.data(),256);
		AT_PORTABLE_TEST_ASSERT(context, actual==payload && f->State().m6809.mX==f->RAM()+0x180 && f->State().m6809.mS==f->RAM()+0x3F0 && !(f->Status()&0x1D));
	} return true;
}

bool ATTestAltirraPercomIRQ(ATPortableTestContext& context) {
	for(auto type:variants) { Config c; c.type=type; auto f=std::make_unique<Machine>(c); Code code; code.Emit({0x10,0xCE,uint8((f->RAM()+0x3F0)>>8),0xF0,0x7F,uint8(f->RAM()>>8),0x50,0x1C,0xEF}); if(type==Hardware::AT88) code.Write(9,0x1C); code.Halt(); f->MakeROM(code.bytes);
		Bytes handler {0x7C,uint8(f->RAM()>>8),0x50,0xB6,uint8(f->FDC()>>8),uint8(f->FDC())}; if(type==Hardware::AT88) handler.insert(handler.end(),{0xB6,0,8}); handler.push_back(0x3B); f->Handler(0xFCA0,handler,type==Hardware::RFD ? 0xFFFC : 0xFFF8); f->Run(1000); AT_PORTABLE_TEST_ASSERT(context, f->device->ReadByte(f->RAM()+0x50)==0);
		f->WriteFDC(0,0xD8); f->Run(1000); AT_PORTABLE_TEST_ASSERT(context, f->device->ReadByte(f->RAM()+0x50)==1); f->Run(1000); AT_PORTABLE_TEST_ASSERT(context, f->device->ReadByte(f->RAM()+0x50)==1);
	} return true;
}

bool ATTestAltirraPercomWatchdog(ATPortableTestContext& context) {
	auto f=std::make_unique<Machine>(); Code code; code.Emit({0x10,0xCE,0xDF,0xF0,0x7F,0xDC,0x50}); code.Write(0xD018,1); code.Halt(); f->MakeROM(code.bytes); f->Handler(0xFCA0,{0x7C,0xDC,0x50,0x3B},0xFFFC); f->Run(1000);
	f->device->DebugReadByte(f->FDC()); f->Run(1900000); AT_PORTABLE_TEST_ASSERT(context, f->device->ReadByte(0xDC50)==0); f->device->ReadByte(f->FDC()); f->Run(1700000); AT_PORTABLE_TEST_ASSERT(context, f->device->ReadByte(0xDC50)==0); f->Run(200000); AT_PORTABLE_TEST_ASSERT(context, f->device->ReadByte(0xDC50)==1 && f->State().m6809.mS==0xDFF0); return true;
}

bool ATTestAltirraPercomAudio(ATPortableTestContext& context) {
	for(auto type:variants) { Config c; c.type=type; auto f=std::make_unique<Machine>(c); static_cast<IATDeviceAudioOutput *>(f->device->AsInterface(IATDeviceAudioOutput::kTypeID))->InitAudioOutput(&f->mixer); f->Slot().mDisk.SetDriveSoundsEnabled(true); f->Select(0,true); AT_PORTABLE_TEST_ASSERT(context, f->Slot().mbMotor && DriveBasePortable::Audible(f->mixer.Render())); f->Slot(1).mDisk.SetDriveSoundsEnabled(false); f->Select(1,true); AT_PORTABLE_TEST_ASSERT(context, !f->Slot().mbMotor && f->Slot(1).mbMotor && !DriveBasePortable::Audible(f->mixer.Render())); f->Slot(1).mDisk.SetDriveSoundsEnabled(true); AT_PORTABLE_TEST_ASSERT(context, DriveBasePortable::Audible(f->mixer.Render())); }
	return true;
}

bool ATTestAltirraPercomPrinter(ATPortableTestContext& context) {
	Config c; c.type=Hardware::AT88SPD; auto f=std::make_unique<Machine>(c); AT_PORTABLE_TEST_ASSERT(context, f->services.printers.GetOutputCount()==0); Print(*f,'A'); f->device->WriteByte(11,0x3C); AT_PORTABLE_TEST_ASSERT(context, f->services.printers.GetOutputCount()==1 && Printed(*f)==L"A"); Print(*f,'B'); AT_PORTABLE_TEST_ASSERT(context, Printed(*f)==L"AB"); return true;
}

bool ATTestAltirraPercomPrinterChild(ATPortableTestContext& context) {
	Config c; c.type=Hardware::AT88SPD; auto f=std::make_unique<Machine>(c); auto *bus=f->device->GetDeviceBus(0); vdrefptr<Printer> printer(new Printer); Print(*f,'A');
	AT_PORTABLE_TEST_ASSERT(context, !strcmp(bus->GetBusTag(),"parport") && !strcmp(bus->GetSupportedType(0),"parallel") && !bus->GetSupportedType(1) && !f->device->GetDeviceBus(1)); bus->AddChildDevice(printer);
	AT_PORTABLE_TEST_ASSERT(context, printer->GetParent()==static_cast<IATDeviceParent *>(f->device.get()) && f->services.printers.GetOutputCount()==0); Print(*f,0xA6); AT_PORTABLE_TEST_ASSERT(context, printer->bytes==Bytes{0xA6} && f->services.printers.GetOutputCount()==0); bus->RemoveChildDevice(printer); Print(*f,'C'); AT_PORTABLE_TEST_ASSERT(context, !printer->GetParent() && Printed(*f)==L"C"); bus->AddChildDevice(printer); f->Stop(); AT_PORTABLE_TEST_ASSERT(context, !printer->GetParent()); return true;
}

bool ATTestAltirraPercomPrinterReset(ATPortableTestContext& context) {
	Config c; c.type=Hardware::AT88SPD; auto f=std::make_unique<Machine>(c); for(uint32 i=0;i<3;++i) f->device->PeripheralColdReset(); AT_PORTABLE_TEST_ASSERT(context, f->services.printers.GetOutputCount()==0); Print(*f,'A'); f->device->PeripheralColdReset(); AT_PORTABLE_TEST_ASSERT(context, f->services.printers.GetOutputCount()==1 && Printed(*f)==L"A"); Print(*f,'B'); AT_PORTABLE_TEST_ASSERT(context, Printed(*f)==L"AB"); return true;
}

bool ATTestAltirraPercomDualFDC(ATPortableTestContext& context) {
	Config c; c.type=Hardware::AT88; auto f=std::make_unique<Machine>(c); f->Select(0,false); f->WriteFDC(1,0x37); f->Select(0,true); f->WriteFDC(1,0x68); f->Select(0,false); AT_PORTABLE_TEST_ASSERT(context, f->device->DebugReadByte(f->FDC()+1)==uint8(~0x37)); f->Select(0,true); AT_PORTABLE_TEST_ASSERT(context, f->device->DebugReadByte(f->FDC()+1)==uint8(~0x68));
	f->device->WriteByte(9,0x1C); f->WriteFDC(0,0xD8); f->Run(100); AT_PORTABLE_TEST_ASSERT(context, f->device->ReadByte(9)&0x40); f->device->ReadByte(8); f->Select(0,false); f->WriteFDC(0,0xD8); f->Run(100); AT_PORTABLE_TEST_ASSERT(context, f->device->ReadByte(9)&0x40);
	c.size=256; auto routing=std::make_unique<Machine>(c); Code code; code.Emit({0x10,0xCE,0x13,0xF0,0x7F,0x10,0x50}); code.Halt(); routing->MakeROM(code.bytes); routing->Handler(0xFCA0,{0x7C,0x10,0x50,0xB6,0,7,0x3B},0xFFF6); routing->Run(1000);
	routing->Select(0,true); routing->WriteFDC(1,10); routing->WriteFDC(2,4); routing->WriteFDC(0,0x80); routing->Run(5000); AT_PORTABLE_TEST_ASSERT(context, routing->Chip().GetDrqStatus()); routing->Select(0,false);
	auto state=routing->State(); state.m6809.mCC &= ~0x40; routing->device->SetExecState(state); routing->Run(1000); AT_PORTABLE_TEST_ASSERT(context, routing->device->ReadByte(0x1050)==0); routing->Select(0,true); routing->Run(1000); AT_PORTABLE_TEST_ASSERT(context, routing->device->ReadByte(0x1050)>0);
	c.dd=false; auto single=std::make_unique<Machine>(c); single->Select(0,false); single->WriteFDC(1,0x79); single->Select(0,true); AT_PORTABLE_TEST_ASSERT(context, single->device->DebugReadByte(single->FDC()+1)==uint8(~0x79)); return true;
}

bool ATTestAltirraPercomClock(ATPortableTestContext& context) {
	for(bool twice:{false,true}) { auto f=std::make_unique<Machine>(); f->Select(); f->device->WriteByte(0xD018,twice ? 2 : 0); f->WriteFDC(1,10); f->WriteFDC(2,4); f->WriteFDC(0,0x80); std::vector<uint32> ticks;
		for(uint32 i=0;i<40000;++i) { f->Run(1); if(f->Chip().GetDrqStatus()) { ticks.push_back(f->host->CPUGetCycle()); f->device->ReadByte(f->FDC()+3); } if(f->Chip().GetIrqStatus()) break; }
		AT_PORTABLE_TEST_ASSERT(context, ticks.size()==128); const double period=double(ticks.back()-ticks.front())/127; AT_PORTABLE_TEST_ASSERT(context, std::abs(period-(twice ? 32 : 64)*1.7897725)<1);
	} return true;
}

bool ATTestAltirraPercomResetShutdown(ATPortableTestContext& context) {
	for(auto type:variants) { Config c; c.type=type; auto f=std::make_unique<Machine>(c); f->host->ReceiveClock(); f->Select(0); f->device->WriteByte(f->RAM()+0x50,0x37); f->device->WriteByte(f->ACIA(),0x15); f->host->mManager.PokeyWriteSIO(0x67,false,93,0,false,false); f->device->WarmReset(); AT_PORTABLE_TEST_ASSERT(context, f->device->ReadByte(f->RAM()+0x50)==0x37 && f->device->ReadByte(f->ACIA()+1)==0x67); f->device->WriteByte(f->ACIA()+1,0xA6); f->device->PeripheralColdReset();
		AT_PORTABLE_TEST_ASSERT(context, f->device->ReadByte(f->RAM()+0x50)==0xA5 && f->host->Collect(4000).empty()); f->Select(3); f->WriteFDC(0,0x80); if(type==Hardware::RFD) { f->device->WriteByte(0xD018,1); f->device->ReadByte(f->FDC()); } f->device->OnDiskChanged(3,true); f->device->SetHistoryEnabled(true); f->Stop();
		for(uint32 i=0;i<4;++i) AT_PORTABLE_TEST_ASSERT(context, !f->Slot(i).mbMotor && f->Slot(i).mDisk.GetClientCount()==0); AT_PORTABLE_TEST_ASSERT(context, !f->device->GetHistoryEnabled()); f->device->Shutdown(); f->device=nullptr; f->host->Tick(2000000);
	} return true;
}
