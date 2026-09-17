// Altirra native audio output interface

#ifndef f_AT_ATAUDIO_AUDIOOUT_H
#define f_AT_ATAUDIO_AUDIOOUT_H

#include <vd2/system/vdtypes.h>

struct ATAudioNativeFormat {
	uint32 mSamplingRate = 0;
	uint32 mChannels = 0;
	uint32 mBitsPerSample = 0;

	constexpr uint32 GetBytesPerSample() const {
		return mBitsPerSample / 8;
	}

	constexpr uint32 GetBlockAlign() const {
		return mChannels * GetBytesPerSample();
	}

	constexpr uint32 GetBytesPerSecond() const {
		return mSamplingRate * GetBlockAlign();
	}

	constexpr bool IsValid() const {
		return mSamplingRate != 0
			&& mChannels != 0
			&& mBitsPerSample != 0
			&& !(mBitsPerSample & 7);
	}
};

class IVDAudioOutput {
public:
	virtual ~IVDAudioOutput() = default;

	virtual uint32 GetPreferredSamplingRate(
		const wchar_t *preferredDevice) const = 0;

	virtual bool Init(
		uint32 bufferSize, uint32 bufferCount,
		const ATAudioNativeFormat& format,
		const wchar_t *preferredDevice) = 0;
	virtual void Shutdown() = 0;
	virtual void GoSilent() = 0;

	virtual bool IsSilent() = 0;
	virtual bool IsFrozen() = 0;
	virtual uint32 GetAvailSpace() = 0;
	virtual uint32 GetBufferLevel() = 0;
	virtual uint32 EstimateHWBufferLevel(bool *underflowDetected) = 0;
	virtual sint32 GetPosition() = 0;
	virtual sint32 GetPositionBytes() = 0;
	virtual double GetPositionTime() = 0;

	// Returns the mixing rate in Hz. This is the rate at which audio must
	// be produced. It may differ from the requested rate.
	virtual uint32 GetMixingRate() const = 0;

	virtual bool Start() = 0;
	virtual bool Stop() = 0;
	virtual bool Flush() = 0;

	virtual bool Write(const void *data, uint32 len) = 0;
	virtual bool Finalize(uint32 timeout = ~uint32(0)) = 0;
};

#ifdef _WIN32
IVDAudioOutput *VDCreateAudioOutputWaveOutW32();
IVDAudioOutput *VDCreateAudioOutputDirectSoundW32();
IVDAudioOutput *VDCreateAudioOutputXAudio2W32();
IVDAudioOutput *VDCreateAudioOutputWASAPIW32();
#endif

#endif
