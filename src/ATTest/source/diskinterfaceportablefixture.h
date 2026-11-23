// Real disk images and host files; only UI/event sinks are observed here.
#ifndef f_ATTEST_DISKINTERFACEPORTABLEFIXTURE_H
#define f_ATTEST_DISKINTERFACEPORTABLEFIXTURE_H
#include <array>
#include <algorithm>
#include <memory>
#include <cstdlib>
#include <vector>
#include <vd2/system/error.h>
#include <vd2/system/filesys.h>
#include <vd2/system/process.h>
#include <vd2/system/thread.h>
#include <diskinterface.h>
#include <at/attest/portabletest.h>
#if defined(__APPLE__)
#include <CoreFoundation/CoreFoundation.h>
#endif

struct DiskInterfaceSandbox {
	DiskInterfaceSandbox() {
		static uint32 sequence = 0; VDStringW name;
		name.sprintf(L"altirra-disk-interface-%u-%llu-%u", static_cast<unsigned>(VDGetCurrentProcessId()),
			static_cast<unsigned long long>(VDGetCurrentTick64()), ++sequence);
		mPath = VDGetFullPath(name.c_str()); VDCreateDirectory(mPath.c_str());
	}
	~DiskInterfaceSandbox() {
		for (const auto& path : mFiles) VDRemoveFile(path.c_str());
		try { VDRemoveDirectory(mPath.c_str()); } catch (...) {}
	}
	VDStringW File(const wchar_t *name) { auto path = VDMakePath(mPath.c_str(), name); mFiles.push_back(path); return path; }
	VDStringW mPath; std::vector<VDStringW> mFiles;
};

struct DiskInterfaceProbe {
	DiskInterfaceProbe(uint32 index = 0) {
		ATDiskInterfaceConnections c;
		c.mpSetDiskErrorState = [this](uint32 i, bool v) { mErrorIndex = i; mbError = v; };
		c.mpSetDiskMotorActivity = [this](uint32 i, bool v) { mMotorIndex = i; mbMotor = v; };
		c.mpSetStatusCounter = [this](uint32 i, uint32 v) { mCounterIndex = i; mCounter = v; };
		c.mpSetStatusFlags = [this](uint32 v) { mSetFlags = v; };
		c.mpResetStatusFlags = [this](uint32 v, uint32 h) { mResetFlags = v; mHold = h; };
		c.mpSetDiskLEDState = [this](uint32 i, sint32 v) { mLEDIndex = i; mLED = v; };
		c.mpReportError = [this](const wchar_t *s) { mErrors.emplace_back(s); };
		c.mpOnSectorBreakpoint = [this] { ++mBreaks; };
		mDisk.Init(index, std::move(c));
	}
	~DiskInterfaceProbe() { if (mbExplicitShutdown) mDisk.Shutdown(); }
	void Write(uint8 value, uint32 sector = 3) {
		std::array<uint8, 256> data; data.fill(value);
		if (!mDisk.GetDiskImage()->WriteVirtualSector(sector, data.data(), mDisk.GetDiskImage()->GetSectorSize(sector))) std::abort();
		mDisk.OnDiskModified();
	}
	uint8 Read(uint32 sector = 3) {
		std::array<uint8, 256> data {}; mDisk.GetDiskImage()->ReadVirtualSector(sector, data.data(), data.size()); return data[0];
	}
	uint32 mErrorIndex = 0, mMotorIndex = 0, mCounterIndex = 0, mLEDIndex = 0;
	uint32 mCounter = 0, mSetFlags = 0, mResetFlags = 0, mHold = 0, mBreaks = 0;
	sint32 mLED = -1; bool mbError = false, mbMotor = false, mbExplicitShutdown = true;
	std::vector<VDStringW> mErrors;
	ATDiskInterface mDisk;
};

struct DiskInterfaceClientProbe final : IATDiskInterfaceClient {
	~DiskInterfaceClientProbe() { if (mpInterface) mpInterface->RemoveClient(this); }
	void Attach(ATDiskInterface& d) { mpInterface = &d; d.AddClient(this); }
	void OnDiskChanging() override { ++mChanging; if (mpChanging) mpChanging(); }
	void OnDiskChanged(bool removed) override { mChanges.push_back(removed); if (mpChanged) mpChanged(); }
	void OnWriteModeChanged() override { ++mWrite; }
	void OnTimingModeChanged() override { ++mTiming; if (mpTiming) mpTiming(); }
	void OnAudioModeChanged() override { ++mAudio; }
	bool IsImageSupported(const IATDiskImage&) const override { return mbSupported; }
	vdfunction<void()> mpChanging, mpChanged, mpTiming;
	uint32 mChanging = 0, mWrite = 0, mTiming = 0, mAudio = 0;
	std::vector<bool> mChanges; bool mbSupported = true;
	ATDiskInterface *mpInterface = nullptr;
};

struct DiskStateBinding {
	DiskStateBinding(ATDiskInterface& d, const vdfunction<void()>& fn) : mDisk(d), mFn(fn) { d.AddStateChangeHandler(&fn); }
	~DiskStateBinding() { mDisk.RemoveStateChangeHandler(&mFn); }
	ATDiskInterface& mDisk; const vdfunction<void()>& mFn;
};

inline void DiskInterfacePump() {
#if defined(__APPLE__)
	CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.005, true);
#else
	ATPortableTestPumpMessages(); VDThreadSleep(5);
#endif
}
#endif
