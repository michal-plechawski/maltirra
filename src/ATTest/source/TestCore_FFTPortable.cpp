// Altirra portable FFT and IMDCT tests

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <limits>
#include <numbers>
#include <type_traits>

#include <vd2/system/vdstl.h>
#include <at/atcore/fft.h>
#include <at/attest/portabletest.h>

namespace {
	bool ATNearlyEqual(float x, float y, float tolerance) {
		return std::abs(x - y) <= tolerance;
	}

	template<int N>
	void ATMakeFFTInput(std::array<float, N>& input) {
		for(int i = 0; i < N; ++i) {
			const float x = static_cast<float>(i);
			input[i] = 0.65f * std::sin(x * 0.173f)
				+ 0.23f * std::cos(x * 0.071f)
				+ static_cast<float>((i * 17 + 5) % 23 - 11) * 0.013f;
		}
	}

	template<int N>
	bool ATCheckFFTReference(bool optimizeForSpeed) {
		alignas(64) std::array<float, N> input {};
		alignas(64) std::array<float, N> actual {};
		alignas(64) std::array<float, N> expected {};
		ATMakeFFTInput<N>(input);

		const double scale = -2.0 * std::numbers::pi_v<double> / N;
		for(int frequency = 0; frequency <= N / 2; ++frequency) {
			double real = 0;
			double imaginary = 0;

			for(int sample = 0; sample < N; ++sample) {
				const double phase = scale * frequency * sample;
				real += input[sample] * std::cos(phase);
				imaginary += input[sample] * std::sin(phase);
			}

			if (frequency == N / 2) {
				expected[1] = static_cast<float>(real);
			} else {
				expected[frequency * 2] = static_cast<float>(real);
				expected[frequency * 2 + 1] = static_cast<float>(imaginary);
			}
		}

		ATFFT<N> fft(optimizeForSpeed);
		fft.Forward(actual.data(), input.data());

		for(int i = 0; i < N; ++i) {
			if (!ATNearlyEqual(actual[i], expected[i], 0.0025f))
				return false;
		}

		return true;
	}

	template<int N>
	bool ATCheckFFTRoundTrip(bool optimizeForSpeed) {
		alignas(64) std::array<float, N> input {};
		alignas(64) std::array<float, N> frequency {};
		alignas(64) std::array<float, N> output {};
		ATMakeFFTInput<N>(input);

		ATFFT<N> fft(optimizeForSpeed);
		fft.Forward(frequency.data(), input.data());
		fft.Inverse(output.data(), frequency.data());

		for(int i = 0; i < N; ++i) {
			const float expected = input[i] * (N / 2.0f);
			const float tolerance = 0.002f * std::max(1.0f, std::abs(expected));

			if (!ATNearlyEqual(output[i], expected, tolerance))
				return false;
		}

		frequency = input;
		fft.Forward(frequency.data());
		fft.Inverse(frequency.data());
		for(int i = 0; i < N; ++i) {
			const float expected = input[i] * (N / 2.0f);
			const float tolerance = 0.002f * std::max(1.0f, std::abs(expected));

			if (!ATNearlyEqual(frequency[i], expected, tolerance))
				return false;
		}

		return true;
	}

	template<int N>
	bool ATCheckFFTMultiplyAdd(bool optimizeForSpeed) {
		static constexpr int kOffset = 1;
		static constexpr int kStorageSize = N * 4 + 2;
		alignas(64) std::array<float, kStorageSize> destinationStorage {};
		alignas(64) std::array<float, kStorageSize> source1Storage {};
		alignas(64) std::array<float, kStorageSize> source2Storage {};

		for(int i = 0; i < kStorageSize; ++i) {
			destinationStorage[i] = 1000.0f + i * 0.25f;
			source1Storage[i] = static_cast<float>((i * 7) % 19 - 9) * 0.17f;
			source2Storage[i] = static_cast<float>((i * 11) % 17 - 8) * 0.09f;
		}

		float *const destination = destinationStorage.data() + kOffset;
		const float *const source1 = source1Storage.data() + kOffset;
		const float *const source2 = source2Storage.data() + kOffset;
		const std::array<float, kStorageSize> original = destinationStorage;
		std::array<float, N> expected {};
		std::copy_n(destination, N, expected.begin());
		expected[0] += source1[0] * source2[0];
		expected[1] += source1[1] * source2[1];

		for(int i = 2; i < N; i += 2) {
			const float real1 = source1[i];
			const float imaginary1 = source1[i + 1];
			const float real2 = source2[i];
			const float imaginary2 = source2[i + 1];

			expected[i] += real1 * real2 - imaginary1 * imaginary2;
			expected[i + 1] += real1 * imaginary2 + imaginary1 * real2;
		}

		ATFFT<N> fft(optimizeForSpeed);
		fft.MultiplyAdd(destination, source1, source2);

		for(int i = 0; i < N; ++i) {
			const float tolerance = 4.0f
				* std::numeric_limits<float>::epsilon()
				* std::max(1.0f, std::abs(expected[i]));
			if (!ATNearlyEqual(destination[i], expected[i], tolerance)) {
				std::fprintf(stderr,
					"FFT MultiplyAdd<%d> mismatch at %d: actual=%.9g expected=%.9g delta=%.9g\n",
					N, i, destination[i], expected[i],
					destination[i] - expected[i]);
				return false;
			}
		}

		for(int i = 0; i < kStorageSize; ++i) {
			if (i >= kOffset && i < kOffset + N)
				continue;

			if (destinationStorage[i] != original[i]) {
				std::fprintf(stderr,
					"FFT MultiplyAdd<%d> overwrote guard at %d\n", N, i);
				return false;
			}
		}

		return true;
	}

	bool ATCheckIMDCT(bool optimizeForSpeed) {
		static constexpr int N = 64;
		alignas(64) std::array<float, N> input {};
		std::array<float, N> expected {};
		std::array<float, 8 * N> cosineTable {};

		for(int i = 0; i < 8 * N; ++i) {
			cosineTable[i] = std::cos(
				-2.0f * std::numbers::pi_v<float> * i / (8 * N));
		}

		ATFFTAllocator allocator;
		ATIMDCT imdct;
		imdct.Reserve(allocator, N, optimizeForSpeed);
		allocator.Finalize();
		imdct.Bind(allocator);

		for(const int impulse : { 0, 1, 7, 31, 32, 63 }) {
			input.fill(0);
			input[impulse] = 1;

			for(int i = 0; i < N / 2; ++i) {
				expected[i] = cosineTable[
					((i * 2 + (1 + N + N * 2)) * (impulse * 2 + 1))
						& (8 * N - 1)];
				expected[i + N / 2] = cosineTable[
					((i * 2 + (1 + N + N)) * (impulse * 2 + 1))
						& (8 * N - 1)];
			}

			imdct.Transform(input.data());
			for(int i = 0; i < N; ++i) {
				if (!ATNearlyEqual(input[i], expected[i], 0.002f))
					return false;
			}
		}

		return true;
	}
}

bool ATTestCoreFFT(ATPortableTestContext& context) {
	for(const bool optimizeForSpeed : { false, true }) {
		AT_PORTABLE_TEST_ASSERT(context,
			ATCheckFFTReference<16>(optimizeForSpeed));
		AT_PORTABLE_TEST_ASSERT(context,
			ATCheckFFTReference<32>(optimizeForSpeed));
		AT_PORTABLE_TEST_ASSERT(context,
			ATCheckFFTRoundTrip<16>(optimizeForSpeed));
		AT_PORTABLE_TEST_ASSERT(context,
			ATCheckFFTRoundTrip<32>(optimizeForSpeed));
		AT_PORTABLE_TEST_ASSERT(context,
			ATCheckFFTRoundTrip<64>(optimizeForSpeed));
		AT_PORTABLE_TEST_ASSERT(context,
			ATCheckFFTRoundTrip<256>(optimizeForSpeed));
		AT_PORTABLE_TEST_ASSERT(context,
			ATCheckFFTRoundTrip<2048>(optimizeForSpeed));
		AT_PORTABLE_TEST_ASSERT(context,
			ATCheckFFTMultiplyAdd<32>(optimizeForSpeed));
		AT_PORTABLE_TEST_ASSERT(context,
			ATCheckFFTMultiplyAdd<256>(optimizeForSpeed));
		AT_PORTABLE_TEST_ASSERT(context,
			ATCheckIMDCT(optimizeForSpeed));
	}

	return true;
}
