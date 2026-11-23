// Portable tests of the production numbered drive/media interface.
#include <at/attest/portabletest.h>
#include "diskinterfaceportablefixture.h"

bool ATTestAltirraDiskInterfaceMedia(ATPortableTestContext& context) {
	DiskInterfaceProbe f; auto& d = f.mDisk;
	AT_PORTABLE_TEST_ASSERT(context, !d.IsDiskLoaded() && !d.IsDirty() && !d.IsDiskBacked() && !d.TryEnableWrite());
	AT_PORTABLE_TEST_ASSERT(context, d.GetMountedImageLabel() == L"(No disk)" && d.GetSectorSize(1) == 0);
	d.CreateDisk(720, 3, 256);
	AT_PORTABLE_TEST_ASSERT(context, d.IsDirty() && d.IsDiskWritable() && d.IsFormatAllowed() && !d.IsDiskBacked());
	AT_PORTABLE_TEST_ASSERT(context, d.GetSectorSize(1) == 128 && d.GetSectorSize(3) == 128 && d.GetSectorSize(4) == 256);
	AT_PORTABLE_TEST_ASSERT(context, d.GetSectorSize(0) == 0 && d.GetSectorSize(721) == 0);
	ATDiskPhysicalSectorInfo info {};
	AT_PORTABLE_TEST_ASSERT(context, d.GetSectorPhantomCount(1) == 1 && d.GetSectorPhantomCount(0) == 0);
	AT_PORTABLE_TEST_ASSERT(context, d.GetSectorInfo(1, 0, info) && info.mImageSize == 128 && info.mFDCStatus == 0xFF);
	AT_PORTABLE_TEST_ASSERT(context, !d.GetSectorInfo(0, 0, info) && !d.GetSectorInfo(1, -1, info) && !d.GetSectorInfo(1, 1, info));
	d.SetWriteMode(kATMediaWriteMode_RO); AT_PORTABLE_TEST_ASSERT(context, !d.IsDiskWritable() && !d.IsFormatAllowed());
	AT_PORTABLE_TEST_ASSERT(context, d.TryEnableWrite() && d.GetWriteMode() == kATMediaWriteMode_VRW);
	d.SetWriteMode(kATMediaWriteMode_VRWSafe); AT_PORTABLE_TEST_ASSERT(context, d.IsDiskWritable() && !d.IsFormatAllowed());
	const auto geometry = ATDiskCreateDefaultGeometry(1040, 128, 3); d.FormatDisk(geometry);
	AT_PORTABLE_TEST_ASSERT(context, d.GetDiskImage()->GetVirtualSectorCount() == 1040);
	d.UnloadDisk(); AT_PORTABLE_TEST_ASSERT(context, !d.IsDiskLoaded() && d.GetSectorPhantomCount(1) == 0);
	return true;
}

bool ATTestAltirraDiskInterfaceClients(ATPortableTestContext& context) {
	DiskInterfaceProbe f; auto& d = f.mDisk; DiskInterfaceClientProbe a, b;
	d.CreateDisk(720, 3, 128);
	AT_PORTABLE_TEST_ASSERT(context, d.IsImageSupported(*d.GetDiskImage()));
	a.mbSupported = false; a.Attach(d);
	AT_PORTABLE_TEST_ASSERT(context, !d.IsImageSupported(*d.GetDiskImage()));
	b.Attach(d); AT_PORTABLE_TEST_ASSERT(context, !d.IsImageSupported(*d.GetDiskImage())); // a full drive's geometry restriction still applies
	a.mbSupported = true; AT_PORTABLE_TEST_ASSERT(context, d.IsImageSupported(*d.GetDiskImage()));
	AT_PORTABLE_TEST_ASSERT(context, d.GetClientCount() == 2);
	d.SetAccurateSectorTimingEnabled(true); d.SetAccurateSectorTimingEnabled(true); d.SetDriveSoundsEnabled(true);
	AT_PORTABLE_TEST_ASSERT(context, a.mTiming == 1 && b.mTiming == 1 && a.mAudio == 1 && b.mAudio == 1);
	d.SetWriteMode(kATMediaWriteMode_RO); AT_PORTABLE_TEST_ASSERT(context, a.mWrite == 1 && b.mWrite == 1);
	d.FormatDisk(720, 3, 256);
	AT_PORTABLE_TEST_ASSERT(context, a.mChanging == 1 && a.mChanges == std::vector<bool>({false}));
	d.RemoveClient(&a); d.RemoveClient(&b);
	AT_PORTABLE_TEST_ASSERT(context, d.GetClientCount() == 0);
	return true;
}

bool ATTestAltirraDiskInterfaceSwap(ATPortableTestContext& context) {
	DiskInterfaceProbe a(1), b(4); auto& x = a.mDisk; auto& y = b.mDisk;
	x.CreateDisk(720, 3, 128); y.CreateDisk(1040, 3, 128); a.Write(0x11); b.Write(0x22);
	x.SetWriteMode(kATMediaWriteMode_RO); y.SetWriteMode(kATMediaWriteMode_VRWSafe);
	x.SetDriveSoundsEnabled(true); x.SetSectorBreakpoint(7); y.SetAccurateSectorTimingEnabled(true);
	uint32 notifications = 0; vdfunction<void()> changed = [&] { ++notifications; };
	DiskStateBinding boundX(x, changed), boundY(y, changed); x.SwapSettings(y);
	AT_PORTABLE_TEST_ASSERT(context, a.Read() == 0x22 && b.Read() == 0x11 && x.GetDiskImage()->GetVirtualSectorCount() == 1040);
	AT_PORTABLE_TEST_ASSERT(context, x.GetWriteMode() == kATMediaWriteMode_VRWSafe && y.GetWriteMode() == kATMediaWriteMode_RO);
	AT_PORTABLE_TEST_ASSERT(context, x.IsAccurateSectorTimingEnabled() && y.AreDriveSoundsEnabled() && y.GetSectorBreakpoint() == 7);
	AT_PORTABLE_TEST_ASSERT(context, notifications == 2); x.SwapSettings(x);
	AT_PORTABLE_TEST_ASSERT(context, notifications == 2);
	x.RemoveStateChangeHandler(&changed); y.RemoveStateChangeHandler(&changed);
	return true;
}

bool ATTestAltirraDiskInterfaceIndicators(ATPortableTestContext& context) {
	DiskInterfaceProbe f(3); auto& d = f.mDisk;
	d.SetShowMotorActive(true); d.SetShowActivity(true, 42); d.SetShowLEDReadout(0x123);
	AT_PORTABLE_TEST_ASSERT(context, f.mbMotor && f.mMotorIndex == 3 && f.mCounterIndex == 3 && f.mCounter == 4 && f.mSetFlags == 8 && f.mLED == 0x123 && f.mLEDIndex == 3);
	d.SetShowSectorCounter(true); d.SetShowActivity(true, 42); d.SetShowActivity(false, 0, 150);
	AT_PORTABLE_TEST_ASSERT(context, f.mCounter == 42 && f.mResetFlags == 8 && f.mHold == 150);
	d.SetSectorBreakpoint(9); d.CheckSectorBreakpoint(8); d.CheckSectorBreakpoint(9);
	AT_PORTABLE_TEST_ASSERT(context, f.mBreaks == 1); d.SetSectorBreakpoint(-1); d.CheckSectorBreakpoint(9);
	AT_PORTABLE_TEST_ASSERT(context, f.mBreaks == 1);
	return true;
}

bool ATTestAltirraDiskInterfaceReentry(ATPortableTestContext& context) {
	DiskInterfaceProbe f; auto& d = f.mDisk; std::array<DiskInterfaceClientProbe, 3> clients;
	for (auto& c : clients) c.Attach(d);
	clients[0].mpTiming = [&] { d.RemoveClient(&clients[0]); d.RemoveClient(&clients[1]); };
	d.SetAccurateSectorTimingEnabled(true);
	AT_PORTABLE_TEST_ASSERT(context, clients[0].mTiming == 1 && clients[1].mTiming == 0 && clients[2].mTiming == 1);
	d.RemoveClient(&clients[2]);
	// State handlers already use the core notification list: removal and addition
	// during a notification must not call a deleted observer.
	std::vector<int> calls; vdfunction<void()> first, second, third; bool thirdAdded = false;
	first = [&] { calls.push_back(1); d.RemoveStateChangeHandler(&second); if (!thirdAdded) { d.AddStateChangeHandler(&third); thirdAdded = true; } };
	second = [&] { calls.push_back(2); }; third = [&] { calls.push_back(3); };
	DiskStateBinding boundFirst(d, first), boundSecond(d, second);
	// Bind the third handler for cleanup, then remove it until the first adds it.
	DiskStateBinding boundThird(d, third); d.RemoveStateChangeHandler(&third); d.CreateDisk(720, 3, 128);
	AT_PORTABLE_TEST_ASSERT(context, std::find(calls.begin(), calls.end(), 2) == calls.end() && std::find(calls.begin(), calls.end(), 3) != calls.end());
	d.RemoveStateChangeHandler(&first); d.RemoveStateChangeHandler(&third);
	return true;
}
