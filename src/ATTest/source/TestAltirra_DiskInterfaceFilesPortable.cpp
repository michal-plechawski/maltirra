// Real ATR/XFD persistence, virtual folders, lazy flush and disk error handling.
#include <at/attest/portabletest.h>
#include <at/atcore/vfs.h>
#include <vd2/system/zip.h>
#include "diskinterfaceportablefixture.h"

bool ATTestAltirraDiskInterfaceFiles(ATPortableTestContext& context) {
	DiskInterfaceSandbox sandbox;
	for (bool atr : {true, false}) {
		auto path = sandbox.File(atr ? L"disk.atr" : L"disk.xfd"); DiskInterfaceProbe f; auto& d = f.mDisk;
		d.CreateDisk(720, 3, 128); f.Write(0x11);
		d.SaveDiskAs(path.c_str(), atr ? kATDiskImageFormat_ATR : kATDiskImageFormat_XFD);
		AT_PORTABLE_TEST_ASSERT(context, d.IsDiskBacked() && !d.IsDirty() && VDStringW(d.GetPath()) == path);
		f.Write(0x22); AT_PORTABLE_TEST_ASSERT(context, d.CanRevert() && d.IsDirty());
		AT_PORTABLE_TEST_ASSERT(context, d.RevertDisk() && f.Read() == 0x11 && !d.IsDirty() && d.GetWriteMode() == kATMediaWriteMode_VRW);
		f.Write(0x33); d.SaveDisk();
		AT_PORTABLE_TEST_ASSERT(context, !d.IsDirty());
		d.LoadDisk(d.GetPath()); AT_PORTABLE_TEST_ASSERT(context, f.Read() == 0x33 && d.GetWriteMode() == kATMediaWriteMode_RO);
		AT_PORTABLE_TEST_ASSERT(context, d.GetDiskImage()->IsUpdatable());
		d.SetWriteMode(kATMediaWriteMode_RW); d.FormatDisk(1040, 3, 128); d.SaveDisk();
		d.LoadDisk(path.c_str()); AT_PORTABLE_TEST_ASSERT(context, d.GetDiskImage()->GetVirtualSectorCount() == 1040);
		// Reload the last reference owned by the slot; both path arguments may alias its state.
		auto *image = d.GetDiskImage(); d.LoadDisk(d.GetPath(), d.GetPath(), image);
		AT_PORTABLE_TEST_ASSERT(context, d.GetDiskImage() == image && VDStringW(d.GetPath()) == path);
	}
	// A real ZIP view stays virtual: updates must never overwrite its container.
	const auto sourcePath = sandbox.File(L"source.atr"), archivePath = sandbox.File(L"disks.zip");
	DiskInterfaceProbe archived; archived.mDisk.CreateDisk(720, 3, 128); archived.Write(0x44);
	archived.mDisk.SaveDiskAs(sourcePath.c_str(), kATDiskImageFormat_ATR);
	{
		VDFileStream source(sourcePath.c_str(), nsVDFile::kRead | nsVDFile::kOpenExisting);
		std::vector<uint8> bytes(static_cast<size_t>(source.Length())); source.Read(bytes.data(), static_cast<sint32>(bytes.size()));
		VDFileStream output(archivePath.c_str(), nsVDFile::kWrite | nsVDFile::kCreateAlways);
		std::unique_ptr<IVDZipArchiveWriter> writer(VDCreateZipArchiveWriter(output));
		writer->BeginFile(L"source.atr", VDDeflateCompressionLevel::Best).Write(bytes.data(), static_cast<sint32>(bytes.size()));
		writer->EndFile(); writer->Finalize();
	}
	const auto virtualPath = ATMakeVFSPathForZipFile(archivePath.c_str(), L"source.atr");
	archived.mDisk.LoadDisk(virtualPath.c_str());
	AT_PORTABLE_TEST_ASSERT(context, archived.Read() == 0x44 && !archived.mDisk.GetDiskImage()->IsUpdatable());
	archived.mDisk.TryEnableWrite(); archived.Write(0x88);
	archived.mDisk.SaveDiskAs(sourcePath.c_str(), kATDiskImageFormat_ATR);
	AT_PORTABLE_TEST_ASSERT(context, archived.mDisk.GetDiskImage()->IsUpdatable());
	archived.mDisk.LoadDisk(virtualPath.c_str()); AT_PORTABLE_TEST_ASSERT(context, archived.Read() == 0x44);
	return true;
}

bool ATTestAltirraDiskInterfaceFolders(ATPortableTestContext& context) {
	DiskInterfaceSandbox sandbox; auto file = sandbox.File(L"HELLO.TXT");
	{ VDFileStream stream(file.c_str(), nsVDFile::kWrite | nsVDFile::kCreateAlways); stream.Write("HELLO", 5); }
	for (bool sdfs : {false, true}) {
		DiskInterfaceProbe f; auto& d = f.mDisk; d.MountFolder(sandbox.mPath.c_str(), sdfs);
		AT_PORTABLE_TEST_ASSERT(context, d.IsDiskLoaded() && d.GetDiskImage()->IsDynamic() && d.IsDiskBacked());
		AT_PORTABLE_TEST_ASSERT(context, !d.IsDiskWritable() && !d.IsFormatAllowed() && !d.TryEnableWrite());
		const VDStringW mountPath(d.GetPath()); d.UnloadDisk(); d.LoadDisk(mountPath.c_str());
		AT_PORTABLE_TEST_ASSERT(context, d.GetDiskImage()->IsDynamic() && VDStringW(d.GetPath()) == mountPath);
		std::array<uint8, 128> boot {};
		AT_PORTABLE_TEST_ASSERT(context, d.GetDiskImage()->ReadVirtualSector(0, boot.data(), boot.size()) == 128);
		bool rejected = false; try { d.SaveDiskAs(file.c_str(), kATDiskImageFormat_ATR); } catch (const MyError&) { rejected = true; }
		AT_PORTABLE_TEST_ASSERT(context, rejected);
	}
	return true;
}

bool ATTestAltirraDiskInterfaceAutoFlush(ATPortableTestContext& context) {
	DiskInterfaceSandbox sandbox; auto path = sandbox.File(L"lazy.atr"); DiskInterfaceProbe f; auto& d = f.mDisk;
	d.CreateDisk(720, 3, 128); d.SaveDiskAs(path.c_str(), kATDiskImageFormat_ATR); d.SetWriteMode(kATMediaWriteMode_RW);
	f.Write(0x11); const uint64 initial = VDGetCurrentTick64();
	while (VDGetCurrentTick64() - initial < 300) DiskInterfacePump();
	f.Write(0x55); const uint64 lastWrite = VDGetCurrentTick64();
	while (d.IsDirty() && VDGetCurrentTick64() - lastWrite < 6000) DiskInterfacePump();
	AT_PORTABLE_TEST_ASSERT(context, !d.IsDirty() && VDGetCurrentTick64() - lastWrite >= 1740);
	d.LoadDisk(path.c_str()); AT_PORTABLE_TEST_ASSERT(context, f.Read() == 0x55);
	// An unmount must save an auto-flush image even before the timer fires.
	d.SetWriteMode(kATMediaWriteMode_RW); f.Write(0xA6); d.UnloadDisk();
	d.LoadDisk(path.c_str()); AT_PORTABLE_TEST_ASSERT(context, f.Read() == 0xA6);
	d.SetWriteMode(kATMediaWriteMode_RW); f.Write(0xCC); d.SetWriteMode(kATMediaWriteMode_VRW);
	const uint64 cancelled = VDGetCurrentTick64(); while (VDGetCurrentTick64() - cancelled < 600) DiskInterfacePump();
	vdrefptr<IATDiskImage> stored; ATLoadDiskImage(path.c_str(), ~stored); std::array<uint8, 128> sector {};
	stored->ReadVirtualSector(3, sector.data(), sector.size());
	AT_PORTABLE_TEST_ASSERT(context, d.IsDirty() && sector[0] == 0xA6);
	d.SaveDisk(); AT_PORTABLE_TEST_ASSERT(context, !d.IsDirty());
	return true;
}

bool ATTestAltirraDiskInterfaceErrors(ATPortableTestContext& context) {
	DiskInterfaceSandbox sandbox; auto path = sandbox.File(L"good.atr"); DiskInterfaceProbe f(2); auto& d = f.mDisk;
	bool rejected = false; try { d.SaveDiskAs(path.c_str(), kATDiskImageFormat_ATR); } catch (const MyError&) { rejected = true; }
	AT_PORTABLE_TEST_ASSERT(context, rejected);
	d.CreateDisk(720, 3, 128); d.SaveDiskAs(path.c_str(), kATDiskImageFormat_ATR);
	const auto badPath = VDMakePath(sandbox.mPath.c_str(), L"missing/image.atr");
	d.GetDiskImage()->SetPath(badPath.c_str(), kATDiskImageFormat_ATR); d.SetWriteMode(kATMediaWriteMode_RW); f.Write(0x77);
	d.Flush();
	AT_PORTABLE_TEST_ASSERT(context, d.IsDirty() && !(d.GetWriteMode() & kATMediaWriteMode_AutoFlush) && f.mbError && f.mErrorIndex == 2 && f.mErrors.size() == 1);
	AT_PORTABLE_TEST_ASSERT(context, VDStringW(f.mErrors[0]).find(L'D') == 0);
	rejected = false; try { d.SaveDisk(); } catch (const MyError&) { rejected = true; }
	AT_PORTABLE_TEST_ASSERT(context, rejected && d.IsDirty() && f.mbError);
	d.SaveDiskAs(path.c_str(), kATDiskImageFormat_ATR);
	AT_PORTABLE_TEST_ASSERT(context, !d.IsDirty() && !f.mbError);
	const auto oldPath = VDStringW(d.GetPath()); rejected = false;
	try { d.LoadDisk(badPath.c_str()); } catch (const MyError&) { rejected = true; }
	AT_PORTABLE_TEST_ASSERT(context, rejected && d.IsDiskLoaded() && VDStringW(d.GetPath()) == oldPath);
	return true;
}

bool ATTestAltirraDiskInterfaceLifecycle(ATPortableTestContext& context) {
	DiskInterfaceSandbox sandbox; auto path = sandbox.File(L"shutdown.atr");
	{
		DiskInterfaceProbe f; auto& d = f.mDisk;
		d.CreateDisk(720, 3, 128); d.SaveDiskAs(path.c_str(), kATDiskImageFormat_ATR); d.SetWriteMode(kATMediaWriteMode_RW); f.Write(0x42);
		d.Shutdown(); d.Shutdown();
	}
	{
		DiskInterfaceProbe f; f.mDisk.LoadDisk(path.c_str());
		AT_PORTABLE_TEST_ASSERT(context, f.Read() == 0x42);
		f.mDisk.SetWriteMode(kATMediaWriteMode_RW); f.Write(0x53); f.mbExplicitShutdown = false;
	}
	const uint64 start = VDGetCurrentTick64(); while (VDGetCurrentTick64() - start < 600) DiskInterfacePump();
	DiskInterfaceProbe check; check.mDisk.LoadDisk(path.c_str());
	AT_PORTABLE_TEST_ASSERT(context, check.Read() == 0x53);
	return true;
}
