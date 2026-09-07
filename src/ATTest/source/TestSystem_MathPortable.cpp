// Altirra portable system math tests

#include <limits>
#include <vd2/system/math.h>
#include <at/attest/portabletest.h>

namespace {
	template<typename T, typename R>
	bool CheckRoundVectors(R (*function)(T)) {
		static constexpr struct {
			T mInput;
			R mExpected;
		} kCases[] = {
			{ (T)0.0, (R)0 },
			{ (T)0.45, (R)0 },
			{ (T)0.55, (R)1 },
			{ (T)1.45, (R)1 },
			{ (T)1.55, (R)2 },
			{ (T)-0.45, (R)0 },
			{ (T)-0.55, (R)-1 },
			{ (T)-1.45, (R)-1 },
			{ (T)-1.55, (R)-2 },
		};

		for(const auto& testCase : kCases) {
			if (function(testCase.mInput) != testCase.mExpected)
				return false;
		}

		return true;
	}
}

bool ATTestSystemMath(ATPortableTestContext& context) {
	AT_PORTABLE_TEST_ASSERT(context,
		(CheckRoundVectors<float, int>(static_cast<int (*)(float)>(VDRoundToInt))));
	AT_PORTABLE_TEST_ASSERT(context,
		(CheckRoundVectors<double, int>(static_cast<int (*)(double)>(VDRoundToInt))));
	AT_PORTABLE_TEST_ASSERT(context,
		(CheckRoundVectors<float, sint32>(static_cast<sint32 (*)(float)>(VDRoundToInt32))));
	AT_PORTABLE_TEST_ASSERT(context,
		(CheckRoundVectors<double, sint32>(static_cast<sint32 (*)(double)>(VDRoundToInt32))));
	AT_PORTABLE_TEST_ASSERT(context,
		(CheckRoundVectors<float, sint64>(static_cast<sint64 (*)(float)>(VDRoundToInt64))));
	AT_PORTABLE_TEST_ASSERT(context,
		(CheckRoundVectors<double, sint64>(static_cast<sint64 (*)(double)>(VDRoundToInt64))));

	AT_PORTABLE_TEST_ASSERT(context, VDFloorToInt(-1.5) == -2);
	AT_PORTABLE_TEST_ASSERT(context, VDFloorToInt64(1.5) == 1);
	AT_PORTABLE_TEST_ASSERT(context, VDCeilToInt(-1.5) == -1);
	AT_PORTABLE_TEST_ASSERT(context, VDCeilToInt64(1.5) == 2);
	AT_PORTABLE_TEST_ASSERT(context, VDClampToUint32((sint64)-1) == 0);
	AT_PORTABLE_TEST_ASSERT(context, VDClampToUint32((sint64)UINT32_MAX + 1) == UINT32_MAX);
	AT_PORTABLE_TEST_ASSERT(context, VDClampToSint32(UINT32_MAX) == INT32_MAX);
	AT_PORTABLE_TEST_ASSERT(context, VDClampToUint16(UINT32_MAX) == UINT16_MAX);

	uint32 remainder = 0;
	AT_PORTABLE_TEST_ASSERT(context, (uint64)VDFractionScale64(100, 7, 9, remainder) == 77);
	AT_PORTABLE_TEST_ASSERT(context, remainder == 7);
	AT_PORTABLE_TEST_ASSERT(context,
		(uint64)VDFractionScale64(
			UINT64_C(0x123456789ABCDEF0), UINT32_C(0x89ABCDEF), UINT32_C(0xFEDCBA98), remainder)
		== UINT64_C(0x09D56A235FFE8387));
	AT_PORTABLE_TEST_ASSERT(context, remainder == UINT32_C(0x2A7823E8));
	AT_PORTABLE_TEST_ASSERT(context,
		(uint64)VDFractionScale64(UINT64_MAX, UINT32_MAX - 1, UINT32_MAX, remainder)
		== UINT64_C(0xFFFFFFFEFFFFFFFE));
	AT_PORTABLE_TEST_ASSERT(context, remainder == 0);
	remainder = UINT32_MAX;
	AT_PORTABLE_TEST_ASSERT(context,
		(uint64)VDFractionScale64(UINT64_MAX, UINT32_MAX, 1, remainder) == UINT64_MAX);
	AT_PORTABLE_TEST_ASSERT(context, remainder == 0);
	AT_PORTABLE_TEST_ASSERT(context, VDUMulDiv64x32(100, 7, 9) == 77);

	AT_PORTABLE_TEST_ASSERT(context, VDMulDiv64(100, 100, 6) == 1667);
	AT_PORTABLE_TEST_ASSERT(context, VDMulDiv64(-100, 100, 8) == -1250);
	AT_PORTABLE_TEST_ASSERT(context, VDMulDiv64(-100, -100, -8) == -1250);
	AT_PORTABLE_TEST_ASSERT(context,
		VDMulDiv64(std::numeric_limits<sint64>::min(), 1, 1)
		== std::numeric_limits<sint64>::min());
	AT_PORTABLE_TEST_ASSERT(context,
		VDMulDiv64(-INT64_C(1000000000000), -INT64_C(100000), 17)
		== INT64_C(5882352941176471));

	const float finiteValues[] = {
		0.0f,
		-0.0f,
		1.0f,
		std::numeric_limits<float>::denorm_min(),
		std::numeric_limits<float>::max(),
	};
	AT_PORTABLE_TEST_ASSERT(context, VDVerifyFiniteFloats(finiteValues, 5));
	AT_PORTABLE_TEST_ASSERT(context, VDVerifyFiniteFloats(nullptr, 0));

	const float infinity = std::numeric_limits<float>::infinity();
	const float negativeInfinity = -std::numeric_limits<float>::infinity();
	const float nan = std::numeric_limits<float>::quiet_NaN();
	AT_PORTABLE_TEST_ASSERT(context, !VDVerifyFiniteFloats(&infinity, 1));
	AT_PORTABLE_TEST_ASSERT(context, !VDVerifyFiniteFloats(&negativeInfinity, 1));
	AT_PORTABLE_TEST_ASSERT(context, !VDVerifyFiniteFloats(&nan, 1));

	{
		VDFastMathScope scope;
	}

	return true;
}
