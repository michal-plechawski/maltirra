#include <vd2/system/binary.h>
#include <vd2/Tessa/Program.h>

bool VDTExtractMultiTargetProgram(VDTData srcdata, const uint32 *targets, VDTData& program) {
	if (!srcdata.mpData || !targets)
		return false;

	const uint8 *data8 = static_cast<const uint8 *>(srcdata.mpData);
	uint32 tableOffset = 0;

	for(;;) {
		if (srcdata.mLength - tableOffset < sizeof(uint32))
			return false;

		const uint32 targetId = VDReadUnalignedU32(data8 + tableOffset);

		if (!targetId)
			return false;

		if (srcdata.mLength - tableOffset < 12)
			return false;

		for(const uint32 *p = targets; *p; ++p) {
			if (*p == targetId) {
				const uint32 programOffset = VDReadUnalignedU32(data8 + tableOffset + 4);
				const uint32 programLength = VDReadUnalignedU32(data8 + tableOffset + 8);

				if (programOffset > srcdata.mLength || programLength > srcdata.mLength - programOffset)
					return false;

				program.mpData = data8 + programOffset;
				program.mLength = programLength;
				return true;
			}
		}

		tableOffset += 12;
	}
}
