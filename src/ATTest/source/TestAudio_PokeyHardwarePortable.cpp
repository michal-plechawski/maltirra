// Portable behavioral tests for the production POKEY register and timer core.

#include <memory>
#include <algorithm>
#include <utility>
#include <vector>

#include <at/ataudio/pokey.h>
#include <at/ataudio/pokeytables.h>
#include <at/atcore/ksyms.h>
#include <at/atcore/scheduler.h>
#include <at/attest/portabletest.h>

namespace {
	class PokeyHardwareConnections final : public IATPokeyEmulatorConnections {
	public:
		void PokeyAssertIRQ(bool cpuBased) override { mIRQs.emplace_back(cpuBased, true); }
		void PokeyNegateIRQ(bool cpuBased) override { mIRQs.emplace_back(cpuBased, false); }
		void PokeyBreak() override { ++mBreakCount; }
		bool PokeyIsInInterrupt() const override { return false; }
		bool PokeyIsKeyPushOK(uint8, bool) const override { return true; }

		std::vector<std::pair<bool, bool>> mIRQs;
		unsigned mBreakCount = 0;
	};

	struct PokeyHardwareFixture {
		ATScheduler mScheduler;
		PokeyHardwareConnections mConnections;
		ATPokeyTables mTables;
		ATPokeyEmulator mPokey { false };

		PokeyHardwareFixture() {
			mPokey.Init(&mConnections, &mScheduler, nullptr, &mTables);
			mPokey.ColdReset();
		}

		~PokeyHardwareFixture() { mPokey.ColdReset(); }

		void Write(uint8 reg, uint8 value) { mPokey.WriteByte(reg, value); }
		void Tick(uint32 cycles = 1) {
			while (cycles--)
				ATSCHEDULER_ADVANCE(&mScheduler);
		}
	};

	bool TestPokeyRegisters(ATPortableTestContext& context) {
		auto storage = std::make_unique<PokeyHardwareFixture>();
		auto& f = *storage;
		const auto write = [&f](uint8 reg, uint8 value) { f.Write(reg, value); };

		write(0x00, 0x35); // AUDF1
		write(0x01, 0xAF); // AUDC1: control/volume register is fully writable
		write(0x02, 0x72);
		write(0x03, 0x5C);
		write(0x04, 0x19);
		write(0x05, 0xE3);
		write(0x06, 0xA4);
		write(0x07, 0x81);
		write(0x08, 0xFF); // AUDCTL retains the hardware's eight control bits
		write(0x0E, 0xFF); // IRQEN bit 3 is unimplemented
		write(0x0F, 0xFF); // SKCTL is eight-bit; low bits control clock reset/run

		ATPokeyRegisterState state {};
		f.mPokey.GetRegisterState(state);
		AT_PORTABLE_TEST_ASSERT(context, f.mPokey.ReadBackWriteRegister(0x00) == 0x35);
		AT_PORTABLE_TEST_ASSERT(context, f.mPokey.ReadBackWriteRegister(0x01) == 0xAF);
		AT_PORTABLE_TEST_ASSERT(context, f.mPokey.ReadBackWriteRegister(0x02) == 0x72);
		AT_PORTABLE_TEST_ASSERT(context, f.mPokey.ReadBackWriteRegister(0x03) == 0x5C);
		AT_PORTABLE_TEST_ASSERT(context, f.mPokey.ReadBackWriteRegister(0x04) == 0x19);
		AT_PORTABLE_TEST_ASSERT(context, f.mPokey.ReadBackWriteRegister(0x05) == 0xE3);
		AT_PORTABLE_TEST_ASSERT(context, f.mPokey.ReadBackWriteRegister(0x06) == 0xA4);
		AT_PORTABLE_TEST_ASSERT(context, f.mPokey.ReadBackWriteRegister(0x07) == 0x81);
		AT_PORTABLE_TEST_ASSERT(context, f.mPokey.ReadBackWriteRegister(0x08) == 0xFF);
		AT_PORTABLE_TEST_ASSERT(context, f.mPokey.ReadBackWriteRegister(0x0E) == 0xFF);
		AT_PORTABLE_TEST_ASSERT(context, f.mPokey.ReadBackWriteRegister(0x0F) == 0xFF);
		AT_PORTABLE_TEST_ASSERT(context, f.mPokey.ReadBackWriteRegister(0x0A) == -1);
		AT_PORTABLE_TEST_ASSERT(context, f.mPokey.DebugReadByte(0x0E) == 0xF7);

		// IRQST bit 3 has its reset-low serial-output meaning, while IRQEN bit 3
		// is unimplemented and does not alter that status bit.
		write(0x0E, 0x08);
		AT_PORTABLE_TEST_ASSERT(context, f.mPokey.ReadBackWriteRegister(0x0E) == 0x08);
		AT_PORTABLE_TEST_ASSERT(context, f.mPokey.DebugReadByte(0x0E) == 0xF7);

		// SKRES clears the three latched keyboard status flags without changing SKCTL.
		f.mPokey.PushBreak();
		write(0x0A, 0);
		AT_PORTABLE_TEST_ASSERT(context, (f.mPokey.DebugReadByte(0x0F) & 0xE0) == 0xE0);
		return true;
	}

	bool TestSlowTimer(ATPortableTestContext& context, uint8 frequencyRegister, uint8 irqMask, bool clock15KHz) {
		auto storage = std::make_unique<PokeyHardwareFixture>();
		auto& f = *storage;
		f.Write(0x0F, 0x03);
		f.Write(0x08, clock15KHz ? 0x01 : 0x00);
		f.Write(frequencyRegister, 0);
		f.Write(0x09, 0);
		f.Write(0x0E, irqMask);
		const size_t irqStart = f.mConnections.mIRQs.size();
		// Releasing SKCTL reset starts the polynomial divider partway through
		// its period: the first edge is at 22 (64 kHz) or 81 (15 kHz) CPU ticks,
		// followed by three ticks for the borrow pipeline. STIMER's reload at
		// tick 4 does not reset the divider phase. Each case starts a fresh clock.
		const uint32 deadline = (clock15KHz ? 81 : 22) + 3;
		f.Tick(deadline - 1);
		AT_PORTABLE_TEST_ASSERT(context, f.mPokey.DebugReadByte(0x0E) & irqMask);
		AT_PORTABLE_TEST_ASSERT(context, std::find(f.mConnections.mIRQs.begin() + irqStart,
			f.mConnections.mIRQs.end(), std::pair<bool, bool>{ false, true }) == f.mConnections.mIRQs.end());
		f.Tick();
		AT_PORTABLE_TEST_ASSERT(context, !(f.mPokey.DebugReadByte(0x0E) & irqMask));
		AT_PORTABLE_TEST_ASSERT(context, std::find(f.mConnections.mIRQs.begin() + irqStart,
			f.mConnections.mIRQs.end(), std::pair<bool, bool>{ false, true }) != f.mConnections.mIRQs.end());
		f.Write(0x0E, 0);
		AT_PORTABLE_TEST_ASSERT(context, f.mPokey.DebugReadByte(0x0E) & irqMask);
		AT_PORTABLE_TEST_ASSERT(context, f.mConnections.mIRQs.back() == (std::pair<bool, bool>{ true, false }));
		return true;
	}

	bool TestPokeyTimers(ATPortableTestContext& context) {
		auto storage = std::make_unique<PokeyHardwareFixture>();
		auto& f = *storage;
		f.Write(0x0F, 0x03); // release timer clocks from reset and enable 64 kHz clock
		// Timer 1 at the fast clock: IRQST starts high, STIMER restarts the real
		// counter, and its expiry asserts active-low IRQST and the CPU IRQ callback.
		f.Write(0x0E, 0x00);
		f.Write(0x08, 0x40); // timer 1 uses 1.79 MHz clock
		f.Write(0x00, 0x02);
		f.Write(0x09, 0);
		f.Write(0x0E, 0x01);
		AT_PORTABLE_TEST_ASSERT(context, f.mPokey.DebugReadByte(0x0E) & 0x01);
		// The timer counter first reloads on cycle 3, then AUDF+1 fast-clock
		// cycles later, with IRQST asserting one cycle after that borrow.
		constexpr uint32 kTimer1Deadline = 3 + (2 + 1) + 3 + 1;
		const size_t timer1IRQStart = f.mConnections.mIRQs.size();
		f.Tick(kTimer1Deadline - 1);
		AT_PORTABLE_TEST_ASSERT(context, f.mPokey.DebugReadByte(0x0E) & 0x01);
		f.Tick();
		AT_PORTABLE_TEST_ASSERT(context, !(f.mPokey.DebugReadByte(0x0E) & 0x01));
		AT_PORTABLE_TEST_ASSERT(context, std::find(f.mConnections.mIRQs.begin() + timer1IRQStart, f.mConnections.mIRQs.end(), std::pair<bool, bool>{ false, true }) != f.mConnections.mIRQs.end());

		// Disabling the source acknowledges its pending status and deasserts IRQ.
		f.Write(0x0E, 0);
		AT_PORTABLE_TEST_ASSERT(context, f.mPokey.DebugReadByte(0x0E) & 0x01);
		AT_PORTABLE_TEST_ASSERT(context, std::find(f.mConnections.mIRQs.begin(), f.mConnections.mIRQs.end(), std::pair<bool, bool>{ true, false }) != f.mConnections.mIRQs.end());

		return TestSlowTimer(context, 0x02, 0x02, false)
			&& TestSlowTimer(context, 0x06, 0x04, false)
			&& TestSlowTimer(context, 0x02, 0x02, true)
			&& TestSlowTimer(context, 0x06, 0x04, true);
	}
}

bool ATTestAudioPokeyRegisters(ATPortableTestContext& context) { return TestPokeyRegisters(context); }
bool ATTestAudioPokeyTimers(ATPortableTestContext& context) { return TestPokeyTimers(context); }
