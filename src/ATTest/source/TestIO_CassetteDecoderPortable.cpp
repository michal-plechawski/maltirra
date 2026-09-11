// Altirra portable cassette decoder tests

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

#include <at/attest/portabletest.h>
#include <at/atio/cassettedecoder.h>
#include <at/atio/internal/cassettedecoder.h>

namespace {
	using ATStereoSamples = std::vector<std::array<sint16, 2>>;

	bool ATGetPackedBit(const std::vector<uint32>& bits, size_t index) {
		return (bits[index >> 5] & (UINT32_C(0x80000000) >> (index & 31))) != 0;
	}

	ATStereoSamples ATMakeFSKTone(bool mark, size_t count) {
		static constexpr sint16 kSpaceWave[8] {
			0, 23170, 32767, 23170, 0, -23170, -32767, -23170
		};
		static constexpr sint16 kMarkWave[6] {
			0, 28377, 28377, 0, -28377, -28377
		};
		ATStereoSamples samples(count);

		for(size_t i = 0; i < count; ++i) {
			samples[i][0] = mark ? kMarkWave[i % 6] : kSpaceWave[i % 8];
			samples[i][1] = static_cast<sint16>((i * 313 + 29) & 0x7FFF);
		}

		return samples;
	}

	std::vector<uint32> ATDecodeFSK(
		const ATStereoSamples& samples, bool chunked) {
		ATCassetteDecoderFSK decoder;
		std::vector<uint32> bits((samples.size() + 31) >> 5, 0);
		static constexpr uint32 kChunks[] { 1, 7, 31, 5, 48 };
		size_t position = 0;
		size_t chunkIndex = 0;

		while(position < samples.size()) {
			const uint32 count = static_cast<uint32>(std::min<size_t>(
				chunked ? kChunks[chunkIndex++ % std::size(kChunks)] : samples.size(),
				samples.size() - position));
			decoder.Process<false>(
				&samples[position][0], count,
				bits.data() + (position >> 5),
				static_cast<uint32>(position & 31), nullptr);
			position += count;
		}

		return bits;
	}

	bool ATCheckFSKDecoder(ATPortableTestContext& context) {
		ATCassetteDecoderFSK emptyDecoder;
		uint32 emptyBits = UINT32_C(0xA5A5A5A5);
		emptyDecoder.Process<false>(nullptr, 0, &emptyBits, 0, nullptr);
		AT_PORTABLE_TEST_ASSERT(context, emptyBits == UINT32_C(0xA5A5A5A5));

		for(bool mark : { false, true }) {
			const ATStereoSamples samples = ATMakeFSKTone(mark, 192);
			const std::vector<uint32> whole = ATDecodeFSK(samples, false);
			const std::vector<uint32> chunked = ATDecodeFSK(samples, true);

			AT_PORTABLE_TEST_ASSERT(context, chunked == whole);
			for(size_t i = 32; i < samples.size(); ++i)
				AT_PORTABLE_TEST_ASSERT(context, ATGetPackedBit(whole, i) == mark);

			ATCassetteDecoderFSK analyzedDecoder;
			std::vector<uint32> analyzedBits((samples.size() + 31) >> 5, 0);
			std::vector<float> analysis(samples.size() * 6, 123.0f);
			analyzedDecoder.Process<true>(
				&samples[0][0], static_cast<uint32>(samples.size()),
				analyzedBits.data(), 0, analysis.data());
			AT_PORTABLE_TEST_ASSERT(context, analyzedBits == whole);

			for(size_t i = 0; i < samples.size(); ++i) {
				AT_PORTABLE_TEST_ASSERT(context, std::isfinite(analysis[i * 6 + 0]));
				AT_PORTABLE_TEST_ASSERT(context, std::isfinite(analysis[i * 6 + 1]));
				AT_PORTABLE_TEST_ASSERT(context, std::isfinite(analysis[i * 6 + 2]));
				AT_PORTABLE_TEST_ASSERT(context,
					analysis[i * 6 + 3] == -0.8f || analysis[i * 6 + 3] == 0.8f);
				AT_PORTABLE_TEST_ASSERT(context, analysis[i * 6 + 4] == 123.0f);
				AT_PORTABLE_TEST_ASSERT(context, analysis[i * 6 + 5] == 123.0f);
			}
		}

		return true;
	}

	ATStereoSamples ATMakeTurboSamples(size_t count) {
		ATStereoSamples samples(count);
		for(size_t i = 0; i < count; ++i) {
			const sint16 level = ((i / 7) & 1) ? 20000 : -20000;
			samples[i][0] = level;
			samples[i][1] = static_cast<sint16>((i * 197 + 11) & 0x7FFF);
		}
		return samples;
	}

	std::vector<uint32> ATDecodeTurbo(
		ATCassetteTurboDecodeAlgorithm algorithm,
		const ATStereoSamples& samples,
		bool chunked) {
		ATCassetteDecoderTurbo decoder;
		decoder.Init(algorithm, false);
		static constexpr uint32 kChunks[] { 1, 19, 32, 3, 71 };
		size_t position = 0;
		size_t chunkIndex = 0;

		while(position < samples.size()) {
			const uint32 count = static_cast<uint32>(std::min<size_t>(
				chunked ? kChunks[chunkIndex++ % std::size(kChunks)] : samples.size(),
				samples.size() - position));
			decoder.Process(&samples[position][0], count, nullptr);
			position += count;
		}

		const vdfastvector<uint32> result = decoder.Finalize();
		return std::vector<uint32>(result.begin(), result.end());
	}

	bool ATCheckTurboDecoders(ATPortableTestContext& context) {
		static constexpr ATCassetteTurboDecodeAlgorithm kAlgorithms[] {
			ATCassetteTurboDecodeAlgorithm::SlopeNoFilter,
			ATCassetteTurboDecodeAlgorithm::SlopeFilter,
			ATCassetteTurboDecodeAlgorithm::PeakFilter,
			ATCassetteTurboDecodeAlgorithm::PeakFilterBalanceLoHi,
			ATCassetteTurboDecodeAlgorithm::PeakFilterBalanceHiLo,
		};
		const ATStereoSamples samples = ATMakeTurboSamples(257);
		ATCassetteDecoderTurbo emptyDecoder;
		emptyDecoder.Init(ATCassetteTurboDecodeAlgorithm::SlopeNoFilter, false);
		emptyDecoder.Process(nullptr, 0, nullptr);
		AT_PORTABLE_TEST_ASSERT(context, emptyDecoder.Finalize().empty());

		for(const ATCassetteTurboDecodeAlgorithm algorithm : kAlgorithms) {
			const std::vector<uint32> whole =
				ATDecodeTurbo(algorithm, samples, false);
			const std::vector<uint32> chunked =
				ATDecodeTurbo(algorithm, samples, true);
			AT_PORTABLE_TEST_ASSERT(context, whole.size() == 9);
			AT_PORTABLE_TEST_ASSERT(context, chunked == whole);

			ATCassetteDecoderTurbo analyzedDecoder;
			analyzedDecoder.Init(algorithm, true);
			std::vector<float> analysis(samples.size() * 6, 123.0f);
			analyzedDecoder.Process(
				&samples[0][0], static_cast<uint32>(samples.size()), analysis.data());
			const vdfastvector<uint32> analyzedBits = analyzedDecoder.Finalize();
			AT_PORTABLE_TEST_ASSERT(context, analyzedBits.size() == whole.size());
			for(size_t i = 0; i < whole.size(); ++i)
				AT_PORTABLE_TEST_ASSERT(context, analyzedBits[i] == whole[i]);

			for(size_t i = 0; i < samples.size(); ++i) {
				AT_PORTABLE_TEST_ASSERT(context, analysis[i * 6 + 0] == 123.0f);
				AT_PORTABLE_TEST_ASSERT(context, analysis[i * 6 + 1] == 123.0f);
				AT_PORTABLE_TEST_ASSERT(context, analysis[i * 6 + 2] == 123.0f);
				AT_PORTABLE_TEST_ASSERT(context, analysis[i * 6 + 3] == 123.0f);
				AT_PORTABLE_TEST_ASSERT(context, std::isfinite(analysis[i * 6 + 4]));
				AT_PORTABLE_TEST_ASSERT(context,
					analysis[i * 6 + 5] == -0.8f || analysis[i * 6 + 5] == 0.8f);
			}
		}

		const std::vector<uint32> slope = ATDecodeTurbo(
			ATCassetteTurboDecodeAlgorithm::SlopeNoFilter, samples, false);
		for(size_t i = 0; i < samples.size(); ++i)
			AT_PORTABLE_TEST_ASSERT(
				context, ATGetPackedBit(slope, i) == (samples[i][0] > 0));

		return true;
	}

	bool ATCheckTurboReset(ATPortableTestContext& context) {
		const ATStereoSamples positive(64, { 20000, 0 });
		const ATStereoSamples silence(64, { 0, 0 });
		ATCassetteDecoderTurbo decoder;
		decoder.Init(ATCassetteTurboDecodeAlgorithm::SlopeNoFilter, false);
		decoder.Process(&positive[0][0], static_cast<uint32>(positive.size()), nullptr);
		(void)decoder.Finalize();

		decoder.Reset();
		decoder.Process(&silence[0][0], static_cast<uint32>(silence.size()), nullptr);
		const vdfastvector<uint32> resetBits = decoder.Finalize();
		for(uint32 word : resetBits)
			AT_PORTABLE_TEST_ASSERT(context, word == 0);

		return true;
	}

	bool ATNearlyEqual(float x, float y) {
		return std::fabs(x - y)
			<= 1e-5f * std::max({ 1.0f, std::fabs(x), std::fabs(y) });
	}

	bool ATCheckFIRPrimitive(ATPortableTestContext& context) {
		std::array<float, 16> reference {};
		std::array<float, 16> stored {};
		ATCassetteDecoderFIRState state;
		ATCassetteDecoderFIRInit(state, reference.data());

		for(int i = 0; i < 193; ++i) {
			const float sample = static_cast<float>((i * 7919) % 40001 - 20000);
			for(size_t j = 0; j < reference.size(); ++j)
				reference[j] += sample * kATCassetteDecoderHPFKernel[j];

			const float expected = reference[0];
			for(size_t j = 0; j + 1 < reference.size(); ++j)
				reference[j] = reference[j + 1];
			reference.back() = 0;

			AT_PORTABLE_TEST_ASSERT(
				context, ATNearlyEqual(ATCassetteDecoderFIRProcess(state, sample), expected));
		}

		ATCassetteDecoderFIRStore(stored.data(), state);
		for(size_t i = 0; i < stored.size(); ++i)
			AT_PORTABLE_TEST_ASSERT(context, ATNearlyEqual(stored[i], reference[i]));

		return true;
	}
}

bool ATTestIOCassetteDecoder(ATPortableTestContext& context) {
	return ATCheckFSKDecoder(context)
		&& ATCheckTurboDecoders(context)
		&& ATCheckTurboReset(context)
		&& ATCheckFIRPrimitive(context);
}
