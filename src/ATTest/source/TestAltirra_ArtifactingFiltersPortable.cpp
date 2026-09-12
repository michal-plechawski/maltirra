// Altirra portable video artifacting filter tests

#include <cmath>
#include <cstddef>
#include <initializer_list>

#include <at/attest/portabletest.h>
#include <artifacting_filters.h>

namespace {
	bool NearFilterValue(float actual, float expected) {
		return std::abs(actual - expected) < 1e-5f;
	}

	bool SameKernel(const ATFilterKernel& actual, int offset,
		std::initializer_list<float> expected) {
		if (actual.mOffset != offset || actual.mCoeffs.size() != expected.size())
			return false;

		size_t i = 0;
		for(float value : expected) {
			if (!NearFilterValue(actual.mCoeffs[i++], value))
				return false;
		}

		return true;
	}
}

bool ATTestAltirraArtifactingFilters(ATPortableTestContext& context) {
	ATFilterKernel x;
	AT_PORTABLE_TEST_ASSERT(context, x.mOffset == 0);
	x.Init(-1, {1, 2});
	ATFilterKernel y;
	y.Init(1, {3, 4});
	ATFilterKernel z;
	z.Init(0, {3, 4});

	ATFilterKernel product;
	ATFilterKernelConvolve(product, x, y);
	AT_PORTABLE_TEST_ASSERT(context, SameKernel(product, 0, {3, 10, 8}));
	AT_PORTABLE_TEST_ASSERT(context, SameKernel(x * y, 0, {3, 10, 8}));
	float values[] {1, 2, 3};
	AT_PORTABLE_TEST_ASSERT(context, NearFilterValue(ATFilterKernelEvaluate(product, values), 47));

	AT_PORTABLE_TEST_ASSERT(context, SameKernel(x + z, -1, {1, 5, 4}));
	AT_PORTABLE_TEST_ASSERT(context, SameKernel(x - z, -1, {1, -1, -4}));
	AT_PORTABLE_TEST_ASSERT(context, SameKernel(x * 2.0f, -1, {2, 4}));
	AT_PORTABLE_TEST_ASSERT(context, SameKernel(-x, -1, {-1, -2}));
	AT_PORTABLE_TEST_ASSERT(context, SameKernel(x << 2, -3, {1, 2}));
	AT_PORTABLE_TEST_ASSERT(context, SameKernel(x >> 2, 1, {1, 2}));

	ATFilterKernel sum(x);
	sum += z;
	AT_PORTABLE_TEST_ASSERT(context, SameKernel(sum, -1, {1, 5, 4}));
	sum -= z;
	AT_PORTABLE_TEST_ASSERT(context, SameKernel(sum, -1, {1, 2, 0}));
	sum *= 2.0f;
	AT_PORTABLE_TEST_ASSERT(context, SameKernel(sum, -1, {2, 4, 0}));
	AT_PORTABLE_TEST_ASSERT(context, SameKernel(sum.trim(), -1, {2, 4}));

	ATFilterKernelReverse(x);
	AT_PORTABLE_TEST_ASSERT(context, SameKernel(x, 0, {2, 1}));
	AT_PORTABLE_TEST_ASSERT(context, SameKernel(~x, -1, {1, 2}));

	ATFilterKernel modulated;
	modulated.Init(-1, {1, 2, 3, 4});
	ATFilterKernel mask;
	mask.Init(0, {1, -1});
	AT_PORTABLE_TEST_ASSERT(context, SameKernel(modulated ^ mask, -1, {-1, 2, -3, 4}));

	float accumulated[5] {};
	ATFilterKernelAccumulate(x, accumulated + 1);
	AT_PORTABLE_TEST_ASSERT(context, NearFilterValue(accumulated[1], 2));
	AT_PORTABLE_TEST_ASSERT(context, NearFilterValue(accumulated[2], 1));
	ATFilterKernelAccumulateSub(x, accumulated + 1);
	AT_PORTABLE_TEST_ASSERT(context, NearFilterValue(accumulated[1], 0));
	AT_PORTABLE_TEST_ASSERT(context, NearFilterValue(accumulated[2], 0));
	ATFilterKernelAccumulate(x, accumulated + 1, 2.0f);
	AT_PORTABLE_TEST_ASSERT(context, NearFilterValue(accumulated[1], 4));
	AT_PORTABLE_TEST_ASSERT(context, NearFilterValue(accumulated[2], 2));

	ATFilterKernel window;
	window.Init(-1, {0, 2, 3, 0});
	float clipped[3] {};
	ATFilterKernelAccumulateWindow(window, clipped, 0, 3, 1.5f);
	AT_PORTABLE_TEST_ASSERT(context, NearFilterValue(clipped[0], 3));
	AT_PORTABLE_TEST_ASSERT(context, NearFilterValue(clipped[1], 4.5f));
	AT_PORTABLE_TEST_ASSERT(context, NearFilterValue(clipped[2], 0));
	ATFilterKernelAccumulateWindow(window, clipped, 20, 3, 1.5f);
	AT_PORTABLE_TEST_ASSERT(context, NearFilterValue(clipped[0], 3));
	AT_PORTABLE_TEST_ASSERT(context, NearFilterValue(clipped[1], 4.5f));

	ATFilterKernel cubic;
	ATFilterKernelSetBicubic(cubic, 0, -0.75f);
	AT_PORTABLE_TEST_ASSERT(context, SameKernel(cubic, -1, {0, 1, 0, 0}));
	float coeffs[4] {};
	ATFilterKernelEvalCubic4(coeffs, 0.25f, -0.75f);
	ATFilterKernelSetBicubic(cubic, 0.25f, -0.75f);
	float coefficientSum = 0;
	for(size_t i = 0; i < 4; ++i) {
		AT_PORTABLE_TEST_ASSERT(context, NearFilterValue(coeffs[i], cubic.mCoeffs[i]));
		coefficientSum += coeffs[i];
	}
	AT_PORTABLE_TEST_ASSERT(context, NearFilterValue(coefficientSum, 1));

	ATFilterKernel samples;
	samples.Init(0, {1, 2, 3});
	AT_PORTABLE_TEST_ASSERT(context,
		SameKernel(ATFilterKernelSampleBicubic(samples, 0, 1, -0.75f),
			-3, {0, 0, 1, 2, 3, 0}));
	ATFilterKernel points;
	points.Init(-2, {1, 2, 3, 4, 5});
	AT_PORTABLE_TEST_ASSERT(context,
		SameKernel(ATFilterKernelSamplePoint(points, 0, 2), -1, {1, 3, 5}));

	ATFilterKernel empty;
	ATFilterKernelConvolve(product, x, empty);
	AT_PORTABLE_TEST_ASSERT(context, product.mCoeffs.empty());
	ATFilterKernelReverse(empty);
	AT_PORTABLE_TEST_ASSERT(context, empty.mCoeffs.empty());
	AT_PORTABLE_TEST_ASSERT(context, NearFilterValue(ATFilterKernelEvaluate(empty, nullptr), 0));

	return true;
}
