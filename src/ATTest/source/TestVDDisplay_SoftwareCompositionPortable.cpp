// Portable compositor lifecycle, output pixels, cancellation, and reentry.

#include <stdexcept>
#include <vector>
#include <at/attest/portabletest.h>
#include <vd2/VDDisplay/internal/softwarecomposition.h>

namespace {
	struct Counts {
		int mAttach = 0;
		int mDetach = 0;
		int mDestroyed = 0;
	};
	class TestCompositor final : public vdrefcounted<IVDDisplayCompositor> {
	public:
		explicit TestCompositor(Counts& counts) : mCounts(counts) {}
		~TestCompositor() { ++mCounts.mDestroyed; }
		void AttachCompositor(IVDDisplayCompositionEngine& engine) override {
			mpEngine = &engine;
			++mCounts.mAttach;
			if (mOnAttach) mOnAttach();
		}
		void DetachCompositor() override {
			mpEngine = nullptr;
			++mCounts.mDetach;
			if (mOnDetach) mOnDetach();
		}
		void PreComposite(const VDDisplayCompositeInfo& info) override {
			mInfo = info;
			if (mpEvents) mpEvents->push_back(1);
			// UI asks once after attach, including an empty shader path. This
			// software backend must allow that call just as GDI does.
			mpEngine->LoadCustomEffect(L"");
			if (mOnPre) mOnPre();
		}
		void Composite(IVDDisplayRenderer& renderer, const VDDisplayCompositeInfo& info) override {
			if (mpEvents) mpEvents->push_back(3);
			if (mbThrow) throw std::runtime_error("test composition failure");
			renderer.SetColorRGB(0x00ABCDEF);
			renderer.FillRect((sint32)info.mWidth - 2, 0, 2, 1);
			if (mOnComposite) mOnComposite();
		}
		IVDDisplayCompositionEngine *mpEngine = nullptr;
		VDDisplayCompositeInfo mInfo {};
		std::vector<int> *mpEvents = nullptr;
		vdfunction<void()> mOnAttach, mOnDetach, mOnPre, mOnComposite;
		bool mbThrow = false;
	private:
		Counts& mCounts;
	};
}

bool ATTestVDDisplaySoftwareComposition(ATPortableTestContext& context) {
	Counts firstCounts, secondCounts, thirdCounts;
	std::vector<int> events;
	vdrefptr<TestCompositor> first(new TestCompositor(firstCounts));
	vdrefptr<TestCompositor> second(new TestCompositor(secondCounts));
	vdrefptr<TestCompositor> third(new TestCompositor(thirdCounts));
	VDDisplaySoftwareComposition engine;
	const auto paint = [&](const VDPixmap& px) {
		events.push_back(2);
		for(int y = 0; y < px.h; ++y)
			for(int x = 0; x < px.w; ++x)
				px.GetPixelRow<uint32>(y)[x] = y == 0 ? 0x00112233 : 0x00445566;
		return true;
	};
	AT_PORTABLE_TEST_ASSERT(context, !engine.GetPixmap().data);
	AT_PORTABLE_TEST_ASSERT(context, !engine.Render(0, 3, paint));
	AT_PORTABLE_TEST_ASSERT(context, !engine.Render(INT32_MAX, INT32_MAX, paint));
	AT_PORTABLE_TEST_ASSERT(context, !engine.Render(600000000, 1, paint));
	engine.SetCompositor(first);
	first->mpEvents = &events;
	AT_PORTABLE_TEST_ASSERT(context, firstCounts.mAttach == 1);
	engine.SetCompositor(first);
	AT_PORTABLE_TEST_ASSERT(context, firstCounts.mAttach == 1 && firstCounts.mDetach == 0);
	AT_PORTABLE_TEST_ASSERT(context, engine.Render(6, 3, paint));
	AT_PORTABLE_TEST_ASSERT(context, events == std::vector<int>({1, 2, 3}));
	AT_PORTABLE_TEST_ASSERT(context, first->mInfo.mWidth == 6 && first->mInfo.mHeight == 3);
	AT_PORTABLE_TEST_ASSERT(context, engine.GetPixmap().GetPixelRow<uint32>(0)[0] == 0x00112233);
	AT_PORTABLE_TEST_ASSERT(context, engine.GetPixmap().GetPixelRow<uint32>(0)[4] == 0x00ABCDEF);
	AT_PORTABLE_TEST_ASSERT(context, engine.GetPixmap().GetPixelRow<uint32>(2)[5] == 0x00445566);

	first->mbThrow = true;
	bool threw = false;
	try { engine.Render(2, 1, paint); }
	catch (const std::runtime_error&) { threw = true; }
	AT_PORTABLE_TEST_ASSERT(context, threw);
	AT_PORTABLE_TEST_ASSERT(context, engine.GetPixmap().w == 6 && engine.GetPixmap().h == 3);
	AT_PORTABLE_TEST_ASSERT(context, engine.GetPixmap().GetPixelRow<uint32>(0)[4] == 0x00ABCDEF);
	first->mbThrow = false;
	AT_PORTABLE_TEST_ASSERT(context, !engine.Render(2, 1, paint, [](const VDPixmap&) { return false; }));
	AT_PORTABLE_TEST_ASSERT(context, engine.GetPixmap().w == 6);
	bool recursiveRejected = false;
	first->mOnComposite = [&] { recursiveRejected = !engine.Render(2, 2, paint); };
	AT_PORTABLE_TEST_ASSERT(context, engine.Render(4, 2, paint));
	AT_PORTABLE_TEST_ASSERT(context, recursiveRejected);
	first->mOnComposite = nullptr;

	// Changing the attachment from PreComposite must cancel the old output.
	first->mOnPre = [&] { engine.SetCompositor(second); };
	AT_PORTABLE_TEST_ASSERT(context, !engine.Render(8, 4, paint));
	AT_PORTABLE_TEST_ASSERT(context, engine.GetPixmap().w == 4);
	AT_PORTABLE_TEST_ASSERT(context, firstCounts.mDetach == 1 && secondCounts.mAttach == 1);
	AT_PORTABLE_TEST_ASSERT(context, engine.Render(8, 4, paint));

	// A detach callback can supersede the candidate before it is attached.
	second->mOnDetach = [&] { engine.SetCompositor(third); };
	engine.SetCompositor(first);
	AT_PORTABLE_TEST_ASSERT(context, firstCounts.mAttach == 1);
	AT_PORTABLE_TEST_ASSERT(context, secondCounts.mDetach == 1 && thirdCounts.mAttach == 1);
	second->mOnDetach = nullptr;
	first->mOnPre = nullptr;

	// Shutdown during the detach half of replacement must not subsequently
	// attach the candidate that was selected before shutdown.
	{
		VDDisplaySoftwareComposition shuttingDown;
		shuttingDown.SetCompositor(first);
		first->mOnDetach = [&] { shuttingDown.Shutdown(); };
		shuttingDown.SetCompositor(second);
		AT_PORTABLE_TEST_ASSERT(context, secondCounts.mAttach == 1);
		AT_PORTABLE_TEST_ASSERT(context, !shuttingDown.Render(2, 2, paint));
		first->mOnDetach = nullptr;
	}

	// Clear from a callback invalidates the transaction but cannot free the
	// staging target while the callback still writes into it.
	third->mOnComposite = [&] { engine.Clear(); };
	AT_PORTABLE_TEST_ASSERT(context, !engine.Render(3, 3, paint));
	AT_PORTABLE_TEST_ASSERT(context, !engine.GetPixmap().data);
	third->mOnComposite = nullptr;
	AT_PORTABLE_TEST_ASSERT(context, engine.Render(3, 3, paint));
	AT_PORTABLE_TEST_ASSERT(context, !engine.Render(3, 3, [&](const VDPixmap& target) {
		engine.Clear();
		target.GetPixelRow<uint32>(0)[0] = 0x00123456;
		return true;
	}));
	AT_PORTABLE_TEST_ASSERT(context, !engine.GetPixmap().data);

	// Shutdown is terminal, including requests made inside a detach callback.
	third->mOnDetach = [&] { engine.SetCompositor(first); };
	engine.Shutdown();
	AT_PORTABLE_TEST_ASSERT(context, thirdCounts.mDetach == 1);
	AT_PORTABLE_TEST_ASSERT(context, !engine.Render(3, 3, paint));
	AT_PORTABLE_TEST_ASSERT(context, !engine.GetPixmap().data);
	first = nullptr;
	second = nullptr;
	third = nullptr;
	AT_PORTABLE_TEST_ASSERT(context, firstCounts.mDestroyed == 1);
	AT_PORTABLE_TEST_ASSERT(context, secondCounts.mDestroyed == 1);
	AT_PORTABLE_TEST_ASSERT(context, thirdCounts.mDestroyed == 1);
	return true;
}
