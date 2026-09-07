// Altirra portable vector and linear equation tests

#include <cmath>
#include <limits>

#include <at/attest/portabletest.h>
#include <vd2/system/vectors.h>

namespace {
	bool NearlyEqual(double x, double y, double tolerance = 1e-10) {
		return std::fabs(x - y) <= tolerance;
	}
}

bool ATTestSystemVectors(ATPortableTestContext& context) {
	AT_PORTABLE_TEST_ASSERT(context, VDSolveLinearEquation(nullptr, 0, 0, nullptr));
	AT_PORTABLE_TEST_ASSERT(context, !VDSolveLinearEquation(nullptr, -1, 0, nullptr));

	{
		double matrix[] = { 4.0 };
		double result[] = { 20.0 };

		AT_PORTABLE_TEST_ASSERT(context, VDSolveLinearEquation(matrix, 1, 1, result));
		AT_PORTABLE_TEST_ASSERT(context, NearlyEqual(result[0], 5.0));
	}

	{
		// This system requires a row swap on the first pivot.
		double matrix[] = {
			0.0, 2.0,
			1.0, 3.0,
		};
		double result[] = { -4.0, -2.0 };

		AT_PORTABLE_TEST_ASSERT(context, VDSolveLinearEquation(matrix, 2, 2, result));
		AT_PORTABLE_TEST_ASSERT(context, NearlyEqual(result[0], 4.0));
		AT_PORTABLE_TEST_ASSERT(context, NearlyEqual(result[1], -2.0));
	}

	{
		// The two padding elements in each row must not participate in the solve.
		double matrix[] = {
			 3.0,  2.0, -1.0, 101.0, 102.0,
			 2.0, -2.0,  4.0, 103.0, 104.0,
			-1.0,  0.5, -1.0, 105.0, 106.0,
		};
		double result[] = { 1.0, -2.0, 0.0 };

		AT_PORTABLE_TEST_ASSERT(context, VDSolveLinearEquation(matrix, 3, 5, result));
		AT_PORTABLE_TEST_ASSERT(context, NearlyEqual(result[0], 1.0));
		AT_PORTABLE_TEST_ASSERT(context, NearlyEqual(result[1], -2.0));
		AT_PORTABLE_TEST_ASSERT(context, NearlyEqual(result[2], -2.0));
		AT_PORTABLE_TEST_ASSERT(context, matrix[3] == 101.0 && matrix[4] == 102.0);
		AT_PORTABLE_TEST_ASSERT(context, matrix[8] == 103.0 && matrix[9] == 104.0);
		AT_PORTABLE_TEST_ASSERT(context, matrix[13] == 105.0 && matrix[14] == 106.0);
	}

	{
		double singularMatrix[] = {
			1.0, 2.0,
			2.0, 4.0,
		};
		double result[] = { 3.0, 6.0 };

		AT_PORTABLE_TEST_ASSERT(context,
			!VDSolveLinearEquation(singularMatrix, 2, 2, result));
	}

	{
		double matrix[] = {
			1e-8, 0.0,
			0.0,  1.0,
		};
		double result[] = { 1e-8, 1.0 };

		AT_PORTABLE_TEST_ASSERT(context,
			!VDSolveLinearEquation(matrix, 2, 2, result, 1e-5));

		double preciseMatrix[] = {
			1e-8, 0.0,
			0.0,  1.0,
		};
		double preciseResult[] = { 1e-8, 1.0 };

		AT_PORTABLE_TEST_ASSERT(context,
			VDSolveLinearEquation(preciseMatrix, 2, 2, preciseResult, 1e-10));
		AT_PORTABLE_TEST_ASSERT(context, NearlyEqual(preciseResult[0], 1.0));
		AT_PORTABLE_TEST_ASSERT(context, NearlyEqual(preciseResult[1], 1.0));
	}

	{
		const vdrect32 rect(-10, -20, 30, 40);

		AT_PORTABLE_TEST_ASSERT(context, rect.contains(vdpoint32(-10, -20)));
		AT_PORTABLE_TEST_ASSERT(context, rect.contains(vdpoint32(29, 39)));
		AT_PORTABLE_TEST_ASSERT(context, !rect.contains(vdpoint32(30, 39)));
		AT_PORTABLE_TEST_ASSERT(context, !rect.contains(vdpoint32(29, 40)));
		AT_PORTABLE_TEST_ASSERT(context, !rect.contains(vdpoint32(-11, 0)));
		AT_PORTABLE_TEST_ASSERT(context, !rect.contains(vdpoint32(0, -21)));
	}

	AT_PORTABLE_TEST_ASSERT(context,
		!vdrect32(0, 0, 0, 10).contains(vdpoint32(0, 0)));
	AT_PORTABLE_TEST_ASSERT(context,
		!vdrect32(10, 0, 5, 10).contains(vdpoint32(11, 5)));
	AT_PORTABLE_TEST_ASSERT(context,
		!vdrect32(0, 10, 10, 5).contains(vdpoint32(5, 11)));

	{
		const sint32 minValue = std::numeric_limits<sint32>::min();
		const sint32 maxValue = std::numeric_limits<sint32>::max();
		const vdrect32 extremeRect(minValue, minValue, maxValue, maxValue);

		AT_PORTABLE_TEST_ASSERT(context,
			extremeRect.contains(vdpoint32(minValue, minValue)));
		AT_PORTABLE_TEST_ASSERT(context,
			extremeRect.contains(vdpoint32(maxValue - 1, maxValue - 1)));
		AT_PORTABLE_TEST_ASSERT(context,
			!extremeRect.contains(vdpoint32(maxValue, maxValue - 1)));
		AT_PORTABLE_TEST_ASSERT(context,
			!extremeRect.contains(vdpoint32(maxValue - 1, maxValue)));
	}

	return true;
}
