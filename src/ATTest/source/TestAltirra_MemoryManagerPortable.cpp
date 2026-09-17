// Altirra portable memory manager tests

#include <algorithm>
#include <array>
#include <memory>
#include <string>
#include <vector>

#include <at/atcore/consoleoutput.h>
#include <at/attest/portabletest.h>
#include <memorymanager.h>

namespace {
	class CaptureOutput final : public ATConsoleOutput {
	public:
		void WriteLine(const char *s) override {
			mLines.emplace_back(s);
		}

		std::vector<std::string> mLines;
	};

	struct HandlerState {
		sint32 mDebugValue = -1;
		sint32 mReadValue = -1;
		bool mbHandleWrite = false;
		uint32 mDebugAddress = 0;
		uint32 mReadAddress = 0;
		uint32 mWriteAddress = 0;
		uint8 mWriteValue = 0;
	};

	sint32 DebugReadHandler(void *thisptr, uint32 address) {
		HandlerState& state = *static_cast<HandlerState *>(thisptr);
		state.mDebugAddress = address;
		return state.mDebugValue;
	}

	sint32 ReadHandler(void *thisptr, uint32 address) {
		HandlerState& state = *static_cast<HandlerState *>(thisptr);
		state.mReadAddress = address;
		return state.mReadValue;
	}

	bool WriteHandler(void *thisptr, uint32 address, uint8 value) {
		HandlerState& state = *static_cast<HandlerState *>(thisptr);
		state.mWriteAddress = address;
		state.mWriteValue = value;
		return state.mbHandleWrite;
	}
}

bool ATTestAltirraMemoryManager(ATPortableTestContext& context) {
	auto memory = std::make_unique<ATMemoryManager>();
	memory->Init();

	AT_PORTABLE_TEST_ASSERT(context, memory->ReadByte(0x1234) == 0xFF);
	AT_PORTABLE_TEST_ASSERT(context, memory->DebugReadByte(0x1234) == 0xFF);

	alignas(2) std::array<uint8, 512> baseMemory {};
	baseMemory[0x12] = 0x21;
	baseMemory[0x112] = 0x31;
	ATMemoryLayer *baseLayer = memory->CreateLayer(kATMemoryPri_BaseRAM, baseMemory.data(), 0x20, 2, false);
	memory->SetLayerName(baseLayer, "portable base RAM");
	memory->EnableLayer(baseLayer, true);

	AT_PORTABLE_TEST_ASSERT(context, memory->ReadByte(0x2012) == 0x21);
	AT_PORTABLE_TEST_ASSERT(context, memory->DebugReadByte(0x2112) == 0x31);
	AT_PORTABLE_TEST_ASSERT(context, memory->AnticReadByte(0x2012) == 0x21);
	memory->WriteByte(0x2112, 0x41);
	AT_PORTABLE_TEST_ASSERT(context, baseMemory[0x112] == 0x41);

	memory->SetLayerMemory(baseLayer, baseMemory.data(), 0x20, 2, 0);
	AT_PORTABLE_TEST_ASSERT(context, memory->ReadByte(0x2112) == 0x21);
	memory->WriteByte(0x2112, 0x51);
	AT_PORTABLE_TEST_ASSERT(context, baseMemory[0x12] == 0x51);
	memory->SetLayerReadOnly(baseLayer, true);
	memory->WriteByte(0x2012, 0x61);
	AT_PORTABLE_TEST_ASSERT(context, baseMemory[0x12] == 0x51);
	memory->SetLayerReadOnly(baseLayer, false);

	alignas(2) std::array<uint8, 256> overlayMemory {};
	overlayMemory[0x12] = 0xA2;
	ATMemoryLayer *overlayLayer = memory->CreateLayer(kATMemoryPri_ROM, overlayMemory.data(), 0x20, 1, true);
	memory->EnableLayer(overlayLayer, kATMemoryAccessMode_CPURead, true);
	AT_PORTABLE_TEST_ASSERT(context, memory->ReadByte(0x2012) == 0xA2);
	AT_PORTABLE_TEST_ASSERT(context, memory->AnticReadByte(0x2012) == 0x51);
	memory->EnableLayer(overlayLayer, kATMemoryAccessMode_CPURead, false);
	AT_PORTABLE_TEST_ASSERT(context, memory->ReadByte(0x2012) == 0x51);

	alignas(2) std::array<uint8, 256> handlerBacking {};
	handlerBacking[0x34] = 0x34;
	ATMemoryLayer *handlerBackingLayer = memory->CreateLayer(kATMemoryPri_BaseRAM, handlerBacking.data(), 0x30, 1, false);
	memory->EnableLayer(handlerBackingLayer, true);

	HandlerState handlerState;
	ATMemoryHandlerTable handlers {};
	handlers.mbPassReads = true;
	handlers.mbPassAnticReads = true;
	handlers.mbPassWrites = true;
	handlers.mpThis = &handlerState;
	handlers.mpDebugReadHandler = DebugReadHandler;
	handlers.mpReadHandler = ReadHandler;
	handlers.mpWriteHandler = WriteHandler;
	ATMemoryLayer *handlerLayer = memory->CreateLayer(kATMemoryPri_Hardware, handlers, 0x30, 1);
	memory->EnableLayer(handlerLayer, true);

	AT_PORTABLE_TEST_ASSERT(context, memory->ReadByte(0x3034) == 0x34);
	AT_PORTABLE_TEST_ASSERT(context, handlerState.mReadAddress == 0x3034);
	handlerState.mReadValue = 0x73;
	AT_PORTABLE_TEST_ASSERT(context, memory->ReadByte(0x3034) == 0x73);
	handlerState.mDebugValue = 0x74;
	AT_PORTABLE_TEST_ASSERT(context, memory->DebugReadByte(0x3034) == 0x74);
	AT_PORTABLE_TEST_ASSERT(context, handlerState.mDebugAddress == 0x3034);
	memory->WriteByte(0x3034, 0x81);
	AT_PORTABLE_TEST_ASSERT(context, handlerBacking[0x34] == 0x81);
	handlerState.mbHandleWrite = true;
	memory->WriteByte(0x3034, 0x82);
	AT_PORTABLE_TEST_ASSERT(context, handlerState.mWriteAddress == 0x3034);
	AT_PORTABLE_TEST_ASSERT(context, handlerState.mWriteValue == 0x82);
	AT_PORTABLE_TEST_ASSERT(context, handlerBacking[0x34] == 0x81);

	alignas(2) std::array<uint8, 256> highMemory {};
	highMemory[0x56] = 0x95;
	memory->SetHighMemoryEnabled(true);
	ATMemoryLayer *highLayer = memory->CreateLayer(kATMemoryPri_ExtRAM, highMemory.data(), 0x234, 1, false);
	memory->EnableLayer(highLayer, kATMemoryAccessMode_RW, true);
	AT_PORTABLE_TEST_ASSERT(context, memory->ExtReadByte(0x3456, 2) == 0x95);
	memory->ExtWriteByte(0x3456, 2, 0x96);
	AT_PORTABLE_TEST_ASSERT(context, highMemory[0x56] == 0x96);

	alignas(2) std::array<uint8, 256> zeroPage {};
	zeroPage[0x78] = 0xA7;
	ATMemoryLayer *zeroLayer = memory->CreateLayer(kATMemoryPri_BaseRAM, zeroPage.data(), 0, 1, false);
	memory->EnableLayer(zeroLayer, kATMemoryAccessMode_RW, true);
	memory->SetWrapBankZeroEnabled(true);
	AT_PORTABLE_TEST_ASSERT(context, memory->ExtReadByte(0x0078, 1) == 0xA7);

	memory->SetFastBusEnabled(true);
	memory->SetLayerFastBus(baseLayer, false);
	AT_PORTABLE_TEST_ASSERT(context, memory->ExtReadByteAccel(0x2012, 0, false) == ATMemoryManager::kChipReadNeedsDelay);
	memory->SetLayerFastBus(baseLayer, true);
	AT_PORTABLE_TEST_ASSERT(context, memory->ExtReadByteAccel(0x2012, 0, false) == 0x51);

	CaptureOutput output;
	memory->DumpStatus(output);
	AT_PORTABLE_TEST_ASSERT(context, output.mLines.size() >= 2);
	AT_PORTABLE_TEST_ASSERT(context, output.mLines[0].find("Bus Mode") != std::string::npos);
	AT_PORTABLE_TEST_ASSERT(context, std::any_of(output.mLines.begin(), output.mLines.end(), [](const std::string& line) {
		return line.find("portable base RAM") != std::string::npos;
	}));

	memory->DeleteLayer(zeroLayer);
	memory->DeleteLayer(highLayer);
	memory->DeleteLayer(handlerLayer);
	memory->DeleteLayer(handlerBackingLayer);
	memory->DeleteLayer(overlayLayer);
	memory->DeleteLayer(baseLayer);
	return true;
}
