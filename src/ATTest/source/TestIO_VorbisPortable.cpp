// Altirra portable Vorbis decoder tests

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <vector>

#include <at/attest/portabletest.h>
#include <at/atio/vorbisdecoder.h>
#include <at/atio/vorbismisc.h>

namespace {
	std::vector<uint8_t> ATLoadVorbisFixture() {
		static constexpr const char *kPaths[] {
			"dist/extras/sampledevices/pluck.ogg",
			"../../dist/extras/sampledevices/pluck.ogg"
		};

		FILE *file = nullptr;
		for(const char *path : kPaths) {
			file = fopen(path, "rb");
			if (file)
				break;
		}

		if (!file)
			return {};

		if (fseek(file, 0, SEEK_END)) {
			fclose(file);
			return {};
		}

		const long length = ftell(file);
		if (length <= 0 || fseek(file, 0, SEEK_SET)) {
			fclose(file);
			return {};
		}

		std::vector<uint8_t> data(static_cast<size_t>(length));
		const size_t actual = fread(data.data(), 1, data.size(), file);
		fclose(file);

		if (actual != data.size())
			return {};

		return data;
	}

	bool ATCheckVorbisPrimitives(ATPortableTestContext& context) {
		static constexpr char kCRCInput[] = "123456789";
		AT_PORTABLE_TEST_ASSERT(context,
			ATVorbisComputeCRC(kCRCInput, 4, kCRCInput + 4, 5)
				== UINT32_C(0x89A1897F));

		const std::array<float, 8> floatSamples {
			-2.0f, -1.0f, -0.5f, -0.25f,
			0.0f, 0.25f, 0.5f, 2.0f
		};
		std::array<sint16, 8> pcmSamples {};
		ATVorbisConvertF32ToS16(
			pcmSamples.data(), floatSamples.data(), floatSamples.size());
		const std::array<sint16, 8> expectedPCM {
			-32767, -32767, -16384, -8192,
			0, 8192, 16384, 32767
		};
		AT_PORTABLE_TEST_ASSERT(context, pcmSamples == expectedPCM);

		const std::array<float, 16> interleaved {
			0.0f, 10.0f, 1.0f, 11.0f,
			2.0f, 12.0f, 3.0f, 13.0f,
			4.0f, 14.0f, 5.0f, 15.0f,
			6.0f, 16.0f, 7.0f, 17.0f
		};
		std::array<float, 8> left {};
		std::array<float, 8> right {};
		float *channels[] { left.data(), right.data() };
		ATVorbisDeinterleaveResidue(
			channels, interleaved.data(), left.size(), std::size(channels));

		for(size_t i = 0; i < left.size(); ++i) {
			AT_PORTABLE_TEST_ASSERT(context, left[i] == static_cast<float>(i));
			AT_PORTABLE_TEST_ASSERT(context, right[i] == static_cast<float>(i + 10));
		}

		std::array<float, 8> magnitudes {
			2.0f, 2.0f, -2.0f, -2.0f,
			5.0f, 5.0f, -5.0f, -5.0f
		};
		std::array<float, 8> angles {
			1.0f, -1.0f, 1.0f, -1.0f,
			3.0f, -3.0f, 3.0f, -3.0f
		};
		ATVorbisDecoupleChannels(
			magnitudes.data(), angles.data(), magnitudes.size());
		const std::array<float, 8> expectedMagnitudes {
			2.0f, 1.0f, -2.0f, -1.0f,
			5.0f, 2.0f, -5.0f, -2.0f
		};
		const std::array<float, 8> expectedAngles {
			1.0f, 2.0f, -1.0f, -2.0f,
			2.0f, 5.0f, -2.0f, -5.0f
		};
		AT_PORTABLE_TEST_ASSERT(context, magnitudes == expectedMagnitudes);
		AT_PORTABLE_TEST_ASSERT(context, angles == expectedAngles);

		return true;
	}

	bool ATCheckVorbisDecoder(ATPortableTestContext& context) {
		const std::vector<uint8_t> data = ATLoadVorbisFixture();
		AT_PORTABLE_TEST_ASSERT(context, !data.empty());

		size_t offset = 0;
		ATVorbisDecoder decoder;
		decoder.Init(
			[&](void *dst, size_t length) -> size_t {
				const size_t actual = std::min(length, data.size() - offset);
				memcpy(dst, data.data() + offset, actual);
				offset += actual;
				return actual;
			}
		);
		decoder.ReadHeaders();

		AT_PORTABLE_TEST_ASSERT(context, decoder.GetChannelCount() == 1);
		AT_PORTABLE_TEST_ASSERT(context, decoder.GetSampleRate() == 44100);

		std::array<sint16, 1024 * 2> samples {};
		uint64_t sampleCount = 0;
		uint64_t absoluteSum = 0;
		uint32_t nonZeroSamples = 0;
		uint32_t peak = 0;

		while(decoder.ReadAudioPacket()) {
			while(decoder.GetAvailableSamples()) {
				const uint32_t actual = decoder.ReadInterleavedSamplesStereoS16(
					samples.data(), static_cast<uint32_t>(samples.size() / 2));
				AT_PORTABLE_TEST_ASSERT(context, actual > 0);

				for(uint32_t i = 0; i < actual; ++i) {
					AT_PORTABLE_TEST_ASSERT(context, samples[i * 2] == samples[i * 2 + 1]);
					const sint16 sample = samples[i * 2];
					const uint32_t magnitude = sample < 0
						? static_cast<uint32_t>(-static_cast<sint32>(sample))
						: static_cast<uint32_t>(sample);
					absoluteSum += magnitude;
					if (magnitude) {
						++nonZeroSamples;
						peak = std::max(peak, magnitude);
					}
				}

				sampleCount += actual;
			}
		}

		if (sampleCount != 33472) {
			fprintf(
				stderr, "Vorbis sample count mismatch: actual=%llu expected=33472\n",
				static_cast<unsigned long long>(sampleCount));
		}
		AT_PORTABLE_TEST_ASSERT(context, sampleCount == 33472);
		AT_PORTABLE_TEST_ASSERT(context, decoder.GetSampleCount() == sampleCount);
		AT_PORTABLE_TEST_ASSERT(context, nonZeroSamples > 20000);
		AT_PORTABLE_TEST_ASSERT(context, peak > 1000);
		AT_PORTABLE_TEST_ASSERT(context, absoluteSum > UINT64_C(10000000));

		return true;
	}
}

bool ATTestIOVorbis(ATPortableTestContext& context) {
	if (!ATCheckVorbisPrimitives(context))
		return false;

	return ATCheckVorbisDecoder(context);
}
