// Altirra portable 4x pixmap resolve tests

#include <array>
#include <cstring>

#include <at/attest/portabletest.h>
#include <vd2/Kasumi/pixmap.h>
#include <vd2/Kasumi/region.h>

void VDPixmapResolve4x_Scalar(
	void *dst, ptrdiff_t dstPitch, const void *src, ptrdiff_t srcPitch,
	uint32 width, uint32 height);

#if VD_CPU_ARM64
void VDPixmapResolve4x_NEON(
	void *dst, ptrdiff_t dstPitch, const void *src, ptrdiff_t srcPitch,
	uint32 width, uint32 height);
#elif VD_CPU_X86 || VD_CPU_X64
void VDPixmapResolve4x_SSE2(
	void *dst, ptrdiff_t dstPitch, const void *src, ptrdiff_t srcPitch,
	uint32 width, uint32 height);
#endif

namespace {
	constexpr uint8 kGuard = 0xCD;
	constexpr uint32 kWidth = 7;
	constexpr uint32 kHeight = 3;
	constexpr size_t kSourcePrefix = 3;
	constexpr size_t kSourcePitch = kWidth * 16 + 11;
	constexpr size_t kSourceRows = kHeight * 4;
	constexpr size_t kSourceSuffix = 9;
	constexpr size_t kSourceSize =
		kSourcePrefix + kSourcePitch * kSourceRows + kSourceSuffix;
	constexpr size_t kDestinationPrefix = 4;
	constexpr size_t kDestinationPitch = kWidth * 4 + 12;
	constexpr size_t kDestinationSuffix = 9;
	constexpr size_t kDestinationSize =
		kDestinationPrefix + kDestinationPitch * kHeight + kDestinationSuffix;

	using SourceStorage = std::array<uint8, kSourceSize>;
	using DestinationStorage = std::array<uint8, kDestinationSize>;
	using ExpectedPixels = std::array<uint8, kWidth * kHeight * 4>;

	uint8 *ATSourcePixel(SourceStorage& storage, uint32 x, uint32 y) {
		return storage.data() + kSourcePrefix + y * kSourcePitch + x * 4;
	}

	const uint8 *ATSourcePixel(
		const SourceStorage& storage, uint32 x, uint32 y) {
		return storage.data() + kSourcePrefix + y * kSourcePitch + x * 4;
	}

	bool ATIsRmsHalfTie(uint32 sumSquares) {
		for(uint32 value = 0; value < 255; ++value) {
			if (sumSquares == 16 * value * value + 16 * value + 4)
				return true;
		}

		return false;
	}

	uint8 ATRoundRms(uint32 sumSquares) {
		uint32 value = 0;

		while(value < 255) {
			const uint32 threshold = 4 * value + 2;
			if (sumSquares < threshold * threshold)
				break;

			++value;
		}

		return static_cast<uint8>(value);
	}

	void ATAvoidRoundingTies(SourceStorage& storage) {
		for(uint32 y = 0; y < kHeight; ++y) {
			for(uint32 x = 0; x < kWidth; ++x) {
				for(uint32 channel = 0; channel < 3; ++channel) {
					uint32 sumSquares = 0;
					for(uint32 dy = 0; dy < 4; ++dy) {
						for(uint32 dx = 0; dx < 4; ++dx) {
							const uint8 value = ATSourcePixel(
								storage, x * 4 + dx, y * 4 + dy)[channel];
							sumSquares += value * value;
						}
					}

					if (ATIsRmsHalfTie(sumSquares)) {
						uint8& value = ATSourcePixel(
							storage, x * 4 + 3, y * 4 + 3)[channel];
						value += value == 255 ? -1 : 1;
					}
				}

				uint32 alphaSum = 0;
				for(uint32 dy = 0; dy < 4; ++dy) {
					for(uint32 dx = 0; dx < 4; ++dx)
						alphaSum += ATSourcePixel(
							storage, x * 4 + dx, y * 4 + dy)[3];
				}

				if ((alphaSum & 15) == 8) {
					uint8& value = ATSourcePixel(
						storage, x * 4 + 3, y * 4 + 3)[3];
					value += value == 255 ? -1 : 1;
				}
			}
		}
	}

	SourceStorage ATMakeSource() {
		SourceStorage storage;
		storage.fill(kGuard);

		uint32 randomState = 0xC001D00D;
		for(uint32 y = 0; y < kSourceRows; ++y) {
			for(uint32 x = 0; x < kWidth * 4; ++x) {
				uint8 *pixel = ATSourcePixel(storage, x, y);
				for(uint32 channel = 0; channel < 4; ++channel) {
					randomState = randomState * 1664525U + 1013904223U;
					pixel[channel] = static_cast<uint8>(randomState >> 24);
				}
			}
		}

		for(uint32 dy = 0; dy < 4; ++dy) {
			for(uint32 dx = 0; dx < 4; ++dx) {
				uint8 *black = ATSourcePixel(storage, dx, dy);
				black[0] = black[1] = black[2] = 0;
				black[3] = 16;

				uint8 *white = ATSourcePixel(storage, 4 + dx, dy);
				white[0] = white[1] = white[2] = 255;
				white[3] = 240;
			}
		}

		ATAvoidRoundingTies(storage);
		return storage;
	}

	ExpectedPixels ATResolveExpected(const SourceStorage& source) {
		ExpectedPixels expected {};

		for(uint32 y = 0; y < kHeight; ++y) {
			for(uint32 x = 0; x < kWidth; ++x) {
				uint32 sums[4] {};

				for(uint32 dy = 0; dy < 4; ++dy) {
					for(uint32 dx = 0; dx < 4; ++dx) {
						const uint8 *pixel = ATSourcePixel(
							source, x * 4 + dx, y * 4 + dy);
						for(uint32 channel = 0; channel < 3; ++channel)
							sums[channel] += pixel[channel] * pixel[channel];
						sums[3] += pixel[3];
					}
				}

				uint8 *pixel = expected.data() + (y * kWidth + x) * 4;
				for(uint32 channel = 0; channel < 3; ++channel)
					pixel[channel] = ATRoundRms(sums[channel]);
				pixel[3] = static_cast<uint8>((sums[3] + 8) / 16);
			}
		}

		return expected;
	}

	DestinationStorage ATMakeExpectedStorage(const ExpectedPixels& expected) {
		DestinationStorage storage;
		storage.fill(kGuard);

		for(uint32 y = 0; y < kHeight; ++y) {
			memcpy(
				storage.data() + kDestinationPrefix + y * kDestinationPitch,
				expected.data() + y * kWidth * 4,
				kWidth * 4);
		}

		return storage;
	}

	template<typename ResolveFn>
	DestinationStorage ATRunResolve(
		ResolveFn resolve, const SourceStorage& source) {
		DestinationStorage destination;
		destination.fill(kGuard);
		resolve(
			destination.data() + kDestinationPrefix, kDestinationPitch,
			source.data() + kSourcePrefix, kSourcePitch,
			kWidth, kHeight);
		return destination;
	}
}

bool ATTestKasumiResolve4x(ATPortableTestContext& context) {
	const SourceStorage source = ATMakeSource();
	const SourceStorage originalSource = source;
	const ExpectedPixels expectedPixels = ATResolveExpected(source);
	const DestinationStorage expectedStorage =
		ATMakeExpectedStorage(expectedPixels);

	const DestinationStorage scalarStorage =
		ATRunResolve(VDPixmapResolve4x_Scalar, source);
	AT_PORTABLE_TEST_ASSERT(context, scalarStorage == expectedStorage);

#if VD_CPU_ARM64
	const DestinationStorage acceleratedStorage =
		ATRunResolve(VDPixmapResolve4x_NEON, source);
	AT_PORTABLE_TEST_ASSERT(context, acceleratedStorage == expectedStorage);
#elif VD_CPU_X86 || VD_CPU_X64
	const DestinationStorage acceleratedStorage =
		ATRunResolve(VDPixmapResolve4x_SSE2, source);
	AT_PORTABLE_TEST_ASSERT(context, acceleratedStorage == expectedStorage);
#endif

	constexpr uint32 kPublicWidth = kWidth + 2;
	constexpr uint32 kPublicHeight = kHeight + 2;
	constexpr size_t kPublicPitch = kPublicWidth * 4 + 12;
	constexpr size_t kPublicSize =
		kDestinationPrefix + kPublicPitch * kPublicHeight + kDestinationSuffix;
	std::array<uint8, kPublicSize> publicStorage;
	std::array<uint8, kPublicSize> expectedPublicStorage;
	publicStorage.fill(kGuard);
	expectedPublicStorage.fill(kGuard);
	for(uint32 y = 0; y < kHeight; ++y) {
		memcpy(
			expectedPublicStorage.data() + kDestinationPrefix + y * kPublicPitch,
			expectedPixels.data() + y * kWidth * 4,
			kWidth * 4);
	}

	VDPixmap sourcePixmap {};
	sourcePixmap.data = const_cast<uint8 *>(source.data()) + kSourcePrefix;
	sourcePixmap.w = kWidth * 4;
	sourcePixmap.h = kHeight * 4;
	sourcePixmap.pitch = kSourcePitch;
	sourcePixmap.format = nsVDPixmap::kPixFormat_XRGB8888;

	VDPixmap destinationPixmap {};
	destinationPixmap.data = publicStorage.data() + kDestinationPrefix;
	destinationPixmap.w = kPublicWidth;
	destinationPixmap.h = kPublicHeight;
	destinationPixmap.pitch = kPublicPitch;
	destinationPixmap.format = nsVDPixmap::kPixFormat_XRGB8888;

	VDPixmapResolve4x(destinationPixmap, sourcePixmap);
	AT_PORTABLE_TEST_ASSERT(context, publicStorage == expectedPublicStorage);
	AT_PORTABLE_TEST_ASSERT(context, source == originalSource);

	std::array<uint8, 32> noOpStorage;
	noOpStorage.fill(kGuard);
	const auto originalNoOpStorage = noOpStorage;
	destinationPixmap.data = noOpStorage.data() + 4;
	destinationPixmap.w = 0;
	destinationPixmap.h = 1;
	destinationPixmap.pitch = 8;
	VDPixmapResolve4x(destinationPixmap, sourcePixmap);
	AT_PORTABLE_TEST_ASSERT(context, noOpStorage == originalNoOpStorage);

	return true;
}
