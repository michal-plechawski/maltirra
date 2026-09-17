// Altirra portable audio filter tests

#include <array>

#include <vd2/system/vdtypes.h>
#include <at/ataudio/audiofilters.h>
#include <at/attest/portabletest.h>
#include <vd2/system/math.h>

bool ATTestAudioFilters(ATPortableTestContext& context) {
	constexpr uint32 sampleCount = 4;
	constexpr uint64 step = UINT64_C(1) << 32;

	alignas(16) const std::array<float, 16> source {
		-0.75f, -0.50f, -0.25f, 0.25f,
		-0.50f, 0.75f, -1.00f, 0.50f,
		0.125f, -0.125f, 0.625f, -0.625f,
		0.375f, -0.375f, 0.875f, -0.875f,
	};
	alignas(16) std::array<float, 16> rightSource {};
	for(size_t i = 0; i < source.size(); ++i)
		rightSource[i] = -source[i];

	std::array<sint16, sampleCount> mono {};
	const uint64 monoPosition = ATFilterResampleMono16(
		mono.data(), source.data(), sampleCount, 0, step, false);
	AT_PORTABLE_TEST_ASSERT(context, monoPosition == sampleCount * step);
	for(uint32 i = 0; i < sampleCount; ++i)
		AT_PORTABLE_TEST_ASSERT(context, mono[i] == VDClampedRoundFixedToInt16Fast(source[i + 3]));

	std::array<sint16, sampleCount * 2> doubled {};
	const uint64 doubledPosition = ATFilterResampleMonoToStereo16(
		doubled.data(), source.data(), sampleCount, 0, step, false);
	AT_PORTABLE_TEST_ASSERT(context, doubledPosition == sampleCount * step);
	for(uint32 i = 0; i < sampleCount; ++i) {
		AT_PORTABLE_TEST_ASSERT(context, doubled[i * 2] == mono[i]);
		AT_PORTABLE_TEST_ASSERT(context, doubled[i * 2 + 1] == mono[i]);
	}

	std::array<sint16, sampleCount * 2> stereo {};
	const uint64 stereoPosition = ATFilterResampleStereo16(
		stereo.data(), source.data(), rightSource.data(), sampleCount, 0, step, false);
	AT_PORTABLE_TEST_ASSERT(context, stereoPosition == sampleCount * step);
	for(uint32 i = 0; i < sampleCount; ++i) {
		AT_PORTABLE_TEST_ASSERT(context, stereo[i * 2] == mono[i]);
		AT_PORTABLE_TEST_ASSERT(context,
			stereo[i * 2 + 1] == VDClampedRoundFixedToInt16Fast(rightSource[i + 3]));
	}

	alignas(16) const std::array<float, 16> silence {};
	const std::array<float, sampleCount> auxLeft { 0.25f, -0.25f, 0.5f, -0.5f };
	const std::array<float, sampleCount> auxRight { -0.5f, 0.5f, -0.25f, 0.25f };
	std::array<sint16, sampleCount * 2> mixed {};
	ATFilterResampleMonoToStereoAdd16(
		mixed.data(),
		silence.data(),
		auxLeft.data(),
		auxRight.data(),
		sampleCount,
		0,
		step,
		false);
	for(uint32 i = 0; i < sampleCount; ++i) {
		AT_PORTABLE_TEST_ASSERT(context,
			mixed[i * 2] == VDClampedRoundFixedToInt16Fast(auxLeft[i]));
		AT_PORTABLE_TEST_ASSERT(context,
			mixed[i * 2 + 1] == VDClampedRoundFixedToInt16Fast(auxRight[i]));
	}

	ATAudioFilter filter;
	filter.SetScale(0.5f);
	AT_PORTABLE_TEST_ASSERT(context, filter.GetScale() == 0.5f);

	float differences[] { 1.0f, 3.0f, 6.0f, 10.0f };
	filter.PreFilterDiff(differences, 3);
	AT_PORTABLE_TEST_ASSERT(context, differences[0] == 1.0f);
	AT_PORTABLE_TEST_ASSERT(context, differences[1] == 2.0f);
	AT_PORTABLE_TEST_ASSERT(context, differences[2] == 3.0f);
	filter.PreFilterDiff(differences + 3, 1);
	AT_PORTABLE_TEST_ASSERT(context, differences[3] == 4.0f);

	return true;
}
