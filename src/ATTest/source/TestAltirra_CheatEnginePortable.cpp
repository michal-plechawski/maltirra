// Altirra portable cheat engine tests

#include <array>
#include <vector>

#include <at/attest/portabletest.h>
#include <cheatengine.h>
#include <vd2/system/filesys.h>
#include <vd2/system/thread.h>
#include <vd2/system/time.h>

namespace {
	std::vector<uint32> GetValidOffsets(const ATCheatEngine& engine) {
		const uint32 count = engine.GetValidOffsets(nullptr, 0);
		std::vector<uint32> offsets(count);
		if (count)
			engine.GetValidOffsets(offsets.data(), count);
		return offsets;
	}

	std::vector<uint32> RunSnapshot(
		ATCheatSnapshotMode mode,
		const std::vector<uint8>& before,
		const std::vector<uint8>& after,
		bool bit16,
		uint32 referenceValue = 0) {
		std::vector<uint8> memory = before;
		ATCheatEngine engine;
		engine.Init(memory.data(), (uint32)memory.size());
		engine.Snapshot(kATCheatSnapMode_Replace, 0, bit16);
		memory = after;
		engine.Snapshot(mode, referenceValue, bit16);
		return GetValidOffsets(engine);
	}

	class ATCheatFileSandbox {
	public:
		ATCheatFileSandbox() {
			static uint32 sequence = 0;
			VDStringW name;
			name.sprintf(
				L"altirra-cheat-test-%u-%llu-%u.a8c",
				static_cast<unsigned>(VDGetCurrentProcessId()),
				static_cast<unsigned long long>(VDGetCurrentTick64()),
				static_cast<unsigned>(++sequence));
			mPath = VDGetFullPath(name.c_str());
		}

		~ATCheatFileSandbox() {
			VDRemoveFile(mPath.c_str());
		}

		VDStringW mPath;
	};
}

bool ATTestAltirraCheatEngine(ATPortableTestContext& context) {
	const std::vector<uint8> before8 { 1, 2, 3, 4, 5 };
	const std::vector<uint8> after8 { 1, 1, 4, 4, 5 };
	AT_PORTABLE_TEST_ASSERT(
		context,
		RunSnapshot(kATCheatSnapMode_Equal, before8, after8, false) ==
			(std::vector<uint32> { 0, 3, 4 }));
	AT_PORTABLE_TEST_ASSERT(
		context,
		RunSnapshot(kATCheatSnapMode_NotEqual, before8, after8, false) ==
			(std::vector<uint32> { 1, 2 }));
	AT_PORTABLE_TEST_ASSERT(
		context,
		RunSnapshot(kATCheatSnapMode_Less, before8, after8, false) ==
			(std::vector<uint32> { 1 }));
	AT_PORTABLE_TEST_ASSERT(
		context,
		RunSnapshot(kATCheatSnapMode_LessEqual, before8, after8, false) ==
			(std::vector<uint32> { 0, 1, 3, 4 }));
	AT_PORTABLE_TEST_ASSERT(
		context,
		RunSnapshot(kATCheatSnapMode_Greater, before8, after8, false) ==
			(std::vector<uint32> { 2 }));
	AT_PORTABLE_TEST_ASSERT(
		context,
		RunSnapshot(kATCheatSnapMode_GreaterEqual, before8, after8, false) ==
			(std::vector<uint32> { 0, 2, 3, 4 }));
	AT_PORTABLE_TEST_ASSERT(
		context,
		RunSnapshot(kATCheatSnapMode_EqualRef, before8, after8, false, 4) ==
			(std::vector<uint32> { 2, 3 }));

	const std::vector<uint8> before16 { 1, 0, 5, 0 };
	const std::vector<uint8> after16 { 2, 0, 4, 0 };
	AT_PORTABLE_TEST_ASSERT(
		context,
		RunSnapshot(kATCheatSnapMode_Less, before16, after16, true) ==
			(std::vector<uint32> { 1, 2 }));
	AT_PORTABLE_TEST_ASSERT(
		context,
		RunSnapshot(kATCheatSnapMode_Greater, before16, after16, true) ==
			(std::vector<uint32> { 0 }));

	std::vector<uint8> replace16Memory { 1, 2, 3, 4 };
	ATCheatEngine replace16;
	replace16.Init(replace16Memory.data(), (uint32)replace16Memory.size());
	replace16.Snapshot(kATCheatSnapMode_Replace, 0, true);
	AT_PORTABLE_TEST_ASSERT(
		context,
		GetValidOffsets(replace16) == (std::vector<uint32> { 0, 1, 2 }));
	AT_PORTABLE_TEST_ASSERT(context, replace16.GetOffsetCurrentValue(1, true) == 0x0302);
	AT_PORTABLE_TEST_ASSERT(context, replace16.GetOffsetCurrentValue(3, true) == 0);

	// Empty and one-byte memory ranges must not underflow 16-bit bounds.
	ATCheatEngine empty;
	empty.Init(nullptr, 0);
	empty.Snapshot(kATCheatSnapMode_Replace, 0, true);
	AT_PORTABLE_TEST_ASSERT(context, empty.GetValidOffsets(nullptr, 0) == 0);
	AT_PORTABLE_TEST_ASSERT(context, empty.GetOffsetCurrentValue(0, true) == 0);
	empty.AddCheat(0, true);
	empty.AddCheat(0, false);
	AT_PORTABLE_TEST_ASSERT(context, empty.GetCheatCount() == 0);

	uint8 oneByteMemory = 0x5A;
	ATCheatEngine oneByte;
	oneByte.Init(&oneByteMemory, 1);
	oneByte.AddCheat(0, true);
	AT_PORTABLE_TEST_ASSERT(context, oneByte.GetCheatCount() == 0);
	oneByte.AddCheat(0, false);
	AT_PORTABLE_TEST_ASSERT(context, oneByte.GetCheatCount() == 1);
	AT_PORTABLE_TEST_ASSERT(context, oneByte.GetOffsetCurrentValue(0, false) == 0x5A);

	std::array<uint8, 4> memory { 0x10, 0x20, 0x30, 0x40 };
	ATCheatEngine engine;
	engine.Init(memory.data(), (uint32)memory.size());
	engine.AddCheat(1, false);
	engine.AddCheat(2, true);
	engine.AddCheat(4, false);
	engine.AddCheat(3, true);
	AT_PORTABLE_TEST_ASSERT(context, engine.GetCheatCount() == 2);
	AT_PORTABLE_TEST_ASSERT(context, engine.GetCheatByIndex(0).mValue == 0x20);
	AT_PORTABLE_TEST_ASSERT(context, engine.GetCheatByIndex(1).mValue == 0x4030);

	engine.UpdateCheat(0, { 0, 0x00AA, false, true });
	engine.UpdateCheat(1, { 2, 0xBEEF, true, true });
	engine.AddCheat({ 1, 0x0077, false, false });
	engine.UpdateCheat(99, { 0, 0, false, false });
	engine.ApplyCheats();
	AT_PORTABLE_TEST_ASSERT(context, memory[0] == 0xAA);
	AT_PORTABLE_TEST_ASSERT(context, memory[1] == 0x20);
	AT_PORTABLE_TEST_ASSERT(context, memory[2] == 0xEF && memory[3] == 0xBE);

	engine.RemoveCheatByIndex(99);
	AT_PORTABLE_TEST_ASSERT(context, engine.GetCheatCount() == 3);
	engine.RemoveCheatByIndex(2);
	AT_PORTABLE_TEST_ASSERT(context, engine.GetCheatCount() == 2);
	engine.AddCheat({ 1, 0x0077, false, false });

	ATCheatFileSandbox file;
	engine.Save(file.mPath.c_str());
	std::array<uint8, 4> loadedMemory {};
	ATCheatEngine loaded;
	loaded.Init(loadedMemory.data(), (uint32)loadedMemory.size());
	loaded.Load(file.mPath.c_str());
	AT_PORTABLE_TEST_ASSERT(context, loaded.GetCheatCount() == 3);
	AT_PORTABLE_TEST_ASSERT(context, loaded.GetCheatByIndex(0).mAddress == 0);
	AT_PORTABLE_TEST_ASSERT(context, loaded.GetCheatByIndex(0).mValue == 0xAA);
	AT_PORTABLE_TEST_ASSERT(context, loaded.GetCheatByIndex(1).mb16Bit);
	AT_PORTABLE_TEST_ASSERT(context, loaded.GetCheatByIndex(1).mValue == 0xBEEF);
	AT_PORTABLE_TEST_ASSERT(context, !loaded.GetCheatByIndex(2).mbEnabled);
	loaded.ApplyCheats();
	AT_PORTABLE_TEST_ASSERT(context, loadedMemory[0] == 0xAA);
	AT_PORTABLE_TEST_ASSERT(context, loadedMemory[1] == 0);
	AT_PORTABLE_TEST_ASSERT(context, loadedMemory[2] == 0xEF && loadedMemory[3] == 0xBE);

	loaded.Snapshot(kATCheatSnapMode_Replace, 0, false);
	loaded.Clear();
	AT_PORTABLE_TEST_ASSERT(context, loaded.GetCheatCount() == 0);
	AT_PORTABLE_TEST_ASSERT(context, loaded.GetValidOffsets(nullptr, 0) == 0);
	return true;
}
