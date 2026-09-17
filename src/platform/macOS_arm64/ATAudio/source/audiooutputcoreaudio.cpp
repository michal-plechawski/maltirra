// Altirra macOS native Core Audio output

#include <stdafx.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <thread>

#include <AudioUnit/AudioUnit.h>
#include <CoreAudio/CoreAudio.h>

#include <at/ataudio/internal/audiooutputbackend.h>
#include <at/ataudio/internal/audioringbuffer.h>

namespace {
	class ATAudioOutputCoreAudio final : public IVDAudioOutput {
	public:
		~ATAudioOutputCoreAudio() override;

		uint32 GetPreferredSamplingRate(
			const wchar_t *preferredDevice) const override;
		bool Init(
			uint32 bufferSize, uint32 bufferCount,
			const ATAudioNativeFormat& format,
			const wchar_t *preferredDevice) override;
		void Shutdown() override;
		void GoSilent() override;

		bool IsSilent() override;
		bool IsFrozen() override;
		uint32 GetAvailSpace() override;
		uint32 GetBufferLevel() override;
		uint32 EstimateHWBufferLevel(bool *underflowDetected) override;
		sint32 GetPosition() override;
		sint32 GetPositionBytes() override;
		double GetPositionTime() override;
		uint32 GetMixingRate() const override;

		bool Start() override;
		bool Stop() override;
		bool Flush() override;
		bool Write(const void *data, uint32 len) override;
		bool Finalize(uint32 timeout) override;

	private:
		static OSStatus RenderCallback(
			void *context,
			AudioUnitRenderActionFlags *actionFlags,
			const AudioTimeStamp *timestamp,
			UInt32 busNumber,
			UInt32 frameCount,
			AudioBufferList *data);
		OSStatus Render(UInt32 frameCount, AudioBufferList *data);

		AudioComponentInstance mpAudioUnit = nullptr;
		ATAudioRingBuffer mRingBuffer;
		uint32 mBlockAlign = 0;
		uint32 mSamplingRate = 0;
		std::atomic<uint64> mBytesPlayed { 0 };
		std::atomic<bool> mbUnderflowDetected { false };
		bool mbStarted = false;
		bool mbSilent = false;
	};

	ATAudioOutputCoreAudio::~ATAudioOutputCoreAudio() {
		Shutdown();
	}

	uint32 ATAudioOutputCoreAudio::GetPreferredSamplingRate(
		const wchar_t *) const {
		AudioDeviceID device = kAudioObjectUnknown;
		UInt32 size = sizeof device;
		AudioObjectPropertyAddress address {
			kAudioHardwarePropertyDefaultOutputDevice,
			kAudioObjectPropertyScopeGlobal,
			kAudioObjectPropertyElementMain
		};

		if (AudioObjectGetPropertyData(
			kAudioObjectSystemObject, &address, 0, nullptr,
			&size, &device) != noErr || device == kAudioObjectUnknown) {
			return 0;
		}

		Float64 samplingRate = 0;
		size = sizeof samplingRate;
		address.mSelector = kAudioDevicePropertyNominalSampleRate;
		address.mScope = kAudioDevicePropertyScopeOutput;
		if (AudioObjectGetPropertyData(
			device, &address, 0, nullptr,
			&size, &samplingRate) != noErr || samplingRate < 1) {
			return 0;
		}

		return static_cast<uint32>(std::lround(samplingRate));
	}

	bool ATAudioOutputCoreAudio::Init(
		uint32 bufferSize, uint32 bufferCount,
		const ATAudioNativeFormat& format,
		const wchar_t *) {
		Shutdown();
		mbSilent = false;

		if (!format.IsValid() || !format.GetBlockAlign())
			return false;

		const uint64 capacity =
			static_cast<uint64>(bufferSize) * bufferCount;
		if (!capacity || capacity > ~uint32(0)
			|| !mRingBuffer.Init(
				static_cast<uint32>(capacity), format.GetBlockAlign())) {
			return false;
		}

		AudioComponentDescription description {};
		description.componentType = kAudioUnitType_Output;
		description.componentSubType = kAudioUnitSubType_DefaultOutput;
		description.componentManufacturer = kAudioUnitManufacturer_Apple;

		AudioComponent component = AudioComponentFindNext(nullptr, &description);
		if (!component
			|| AudioComponentInstanceNew(component, &mpAudioUnit) != noErr) {
			Shutdown();
			return false;
		}

		AudioStreamBasicDescription streamFormat {};
		streamFormat.mSampleRate = format.mSamplingRate;
		streamFormat.mFormatID = kAudioFormatLinearPCM;
		streamFormat.mFormatFlags =
			kAudioFormatFlagIsSignedInteger
			| kAudioFormatFlagIsPacked
			| kAudioFormatFlagsNativeEndian;
		streamFormat.mBytesPerPacket = format.GetBlockAlign();
		streamFormat.mFramesPerPacket = 1;
		streamFormat.mBytesPerFrame = format.GetBlockAlign();
		streamFormat.mChannelsPerFrame = format.mChannels;
		streamFormat.mBitsPerChannel = format.mBitsPerSample;

		if (AudioUnitSetProperty(
			mpAudioUnit,
			kAudioUnitProperty_StreamFormat,
			kAudioUnitScope_Input,
			0,
			&streamFormat,
			sizeof streamFormat) != noErr) {
			Shutdown();
			return false;
		}

		AURenderCallbackStruct callback {
			&ATAudioOutputCoreAudio::RenderCallback,
			this
		};
		if (AudioUnitSetProperty(
			mpAudioUnit,
			kAudioUnitProperty_SetRenderCallback,
			kAudioUnitScope_Input,
			0,
			&callback,
			sizeof callback) != noErr
			|| AudioUnitInitialize(mpAudioUnit) != noErr) {
			Shutdown();
			return false;
		}

		mBlockAlign = format.GetBlockAlign();
		mSamplingRate = format.mSamplingRate;
		mBytesPlayed = 0;
		mbUnderflowDetected = false;
		return true;
	}

	void ATAudioOutputCoreAudio::Shutdown() {
		Stop();

		if (mpAudioUnit) {
			AudioUnitUninitialize(mpAudioUnit);
			AudioComponentInstanceDispose(mpAudioUnit);
			mpAudioUnit = nullptr;
		}

		mRingBuffer.Clear();
		mBlockAlign = 0;
		mSamplingRate = 0;
	}

	void ATAudioOutputCoreAudio::GoSilent() {
		Shutdown();
		mbSilent = true;
	}

	bool ATAudioOutputCoreAudio::IsSilent() {
		return mbSilent;
	}

	bool ATAudioOutputCoreAudio::IsFrozen() {
		return !mbStarted || !mRingBuffer.GetLevel();
	}

	uint32 ATAudioOutputCoreAudio::GetAvailSpace() {
		return mRingBuffer.GetSpace();
	}

	uint32 ATAudioOutputCoreAudio::GetBufferLevel() {
		return mRingBuffer.GetLevel();
	}

	uint32 ATAudioOutputCoreAudio::EstimateHWBufferLevel(
		bool *underflowDetected) {
		if (underflowDetected) {
			*underflowDetected =
				mbUnderflowDetected.exchange(false, std::memory_order_acq_rel);
		}

		return mRingBuffer.GetLevel();
	}

	sint32 ATAudioOutputCoreAudio::GetPosition() {
		return static_cast<sint32>(GetPositionTime() * 1000.0);
	}

	sint32 ATAudioOutputCoreAudio::GetPositionBytes() {
		return static_cast<sint32>(mBytesPlayed.load(std::memory_order_acquire));
	}

	double ATAudioOutputCoreAudio::GetPositionTime() {
		const uint32 bytesPerSecond = mSamplingRate * mBlockAlign;
		return bytesPerSecond
			? static_cast<double>(
				mBytesPlayed.load(std::memory_order_acquire)) / bytesPerSecond
			: 0.0;
	}

	uint32 ATAudioOutputCoreAudio::GetMixingRate() const {
		return mSamplingRate;
	}

	bool ATAudioOutputCoreAudio::Start() {
		if (mbSilent)
			return true;

		if (!mpAudioUnit)
			return false;

		if (!mbStarted) {
			if (AudioOutputUnitStart(mpAudioUnit) != noErr)
				return false;

			mbStarted = true;
		}

		return true;
	}

	bool ATAudioOutputCoreAudio::Stop() {
		if (mbStarted) {
			AudioOutputUnitStop(mpAudioUnit);
			mbStarted = false;
		}

		return true;
	}

	bool ATAudioOutputCoreAudio::Flush() {
		return true;
	}

	bool ATAudioOutputCoreAudio::Write(const void *data, uint32 len) {
		if (mbSilent)
			return true;

		if (!mBlockAlign || len % mBlockAlign)
			return false;

		const uint8 *source = static_cast<const uint8 *>(data);
		while(len) {
			const uint32 written = mRingBuffer.Write(source, len);
			if (!written) {
				if (!mbStarted)
					return false;

				std::this_thread::sleep_for(std::chrono::milliseconds(1));
				continue;
			}

			if (source)
				source += written;
			len -= written;
		}

		return true;
	}

	bool ATAudioOutputCoreAudio::Finalize(uint32 timeout) {
		const auto start = std::chrono::steady_clock::now();
		while(mRingBuffer.GetLevel()) {
			if (timeout != ~uint32(0)) {
				const auto elapsed = std::chrono::duration_cast<
					std::chrono::milliseconds>(
						std::chrono::steady_clock::now() - start);
				if (elapsed.count() >= timeout)
					return false;
			}

			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}

		return true;
	}

	OSStatus ATAudioOutputCoreAudio::RenderCallback(
		void *context,
		AudioUnitRenderActionFlags *,
		const AudioTimeStamp *,
		UInt32,
		UInt32 frameCount,
		AudioBufferList *data) {
		return static_cast<ATAudioOutputCoreAudio *>(context)->Render(
			frameCount, data);
	}

	OSStatus ATAudioOutputCoreAudio::Render(
		UInt32 frameCount, AudioBufferList *data) {
		if (!data || !data->mNumberBuffers)
			return noErr;

		AudioBuffer& output = data->mBuffers[0];
		const uint32 requested = std::min<uint32>(
			output.mDataByteSize, frameCount * mBlockAlign);
		const uint32 actual = mRingBuffer.Read(output.mData, requested);
		if (actual < requested) {
			memset(static_cast<uint8 *>(output.mData) + actual,
				0, requested - actual);
			mbUnderflowDetected.store(true, std::memory_order_release);
		}

		mBytesPlayed.fetch_add(actual, std::memory_order_release);
		for(UInt32 i = 1; i < data->mNumberBuffers; ++i) {
			memset(data->mBuffers[i].mData, 0,
				data->mBuffers[i].mDataByteSize);
		}

		return noErr;
	}
}

uint32 ATGetNativeAudioApiCandidates(
	ATAudioApi, ATAudioApi *candidates, uint32 capacity) {
	if (!capacity)
		return 0;

	candidates[0] = kATAudioApi_Auto;
	return 1;
}

IVDAudioOutput *ATCreateNativeAudioOutput(ATAudioApi) {
	return new ATAudioOutputCoreAudio;
}
