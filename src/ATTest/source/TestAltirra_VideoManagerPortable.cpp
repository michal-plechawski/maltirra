// Altirra portable video output manager tests

#include <array>

#include <at/attest/portabletest.h>
#include <vd2/Kasumi/pixmap.h>
#include <videomanager.h>

namespace {
	class TestVideoOutput final : public IATDeviceVideoOutput {
	public:
		TestVideoOutput(const char *name, const wchar_t *displayName)
			: mpName(name), mpDisplayName(displayName) {}

		const char *GetName() const override { return mpName; }
		const wchar_t *GetDisplayName() const override { return mpDisplayName; }
		void Tick(uint32) override {}
		void UpdateFrame() override {}
		const VDPixmap& GetFrameBuffer() override { return mFrameBuffer; }
		const ATDeviceVideoInfo& GetVideoInfo() override { return mVideoInfo; }
		vdpoint32 PixelToCaretPos(const vdpoint32& pos) override { return pos; }
		vdrect32 CharToPixelRect(const vdrect32& rect) override { return rect; }
		int ReadRawText(uint8 *, int, int, int) override { return 0; }
		uint32 GetActivityCounter() override { return mActivityCounter; }

		uint32 mActivityCounter = 0;

	private:
		const char *mpName;
		const wchar_t *mpDisplayName;
		VDPixmap mFrameBuffer {};
		ATDeviceVideoInfo mVideoInfo {};
	};
}

bool ATTestAltirraVideoManager(ATPortableTestContext& context) {
	ATVideoManager manager;
	TestVideoOutput charlie("charlie", L"Charlie");
	TestVideoOutput alpha("alpha", L"alpha");
	TestVideoOutput bravo("bravo", L"BRAVO");
	TestVideoOutput absent("absent", L"Absent");

	std::array<uint32, 3> addedIndices {};
	std::array<uint32, 3> removingIndices {};
	size_t addedCount = 0;
	size_t removingCount = 0;
	IATDeviceVideoOutput *removingOutput = nullptr;

	vdfunction<void(uint32)> onAdded = [&](uint32 index) {
		if (addedCount < addedIndices.size())
			addedIndices[addedCount] = index;
		++addedCount;
	};
	vdfunction<void(uint32)> onRemoving = [&](uint32 index) {
		if (removingCount < removingIndices.size())
			removingIndices[removingCount] = index;
		++removingCount;
		removingOutput = manager.GetOutput(index);
	};
	manager.OnAddedOutput().Add(&onAdded);
	manager.OnRemovingOutput().Add(&onRemoving);

	AT_PORTABLE_TEST_ASSERT(context, manager.GetOutputCount() == 0);
	AT_PORTABLE_TEST_ASSERT(context, manager.GetOutputListChangeCount() == 1);
	AT_PORTABLE_TEST_ASSERT(context, manager.GetOutput(0) == nullptr);
	AT_PORTABLE_TEST_ASSERT(context, manager.IndexOfOutput(nullptr) == -1);
	AT_PORTABLE_TEST_ASSERT(context, manager.CheckForNewlyActiveOutputs() == -1);

	manager.AddVideoOutput(nullptr);
	AT_PORTABLE_TEST_ASSERT(context, manager.GetOutputListChangeCount() == 1);
	AT_PORTABLE_TEST_ASSERT(context, addedCount == 0);

	manager.AddVideoOutput(&charlie);
	manager.AddVideoOutput(&alpha);
	manager.AddVideoOutput(&bravo);
	AT_PORTABLE_TEST_ASSERT(context, manager.GetOutputCount() == 3);
	AT_PORTABLE_TEST_ASSERT(context, manager.GetOutputListChangeCount() == 4);
	AT_PORTABLE_TEST_ASSERT(context, addedCount == 3);
	AT_PORTABLE_TEST_ASSERT(context, addedIndices == (std::array<uint32, 3>{0, 0, 1}));
	AT_PORTABLE_TEST_ASSERT(context, manager.GetOutput(0) == &alpha);
	AT_PORTABLE_TEST_ASSERT(context, manager.GetOutput(1) == &bravo);
	AT_PORTABLE_TEST_ASSERT(context, manager.GetOutput(2) == &charlie);
	AT_PORTABLE_TEST_ASSERT(context, manager.GetOutput(3) == nullptr);
	AT_PORTABLE_TEST_ASSERT(context, manager.GetOutputByName("bravo") == &bravo);
	AT_PORTABLE_TEST_ASSERT(context, manager.GetOutputByName("BRAVO") == nullptr);
	AT_PORTABLE_TEST_ASSERT(context, manager.GetOutputByName("missing") == nullptr);
	AT_PORTABLE_TEST_ASSERT(context, manager.IndexOfOutput(&charlie) == 2);
	AT_PORTABLE_TEST_ASSERT(context, manager.IndexOfOutput(&absent) == -1);
	AT_PORTABLE_TEST_ASSERT(context, manager.CheckForNewlyActiveOutputs() == -1);

	charlie.mActivityCounter = 1;
	bravo.mActivityCounter = 1;
	AT_PORTABLE_TEST_ASSERT(context, manager.CheckForNewlyActiveOutputs() == 1);
	AT_PORTABLE_TEST_ASSERT(context, manager.CheckForNewlyActiveOutputs() == -1);
	alpha.mActivityCounter = 3;
	AT_PORTABLE_TEST_ASSERT(context, manager.CheckForNewlyActiveOutputs() == 0);
	alpha.mActivityCounter = 4;
	manager.ResetActivityCounters();
	AT_PORTABLE_TEST_ASSERT(context, manager.CheckForNewlyActiveOutputs() == -1);

	manager.RemoveVideoOutput(&absent);
	AT_PORTABLE_TEST_ASSERT(context, manager.GetOutputListChangeCount() == 4);
	AT_PORTABLE_TEST_ASSERT(context, removingCount == 0);
	manager.RemoveVideoOutput(&bravo);
	AT_PORTABLE_TEST_ASSERT(context, removingOutput == &bravo);
	AT_PORTABLE_TEST_ASSERT(context, manager.GetOutputListChangeCount() == 5);
	AT_PORTABLE_TEST_ASSERT(context, manager.GetOutputCount() == 2);
	AT_PORTABLE_TEST_ASSERT(context, manager.GetOutput(1) == &charlie);
	manager.RemoveVideoOutput(&alpha);
	AT_PORTABLE_TEST_ASSERT(context, removingOutput == &alpha);
	manager.RemoveVideoOutput(&charlie);
	AT_PORTABLE_TEST_ASSERT(context, removingOutput == &charlie);
	AT_PORTABLE_TEST_ASSERT(context, removingCount == 3);
	AT_PORTABLE_TEST_ASSERT(context, removingIndices == (std::array<uint32, 3>{1, 0, 0}));
	AT_PORTABLE_TEST_ASSERT(context, manager.GetOutputCount() == 0);
	AT_PORTABLE_TEST_ASSERT(context, manager.GetOutputListChangeCount() == 7);
	manager.OnAddedOutput().Remove(&onAdded);
	manager.OnRemovingOutput().Remove(&onRemoving);

	return true;
}
