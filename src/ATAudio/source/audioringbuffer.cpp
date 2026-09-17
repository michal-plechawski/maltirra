// Altirra portable single-producer/single-consumer audio ring buffer

#include <stdafx.h>
#include <at/ataudio/internal/audioringbuffer.h>

bool ATAudioRingBuffer::Init(uint32 capacity, uint32 blockAlign) {
	if (!blockAlign)
		return false;

	capacity -= capacity % blockAlign;
	if (!capacity)
		return false;

	mBuffer.resize(capacity);
	mCapacity = capacity;
	mBlockAlign = blockAlign;
	Clear();
	return true;
}

void ATAudioRingBuffer::Clear() {
	mReadOffset = 0;
	mWriteOffset = 0;
	mLevel.store(0, std::memory_order_release);
}

uint32 ATAudioRingBuffer::GetLevel() const {
	return mLevel.load(std::memory_order_acquire);
}

uint32 ATAudioRingBuffer::GetSpace() const {
	return mCapacity - GetLevel();
}

uint32 ATAudioRingBuffer::Write(const void *source, uint32 bytes) {
	bytes = std::min(bytes, GetSpace());
	bytes -= bytes % mBlockAlign;
	if (!bytes)
		return 0;

	const uint32 firstSize = std::min(bytes, mCapacity - mWriteOffset);
	if (source) {
		memcpy(mBuffer.data() + mWriteOffset, source, firstSize);
		memcpy(mBuffer.data(),
			static_cast<const uint8 *>(source) + firstSize,
			bytes - firstSize);
	} else {
		memset(mBuffer.data() + mWriteOffset, 0, firstSize);
		memset(mBuffer.data(), 0, bytes - firstSize);
	}

	mWriteOffset += bytes;
	if (mWriteOffset >= mCapacity)
		mWriteOffset -= mCapacity;

	mLevel.fetch_add(bytes, std::memory_order_release);
	return bytes;
}

uint32 ATAudioRingBuffer::Read(void *destination, uint32 bytes) {
	bytes = std::min(bytes, GetLevel());
	bytes -= bytes % mBlockAlign;
	if (!bytes)
		return 0;

	const uint32 firstSize = std::min(bytes, mCapacity - mReadOffset);
	memcpy(destination, mBuffer.data() + mReadOffset, firstSize);
	memcpy(static_cast<uint8 *>(destination) + firstSize,
		mBuffer.data(), bytes - firstSize);

	mReadOffset += bytes;
	if (mReadOffset >= mCapacity)
		mReadOffset -= mCapacity;

	mLevel.fetch_sub(bytes, std::memory_order_release);
	return bytes;
}
