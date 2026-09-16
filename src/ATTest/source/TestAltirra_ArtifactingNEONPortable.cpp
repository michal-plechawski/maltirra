// Portable reference tests for the ARM64 NEON artifacting kernels.

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <initializer_list>

#include <at/attest/portabletest.h>
#include <artifacting_neon.h>

#if defined(VD_CPU_ARM64)
namespace {
	constexpr size_t kPixelCount = 8;
	using Pixels = std::array<uint32, kPixelCount>;

	uint8 *AsBytes(Pixels& pixels) {
		return reinterpret_cast<uint8 *>(pixels.data());
	}

	const uint8 *AsBytes(const Pixels& pixels) {
		return reinterpret_cast<const uint8 *>(pixels.data());
	}

	uint32 FloatBits(float value) {
		uint32 bits;
		std::memcpy(&bits, &value, sizeof bits);
		return bits;
	}

	float BitsFloat(uint32 bits) {
		float value;
		std::memcpy(&value, &bits, sizeof value);
		return value;
	}

	bool BytesNear(const Pixels& actual, const Pixels& expected, int tolerance) {
		const uint8 *a = AsBytes(actual);
		const uint8 *e = AsBytes(expected);
		for(size_t i = 0; i < sizeof actual; ++i) {
			if (std::abs((int)a[i] - (int)e[i]) > tolerance)
				return false;
		}
		return true;
	}

	void ReferenceBlend(Pixels& dst, const Pixels& src) {
		uint8 *d = AsBytes(dst);
		const uint8 *s = AsBytes(src);
		for(size_t i = 0; i < sizeof dst; ++i)
			d[i] = (uint8)(((uint32)d[i] + s[i] + 1) >> 1);
	}

	uint8 ReferenceLinearByte(uint8 dst, uint8 src, bool extendedRange) {
		uint32 x = dst;
		if (extendedRange)
			x = x > 0x40 ? x - 0x40 : 0;
		const uint32 meanSquare = (x * x + (uint32)src * src + 1) >> 1;
		uint32 result = (uint32)std::lround(std::sqrt((double)meanSquare));
		if (extendedRange)
			result = std::min<uint32>(result + 0x40, 0xff);
		return (uint8)result;
	}

	void ReferenceLinear(Pixels& dst, const Pixels& src, bool extendedRange) {
		uint8 *d = AsBytes(dst);
		const uint8 *s = AsBytes(src);
		for(size_t i = 0; i < sizeof dst; ++i)
			d[i] = ReferenceLinearByte(d[i], s[i], extendedRange);
	}

	bool TestAverageBlend(ATPortableTestContext& context) {
		const Pixels initialDst {
			0x10203040, 0x50607080, 0x90a0b0c0, 0xd0e0f000,
			0xff00aa55, 0x01030507, 0x2468ace0, 0xfedcba98
		};
		const Pixels initialSrc {
			0xf0e0d0c0, 0xb0a09080, 0x70605040, 0x30201000,
			0x00ff55aa, 0x08060402, 0x13579bdf, 0x01234567
		};

		auto expected = initialDst;
		ReferenceBlend(expected, initialSrc);
		auto actual = initialDst;
		ATArtifactBlend_NEON(actual.data(), initialSrc.data(), (uint32)actual.size());
		AT_PORTABLE_TEST_ASSERT(context, actual == expected);

		auto exchangedDst = initialDst;
		auto exchangedBlend = initialSrc;
		ATArtifactBlendExchange_NEON(exchangedDst.data(), exchangedBlend.data(), (uint32)exchangedDst.size());
		AT_PORTABLE_TEST_ASSERT(context, exchangedDst == expected);
		AT_PORTABLE_TEST_ASSERT(context, exchangedBlend == initialDst);
		return true;
	}

	bool TestLinearBlend(ATPortableTestContext& context) {
		const Pixels initialDst {
			0x00102030, 0x40506070, 0x8090a0b0, 0xc0d0e0f0,
			0xffc08040, 0x20100804, 0x7f3f1f0f, 0xfefdfcfb
		};
		const Pixels initialSrc {
			0xf0d0b090, 0x70503010, 0x20406080, 0xa0c0e0ff,
			0x10305070, 0x90b0d0f0, 0x01050911, 0x80402000
		};

		for(bool extendedRange : { false, true }) {
			auto expected = initialDst;
			ReferenceLinear(expected, initialSrc, extendedRange);
			auto actual = initialDst;
			ATArtifactBlendLinear_NEON(actual.data(), initialSrc.data(), (uint32)actual.size(), extendedRange);
			AT_PORTABLE_TEST_ASSERT(context, BytesNear(actual, expected, 2));

			auto exchangeDst = initialDst;
			auto exchangeState = initialSrc;
			ATArtifactBlendExchangeLinear_NEON(exchangeDst.data(), exchangeState.data(), (uint32)exchangeDst.size(), extendedRange);
			AT_PORTABLE_TEST_ASSERT(context, BytesNear(exchangeDst, expected, 2));

			auto expectedState = initialDst;
			if (extendedRange) {
				uint8 *bytes = AsBytes(expectedState);
				for(size_t i = 0; i < sizeof expectedState; ++i)
					bytes[i] = bytes[i] > 0x40 ? bytes[i] - 0x40 : 0;
			}
			AT_PORTABLE_TEST_ASSERT(context, exchangeState == expectedState);
		}

		return true;
	}

	uint32 ExpectedPersistenceIndex(float emission) {
		return (uint32)std::lround(std::min(std::sqrt(std::max(emission, 0.0f)) * 1023.0f, 1023.0f));
	}

	bool NearIndex(uint32 actual, uint32 expected) {
		return std::abs((int)actual - (int)expected) <= 8;
	}

	bool TestMonoPersistence(ATPortableTestContext& context) {
		std::array<uint32, 1024> palette {};
		for(uint32 i = 0; i < palette.size(); ++i)
			palette[i] = i;

		Pixels input { 0, 16, 48, 80, 112, 160, 208, 255 };
		Pixels copyState {};
		auto copied = input;
		ATArtifactBlendCopyMonoPersistence_NEON(copied.data(), copyState.data(), palette.data(), 0.2f, 0.1f, 1.5f, (uint32)copied.size());
		for(size_t i = 0; i < copied.size(); ++i) {
			const float intensity = (float)(input[i] & 0xff) / 255.0f;
			const float energy = intensity * intensity;
			AT_PORTABLE_TEST_ASSERT(context, NearIndex(copied[i], ExpectedPersistenceIndex(energy)));
			AT_PORTABLE_TEST_ASSERT(context, std::abs(BitsFloat(copyState[i]) - (energy + 1.0f)) < 1e-6f);
		}

		constexpr float factor = 0.2f;
		constexpr float factor2 = 0.1f;
		constexpr float limit = 1.5f;
		Pixels initialState {};
		for(size_t i = 0; i < initialState.size(); ++i)
			initialState[i] = FloatBits(1.1f + (float)i * 0.08f);

		auto constantState = initialState;
		auto blended = input;
		ATArtifactBlendMonoPersistence_NEON(blended.data(), constantState.data(), palette.data(), factor, factor2, limit, (uint32)blended.size());
		AT_PORTABLE_TEST_ASSERT(context, constantState == initialState);

		auto exchanged = input;
		auto exchangedState = initialState;
		ATArtifactBlendExchangeMonoPersistence_NEON(exchanged.data(), exchangedState.data(), palette.data(), factor, factor2, limit, (uint32)exchanged.size());
		AT_PORTABLE_TEST_ASSERT(context, exchanged == blended);

		for(size_t i = 0; i < blended.size(); ++i) {
			const float intensity = (float)(input[i] & 0xff) / 255.0f;
			const float oldEnergy = std::clamp(BitsFloat(initialState[i]) - 1.0f, 0.0f, limit);
			const float totalEnergy = oldEnergy + intensity * intensity;
			const float emission = (factor2 + factor * totalEnergy) * totalEnergy;
			AT_PORTABLE_TEST_ASSERT(context, NearIndex(blended[i], ExpectedPersistenceIndex(emission)));
			AT_PORTABLE_TEST_ASSERT(context,
				std::abs(BitsFloat(exchangedState[i]) - (totalEnergy - emission + 1.0f)) < 1e-5f);
		}

		return true;
	}

	bool TestScanlines(ATPortableTestContext& context) {
		const Pixels upper {
			0x00102030, 0x40506070, 0x8090a0b0, 0xc0d0e0f0,
			0xffeeddcc, 0xbbaa9988, 0x77665544, 0x33221100
		};
		const Pixels lower {
			0xf0e0d0c0, 0xb0a09080, 0x70605040, 0x30201000,
			0x00112233, 0x44556677, 0x8899aabb, 0xccddeeff
		};
		Pixels expected {};
		uint8 *e = AsBytes(expected);
		const uint8 *u = AsBytes(upper);
		const uint8 *l = AsBytes(lower);
		for(size_t i = 0; i < sizeof expected; ++i) {
			const uint32 average = ((uint32)u[i] + l[i] + 1) >> 1;
			e[i] = (uint8)((average * 64 + 64) >> 7);
		}

		Pixels actual {};
		ATArtifactBlendScanlines_NEON(actual.data(), upper.data(), lower.data(), (uint32)actual.size(), 0.5f);
		AT_PORTABLE_TEST_ASSERT(context, actual == expected);
		return true;
	}
}
#endif

bool ATTestAltirraArtifactingNEON(ATPortableTestContext& context) {
#if defined(VD_CPU_ARM64)
	return TestAverageBlend(context)
		&& TestLinearBlend(context)
		&& TestMonoPersistence(context)
		&& TestScanlines(context);
#else
	(void)context;
	return true;
#endif
}
