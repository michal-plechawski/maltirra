// Altirra portable native audio output contract tests

#include <at/attest/portabletest.h>
#include <at/ataudio/audioout.h>

namespace {
	class ATTestNativeAudioOutput final : public IVDAudioOutput {
	public:
		uint32 GetPreferredSamplingRate(const wchar_t *) const override {
			return 48000;
		}

		bool Init(
			uint32 bufferSize, uint32 bufferCount,
			const ATAudioNativeFormat& format,
			const wchar_t *) override {
			mBufferSize = bufferSize;
			mBufferCount = bufferCount;
			mFormat = format;
			return format.IsValid();
		}

		void Shutdown() override {}
		void GoSilent() override { mbSilent = true; }
		bool IsSilent() override { return mbSilent; }
		bool IsFrozen() override { return false; }
		uint32 GetAvailSpace() override { return mBufferSize; }
		uint32 GetBufferLevel() override { return 0; }
		uint32 EstimateHWBufferLevel(bool *underflowDetected) override {
			if (underflowDetected)
				*underflowDetected = false;
			return 0;
		}
		sint32 GetPosition() override { return 0; }
		sint32 GetPositionBytes() override { return 0; }
		double GetPositionTime() override { return 0; }
		uint32 GetMixingRate() const override { return mFormat.mSamplingRate; }
		bool Start() override { return true; }
		bool Stop() override { return true; }
		bool Flush() override { return true; }
		bool Write(const void *, uint32) override { return true; }
		bool Finalize(uint32) override { return true; }

		uint32 mBufferSize = 0;
		uint32 mBufferCount = 0;
		ATAudioNativeFormat mFormat {};
		bool mbSilent = false;
	};
}

bool ATTestAudioNativeOutput(ATPortableTestContext& context) {
	constexpr ATAudioNativeFormat stereo16 { 48000, 2, 16 };
	static_assert(stereo16.IsValid());
	static_assert(stereo16.GetBytesPerSample() == 2);
	static_assert(stereo16.GetBlockAlign() == 4);
	static_assert(stereo16.GetBytesPerSecond() == 192000);

	constexpr ATAudioNativeFormat invalidBits { 48000, 2, 12 };
	static_assert(!invalidBits.IsValid());

	ATTestNativeAudioOutput output;
	IVDAudioOutput& interfaceRef = output;
	AT_PORTABLE_TEST_ASSERT(context,
		interfaceRef.Init(6144, 30, stereo16, nullptr));
	AT_PORTABLE_TEST_ASSERT(context, output.mBufferSize == 6144);
	AT_PORTABLE_TEST_ASSERT(context, output.mBufferCount == 30);
	AT_PORTABLE_TEST_ASSERT(context, interfaceRef.GetMixingRate() == 48000);
	AT_PORTABLE_TEST_ASSERT(context, !interfaceRef.IsSilent());
	interfaceRef.GoSilent();
	AT_PORTABLE_TEST_ASSERT(context, interfaceRef.IsSilent());

	return true;
}
