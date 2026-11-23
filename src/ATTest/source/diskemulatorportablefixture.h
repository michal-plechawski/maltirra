// Production SIO, disk slot/image, drive, clocks and stock-sample player.
#ifndef f_ATTEST_DISKEMULATORPORTABLEFIXTURE_H
#define f_ATTEST_DISKEMULATORPORTABLEFIXTURE_H
#include <array>
#include <memory>
#include <string>
#include <vd2/system/vdtypes.h>
#include <at/ataudio/audiosampleplayer.h>
#include <at/ataudio/audiosamplepool.h>
#include <at/atcore/savestate.h>
#include <at/atcore/snapshot.h>
#include <audiostocksamples.h>
#include <disk.h>
#include <savestateio.h>
#include <trace.h>
#include "siomanagerportablefixture.h"
#include "diskinterfaceportablefixture.h"

class DiskEmulatorPortableFixture {
public:
	DiskEmulatorPortableFixture(bool media = true, uint32 sectorSize = 128, uint32 sectors = 720)
		: mPlayer(mPool, mBus.mMachine.mChips.mVideo.mScheduler) {
		ATAudioRegisterStockSamples(mPool);
		mPlayer.Init(); mPlayer.SetRates(63920.8f, 1, 1.0 / 28.0);
		if (media) mSlot.mDisk.CreateDisk(sectors, sectorSize > 256 ? 0 : 3, sectorSize);
		mDrive.InitSIO(&mBus.mManager);
		mDrive.Init(0, &mSlot.mDisk, &mBus.mMachine.mChips.mVideo.mScheduler, &mBus.mMachine.mSlowScheduler, mPlayer);
		mDrive.SetEnabled(true);
		mBus.mDiskEnabled[0] = true; mBus.mDiskClients[0] = static_cast<uint32>(mSlot.mDisk.GetClientCount());
		mBus.mManager.SetDiskSIOAccelEnabled(true);
	}
	~DiskEmulatorPortableFixture() { mDrive.Shutdown(); mPlayer.Shutdown(); }
	bool Accel(uint8 command, uint8 mode, uint16 length, uint16 sector = 1) {
		auto req = mBus.Request(0x31, mode, length); req.mCommand = command; req.mSector = sector;
		req.mAUX[0] = static_cast<uint8>(sector); req.mAUX[1] = static_cast<uint8>(sector >> 8);
		return mBus.mManager.TryAccelRequest(req, false);
	}
	uint8 Status() { return mBus.Mem(ATKernelSymbols::DSTATS); }
	std::vector<uint8> Buffer(size_t size) {
		std::vector<uint8> b(size); for (size_t i = 0; i < size; ++i) b[i] = mBus.Mem(static_cast<uint16>(0x4000 + i)); return b;
	}
	void Buffer(const std::vector<uint8>& b) { for (size_t i = 0; i < b.size(); ++i) mBus.Mem(static_cast<uint16>(0x4000 + i), b[i]); }
	static std::vector<uint8> Pattern(size_t size, uint8 seed = 0x11) {
		std::vector<uint8> b(size); for (size_t i = 0; i < size; ++i) b[i] = static_cast<uint8>(seed + i * 7); return b;
	}
	void Fill(uint32 index, const std::vector<uint8>& b) {
		if (!mSlot.mDisk.GetDiskImage()->WriteVirtualSector(index, b.data(), static_cast<uint32>(b.size()))) std::abort();
		mSlot.mDisk.OnDiskModified();
	}
	void SerialByte(uint8 c) { auto& p = mBus.mMachine.mChips.mPOKEY; p.WriteByte(0x0D, c); mBus.Tick(1300); }
	void WireCommand(uint8 command, uint16 sector = 1) {
		auto& p = mBus.mMachine.mChips.mPOKEY; p.WriteByte(0x0F, 0x23); p.SetCommandLine(true);
		std::array<uint8, 5> frame {0x31, command, static_cast<uint8>(sector), static_cast<uint8>(sector >> 8), 0};
		frame[4] = ATComputeSIOChecksum(frame.data(), 4);
		for (uint8 c : frame) SerialByte(c);
		p.WriteByte(0x0F, 0x13); p.SetCommandLine(false);
	}
	SIOPortableFixture mBus;
	DiskInterfaceProbe mSlot;
	ATAudioSamplePool mPool;
	ATAudioSamplePlayer mPlayer;
	ATDiskEmulator mDrive;
};

inline std::string DiskStateJSON(IATObjectState& state) {
	VDMemoryBufferStream stream; std::unique_ptr<IATSaveStateSerializer> w(ATCreateSaveStateSerializer());
	w->Serialize(stream, state, L"Disk drive portable state"); const auto data = stream.GetBuffer();
	return std::string(reinterpret_cast<const char *>(data.data()), data.size());
}
inline vdrefptr<IATObjectState> DiskStateDecode(const std::string& json) {
	VDMemoryStream stream(json.data(), static_cast<uint32>(json.size()));
	std::unique_ptr<IATSaveStateDeserializer> r(ATCreateSaveStateDeserializer());
	vdrefptr<IATSerializable> state; r->Deserialize(stream, ~state);
	return vdrefptr<IATObjectState>(static_cast<IATObjectState *>(state.get()));
}
inline uint32 DiskStateNumber(const std::string& json, const char *key) {
	const size_t pos = json.find(std::string("\"") + key + "\"");
	if (pos == std::string::npos) return ~uint32(0);
	const size_t colon = json.find(':', pos); return static_cast<uint32>(std::strtoul(json.c_str() + colon + 1, nullptr, 10));
}
inline double DiskStateFloat(const std::string& json, const char *key) {
	const size_t pos = json.find(std::string("\"") + key + "\"");
	if (pos == std::string::npos) return -1;
	return std::strtod(json.c_str() + json.find(':', pos) + 1, nullptr);
}
#endif
