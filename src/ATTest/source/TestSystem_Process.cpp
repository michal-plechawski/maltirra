// Altirra portable asynchronous process launch tests

#include <vd2/system/Error.h>
#include <vd2/system/filesys.h>
#include <vd2/system/process.h>
#include <vd2/system/thread.h>
#include <vd2/system/time.h>

#include <at/attest/portabletest.h>

namespace {
	class VDProcessTestMarker {
	public:
		explicit VDProcessTestMarker(const VDStringW& path)
			: mPath(path) {
			VDRemoveFile(mPath.c_str());
		}

		~VDProcessTestMarker() {
			VDRemoveFile(mPath.c_str());
		}

		VDStringW mPath;
	};

	bool VDLaunchFails(const wchar_t *path) {
		try {
			VDLaunchProgram(path);
		} catch(const VDException&) {
			return true;
		}
		return false;
	}
}

bool ATTestSystemProcess(ATPortableTestContext& context) {
	static uint32 sequence = 0;
	VDStringW markerName;
	markerName.sprintf(
		L"altirra process marker %u-%llu-%u.tmp",
		static_cast<unsigned>(VDGetCurrentProcessId()),
		static_cast<unsigned long long>(VDGetCurrentTick64()),
		static_cast<unsigned>(++sequence));
	VDProcessTestMarker marker(VDGetFullPath(markerName.c_str()));

#if defined(_WIN32)
	const VDStringW executable = VDMakePath(VDGetSystemPath().c_str(), L"cmd.exe");
	VDStringW arguments;
	arguments.sprintf(L"/D /C type nul > \"%ls\"", marker.mPath.c_str());
#else
	const VDStringW executable(L"/usr/bin/touch");
	VDStringW arguments;
	arguments.sprintf(L"\"%ls\"", marker.mPath.c_str());
#endif

	VDLaunchProgram(executable.c_str(), arguments.c_str());
	const uint64 deadline = VDGetCurrentTick64() + 5000;
	while(!VDDoesPathExist(marker.mPath.c_str()) && VDGetCurrentTick64() < deadline)
		VDThreadSleep(10);
	AT_PORTABLE_TEST_ASSERT(context, VDDoesPathExist(marker.mPath.c_str()));
	AT_PORTABLE_TEST_ASSERT(context, VDRemoveFile(marker.mPath.c_str()));

	VDStringW missingPath(marker.mPath);
	missingPath += L".missing";
	AT_PORTABLE_TEST_ASSERT(context, VDLaunchFails(missingPath.c_str()));
	return true;
}
