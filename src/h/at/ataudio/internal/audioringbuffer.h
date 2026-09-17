// Altirra portable single-producer/single-consumer audio ring buffer

#ifndef f_AT_ATAUDIO_INTERNAL_AUDIORINGBUFFER_H
#define f_AT_ATAUDIO_INTERNAL_AUDIORINGBUFFER_H

#include <atomic>
#include <vector>
#include <vd2/system/vdtypes.h>

class ATAudioRingBuffer {
public:
	bool Init(uint32 capacity, uint32 blockAlign);
	void Clear();

	uint32 GetCapacity() const { return mCapacity; }
	uint32 GetLevel() const;
	uint32 GetSpace() const;

	// Write and Read always transfer whole audio frames. Passing nullptr to
	// Write inserts silence. Exactly one producer and one consumer may call
	// these methods concurrently.
	uint32 Write(const void *source, uint32 bytes);
	uint32 Read(void *destination, uint32 bytes);

private:
	std::vector<uint8> mBuffer;
	uint32 mCapacity = 0;
	uint32 mBlockAlign = 0;
	uint32 mReadOffset = 0;
	uint32 mWriteOffset = 0;
	std::atomic<uint32> mLevel { 0 };
};

#endif
