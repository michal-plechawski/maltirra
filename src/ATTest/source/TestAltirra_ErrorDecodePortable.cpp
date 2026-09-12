// Altirra portable Atari error decoder tests

#include <array>
#include <cwchar>
#include <iterator>

#include <at/attest/portabletest.h>
#include <errordecode.h>

namespace {
	struct ExpectedError {
		uint8 mCode;
		const wchar_t *mpClass;
		const wchar_t *mpMessage;
	};

	constexpr std::array kExpectedErrors {
		ExpectedError {   2, L"Atari BASIC", L"Out of memory" },
		ExpectedError {   3, L"Atari BASIC", L"Value error" },
		ExpectedError {   4, L"Atari BASIC", L"Too many variables" },
		ExpectedError {   5, L"Atari BASIC", L"String length error" },
		ExpectedError {   6, L"Atari BASIC", L"Out of data" },
		ExpectedError {   7, L"Atari BASIC", L"Number &gt;32767" },
		ExpectedError {   8, L"Atari BASIC", L"Input statement error" },
		ExpectedError {   9, L"Atari BASIC", L"DIM error" },
		ExpectedError {  10, L"Atari BASIC", L"Argument stack overflow" },
		ExpectedError {  11, L"Atari BASIC", L"Floating point overflow/underflow" },
		ExpectedError {  12, L"Atari BASIC", L"Line not found" },
		ExpectedError {  13, L"Atari BASIC", L"No matching FOR statement" },
		ExpectedError {  14, L"Atari BASIC", L"Line too long" },
		ExpectedError {  15, L"Atari BASIC", L"GOSUB or FOR line deleted" },
		ExpectedError {  16, L"Atari BASIC", L"RETURN error" },
		ExpectedError {  17, L"Atari BASIC", L"Garbage error" },
		ExpectedError {  18, L"Atari BASIC", L"Invalid string character" },
		ExpectedError {  19, L"Atari BASIC", L"LOAD program too long" },
		ExpectedError {  20, L"Atari BASIC", L"Device number error" },
		ExpectedError {  21, L"Atari BASIC", L"LOAD file error" },
		ExpectedError { 128, L"CIO", L"User break abort" },
		ExpectedError { 129, L"CIO", L"IOCB in use" },
		ExpectedError { 130, L"CIO", L"Unknown device" },
		ExpectedError { 131, L"CIO", L"IOCB write only" },
		ExpectedError { 132, L"CIO", L"Invalid command" },
		ExpectedError { 133, L"CIO", L"IOCB not open" },
		ExpectedError { 134, L"CIO", L"Invalid IOCB" },
		ExpectedError { 135, L"CIO", L"IOCB read only" },
		ExpectedError { 136, L"CIO", L"End of file" },
		ExpectedError { 137, L"CIO", L"Truncated record" },
		ExpectedError { 138, L"CIO/SIO", L"Timeout" },
		ExpectedError { 139, L"CIO/SIO", L"Device NAK" },
		ExpectedError { 140, L"CIO/SIO", L"Bad frame" },
		ExpectedError { 142, L"CIO/SIO", L"Serial input overrun" },
		ExpectedError { 143, L"CIO/SIO", L"Checksum error" },
		ExpectedError { 144, L"CIO/SIO", L"Device error or write protected disk" },
		ExpectedError { 145, L"CIO", L"Bad screen mode" },
		ExpectedError { 146, L"CIO", L"Not supported" },
		ExpectedError { 147, L"CIO", L"Out of memory" },
		ExpectedError { 150, L"CIO", L"850: Port already open" },
		ExpectedError { 150, L"SDX", L"Path not found" },
		ExpectedError { 151, L"CIO", L"850: Concurrent mode I/O not enabled" },
		ExpectedError { 152, L"CIO", L"850: Illegal user-supplied buffer" },
		ExpectedError { 153, L"CIO", L"850: Active concurrent mode I/O" },
		ExpectedError { 154, L"CIO", L"850: Concurrent mode I/O not active" },
		ExpectedError { 160, L"DOS", L"Invalid drive number" },
		ExpectedError { 161, L"DOS", L"Too many open files" },
		ExpectedError { 162, L"DOS", L"Disk full" },
		ExpectedError { 163, L"DOS", L"Fatal disk I/O error" },
		ExpectedError { 164, L"DOS", L"File number mismatch" },
		ExpectedError { 165, L"DOS", L"File name error" },
		ExpectedError { 166, L"DOS", L"POINT data length error" },
		ExpectedError { 167, L"DOS", L"File locked" },
		ExpectedError { 168, L"DOS", L"Command invalid" },
		ExpectedError { 169, L"DOS", L"Directory full" },
		ExpectedError { 170, L"DOS", L"File not found" },
		ExpectedError { 171, L"DOS", L"Invalid POINT" },
		ExpectedError { 172, L"MyDOS", L"File/directory already exists" },
		ExpectedError { 173, L"DOS 3", L"Bad sectors at format time" },
		ExpectedError { 174, L"DOS 3", L"Duplicate filename" },
		ExpectedError { 175, L"DOS 3", L"Bad load file" },
		ExpectedError { 175, L"MyDOS", L"Directory not empty" },
		ExpectedError { 176, L"SDX", L"Access denied\n<b>DOS 3:</b> Incompatible format" },
		ExpectedError { 177, L"DOS 3", L"Disk structure damaged" },
		ExpectedError { 182, L"SDX", L"Path too long" },
		ExpectedError { 255, L"SDX", L"System error" },
	};
}

bool ATTestAltirraErrorDecode(ATPortableTestContext& context) {
	AT_PORTABLE_TEST_ASSERT(context, kExpectedErrors.size() == 66);

	size_t expectedIndex = 0;

	for(uint32 code = 0; code <= UINT8_MAX; ++code) {
		const size_t firstExpectedIndex = expectedIndex;

		while(expectedIndex < kExpectedErrors.size()
			&& kExpectedErrors[expectedIndex].mCode == code)
		{
			++expectedIndex;
		}

		const auto decoded = ATDecodeError(static_cast<uint8>(code));
		const size_t expectedCount = expectedIndex - firstExpectedIndex;

		AT_PORTABLE_TEST_ASSERT(context, decoded.size() == expectedCount);

		for(size_t i = 0; i < expectedCount; ++i) {
			const auto& actual = decoded[i];
			const auto& expected = kExpectedErrors[firstExpectedIndex + i];

			AT_PORTABLE_TEST_ASSERT(
				context,
				!wcscmp(actual.mpClass, expected.mpClass));
			AT_PORTABLE_TEST_ASSERT(
				context,
				!wcscmp(actual.mpMessage, expected.mpMessage));
		}
	}

	AT_PORTABLE_TEST_ASSERT(context, expectedIndex == kExpectedErrors.size());

	return true;
}
