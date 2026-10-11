// Portable behavioral tests for the production PIA core.

#include <array>
#include <algorithm>
#include <memory>
#include <vector>

#include <at/atcore/snapshotimpl.h>
#include <at/atcore/savestate.h>
#include <at/attest/portabletest.h>
#include <at/atcore/scheduler.h>
#include <pia.h>
#include <irqcontroller.h>
#include <savestateio.h>
#include <vd2/system/file.h>

namespace {
	struct IRQRecorder {
		void AssertIRQ(int offset) { mOffsets.push_back(offset); }
		void NegateIRQ() { ++mNegates; }
		std::vector<int> mOffsets;
		uint32 mNegates = 0;
	};

	struct PIATestFixture {
		PIATestFixture() {
			mScheduler.SetRate(VDFraction(17897725, 10));
			mIRQ.Init(&mTarget);
			mPIA.Init(&mScheduler);
			mPIA.SetIRQHandler([this](uint32 sources, bool cpuBased) {
				if (cpuBased)
					mIRQ.Assert(sources, true);
				else
					mIRQ.Negate(sources, false);
			});
			mPIA.ColdReset();
		}

		ATScheduler mScheduler;
		IRQRecorder mTarget;
		ATIRQController mIRQ;
		ATPIAEmulator mPIA;
	};

	struct OutputRecorder {
		static void Callback(void *p, uint32 output) {
			static_cast<OutputRecorder *>(p)->mValues.push_back(output);
		}
		std::vector<uint32> mValues;
	};

	bool TestInputAllocation(ATPortableTestContext& context) {
		auto fixtureStorage = std::make_unique<PIATestFixture>();
		auto& pia = fixtureStorage->mPIA;
		std::array<int, 255> inputs {};
		for(size_t i = 0; i < inputs.size(); ++i) {
			inputs[i] = pia.AllocInput();
			AT_PORTABLE_TEST_ASSERT(context, inputs[i] == static_cast<int>(i));
		}
		AT_PORTABLE_TEST_ASSERT(context, pia.AllocInput() == -1);

		pia.WriteByte(2, 0x04); // Port A data register selected.
		pia.SetInput(inputs[0], 0xFFFFFFFE);
		pia.SetInput(inputs[1], 0xFFFFFFFD);
		AT_PORTABLE_TEST_ASSERT(context, pia.ReadByte(0) == 0xfc);

		pia.FreeInput(inputs[73]);
		const int reused = pia.AllocInput();
		AT_PORTABLE_TEST_ASSERT(context, reused == inputs[73]);
		pia.SetInput(reused, 0xFFFFFFFB);
		AT_PORTABLE_TEST_ASSERT(context, pia.ReadByte(0) == 0xf8);
		pia.FreeInput(reused);
		pia.FreeInput(-1);
		return true;
	}

	bool TestRegistersAndOutputs(ATPortableTestContext& context) {
		auto fixtureStorage = std::make_unique<PIATestFixture>();
		auto& pia = fixtureStorage->mPIA;
		OutputRecorder recorder;
		const int outputSlot = pia.AllocOutput(&OutputRecorder::Callback, &recorder, 0x000000f0 | kATPIAOutput_CA2);
		AT_PORTABLE_TEST_ASSERT(context, outputSlot >= 0);
		AT_PORTABLE_TEST_ASSERT(context, recorder.mValues.empty());

		pia.WriteByte(0, 0xf0); // DDRA while CRA selects DDR.
		pia.WriteByte(1, 0x0f); // DDRB while CRB selects DDR.
		ATPIAState state {};
		pia.GetState(state);
		AT_PORTABLE_TEST_ASSERT(context, state.mDDRA == 0xf0 && state.mDDRB == 0x0f);

		pia.WriteByte(2, 0x04); // Select ORA.
		pia.WriteByte(0, 0xa5);
		pia.WriteByte(3, 0x04); // Select ORB.
		pia.WriteByte(1, 0x5a);
		pia.GetState(state);
		AT_PORTABLE_TEST_ASSERT(context, state.mORA == 0xa5 && state.mORB == 0x5a);
		AT_PORTABLE_TEST_ASSERT(context, pia.GetPortAOutput() == 0xaf);
		// Output bits follow ORB; input bits are pulled high: 0x5a | ~0x0f.
		AT_PORTABLE_TEST_ASSERT(context, pia.GetPortBOutput() == 0xfa);
		AT_PORTABLE_TEST_ASSERT(context, !recorder.mValues.empty());
		AT_PORTABLE_TEST_ASSERT(context, (recorder.mValues.back() & 0xff) == 0xaf);

		const size_t priorCount = recorder.mValues.size();
		pia.ModifyOutputMask(outputSlot, 0x000000f0);
		pia.WriteByte(0, 0xa4);
		AT_PORTABLE_TEST_ASSERT(context, recorder.mValues.size() == priorCount);
		pia.WriteByte(0, 0x34);
		AT_PORTABLE_TEST_ASSERT(context, recorder.mValues.size() == priorCount + 1);
		pia.FreeOutput(outputSlot);
		const size_t freedCount = recorder.mValues.size();
		pia.WriteByte(0, 0x24);
		AT_PORTABLE_TEST_ASSERT(context, recorder.mValues.size() == freedCount);
		return true;
	}

	bool TestDynamicInputs(ATPortableTestContext& context) {
		auto fixtureStorage = std::make_unique<PIATestFixture>();
		auto& pia = fixtureStorage->mPIA;
		pia.WriteByte(3, 0x04);
		const uint32 ta = pia.RegisterDynamicInput(false, [] { return uint8(0xfe); });
		const uint32 tb = pia.RegisterDynamicInput(false, [] { return uint8(0xfd); });
		pia.WriteByte(2, 0x04);
		AT_PORTABLE_TEST_ASSERT(context, pia.ReadByte(0) == 0xfc);
		pia.UnregisterDynamicInput(false, ta);
		AT_PORTABLE_TEST_ASSERT(context, pia.ReadByte(0) == 0xfd);
		pia.UnregisterDynamicInput(false, tb);
		AT_PORTABLE_TEST_ASSERT(context, pia.ReadByte(0) == 0xff);

		// Removing a non-last B input must move the matching B callback even
		// when A is empty; this catches cross-port vector selection errors.
		const uint32 b0 = pia.RegisterDynamicInput(true, [] { return uint8(0xfe); });
		const uint32 b1 = pia.RegisterDynamicInput(true, [] { return uint8(0xfd); });
		const uint32 b2 = pia.RegisterDynamicInput(true, [] { return uint8(0xfb); });
		pia.WriteByte(3, 0x04);
		AT_PORTABLE_TEST_ASSERT(context, pia.ReadByte(1) == 0xf8);
		pia.UnregisterDynamicInput(true, b1);
		AT_PORTABLE_TEST_ASSERT(context, pia.ReadByte(1) == 0xfa);
		pia.UnregisterDynamicInput(true, b0);
		pia.UnregisterDynamicInput(true, b2);
		AT_PORTABLE_TEST_ASSERT(context, pia.ReadByte(1) == 0xff);

		const uint32 a0 = pia.RegisterDynamicInput(false, [] { return uint8(0xf7); });
		const uint32 a1 = pia.RegisterDynamicInput(false, [] { return uint8(0xef); });
		const uint32 ba = pia.RegisterDynamicInput(true, [] { return uint8(0xfe); });
		const uint32 bb = pia.RegisterDynamicInput(true, [] { return uint8(0xfd); });
		const uint32 bc = pia.RegisterDynamicInput(true, [] { return uint8(0xfb); });
		pia.UnregisterDynamicInput(true, bb);
		AT_PORTABLE_TEST_ASSERT(context, pia.ReadByte(1) == 0xfa);
		pia.UnregisterDynamicInput(false, a0);
		pia.UnregisterDynamicInput(false, a1);
		pia.UnregisterDynamicInput(true, ba);
		pia.UnregisterDynamicInput(true, bc);
		return true;
	}

	bool TestIRQEdges(ATPortableTestContext& context) {
		auto fixtureStorage = std::make_unique<PIATestFixture>();
		auto& fixture = *fixtureStorage;
		auto& pia = fixture.mPIA;
		pia.WriteByte(2, 0x01); // Falling CA1 edge, IRQ enabled.
		pia.SetCA1(false);
		AT_PORTABLE_TEST_ASSERT(context, (pia.ReadByte(2) & 0x80) != 0);
		AT_PORTABLE_TEST_ASSERT(context, fixture.mTarget.mOffsets == std::vector<int>({ 0 }));
		pia.WriteByte(2, 0x05); // Read port to acknowledge the latched edge.
		AT_PORTABLE_TEST_ASSERT(context, pia.ReadByte(0) == 0xff);
		AT_PORTABLE_TEST_ASSERT(context, (pia.ReadByte(2) & 0x80) == 0);
		AT_PORTABLE_TEST_ASSERT(context, fixture.mTarget.mNegates == 1);

		pia.WriteByte(2, 0x19); // Rising CA2 input edge with IRQ enabled.
		pia.SetCA2(false);
		pia.SetCA2(true);
		AT_PORTABLE_TEST_ASSERT(context, (pia.ReadByte(2) & 0x40) != 0);
		AT_PORTABLE_TEST_ASSERT(context, fixture.mTarget.mOffsets.back() == 0);
		pia.WriteByte(2, 0x1d);
		pia.ReadByte(0);
		AT_PORTABLE_TEST_ASSERT(context, (pia.ReadByte(2) & 0x40) == 0);

		pia.WriteByte(3, 0x01); // Falling CB1 edge.
		pia.SetCB1(false);
		AT_PORTABLE_TEST_ASSERT(context, (pia.ReadByte(3) & 0x80) != 0);
		AT_PORTABLE_TEST_ASSERT(context, fixture.mTarget.mOffsets.back() == 0);
		pia.WriteByte(3, 0x05);
		pia.ReadByte(1);
		AT_PORTABLE_TEST_ASSERT(context, (pia.ReadByte(3) & 0x80) == 0);
		return true;
	}

	bool TestFloatingInputs(ATPortableTestContext& context) {
		auto makeFloating = [](PIATestFixture& fixture, ATPIAFloatingInputs& floating) {
			floating.mpScheduler = &fixture.mScheduler;
			floating.mFloatingInputMask = 0x03;
			floating.mRandomSeed = 0x1234567;
			floating.mDecayTimeMin = 100;
			floating.mDecayTimeRange = 200;
			fixture.mPIA.SetPortBFloatingInputs(&floating);
			fixture.mPIA.WriteByte(3, 0x04);
			fixture.mPIA.WriteByte(1, 0xff);
			fixture.mPIA.WriteByte(3, 0x00);
			fixture.mPIA.WriteByte(1, 0xff);
			fixture.mPIA.WriteByte(3, 0x04);
			fixture.mPIA.WriteByte(1, 0xff);
			fixture.mPIA.WriteByte(3, 0x00);
			fixture.mPIA.WriteByte(1, 0x00);
			fixture.mPIA.WriteByte(3, 0x04);
		};
		auto aStorage = std::make_unique<PIATestFixture>();
		auto bStorage = std::make_unique<PIATestFixture>();
		ATPIAFloatingInputs a {}, b {};
		const uint64 startTime = aStorage->mScheduler.GetTick64();
		makeFloating(*aStorage, a);
		makeFloating(*bStorage, b);
		AT_PORTABLE_TEST_ASSERT(context, a.mFloatTimers[0] == b.mFloatTimers[0]);
		AT_PORTABLE_TEST_ASSERT(context, a.mFloatTimers[1] == b.mFloatTimers[1]);
		AT_PORTABLE_TEST_ASSERT(context, a.mFloatTimers[0] >= startTime + 100 && a.mFloatTimers[0] <= startTime + 300);
		AT_PORTABLE_TEST_ASSERT(context, a.mFloatTimers[1] >= startTime + 100 && a.mFloatTimers[1] <= startTime + 300);
		AT_PORTABLE_TEST_ASSERT(context, aStorage->mPIA.ReadByte(1) == 0xff);
		const uint64 finalTime = std::max(a.mFloatTimers[0], a.mFloatTimers[1]);
		for(uint64 t = startTime; t < finalTime; ++t)
			ATSCHEDULER_ADVANCE(&aStorage->mScheduler);
		AT_PORTABLE_TEST_ASSERT(context, aStorage->mPIA.ReadByte(1) == 0xfc);
		return true;
	}

	bool TestSnapshot(ATPortableTestContext& context) {
		auto sourceStorage = std::make_unique<PIATestFixture>();
		auto restoredStorage = std::make_unique<PIATestFixture>();
		auto& source = *sourceStorage;
		auto& restored = *restoredStorage;
		OutputRecorder sourceOutput, restoredOutput;
		source.mPIA.AllocOutput(&OutputRecorder::Callback, &sourceOutput, 0x0000ffff | kATPIAOutput_CA2 | kATPIAOutput_CB2);
		restored.mPIA.AllocOutput(&OutputRecorder::Callback, &restoredOutput, 0x0000ffff | kATPIAOutput_CA2 | kATPIAOutput_CB2);
		source.mPIA.WriteByte(0, 0x35);
		source.mPIA.WriteByte(1, 0xA6);
		source.mPIA.WriteByte(2, 0x04);
		source.mPIA.WriteByte(3, 0x04);
		source.mPIA.WriteByte(0, 0xA5);
		source.mPIA.WriteByte(1, 0x5A);
		source.mPIA.WriteByte(2, 0x01);
		source.mPIA.SetCA1(false);
		AT_PORTABLE_TEST_ASSERT(context, source.mTarget.mOffsets.size() == 1);

		vdrefptr<IATObjectState> snapshot;
		source.mPIA.SaveState(~snapshot);
		AT_PORTABLE_TEST_ASSERT(context, !!snapshot);
		VDMemoryBufferStream stream;
		std::unique_ptr<IATSaveStateSerializer> writer(ATCreateSaveStateSerializer());
		writer->Serialize(stream, *snapshot, L"PIA portable core");
		stream.Seek(0);
		std::unique_ptr<IATSaveStateDeserializer> reader(ATCreateSaveStateDeserializer());
		vdrefptr<IATSerializable> decoded;
		reader->Deserialize(stream, ~decoded);
		AT_PORTABLE_TEST_ASSERT(context, !!decoded);
		AT_PORTABLE_TEST_ASSERT(context, &decoded->GetSerializationType() == &snapshot->GetSerializationType());
		restored.mPIA.LoadState(static_cast<IATObjectState *>(decoded.get()));
		restored.mPIA.PostLoadState();
		AT_PORTABLE_TEST_ASSERT(context, restored.mTarget.mOffsets.size() == 1);
		ATPIAState expected {}, actual {};
		source.mPIA.GetState(expected);
		restored.mPIA.GetState(actual);
		AT_PORTABLE_TEST_ASSERT(context, expected.mORA == actual.mORA && expected.mORB == actual.mORB);
		AT_PORTABLE_TEST_ASSERT(context, expected.mDDRA == actual.mDDRA && expected.mDDRB == actual.mDDRB);
		AT_PORTABLE_TEST_ASSERT(context, expected.mCRA == actual.mCRA && expected.mCRB == actual.mCRB);
		AT_PORTABLE_TEST_ASSERT(context, restored.mPIA.GetOutputState() == source.mPIA.GetOutputState());
		AT_PORTABLE_TEST_ASSERT(context, !restoredOutput.mValues.empty());

		// Reading the data register clears the saved edge flag; a subsequent real
		// CA1 transition reasserts the IRQ through the restored controller wiring.
		restored.mPIA.WriteByte(2, 0x05);
		restored.mPIA.ReadByte(0);
		AT_PORTABLE_TEST_ASSERT(context, restored.mPIA.ReadByte(2) & 0x80 ? false : true);
		restored.mPIA.SetCA1(true);
		restored.mPIA.SetCA1(false);
		AT_PORTABLE_TEST_ASSERT(context, restored.mTarget.mOffsets.size() == 2);

		// Replacing an active PIA IRQ with a clean state must negate the actual
		// controller source as part of post-load reconciliation.
		auto cleanStorage = std::make_unique<PIATestFixture>();
		auto activeStorage = std::make_unique<PIATestFixture>();
		activeStorage->mPIA.WriteByte(2, 0x01);
		activeStorage->mPIA.SetCA1(false);
		AT_PORTABLE_TEST_ASSERT(context, activeStorage->mTarget.mOffsets.size() == 1);
		vdrefptr<IATObjectState> cleanState;
		cleanStorage->mPIA.SaveState(~cleanState);
		AT_PORTABLE_TEST_ASSERT(context, !!cleanState);
		activeStorage->mPIA.LoadState(cleanState.get());
		activeStorage->mPIA.PostLoadState();
		AT_PORTABLE_TEST_ASSERT(context, activeStorage->mTarget.mNegates == 1);
		return true;
	}
}

bool ATTestAltirraPIAInputs(ATPortableTestContext& context) {
	return TestInputAllocation(context) && TestDynamicInputs(context);
}
bool ATTestAltirraPIARegisters(ATPortableTestContext& context) { return TestRegistersAndOutputs(context); }
bool ATTestAltirraPIAIRQ(ATPortableTestContext& context) { return TestIRQEdges(context); }
bool ATTestAltirraPIAFloating(ATPortableTestContext& context) { return TestFloatingInputs(context); }
bool ATTestAltirraPIASnapshot(ATPortableTestContext& context) { return TestSnapshot(context); }
