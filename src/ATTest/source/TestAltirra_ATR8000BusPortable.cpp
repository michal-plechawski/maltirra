#include "atr8000portablefixture.h"
#include <cstdio>
using namespace ATR8000Portable;
namespace {
	void Print(Machine& f,uint8 value) { f.Out(0x20,value); f.Out(0x53,0); f.Out(0x53,1); }
	std::wstring Printed(Machine& f) { auto& p=f.services.printers.GetOutput(0); return std::wstring(p.GetTextPointer(0),p.GetLength()); }
	void SerialFrame(Machine& f,uint8 value) { f.Out(0x51,0); for(uint32 i=0;i<8;++i) { f.Run(1391); f.Out(0x51,uint8((value>>i)&1)); } f.Run(1391); f.Out(0x51,1); f.Run(1600); }
}
bool ATTestAltirraATR8000CTCIRQ(ATPortableTestContext& context) {
	for(bool cascade:{false,true}) { auto f=std::make_unique<Machine>(); Code c; c.Emit({0x31,0,0xFF}); c.Write(0x8000,0); c.Emit({0x3E,0x90,0xED,0x47,0xED,0x5E}); c.Out(0x80,0xC0);
		if(cascade) { c.Out(0x83,0xD7); c.Out(0x83,2); c.Out(0x82,7); c.Out(0x82,4); } else { c.Out(0x57,0); c.Out(0x80,0xD7); c.Out(0x80,1); }
		c.Emit({0xFB}); const uint16 loop=uint16(c.bytes.size()); c.Emit({0x76,0xC3,uint8(loop),uint8(loop>>8)}); f->MakeROM(c.bytes); f->Handler(0x100,{0x3A,0,0x80,0x3C,0x32,0,0x80,0xFB,0xED,0x4D}); const uint8 vector[]={0,1}; f->device->WriteMemory(cascade ? 0x90C6 : 0x90C0,vector,2); f->Run(1000);
		if(!cascade) { AT_PORTABLE_TEST_ASSERT(context, f->device->ReadByte(0x8000)==0); f->host->mMachine.mChips.mPOKEY.SetCommandLine(true); f->Run(1000); AT_PORTABLE_TEST_ASSERT(context, f->device->ReadByte(0x8000)==1); } else AT_PORTABLE_TEST_ASSERT(context, f->device->ReadByte(0x8000)>0 && f->device->ReadByte(0x8000)!=0xFF);
	} return true;
}
bool ATTestAltirraATR8000HALTNMI(ATPortableTestContext& context) {
	auto f=std::make_unique<Machine>(); Bytes data(128); for(uint32 i=0;i<128;++i) data[i]=uint8(i*13+7); f->Fill(21,data); Code c; c.Emit({0x31,0,0xFF,0x21,0,0x80,0x06,0x80}); c.Out(0x30,0xC1); c.Out(0x41,1); c.Out(0x42,4); c.Out(0x40,0x80); c.Emit({0x76,0x78,0xB7,0x20,0xFB}); c.Halt(); f->MakeROM(c.bytes); f->Handler(0x66,{0xDB,0x40,0xE6,2,0x28,5,0xDB,0x43,0x77,0x23,0x05,0xED,0x45}); f->Run(100000); Bytes actual(128); f->device->ReadMemory(0x8000,actual.data(),128); for(auto& b:data) b=~b; AT_PORTABLE_TEST_ASSERT(context, actual==data && f->State().mZ80.mB==0 && f->State().mZ80.mSP==0xFF00 && !(f->FDC().DebugReadByte(0)&0x1D)); return true;
}
bool ATTestAltirraATR8000SIOReceive(ATPortableTestContext& context) {
	auto f=std::make_unique<Machine>(); f->host->mManager.PokeyWriteSIO(0xA6,false,1000,0,false,false); AT_PORTABLE_TEST_ASSERT(context, !(f->In(0x70)&0x80)); for(uint32 i=0;i<8;++i) { f->Run(900); AT_PORTABLE_TEST_ASSERT(context, bool(f->In(0x70)&0x80)==bool((0xA6>>i)&1)); } f->Run(900); AT_PORTABLE_TEST_ASSERT(context, f->In(0x70)&0x80); f->device->OnReceiveByte(0,false,0); f->Run(2000); AT_PORTABLE_TEST_ASSERT(context, f->In(0x70)&0x80); return true;
}
bool ATTestAltirraATR8000SIOTransmit(ATPortableTestContext& context) {
	auto f=std::make_unique<Machine>(); f->host->ReceiveClock(); Code c; for(uint32 i=0;i<10;++i) { const uint8 bit=i==9 ? 1 : i==0 ? 0 : uint8((0xA6>>(i-1))&1); c.Out(0x50,bit); c.bytes.insert(c.bytes.end(),47,0); } c.Halt(); f->Program(c.bytes); AT_PORTABLE_TEST_ASSERT(context, f->host->Collect(4000)==Bytes{0xA6}); return true;
}
bool ATTestAltirraATR8000CommandReset(ATPortableTestContext& context) {
	Config c; c.command=true; auto f=std::make_unique<Machine>(c); AT_PORTABLE_TEST_ASSERT(context, !(f->In(0x70)&2)); f->host->mManager.PokeyWriteSIO(0,false,1000,0,false,false); f->Run(100); f->device->WarmReset(); AT_PORTABLE_TEST_ASSERT(context, f->In(0x70)&0x80); f->device->PeripheralColdReset(); AT_PORTABLE_TEST_ASSERT(context, !(f->In(0x70)&2) && (f->In(0x70)&0x80)); return true;
}
bool ATTestAltirraATR8000Audio(ATPortableTestContext& context) {
	auto f=std::make_unique<Machine>(); static_cast<IATDeviceAudioOutput *>(f->device->AsInterface(IATDeviceAudioOutput::kTypeID))->InitAudioOutput(&f->mixer); f->Slot().mDisk.SetDriveSoundsEnabled(true); f->Select(1); f->FDC().WriteByte(0,0x80); f->Run(100); AT_PORTABLE_TEST_ASSERT(context, f->Slot().mbMotor && DriveBasePortable::Audible(f->mixer.Render())); f->Slot(1).mDisk.SetDriveSoundsEnabled(false); f->Select(2); AT_PORTABLE_TEST_ASSERT(context, !f->Slot().mbMotor && f->Slot(1).mbMotor && !DriveBasePortable::Audible(f->mixer.Render())); f->Select(3); AT_PORTABLE_TEST_ASSERT(context, DriveBasePortable::Audible(f->mixer.Render())); return true;
}
bool ATTestAltirraATR8000SerialStatus(ATPortableTestContext& context) {
	auto f=std::make_unique<Machine>(); AT_PORTABLE_TEST_ASSERT(context, f->In(0x50)&0x80); vdrefptr<Serial> serial(new Serial); f->device->GetDeviceBus(0)->AddChildDevice(serial); AT_PORTABLE_TEST_ASSERT(context, !serial->controls.empty() && !serial->controls.back().mbRequestToSend); serial->status.mbClearToSend=true; serial->status.mbRinging=true; AT_PORTABLE_TEST_ASSERT(context, !(f->In(0x50)&6)); f->Out(0x55,0); AT_PORTABLE_TEST_ASSERT(context, serial->controls.back().mbRequestToSend); f->Out(0x55,1); ATPropertySet p; f->device->GetSettings(p); p.SetString("signal1",L"dtr"); p.SetString("signal2",L"dsr"); AT_PORTABLE_TEST_ASSERT(context, f->device->SetSettings(p)); AT_PORTABLE_TEST_ASSERT(context, !serial->controls.back().mbDataTerminalReady && serial->controls.back().mbRequestToSend && (f->In(0x50)&2)); serial->status.mbDataSetReady=true; AT_PORTABLE_TEST_ASSERT(context, !(f->In(0x50)&2)); return true;
}
bool ATTestAltirraATR8000SerialTransmit(ATPortableTestContext& context) {
	auto f=std::make_unique<Machine>(); vdrefptr<Serial> serial(new Serial); f->device->GetDeviceBus(0)->AddChildDevice(serial); f->Out(0x51,0); f->Run(100); f->Out(0x51,1); f->Run(15000); AT_PORTABLE_TEST_ASSERT(context, serial->sent.empty()); SerialFrame(*f,0xA6); AT_PORTABLE_TEST_ASSERT(context, serial->sent==(std::vector<std::pair<uint32,uint8>>{{1200,0xA6}}));
	Code c; for(uint32 i=0;i<10;++i) { c.Out(0x51,i==9 ? 1 : i==0 ? 0 : uint8((0x5B>>(i-1))&1)); c.bytes.insert(c.bytes.end(),47,0); } c.Halt(); f->Program(c.bytes); f->device->OnReceiveByte(0,false,93); f->Run(4000);
	if(serial->sent.size()!=2 || serial->sent.back()!=(std::pair<uint32,uint8>{19245,0x5B})) { for(const auto& [rate,value]:serial->sent) printf("ATR8000 RS232: rate=%u byte=%02X\n",rate,unsigned(value)); }
	AT_PORTABLE_TEST_ASSERT(context, serial->sent.size()==2 && serial->sent.back()==(std::pair<uint32,uint8>{19245,0x5B})); return true;
}
bool ATTestAltirraATR8000SerialReceive(ATPortableTestContext& context) {
	auto f=std::make_unique<Machine>(); vdrefptr<Serial> serial(new Serial); serial->receive.push_back({1200,0xA6}); f->device->GetDeviceBus(0)->AddChildDevice(serial); AT_PORTABLE_TEST_ASSERT(context, !(f->In(0x50)&0x80)); f->Run(745); for(uint32 i=0;i<8;++i) { f->Run(1391); AT_PORTABLE_TEST_ASSERT(context, bool(f->In(0x50)&0x80)==bool((0xA6>>i)&1)); } f->Run(1391); AT_PORTABLE_TEST_ASSERT(context, f->In(0x50)&0x80); return true;
}
bool ATTestAltirraATR8000SerialDetach(ATPortableTestContext& context) {
	auto f=std::make_unique<Machine>(); vdrefptr<Serial> first(new Serial),second(new Serial); auto *bus=f->device->GetDeviceBus(0); first->receive.push_back({1200,0xA6}); bus->AddChildDevice(first); AT_PORTABLE_TEST_ASSERT(context, first->GetParent()==static_cast<IATDeviceParent *>(f->device.get()) && !(f->In(0x50)&0x80)); f->Out(0x51,0); bus->RemoveChildDevice(first); bus->AddChildDevice(second); f->Run(20000); AT_PORTABLE_TEST_ASSERT(context, !first->GetParent() && second->sent.empty() && (f->In(0x50)&0x80)); f->Stop(); AT_PORTABLE_TEST_ASSERT(context, !second->GetParent()); return true;
}
bool ATTestAltirraATR8000Printer(ATPortableTestContext& context) {
	auto f=std::make_unique<Machine>(); AT_PORTABLE_TEST_ASSERT(context, f->services.printers.GetOutputCount()==0 && f->In(0x20)==0x30); Print(*f,'A'); f->Out(0x53,1); AT_PORTABLE_TEST_ASSERT(context, f->services.printers.GetOutputCount()==1 && Printed(*f)==L"A"); f->device->DeviceReset(); AT_PORTABLE_TEST_ASSERT(context, Printed(*f)==L"A"); Print(*f,'B'); AT_PORTABLE_TEST_ASSERT(context, Printed(*f)==L"AB"); return true;
}
bool ATTestAltirraATR8000PrinterChild(ATPortableTestContext& context) {
	auto f=std::make_unique<Machine>(); auto *bus=f->device->GetDeviceBus(1); vdrefptr<PercomPortable::Printer> printer(new PercomPortable::Printer); Print(*f,'A'); AT_PORTABLE_TEST_ASSERT(context, !strcmp(bus->GetBusTag(),"parport") && !strcmp(bus->GetSupportedType(0),"parallel")); bus->AddChildDevice(printer); AT_PORTABLE_TEST_ASSERT(context, printer->GetParentBusIndex()==1 && f->services.printers.GetOutputCount()==0); Print(*f,0xA6); AT_PORTABLE_TEST_ASSERT(context, printer->bytes==Bytes{0xA6}); bus->RemoveChildDevice(printer); Print(*f,'C'); AT_PORTABLE_TEST_ASSERT(context, !printer->GetParent() && Printed(*f)==L"C"); bus->AddChildDevice(printer); f->Stop(); AT_PORTABLE_TEST_ASSERT(context, !printer->GetParent()); return true;
}
bool ATTestAltirraATR8000ResetShutdown(ATPortableTestContext& context) {
	auto f=std::make_unique<Machine>(); f->device->WriteByte(0x8000,0x37); AT_PORTABLE_TEST_ASSERT(context, f->device->GetSupportedButtons()==(1U<<kATDeviceButton_ATR8000Reset) && !f->device->IsButtonDepressed(kATDeviceButton_ATR8000Reset)); f->device->ActivateButton(kATDeviceButton_ATR8000Reset,false); AT_PORTABLE_TEST_ASSERT(context, f->device->ReadByte(0x8000)==0x37); f->device->ActivateButton(kATDeviceButton_ATR8000Reset,true); AT_PORTABLE_TEST_ASSERT(context, f->device->ReadByte(0x8000)==0x37); f->device->PeripheralColdReset(); AT_PORTABLE_TEST_ASSERT(context, f->device->ReadByte(0x8000)==0xFF);
	f->Select(1); f->device->OnDiskChanged(0,true); f->Run(100); f->device->PeripheralColdReset(); f->Select(1); f->Run(100000); AT_PORTABLE_TEST_ASSERT(context, !(f->FDC().DebugReadByte(0)&0x40));
	f->FDC().WriteByte(0,8); AT_PORTABLE_TEST_ASSERT(context, f->WaitIRQ()); f->device->OnReceiveByte(0xA6,false,93); f->device->OnDiskChanged(0,true); f->CTC().WriteByte(2,7); f->CTC().WriteByte(2,4); f->device->SetHistoryEnabled(true); f->Stop(); for(uint32 i=0;i<4;++i) AT_PORTABLE_TEST_ASSERT(context, !f->Slot(i).mbMotor && f->Slot(i).mDisk.GetClientCount()==0); AT_PORTABLE_TEST_ASSERT(context, !f->device->GetHistoryEnabled()); f->device->Shutdown(); f->device=nullptr; f->host->Tick(400000); return true;
}
