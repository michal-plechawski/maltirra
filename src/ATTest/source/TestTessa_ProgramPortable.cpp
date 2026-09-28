// Portable Tessa multi-target program tests.

#include <algorithm>
#include <array>

#include <at/attest/portabletest.h>
#include <vd2/system/binary.h>
#include <vd2/Tessa/Program.h>

namespace {
	void WriteProgramRecord(uint8 *dst, uint32 target, uint32 offset, uint32 length) {
		VDWriteUnalignedU32(dst, target);
		VDWriteUnalignedU32(dst + 4, offset);
		VDWriteUnalignedU32(dst + 8, length);
	}
}

bool ATTestTessaProgram(ATPortableTestContext& context) {
	constexpr uint32 kTarget1 = 0x11111111;
	constexpr uint32 kTarget2 = 0x22222222;
	const uint32 targets[] { 0x33333333, kTarget2, 0 };

	std::array<uint8, 64> blob {};
	WriteProgramRecord(blob.data(), kTarget1, 40, 4);
	WriteProgramRecord(blob.data() + 12, kTarget2, 44, 6);
	VDWriteUnalignedU32(blob.data() + 24, 0);

	const std::array<uint8, 6> expectedProgram {{ 1, 2, 3, 5, 8, 13 }};
	std::copy(expectedProgram.begin(), expectedProgram.end(), blob.begin() + 44);

	VDTData program {};
	AT_PORTABLE_TEST_ASSERT(context,
		VDTExtractMultiTargetProgram({ blob.data(), static_cast<uint32>(blob.size()) }, targets, program));
	AT_PORTABLE_TEST_ASSERT(context, program.mpData == blob.data() + 44);
	AT_PORTABLE_TEST_ASSERT(context, program.mLength == expectedProgram.size());

	const uint32 missingTargets[] { 0x44444444, 0 };
	VDTData unchanged { blob.data() + 1, 7 };
	AT_PORTABLE_TEST_ASSERT(context,
		!VDTExtractMultiTargetProgram({ blob.data(), static_cast<uint32>(blob.size()) }, missingTargets, unchanged));
	AT_PORTABLE_TEST_ASSERT(context, unchanged.mpData == blob.data() + 1);
	AT_PORTABLE_TEST_ASSERT(context, unchanged.mLength == 7);

	AT_PORTABLE_TEST_ASSERT(context,
		!VDTExtractMultiTargetProgram({ blob.data(), 3 }, targets, program));
	AT_PORTABLE_TEST_ASSERT(context,
		!VDTExtractMultiTargetProgram({ blob.data(), 20 }, targets, program));

	WriteProgramRecord(blob.data(), kTarget2, 60, 8);
	AT_PORTABLE_TEST_ASSERT(context,
		!VDTExtractMultiTargetProgram({ blob.data(), static_cast<uint32>(blob.size()) }, targets, program));

	WriteProgramRecord(blob.data(), kTarget2, 65, 0);
	AT_PORTABLE_TEST_ASSERT(context,
		!VDTExtractMultiTargetProgram({ blob.data(), static_cast<uint32>(blob.size()) }, targets, program));

	AT_PORTABLE_TEST_ASSERT(context,
		!VDTExtractMultiTargetProgram({ nullptr, 0 }, targets, program));
	AT_PORTABLE_TEST_ASSERT(context,
		!VDTExtractMultiTargetProgram({ blob.data(), static_cast<uint32>(blob.size()) }, nullptr, program));
	return true;
}
