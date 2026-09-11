// Altirra portable cassette audio filter tests

#include <algorithm>
#include <array>
#include <cstring>
#include <memory>
#include <vector>

#include <at/attest/portabletest.h>
#include <at/atio/cassetteaudiofilters.h>
#include <at/atio/internal/cassetteaudiofilters.h>

namespace {
	class ATVectorAudioSource final : public IATCassetteAudioSource {
	public:
		ATVectorAudioSource(
			const std::vector<std::array<sint16, 2>>& samples,
			uint32 maximumChunk)
			: mSamples(samples)
			, mMaximumChunk(maximumChunk) {
		}

		uint32 ReadAudio(sint16 (*dst)[2], uint32 n) override {
			const uint32 available = static_cast<uint32>(mSamples.size() - mPosition);
			const uint32 actual = std::min({ n, available, mMaximumChunk });

			if (actual) {
				memcpy(dst, mSamples.data() + mPosition, actual * sizeof(*dst));
				mPosition += actual;
			}

			return actual;
		}

	private:
		const std::vector<std::array<sint16, 2>>& mSamples;
		size_t mPosition = 0;
		uint32 mMaximumChunk;
	};

	bool ATReadAll(
		ATPortableTestContext& context,
		IATCassetteAudioSource& source,
		size_t maximumSamples,
		std::vector<std::array<sint16, 2>>& output) {
		std::array<std::array<sint16, 2>, 137> buffer;

		while(output.size() < maximumSamples) {
			const uint32 actual = source.ReadAudio(
				reinterpret_cast<sint16 (*)[2]>(buffer.data()),
				static_cast<uint32>(buffer.size()));
			if (!actual)
				break;

			AT_PORTABLE_TEST_ASSERT(context, actual <= buffer.size());
			output.insert(output.end(), buffer.begin(), buffer.begin() + actual);
		}

		AT_PORTABLE_TEST_ASSERT(context, output.size() < maximumSamples);
		AT_PORTABLE_TEST_ASSERT(context, source.ReadAudio(
			reinterpret_cast<sint16 (*)[2]>(buffer.data()), 1) == 0);
		return true;
	}

	bool ATCheckMinMax(ATPortableTestContext& context) {
		alignas(64) std::array<sint16, 2 * 96> samples;
		uint32 state = UINT32_C(0x91E10DA5);

		for(sint16& sample : samples) {
			state = state * UINT32_C(1664525) + UINT32_C(1013904223);
			sample = static_cast<sint16>(state >> 16);
		}

		for(size_t offset = 8; offset < 16; ++offset) {
			for(uint32 n = 1; n <= 48; ++n) {
				if (offset + n > 88)
					continue;

				sint32 refMinL = 0;
				sint32 refMaxL = 0;
				sint32 refMinR = 0;
				sint32 refMaxR = 0;
				sint32 actualMinL = 0;
				sint32 actualMaxL = 0;
				sint32 actualMinR = 0;
				sint32 actualMaxR = 0;

				const sint16 *const src = samples.data() + offset * 2;
				ATCassetteAudioMinMax16x2_Reference(
					src, n, refMinL, refMaxL, refMinR, refMaxR);
				ATCassetteAudioMinMax16x2_Accelerated(
					src, n,
					actualMinL, actualMaxL, actualMinR, actualMaxR);

				AT_PORTABLE_TEST_ASSERT(context, actualMinL == refMinL);
				AT_PORTABLE_TEST_ASSERT(context, actualMaxL == refMaxL);
				AT_PORTABLE_TEST_ASSERT(context, actualMinR == refMinR);
				AT_PORTABLE_TEST_ASSERT(context, actualMaxR == refMaxR);
			}
		}

		return true;
	}

	bool ATCheckResamplerPrimitive(ATPortableTestContext& context) {
		alignas(16) std::array<sint16, 2 * 160> samples;
		uint32 state = UINT32_C(0x5728C14B);
		for(sint16& sample : samples) {
			state = state * UINT32_C(1103515245) + UINT32_C(12345);
			sample = static_cast<sint16>(static_cast<sint32>(state % 24001) - 12000);
		}

		struct TestCase {
			uint32 mCount;
			uint64 mAccum;
			sint64 mIncrement;
		};
		static constexpr TestCase kCases[] = {
			{ 1,  UINT64_C(0x0000000000000000), INT64_C(0x0000000000000000) },
			{ 7,  UINT64_C(0x0000000012345000), INT64_C(0x0000000080000000) },
			{ 31, UINT64_C(0x000000007FFFF000), INT64_C(0x0000000100000000) },
			{ 53, UINT64_C(0x0000000001357000), INT64_C(0x0000000140000000) },
		};

		for(const TestCase& testCase : kCases) {
			constexpr sint16 kGuard = 12345;
			std::array<sint16, 2 * 64 + 8> reference;
			std::array<sint16, 2 * 64 + 8> actual;
			reference.fill(kGuard);
			actual.fill(kGuard);

			const uint64 referenceAccum = ATCassetteAudioResample16x2_Reference(
				reference.data() + 4, samples.data(), testCase.mCount,
				testCase.mAccum, testCase.mIncrement);
			const uint64 actualAccum = ATCassetteAudioResample16x2_Accelerated(
				actual.data() + 4, samples.data(), testCase.mCount,
				testCase.mAccum, testCase.mIncrement);

			AT_PORTABLE_TEST_ASSERT(context, actualAccum == referenceAccum);
			for(size_t i = 0; i < 4; ++i) {
				AT_PORTABLE_TEST_ASSERT(context, reference[i] == kGuard);
				AT_PORTABLE_TEST_ASSERT(context, actual[i] == kGuard);
			}
			for(size_t i = 0; i < 2 * testCase.mCount; ++i) {
				const int difference =
					static_cast<int>(actual[i + 4]) - reference[i + 4];

				// The SIMD paths round interpolated filter coefficients while the
				// scalar path truncates them, so a few output LSBs may differ.
				AT_PORTABLE_TEST_ASSERT(context, difference >= -3 && difference <= 3);
			}
			for(size_t i = 4 + 2 * testCase.mCount; i < actual.size(); ++i) {
				AT_PORTABLE_TEST_ASSERT(context, reference[i] == kGuard);
				AT_PORTABLE_TEST_ASSERT(context, actual[i] == kGuard);
			}
		}

		return true;
	}

	bool ATCheckPublicFilters(ATPortableTestContext& context) {
		std::vector<std::array<sint16, 2>> samples(1024);
		for(size_t i = 0; i < samples.size(); ++i) {
			samples[i][0] = static_cast<sint16>((i * 101 + 17) & 0x7FFF);
			samples[i][1] = static_cast<sint16>((i * 313 + 29) & 0x7FFF);
		}

		ATVectorAudioSource resamplerSource(samples, 113);
		ATCassetteAudioResampler resampler(
			resamplerSource, UINT64_C(0x0000000100000000));
		std::vector<std::array<sint16, 2>> resampled;
		AT_PORTABLE_TEST_ASSERT(
			context, ATReadAll(context, resampler, samples.size() + 16, resampled));
		AT_PORTABLE_TEST_ASSERT(context, resampled == samples);

		const std::vector<std::array<sint16, 2>> silence(512);
		ATVectorAudioSource compensatorSource(silence, 113);
		ATCassetteAudioFSKSpeedCompensator compensator(compensatorSource);
		std::vector<std::array<sint16, 2>> compensated;
		AT_PORTABLE_TEST_ASSERT(context, ATReadAll(
			context, compensator, silence.size() + 16, compensated));
		AT_PORTABLE_TEST_ASSERT(context, compensated == silence);

		ATVectorAudioSource cancellerSource(samples, 256);
		ATCassetteAudioCrosstalkCanceller canceller(cancellerSource);
		std::vector<std::array<sint16, 2>> cancelled;
		AT_PORTABLE_TEST_ASSERT(context, ATReadAll(
			context, canceller, samples.size() + 16, cancelled));

		// The FFT filter has seven blocks of preroll and four tail blocks, so
		// three 256-sample blocks are retained at the end of a finite stream.
		AT_PORTABLE_TEST_ASSERT(
			context, cancelled.size() == samples.size() - 3 * 256);
		for(size_t i = 0; i < cancelled.size(); ++i)
			AT_PORTABLE_TEST_ASSERT(context, cancelled[i][1] == samples[i][1]);

		return true;
	}

	bool ATCheckPeakMap(ATPortableTestContext& context) {
		const std::vector<std::array<sint16, 2>> samples {
			{ -30000,  12000 }, { -1000, -22000 },
			{  24000,   7000 }, {  1200,  31000 },
			{ -16000, -10000 }, { 15000,   9000 },
			{  -4000, -28000 }, { 29000,  17000 },
		};
		ATVectorAudioSource source(samples, 4);
		vdfastvector<uint8> peaksL;
		vdfastvector<uint8> peaksR;
		ATCassetteAudioPeakMapFilter filter(source, true, 4.0, peaksL, peaksR);
		std::array<std::array<sint16, 2>, 4> output;

		AT_PORTABLE_TEST_ASSERT(context, filter.ReadAudio(
			reinterpret_cast<sint16 (*)[2]>(output.data()), 4) == 4);
		AT_PORTABLE_TEST_ASSERT(context, peaksL.empty());
		AT_PORTABLE_TEST_ASSERT(context, peaksR.empty());
		AT_PORTABLE_TEST_ASSERT(context, filter.ReadAudio(
			reinterpret_cast<sint16 (*)[2]>(output.data()), 4) == 4);
		AT_PORTABLE_TEST_ASSERT(context, peaksL.size() == 2);
		AT_PORTABLE_TEST_ASSERT(context, peaksR.size() == 2);
		AT_PORTABLE_TEST_ASSERT(context, peaksL[0] < peaksL[1]);
		AT_PORTABLE_TEST_ASSERT(context, peaksR[0] < peaksR[1]);

		return true;
	}

	bool ATCheckThreadedQueue(ATPortableTestContext& context) {
		std::vector<std::array<sint16, 2>> input(10000);
		for(size_t i = 0; i < input.size(); ++i) {
			input[i][0] = static_cast<sint16>((i * 101 + 17) & 0xFFFF);
			input[i][1] = static_cast<sint16>((i * 313 + 29) & 0xFFFF);
		}

		ATVectorAudioSource source(input, 257);
		auto queue = std::make_unique<ATCassetteAudioThreadedQueue>(source);
		std::vector<std::array<sint16, 2>> output(input.size());
		size_t outputLevel = 0;
		static constexpr uint32 kRequestSizes[] = { 1, 31, 509, 4096 };
		size_t requestIndex = 0;

		for(;;) {
			const uint32 request = static_cast<uint32>(std::min<size_t>(
				kRequestSizes[requestIndex++ % std::size(kRequestSizes)],
				output.size() - outputLevel));
			if (!request)
				break;

			const uint32 actual = queue->ReadAudio(
				reinterpret_cast<sint16 (*)[2]>(output.data() + outputLevel),
				request);
			outputLevel += actual;
			if (!actual)
				break;
		}

		AT_PORTABLE_TEST_ASSERT(context, outputLevel == input.size());
		AT_PORTABLE_TEST_ASSERT(context, output == input);
		std::array<sint16, 2> extra {};
		AT_PORTABLE_TEST_ASSERT(
			context,
			queue->ReadAudio(
				reinterpret_cast<sint16 (*)[2]>(&extra), 1) == 0);
		queue.reset();

		// Also exercise early cancellation while the producer still has data.
		ATVectorAudioSource cancellationSource(input, 17);
		{
			auto cancelledQueue =
				std::make_unique<ATCassetteAudioThreadedQueue>(cancellationSource);
			std::array<std::array<sint16, 2>, 3> prefix;
			AT_PORTABLE_TEST_ASSERT(
				context,
				cancelledQueue->ReadAudio(
					reinterpret_cast<sint16 (*)[2]>(prefix.data()),
					static_cast<uint32>(prefix.size())) == prefix.size());
		}

		return true;
	}
}

bool ATTestIOCassetteAudioFilters(ATPortableTestContext& context) {
	return ATCheckMinMax(context)
		&& ATCheckResamplerPrimitive(context)
		&& ATCheckPublicFilters(context)
		&& ATCheckPeakMap(context)
		&& ATCheckThreadedQueue(context);
}
