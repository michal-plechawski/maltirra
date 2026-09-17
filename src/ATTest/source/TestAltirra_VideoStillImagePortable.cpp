// Portable video still image device tests.

#include <array>
#include <cwchar>

#include <at/atcore/propertyset.h>
#include <at/attest/portabletest.h>
#include <vd2/Kasumi/pixmap.h>
#include <vd2/system/filesys.h>
#include <vd2/system/time.h>

#include <oshelper.h>
#include <videostillimage.h>

namespace {
	class ATVideoStillImageTestFile {
	public:
		ATVideoStillImageTestFile() {
			mPath.sprintf(
				L"altirra-video-still-image-test-%u-%llu.png",
				static_cast<unsigned>(VDGetCurrentProcessId()),
				static_cast<unsigned long long>(VDGetCurrentTick64()));
			mPath = VDGetFullPath(mPath.c_str());
		}

		~ATVideoStillImageTestFile() {
			VDRemoveFile(mPath.c_str());
		}

		VDStringW mPath;
	};
}

bool ATTestAltirraVideoStillImage(ATPortableTestContext& context) {
	ATVideoStillImageTestFile testFile;
	std::array<uint32, 16> pixels;
	pixels.fill(0x00123456);

	VDPixmap source {};
	source.data = pixels.data();
	source.w = 4;
	source.h = 4;
	source.pitch = 4 * sizeof(uint32);
	source.format = nsVDPixmap::kPixFormat_XRGB8888;
	ATSaveFrame(source, testFile.mPath.c_str());

	ATDeviceVideoStillImage device;
	AT_PORTABLE_TEST_ASSERT(context,
		device.AsInterface(IATDeviceVideoSource::kTypeID) != nullptr);
	AT_PORTABLE_TEST_ASSERT(context, device.IsSaveStateAgnostic());

	ATPropertySet settings;
	settings.SetString("path", testFile.mPath.c_str());
	AT_PORTABLE_TEST_ASSERT(context, device.SetSettings(settings));

	ATPropertySet capturedSettings;
	device.GetSettings(capturedSettings);
	AT_PORTABLE_TEST_ASSERT(context,
		!std::wcscmp(capturedSettings.GetString("path", L""), testFile.mPath.c_str()));

	VDStringW blurb;
	device.GetSettingsBlurb(blurb);
	AT_PORTABLE_TEST_ASSERT(context, blurb == testFile.mPath);

	ATDeviceInfo info {};
	device.GetDeviceInfo(info);
	AT_PORTABLE_TEST_ASSERT(context, info.mpDef == &g_ATDeviceDefVideoStillImage);

	device.Init();
	VDStringW error;
	AT_PORTABLE_TEST_ASSERT(context, !device.GetErrorStatus(0, error));
	AT_PORTABLE_TEST_ASSERT(context, !device.GetErrorStatus(1, error));
	AT_PORTABLE_TEST_ASSERT(context, device.ReadVideoSample(360.0f, 240) == 0x123456);
	AT_PORTABLE_TEST_ASSERT(context, device.ReadVideoSample(360.0f, -1) == 0);
	AT_PORTABLE_TEST_ASSERT(context, device.ReadVideoSample(360.0f, 480) == 0);

	VDStringW missingPath(testFile.mPath);
	missingPath += L".missing";
	settings.SetString("path", missingPath.c_str());
	AT_PORTABLE_TEST_ASSERT(context, device.SetSettings(settings));
	AT_PORTABLE_TEST_ASSERT(context, device.GetErrorStatus(0, error));
	AT_PORTABLE_TEST_ASSERT(context, !device.GetErrorStatus(1, error));
	AT_PORTABLE_TEST_ASSERT(context, device.ReadVideoSample(360.0f, 240) == 0);

	settings.SetString("path", testFile.mPath.c_str());
	AT_PORTABLE_TEST_ASSERT(context, device.SetSettings(settings));
	AT_PORTABLE_TEST_ASSERT(context, !device.GetErrorStatus(0, error));
	AT_PORTABLE_TEST_ASSERT(context, device.ReadVideoSample(360.0f, 240) == 0x123456);
	return true;
}
