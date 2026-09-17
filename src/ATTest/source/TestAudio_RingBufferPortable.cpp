// Altirra portable audio ring buffer tests

#include <array>
#include <atomic>
#include <thread>

#include <at/attest/portabletest.h>
#include <at/ataudio/internal/audioringbuffer.h>

namespace {
	bool ATCheckAudioRingBufferWrap(ATPortableTestContext& context) {
		ATAudioRingBuffer buffer;
		AT_PORTABLE_TEST_ASSERT(context, !buffer.Init(8, 0));
		AT_PORTABLE_TEST_ASSERT(context, buffer.Init(10, 4));
		AT_PORTABLE_TEST_ASSERT(context, buffer.GetCapacity() == 8);

		const std::array<uint8, 12> input {
			0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11
		};
		AT_PORTABLE_TEST_ASSERT(context,
			buffer.Write(input.data(), 6) == 4);
		AT_PORTABLE_TEST_ASSERT(context, buffer.GetLevel() == 4);

		std::array<uint8, 12> output {};
		AT_PORTABLE_TEST_ASSERT(context,
			buffer.Read(output.data(), 2) == 0);
		AT_PORTABLE_TEST_ASSERT(context,
			buffer.Read(output.data(), 4) == 4);
		AT_PORTABLE_TEST_ASSERT(context, output[0] == 0 && output[3] == 3);

		AT_PORTABLE_TEST_ASSERT(context,
			buffer.Write(input.data() + 4, 8) == 8);
		AT_PORTABLE_TEST_ASSERT(context,
			buffer.Read(output.data(), 4) == 4);
		AT_PORTABLE_TEST_ASSERT(context,
			buffer.Write(input.data(), 8) == 4);
		AT_PORTABLE_TEST_ASSERT(context,
			buffer.Read(output.data(), 8) == 8);
		for(uint32 i = 0; i < 4; ++i) {
			AT_PORTABLE_TEST_ASSERT(context, output[i] == i + 8);
			AT_PORTABLE_TEST_ASSERT(context, output[i + 4] == i);
		}

		AT_PORTABLE_TEST_ASSERT(context,
			buffer.Write(nullptr, 8) == 8);
		output.fill(0xFF);
		AT_PORTABLE_TEST_ASSERT(context,
			buffer.Read(output.data(), 8) == 8);
		for(uint32 i = 0; i < 8; ++i)
			AT_PORTABLE_TEST_ASSERT(context, output[i] == 0);

		return true;
	}

	bool ATCheckAudioRingBufferSPSC(ATPortableTestContext& context) {
		constexpr uint32 kTransferSize = 1 << 18;
		ATAudioRingBuffer buffer;
		AT_PORTABLE_TEST_ASSERT(context, buffer.Init(1024, 1));

		std::atomic<bool> valid { true };
		std::thread consumer([&] {
			std::array<uint8, 127> data {};
			uint32 position = 0;

			while(position < kTransferSize) {
				const uint32 request = std::min<uint32>(
					data.size(), kTransferSize - position);
				const uint32 actual = buffer.Read(data.data(), request);
				if (!actual) {
					std::this_thread::yield();
					continue;
				}

				for(uint32 i = 0; i < actual; ++i) {
					if (data[i] != static_cast<uint8>(position + i))
						valid = false;
				}

				position += actual;
			}
		});

		std::array<uint8, 113> data {};
		uint32 position = 0;
		while(position < kTransferSize) {
			const uint32 request = std::min<uint32>(
				data.size(), kTransferSize - position);
			for(uint32 i = 0; i < request; ++i)
				data[i] = static_cast<uint8>(position + i);

			const uint32 actual = buffer.Write(data.data(), request);
			if (!actual) {
				std::this_thread::yield();
				continue;
			}

			position += actual;
		}

		consumer.join();
		AT_PORTABLE_TEST_ASSERT(context, valid.load());
		AT_PORTABLE_TEST_ASSERT(context, buffer.GetLevel() == 0);
		return true;
	}
}

bool ATTestAudioRingBuffer(ATPortableTestContext& context) {
	return ATCheckAudioRingBufferWrap(context)
		&& ATCheckAudioRingBufferSPSC(context);
}
