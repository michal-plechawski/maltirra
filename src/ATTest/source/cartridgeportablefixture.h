// Real cartridge, memory bus, cartridge port, scheduler and image storage.
#ifndef f_ATTEST_CARTRIDGEPORTABLEFIXTURE_H
#define f_ATTEST_CARTRIDGEPORTABLEFIXTURE_H
#include <algorithm>
#include <array>
#include <memory>
#include <string>
#include <vd2/system/vdtypes.h>
#include <vd2/system/file.h>
#include <vd2/system/zip.h>
#include <at/attest/portabletest.h>
#include <at/atcore/deviceindicators.h>
#include <at/atcore/savestate.h>
#include <at/atcore/snapshot.h>
#include <cartridge.h>
#include <cartridgeport.h>
#include <memorymanager.h>
#include <savestateio.h>

struct CartridgeIndicatorProbe final : IATDeviceIndicatorManager {
	void SetStatusFlags(uint32) override {}
	void ResetStatusFlags(uint32, uint32) override {}
	void PulseStatusFlags(uint32) override {}
	void SetStatusCounter(uint32, uint32) override {}
	void SetDiskLEDState(uint32, sint32) override {}
	void SetDiskMotorActivity(uint32, bool) override {}
	void SetDiskErrorState(uint32, bool) override {}
	void SetHActivity(bool) override {}
	void SetIDEActivity(bool, uint32) override {}
	void SetPCLinkActivity(bool) override {}
	void SetFlashWriteActivity() override { ++mWrites; }
	void SetCartridgeActivity(sint32, sint32) override {}
	void SetCassetteIndicatorVisible(bool) override {}
	void SetCassettePosition(float, float, bool, bool) override {}
	void SetRecordingPosition() override {}
	void SetRecordingPositionPaused() override {}
	void SetRecordingPosition(float, sint64, bool) override {}
	void SetModemConnection(const char *) override {}
	void SetStatusMessage(const wchar_t *) override {}
	uint32 AllocateErrorSourceId() override { return 1; }
	void ClearErrors(uint32) override {}
	void ReportError(uint32, const wchar_t *) override {}
	uint32 mWrites = 0;
};

struct CartridgePortableFixture {
	CartridgePortableFixture() : mCart(kATMemoryPri_Cartridge1, kATCartridgePriority_Default, false) {
		mScheduler.SetRate(VDFraction(17897725, 10));
		mMemory.Init(); mRAM.fill(0xCC);
		mpRAM = mMemory.CreateLayer(kATMemoryPri_BaseRAM, mRAM.data(), 0, 0x100, false);
		mMemory.EnableLayer(mpRAM, true);
		mCart.InitCartridge(&mPort); mCart.Init(mMemory, mScheduler, &mIndicators);
	}
	~CartridgePortableFixture() { mCart.Shutdown(); mMemory.DeleteLayer(mpRAM); }
	void Load(ATCartridgeMode mode, bool pattern = true) {
		vdrefptr<IATCartridgeImage> image; ATCreateCartridgeImage(mode, ~image);
		auto *p = static_cast<uint8 *>(image->GetBuffer());
		for (uint32 i = 0; i < image->GetImageSize(); ++i) p[i] = pattern ? static_cast<uint8>(i >> 12) : 0xFF;
		image->SetClean(); mCart.Load(image);
	}
	uint8 Read(uint32 a) { return mMemory.ReadByte(a); }
	uint8 Debug(uint32 a) { return mMemory.DebugReadByte(a); }
	void Write(uint32 a, uint8 v) { mMemory.WriteByte(a, v); }
	void Tick(uint32 n) { while (n--) { ATSCHEDULER_ADVANCE(&mScheduler); } }
	// Address a byte of a Maxflash chip through its actual 8KB bank latch.
	void FlashWrite(uint32 offset, uint8 value) { Write(0xD500 + (offset >> 13), 0); Write(0xA000 + (offset & 0x1FFF), value); }
	void FlashUnlock(uint8 command) { FlashWrite(0x5555, 0xAA); FlashWrite(0x2AAA, 0x55); FlashWrite(0x5555, command); }
	std::array<uint8, 65536> mRAM;
	ATScheduler mScheduler;
	ATMemoryManager mMemory;
	ATCartridgePort mPort;
	CartridgeIndicatorProbe mIndicators;
	ATCartridgeEmulator mCart;
	ATMemoryLayer *mpRAM = nullptr;
};

inline std::string CartridgeStateJSON(IATObjectState& state) {
	VDMemoryBufferStream stream; std::unique_ptr<IATSaveStateSerializer> w(ATCreateSaveStateSerializer());
	w->Serialize(stream, state, L"Cartridge portable state"); const auto b = stream.GetBuffer();
	return std::string(reinterpret_cast<const char *>(b.data()), b.size());
}
inline vdrefptr<IATObjectState> CartridgeStateDecode(const std::string& json) {
	VDMemoryStream stream(json.data(), static_cast<uint32>(json.size()));
	std::unique_ptr<IATSaveStateDeserializer> r(ATCreateSaveStateDeserializer());
	vdrefptr<IATSerializable> state; r->Deserialize(stream, ~state);
	return vdrefptr<IATObjectState>(static_cast<IATObjectState *>(state.get()));
}
inline vdrefptr<IATObjectState> CartridgeStateRoundTrip(IATObjectState& state) {
	VDMemoryBufferStream stream;
	std::unique_ptr<IVDZipArchiveWriter> zip(VDCreateZipArchiveWriter(stream));
	std::unique_ptr<IATSaveStateSerializer> w(ATCreateSaveStateSerializer(L"state.json"));
	w->Serialize(*zip, state); zip->Finalize(); zip.reset(); stream.Seek(0);
	VDZipArchive archive; archive.Init(&stream);
	std::unique_ptr<IATSaveStateDeserializer> r(ATCreateSaveStateDeserializer(L"state.json"));
	vdrefptr<IATSerializable> decoded; r->Deserialize(archive, ~decoded);
	return vdrefptr<IATObjectState>(static_cast<IATObjectState *>(decoded.get()));
}
#endif
