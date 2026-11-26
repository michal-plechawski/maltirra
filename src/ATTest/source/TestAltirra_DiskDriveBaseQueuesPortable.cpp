#include "diskdrivebaseportablefixture.h"
using namespace DriveBasePortable;

bool ATTestAltirraDriveBaseBytes(ATPortableTestContext& context) {
	ATScheduler s; SIO host(s); ATDiskDriveSerialByteTransmitQueue q; q.Init(&s,&host); const uint32 t=s.GetTick();
	q.AddTransmitByte(t+10,'A',93); q.AddTransmitByte(t+10,'B',45); q.AddTransmitByte(t+20,'C',93);
	Tick(s,9); AT_PORTABLE_TEST_ASSERT(context, host.bytes.empty()); Tick(s,1);
	AT_PORTABLE_TEST_ASSERT(context, host.bytes.size()==1 && host.bytes[0].value=='B' && host.bytes[0].time==t+10 && host.bytes[0].rate==45 && host.bytes[0].simulate && !host.bytes[0].framing);
	Tick(s,10); AT_PORTABLE_TEST_ASSERT(context, host.bytes.size()==2 && host.bytes[1].value=='C');
	q.AddTransmitByte(s.GetTick()+10,'X',93); q.Reset(); Tick(s,30); AT_PORTABLE_TEST_ASSERT(context, host.bytes.size()==2);
	// Reset may be requested by the host while a queued byte is delivered.
	host.onByte=[&] { q.Reset(); }; q.AddTransmitByte(s.GetTick()+1,'D',93); q.AddTransmitByte(s.GetTick()+2,'E',93);
	Tick(s,10); AT_PORTABLE_TEST_ASSERT(context, host.bytes.size()==3 && host.bytes.back().value=='D'); q.Shutdown(); q.Shutdown();
	return true;
}

bool ATTestAltirraDriveBaseByteWrap(ATPortableTestContext& context) {
	for (uint32 start : {0x7FFFFFF0u,0xFFFFFFF0u}) {
		ATScheduler s; Position(s,start); SIO host(s); ATDiskDriveSerialByteTransmitQueue q; q.Init(&s,&host);
		q.AddTransmitByte(start+10,1,93); q.AddTransmitByte(start+25,2,45); Tick(s,30);
		AT_PORTABLE_TEST_ASSERT(context, host.bytes.size()==2 && host.bytes[0].time==start+10 && host.bytes[1].time==start+25 && host.bytes[1].value==2);
		q.Shutdown();
	}
	return true;
}

bool ATTestAltirraDriveBaseBits(ATPortableTestContext& context) {
	ATScheduler s; SIO host(s); ATDiskDriveSerialBitTransmitQueue q; q.Init(&s,&host);
	for (uint8 value : {uint8(0),uint8(0xFF),uint8(0xA5),uint8(0x5A)}) {
		const uint32 t=s.GetTick()+10; Frame(q,t,value,93); Tick(s,1100);
		AT_PORTABLE_TEST_ASSERT(context, host.bytes.back().value==value && host.bytes.back().rate==93 && !host.bytes.back().framing && !host.bytes.back().simulate);
	}
	AT_PORTABLE_TEST_ASSERT(context, host.bytes.size()==4 && !host.pins.empty() && host.pins.back().second); q.Shutdown();
	return true;
}

bool ATTestAltirraDriveBaseBitFraming(ATPortableTestContext& context) {
	ATScheduler s; SIO host(s); ATDiskDriveSerialBitTransmitQueue q; q.Init(&s,&host);
	Frame(q,s.GetTick()+10,0x35,93,false); Tick(s,1100);
	AT_PORTABLE_TEST_ASSERT(context, host.bytes.size()==1 && host.bytes[0].value==0x35 && host.bytes[0].framing);
	// A pulse shorter than half a bit cannot become a start bit.
	uint32 t=s.GetTick()+10; q.AddTransmitBit(t,false); q.AddTransmitBit(t+10,true); Tick(s,1000);
	AT_PORTABLE_TEST_ASSERT(context, host.bytes.size()==1);
	host.rate=200; Frame(q,s.GetTick()+10,0xA6,200); Tick(s,2200); AT_PORTABLE_TEST_ASSERT(context, host.bytes.size()==1);
	q.SetMaxCyclesPerBit(256); Frame(q,s.GetTick()+10,0xA6,200); Tick(s,2200);
	AT_PORTABLE_TEST_ASSERT(context, host.bytes.size()==2 && host.bytes.back().value==0xA6 && host.bytes.back().rate==200); q.Shutdown();
	return true;
}

bool ATTestAltirraDriveBaseBitReset(ATPortableTestContext& context) {
	{ ATDiskDriveSerialBitTransmitQueue neverInitialized; }
	ATScheduler s; Position(s,0x7FFFFFE0); SIO host(s); ATDiskDriveSerialBitTransmitQueue q; q.Init(&s,&host);
	Frame(q,s.GetTick()+10,0xAC,93); Tick(s,1100);
	AT_PORTABLE_TEST_ASSERT(context, host.bytes.size()==1 && host.bytes.back().value==0xAC);
	Frame(q,s.GetTick()+10,0x00,93); Tick(s,200); q.Reset(); Tick(s,1000);
	AT_PORTABLE_TEST_ASSERT(context, host.bytes.size()==1 && host.pins.back().second);
	Frame(q,s.GetTick()+10,0x00,93); Tick(s,200); ++host.resetCounter; Tick(s,800);
	AT_PORTABLE_TEST_ASSERT(context, host.bytes.size()==1); q.Reset();
	Frame(q,s.GetTick()+10,0xC3,93); Tick(s,1100);
	AT_PORTABLE_TEST_ASSERT(context, host.bytes.size()==2 && host.bytes.back().value==0xC3); q.Shutdown();
	return true;
}

bool ATTestAltirraDriveBaseCommands(ATPortableTestContext& context) {
	ATScheduler s; SIO host(s); ATDiskDriveSerialCommandQueue q; q.Init(&s,&host);
	std::vector<std::pair<uint32,bool>> changes;
	q.SetOnDriveCommandStateChanged([&](bool v) { changes.push_back({s.GetTick(),v}); }); const uint32 t=s.GetTick();
	q.AddCommandEdge(t+10,1); q.AddCommandEdge(t+20,1); q.AddCommandEdge(t+30,0); Tick(s,9);
	AT_PORTABLE_TEST_ASSERT(context, changes.empty()); Tick(s,21);
	AT_PORTABLE_TEST_ASSERT(context, changes==(std::vector<std::pair<uint32,bool>>{{t+10,true},{t+30,false}}) && !q.GetDriveCommandState());
	q.SetOnDriveCommandStateChanged([&](bool v) { changes.push_back({s.GetTick(),v}); q.Reset(); });
	q.AddCommandEdge(s.GetTick()+1,1); q.AddCommandEdge(s.GetTick()+2,0); Tick(s,10);
	AT_PORTABLE_TEST_ASSERT(context, changes.size()==3 && !q.GetDriveCommandState()); q.Shutdown();
	return true;
}

bool ATTestAltirraDriveBaseCommandWrap(ATPortableTestContext& context) {
	for (uint32 t : {0x7FFFFFF0u,0xFFFFFFF0u}) {
		ATScheduler s; Position(s,t); SIO host(s); ATDiskDriveSerialCommandQueue q; q.Init(&s,&host); std::vector<bool> changes;
		q.SetOnDriveCommandStateChanged([&](bool v) { changes.push_back(v); });
		q.AddCommandEdge(t+10,1); q.AddCommandEdge(t+25,0); Tick(s,30);
		AT_PORTABLE_TEST_ASSERT(context, changes==(std::vector<bool>{true,false})); q.Shutdown();
	}
	return true;
}

bool ATTestAltirraDriveBaseCommandSupersede(ATPortableTestContext& context) {
	ATScheduler s; SIO host(s); ATDiskDriveSerialCommandQueue q; q.Init(&s,&host); std::vector<uint32> changes;
	q.SetOnDriveCommandStateChanged([&](bool) { changes.push_back(s.GetTick()); }); const uint32 t=s.GetTick();
	q.AddCommandEdge(t+100,0); q.AddCommandEdge(t+20,1); Tick(s,20);
	AT_PORTABLE_TEST_ASSERT(context, q.GetDriveCommandState() && changes==std::vector<uint32>{t+20}); Tick(s,100);
	AT_PORTABLE_TEST_ASSERT(context, changes.size()==1); q.Shutdown();
	return true;
}
