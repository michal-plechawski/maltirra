// Altirra portable binary primitive tests

#include <bit>
#include <vd2/system/binary.h>
#include <at/attest/portabletest.h>

bool ATTestSystemBinary(ATPortableTestContext& context) {
	AT_PORTABLE_TEST_ASSERT(context, VDSwizzleU16(UINT16_C(0x1234)) == UINT16_C(0x3412));
	AT_PORTABLE_TEST_ASSERT(context, VDSwizzleU32(UINT32_C(0x12345678)) == UINT32_C(0x78563412));
	AT_PORTABLE_TEST_ASSERT(context, VDSwizzleU64(UINT64_C(0x0123456789ABCDEF)) == UINT64_C(0xEFCDAB8967452301));

	AT_PORTABLE_TEST_ASSERT(context, VDRotateLeftU32(UINT32_C(0x12345678), 0) == UINT32_C(0x12345678));
	AT_PORTABLE_TEST_ASSERT(context, VDRotateLeftU32(UINT32_C(0x12345678), 8) == UINT32_C(0x34567812));
	AT_PORTABLE_TEST_ASSERT(context, VDRotateRightU32(UINT32_C(0x12345678), 8) == UINT32_C(0x78123456));
	AT_PORTABLE_TEST_ASSERT(context, VDRotateRightU32(UINT32_C(0x80000001), 1) == UINT32_C(0xC0000000));

	uint8 storage[32] {};
	void *unaligned = storage + 1;

	VDWriteUnalignedU16(unaligned, UINT16_C(0x1234));
	AT_PORTABLE_TEST_ASSERT(context, VDReadUnalignedU16(unaligned) == UINT16_C(0x1234));

	VDWriteUnalignedU32(unaligned, UINT32_C(0x12345678));
	AT_PORTABLE_TEST_ASSERT(context, VDReadUnalignedU32(unaligned) == UINT32_C(0x12345678));

	VDWriteUnalignedU64(unaligned, UINT64_C(0x0123456789ABCDEF));
	AT_PORTABLE_TEST_ASSERT(context, VDReadUnalignedU64(unaligned) == UINT64_C(0x0123456789ABCDEF));

	VDWriteUnalignedBEU16(unaligned, UINT16_C(0x1234));
	AT_PORTABLE_TEST_ASSERT(context, storage[1] == 0x12 && storage[2] == 0x34);
	AT_PORTABLE_TEST_ASSERT(context, VDReadUnalignedBEU16(unaligned) == UINT16_C(0x1234));

	VDWriteUnalignedBEU32(unaligned, UINT32_C(0x12345678));
	AT_PORTABLE_TEST_ASSERT(context, storage[1] == 0x12 && storage[2] == 0x34 && storage[3] == 0x56 && storage[4] == 0x78);
	AT_PORTABLE_TEST_ASSERT(context, VDReadUnalignedBEU32(unaligned) == UINT32_C(0x12345678));

	VDWriteUnalignedBEU64(unaligned, UINT64_C(0x0123456789ABCDEF));
	AT_PORTABLE_TEST_ASSERT(context, storage[1] == 0x01 && storage[2] == 0x23 && storage[3] == 0x45 && storage[4] == 0x67);
	AT_PORTABLE_TEST_ASSERT(context, storage[5] == 0x89 && storage[6] == 0xAB && storage[7] == 0xCD && storage[8] == 0xEF);
	AT_PORTABLE_TEST_ASSERT(context, VDReadUnalignedBEU64(unaligned) == UINT64_C(0x0123456789ABCDEF));

	constexpr float kFloatValue = -123.5f;
	VDWriteUnalignedBEF(unaligned, kFloatValue);
	AT_PORTABLE_TEST_ASSERT(context, VDReadUnalignedBEF(unaligned) == kFloatValue);
	AT_PORTABLE_TEST_ASSERT(context, VDReadUnalignedBEU32(unaligned) == std::bit_cast<uint32>(kFloatValue));

	constexpr double kDoubleValue = -123.5;
	VDWriteUnalignedBED(unaligned, kDoubleValue);
	AT_PORTABLE_TEST_ASSERT(context, VDReadUnalignedBED(unaligned) == kDoubleValue);
	AT_PORTABLE_TEST_ASSERT(context, VDReadUnalignedBEU64(unaligned) == std::bit_cast<uint64>(kDoubleValue));

	vdbe<uint32> beValue(UINT32_C(0x89ABCDEF));
	AT_PORTABLE_TEST_ASSERT(context, static_cast<uint32>(beValue) == UINT32_C(0x89ABCDEF));

	return true;
}
