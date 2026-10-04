// Portable VDDisplay screen-mask parameter tests.

#include <cmath>

#include <at/attest/portabletest.h>
#include <vd2/VDDisplay/display.h>
#include <vd2/VDDisplay/displaytypes.h>

namespace {
	class ATTestVideoDisplayFrame final : public VDVideoDisplayFrame {
	public:
		~ATTestVideoDisplayFrame() {
			++sDestructionCount;
		}

		static int sDestructionCount;
	};

	int ATTestVideoDisplayFrame::sDestructionCount = 0;

	bool NearlyEqual(float x, float y) {
		return std::fabs(x - y) <= 1e-6f;
	}
}

bool ATTestVDDisplayDisplayTypes(ATPortableTestContext& context) {
	VDDScreenMaskParams params;
	params.mOpenness = 1.0f;
	AT_PORTABLE_TEST_ASSERT(context, params.GetMaskIntensityScale() == 1.0f);

	params.mType = VDDScreenMaskType::ApertureGrille;
	AT_PORTABLE_TEST_ASSERT(context, NearlyEqual(params.GetMaskIntensityScale(), 1.0f / 3.0f));

	params.mType = VDDScreenMaskType::DotTriad;
	AT_PORTABLE_TEST_ASSERT(context, NearlyEqual(params.GetMaskIntensityScale(), 0.3022999f));

	params.mType = VDDScreenMaskType::SlotMask;
	AT_PORTABLE_TEST_ASSERT(context, NearlyEqual(params.GetMaskIntensityScale(), 1.0f / 3.0f));

	params.mOpenness = 0.0f;
	AT_PORTABLE_TEST_ASSERT(context, params.GetMaskIntensityScale() == 0.0f);
	params.mType = VDDScreenMaskType::DotTriad;
	AT_PORTABLE_TEST_ASSERT(context, params.GetMaskIntensityScale() == 0.0f);
	params.mType = VDDScreenMaskType::ApertureGrille;
	AT_PORTABLE_TEST_ASSERT(context, params.GetMaskIntensityScale() == 0.0f);

	ATTestVideoDisplayFrame::sDestructionCount = 0;
	ATTestVideoDisplayFrame *frame = new ATTestVideoDisplayFrame;
	AT_PORTABLE_TEST_ASSERT(context, frame->mPixmap.data == nullptr);
	AT_PORTABLE_TEST_ASSERT(context, frame->mFlags == 0);
	AT_PORTABLE_TEST_ASSERT(context, frame->mFrameNumber == 0);
	AT_PORTABLE_TEST_ASSERT(context, !frame->mbAllowConversion);
	AT_PORTABLE_TEST_ASSERT(context, frame->mpScreenFXEngine == nullptr);
	AT_PORTABLE_TEST_ASSERT(context, frame->mpScreenFX == nullptr);
	AT_PORTABLE_TEST_ASSERT(context, frame->AddRef() == 1);
	AT_PORTABLE_TEST_ASSERT(context, frame->AddRef() == 2);
	AT_PORTABLE_TEST_ASSERT(context, frame->Release() == 1);
	AT_PORTABLE_TEST_ASSERT(context, ATTestVideoDisplayFrame::sDestructionCount == 0);
	AT_PORTABLE_TEST_ASSERT(context, frame->Release() == 0);
	AT_PORTABLE_TEST_ASSERT(context, ATTestVideoDisplayFrame::sDestructionCount == 1);
	return true;
}
