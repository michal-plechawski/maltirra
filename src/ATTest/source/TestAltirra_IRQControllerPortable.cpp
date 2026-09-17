// Altirra portable IRQ controller tests

#include <array>
#include <vector>

#include <at/attest/portabletest.h>
#include <irqcontroller.h>

namespace {
	class IRQTarget {
	public:
		void AssertIRQ(int cycleOffset) {
			mAssertOffsets.push_back(cycleOffset);
		}

		void NegateIRQ() {
			++mNegateCount;
		}

		std::vector<int> mAssertOffsets;
		uint32 mNegateCount = 0;
	};
}

bool ATTestAltirraIRQController(ATPortableTestContext& context) {
	IRQTarget target;
	ATIRQController controller;
	controller.Init(&target);

	controller.Assert(kATIRQSource_POKEY, false);
	AT_PORTABLE_TEST_ASSERT(context, target.mAssertOffsets == std::vector<int>({ -1 }));
	controller.Assert(kATIRQSource_VBXE, true);
	AT_PORTABLE_TEST_ASSERT(context, target.mAssertOffsets.size() == 1);
	controller.Negate(kATIRQSource_POKEY, false);
	AT_PORTABLE_TEST_ASSERT(context, target.mNegateCount == 0);
	controller.Negate(kATIRQSource_VBXE, true);
	AT_PORTABLE_TEST_ASSERT(context, target.mNegateCount == 1);

	controller.Assert(kATIRQSource_PIAA1 | kATIRQSource_PIAB2, true);
	AT_PORTABLE_TEST_ASSERT(context, target.mAssertOffsets == std::vector<int>({ -1, 0 }));
	controller.Negate(kATIRQSource_PIAA1, false);
	AT_PORTABLE_TEST_ASSERT(context, target.mNegateCount == 1);
	controller.Negate(kATIRQSource_PIAB2, false);
	AT_PORTABLE_TEST_ASSERT(context, target.mNegateCount == 2);
	controller.Negate(kATIRQSource_PIAB2, false);
	AT_PORTABLE_TEST_ASSERT(context, target.mNegateCount == 2);

	std::array<uint32, 16> allocated {};
	for(size_t i = 0; i < allocated.size(); ++i)
		allocated[i] = controller.AllocateIRQ();
	for(size_t i = 0; i < allocated.size(); ++i)
		AT_PORTABLE_TEST_ASSERT(context, allocated[i] == (UINT32_C(1) << (i + 16)));
	for(size_t i = allocated.size(); i-- > 0; )
		controller.FreeIRQ(allocated[i]);
	controller.FreeIRQ(0);

	const uint32 customIRQ = controller.AllocateIRQ();
	controller.Assert(customIRQ, false);
	AT_PORTABLE_TEST_ASSERT(context, target.mAssertOffsets.back() == -1);
	controller.FreeIRQ(customIRQ);
	AT_PORTABLE_TEST_ASSERT(context, target.mNegateCount == 3);

	controller.Assert(kATIRQSource_PBI, false);
	const size_t assertionsBeforeReset = target.mAssertOffsets.size();
	controller.ColdReset();
	controller.Negate(kATIRQSource_PBI, false);
	AT_PORTABLE_TEST_ASSERT(context, target.mNegateCount == 3);
	controller.Assert(kATIRQSource_PBI, false);
	AT_PORTABLE_TEST_ASSERT(context, target.mAssertOffsets.size() == assertionsBeforeReset + 1);
	controller.Negate(kATIRQSource_PBI, false);
	AT_PORTABLE_TEST_ASSERT(context, target.mNegateCount == 4);

	return true;
}
