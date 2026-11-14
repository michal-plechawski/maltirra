# macOS ARM64 platform

This directory contains the native macOS ARM64 build entry point and is the
home for future macOS-specific implementations. The build compiles every C++
translation unit below `src/` except for the Windows-only
`src/platform/Windows_x64` subtree.

Run it on Apple Silicon macOS with:

```sh
bash src/platform/macOS_arm64/build.sh
```

Objects and the build manifest are written to `obj/macOS_arm64`. The portable
test runner is written to `out/macOS_arm64/AltirraPortableTests`. Object reuse
is content-based, so a restored GitHub Actions cache remains useful after a
fresh checkout with different file timestamps.

The shared GTIA scanline renderer (`src/Altirra/source/gtiarenderer.cpp`) now
uses platform-specific SSSE3 kernels on Windows and bounded NEON kernels on
macOS. `Altirra_GTIARenderer` checks pixel output, partial spans, register
timing, SECAM, priority, light-pen sensing, and JSON snapshot continuation
(including in-flight GTIA mode transitions) on both platforms.

`Altirra_GTIADisplayMac` connects actual indexed GTIA output and the Atari
palette generator to a native AppKit window/display and checks its rendered
pixels against captures. This is a video-pipeline test, not a running emulator:
Simulator integration is still in the Windows-only subtree.

The full software artifacting engine (`src/Altirra/source/artifacting.cpp`)
is shared too: indexed/RGB conversion, NTSC/PAL artifacting (including 14MHz
high artifacting), signed output, color correction, scanlines, frame blending,
monochrome persistence, and adaptive deinterlacing. The existing SSE2 and NEON
kernels remain selected for their respective CPUs. Portable tests cover the
engine, every default artifacting parameter, and exact fixed-point kernel
oracles. Suspend/resume is checked against uninterrupted processing, including
PAL delay history and palette/filter changes during temporary updates.

The native GTIA display test additionally submits processed NTSC/PAL output
through the AppKit RGB display path at both resolutions.

Diagnostic text formatting and file logging are shared in `consoletext.cpp`.
Both formatted APIs work without a GUI when a log is open; timed messages use
the real Windows ANTIC or an explicitly installed machine-timing provider.
macOS does not fabricate beam positions before machine integration.

The native macOS diagnostic console (`console_macos.mm`) uses a read-only,
selectable NSTextView in a resizable AppKit window, with monospaced text and
horizontal/vertical scrolling. Output does not implicitly open a window.
Call `ATOpenConsole()` or `ATConsoleCreateMac()` on the main thread; serialize
shared output and hook changes on that thread too. Each Write accepts a complete
UTF-8 message, falling back to Latin-1 for legacy byte strings. This is the
diagnostic output panel, not yet a debugger command interpreter.

`Altirra_ConsoleText` checks formatting, beam tags, buffer limits, and actual
log-file bytes on both platforms. `Altirra_ConsoleMac` exercises native text,
Unicode, resizing, closure/reopening, and closure during text-storage callbacks.

The complete VBXE core and device wrapper (`src/Altirra/source/vbxe.cpp`) are
shared: MEMAC A/B mappings, CPU/ANTIC access, blitter DMA and scheduled IRQs,
XDL overlays (bitmap and 80-column text), palettes, and device snapshots. The
core keeps the original algorithms and uses the shared diagnostic console.
Packed character-generator stores no longer alias byte arrays as uint32.

`Altirra_VBXEMemory`, `Altirra_VBXEBlitter`, `Altirra_VBXEVideo`, and
`Altirra_VBXEDevice` use the real memory manager, scheduler, IRQ controller,
palette generator, artifacting engine, and production device factory. Tests
cover MEMAC priority/access bits/window truncation, wrapped multi-row DMA,
completion/ack/reset/shutdown IRQs, guarded SR/HR/text scanlines, palette RAM,
text attribute fetches across the end of RAM, pending-register snapshots,
U1MB register-page overrides, and live backing-RAM
changes. Owned RAM is saved; externally shared RAM is intentionally not saved
by the VBXE device. This is component verification, not a booted Atari machine:
Simulator/application wiring remains to be ported.

The complete GTIA emulator (`src/Altirra/source/gtia.cpp`) is now shared too:
registers, players/missiles, collisions, trigger/switch callbacks, scanlines,
frame allocation/recycling, artifacting, and VBXE RGB rendering. The shared
core calls platform playfield unpackers: Windows retains SSE2 for complete
aligned blocks, while macOS uses bounded NEON blocks; tails never access
padding around the caller's buffers. Empty playfield spans are no-ops.
Standalone heap instances explicitly initialize synchronization, sprite lists,
output pointers, and scanline state instead of relying on static zeroing.

`Altirra_GTIARegisters`, `Altirra_GTIASnapshot`, and `Altirra_GTIAPlayfield`
cover callbacks, SECAM line-edge trigger timing, real serialized snapshots,
pending register changes on repeated restoration, and exact kernel bytes and
bounds. GTIA snapshots now preserve the synchronization cursor; older snapshots
without it default to the start-of-line baseline.

`Altirra_GTIAFrameMac` drives actual 262-line GTIA frames into an attached
AppKit display, then checks captured playfield/background pixels. It also binds
the real VBXE, programs a repeated SR XDL in its RAM, and checks both the
produced RGB frame and native capture against color-corrected palette output.
The deterministic test clock supplies ANTIC-facing inputs; this is not yet a
CPU-driven/booted machine. The display queue and rendering are production code.

The complete ANTIC emulator (`src/Altirra/source/antic.cpp`) is shared:
display-list execution, bitmap/text decode, scrolling, player/missile DMA,
refresh, WSYNC/RDY, NMI timing, tracing, and save states. Its platform kernels
keep SSE2 on Windows x64 and use bounded NEON text decode on macOS; the scalar
refresh contract is also checked on Windows, including the last valid refresh
cycle (106). The 128-byte DMA-pattern alignment and ring-buffer padding remain
intact. ANTIC cancels all scheduler callbacks at shutdown; the Windows simulator
explicitly detaches it before its scheduler and bus are destroyed.

`Altirra_ANTICRegisters`, `Altirra_ANTICTiming`, `Altirra_ANTICSnapshot`, and
`Altirra_ANTICKernels` cover masks and delayed CHBASE writes, WSYNC, DLI/VBI/RNMI,
serialized mid-line buffer/cursor continuation, repeated restoration, pending
event replacement, malformed queue/offset rejection, shutdown cancellation,
and exact SIMD/scalar bytes and bounds. Snapshot restoration fixes include the
queued-write index, copying the actual line buffers, retaining ring offsets,
and remembering the WSYNC held-bus value.

`Altirra_ANTICFrameMac` runs a real RAM-resident text display list through ANTIC
DMA and GTIA, then captures its native AppKit display. It checks display-list,
screen-name and glyph fetches, display-list history, indexed framebuffer bytes,
and captured palette RGB values. The test supplies a memory bus and clocks, not
a CPU or the full simulator; booting a machine and application wiring still
remain to be implemented.

The complete PIA core (`src/Altirra/source/pia.cpp`) is shared: port direction
and output registers, combined external and dynamic inputs, output callbacks,
CA/CB control and IRQ latches, floating port-B input decay, tracing, and saved
states. Counted-input kernels retain SSE2 on Windows x64 and use NEON on macOS;
both operate on exactly 32 aligned bytes and keep the original wrapping counts.

`Altirra_PIAInputs`, `Altirra_PIARegisters`, `Altirra_PIAIRQ`,
`Altirra_PIAFloating`, `Altirra_PIASnapshot`, and `Altirra_PIACountedInputs`
cover input allocation/exhaustion/reuse, callback filtering, dynamic inputs on
both ports, control edges and IRQ acknowledgement, scheduler-relative decay,
real JSON snapshot restoration, and scalar/SIMD equivalence with buffer guards.
Removing a non-final dynamic port-B input now moves the last entry from port B,
not port A. Restored PIA latches also reconcile their shared IRQ contributions
during `PostLoadState`, including loading a clear state over an asserted IRQ.

`Audio_PokeyRegisters` and `Audio_PokeyTimers` exercise the existing production
POKEY core with its real scheduler and renderer tables. Timer tests check the
cycle immediately before and at IRQ assertion for fast timer 1 and slow timers
2/4 at both 64 kHz and 15 kHz, retaining the divider phase after SKCTL reset and
STIMER reload. They also verify active-low status and IRQEN acknowledgement.
These remain component tests: complete simulator/application wiring is still
needed before a machine can boot on macOS.

CPU profiling's complete history-analysis layer is shared in
`src/Altirra/source/profilebuilder.cpp`: sessions and ownership, instruction,
function, call-graph, basic-block and BASIC-line profiling, auxiliary counters,
automatic frame boundaries, frame merging, and inclusive call-graph statistics.
The original live `ATCPUProfiler` collector is also shared and now receives
history from the real main CPU. CPU status flags live in
the shared `cpuflags.h`, with their original values retained.

`Altirra_ProfileBuilder`, `Altirra_ProfileCounters`, `Altirra_ProfileFrames`,
and `Altirra_ProfileCallGraph` verify all five analysis modes, overlapping
history batches, global addresses, taken/untaken branches and page crossings,
redundant operations, call/return and IRQ contexts, PC/function/VBlank frame
boundaries, discarded intervals, and exact exclusive/inclusive cycle totals.
Session reuse and self-move retain correct ownership. Merging now preserves
separate main/interrupt rows at the same PC; BASIC-line and call-graph address
records explicitly initialize their context instead of exposing allocator bits.
These are deterministic history-analysis tests. Separate main-CPU tests below
exercise execution and live collection; full simulator wiring remains unfinished.

The CPU heat-map analysis (`src/Altirra/source/cpuheatmap.cpp`) is now shared:
64K memory provenance/access/bit-validity tracking, register and stack
propagation, the original opcode coverage, early/normal trap configuration,
and diagnostic output through the real simulator event manager and console.
Its common entry point takes the post-instruction stack pointer; the Windows
adapter retains the existing CPU-shaped API and obtains that value from the
real CPU. No substitute CPU or reduced opcode implementation is used.

`Altirra_CPUHeatMapMemory`, `Altirra_CPUHeatMapPropagation`,
`Altirra_CPUHeatMapStack`, `Altirra_CPUHeatMapTraps`, and
`Altirra_CPUHeatMapDiagnostics` cover full-map/range clipping, all load/store
and memory ALU addressing variants, all 24 memory read-modify-write variants,
known/unknown data, stack wrapping, isolated trap flags, early/normal switching,
and separately known N/V diagnostic bits. Large test instances are heap-owned
for Windows' default stack limit. Regression fixes include LDX Z validity
using X instead of A, hardware-store trap selection, hardware RMW classification
and preservation, INC/DEC RAM provenance, and the diagnostic V-bit mask.
Immediate arithmetic keeps the original heat-map lineage behavior.
This layer analyzes completed instructions; it does not itself execute a CPU.
The main CPU and its heat-map adapter are now shared too; complete
simulator/application integration remains to be ported.

The complete CPU verifier (`src/Altirra/source/verifier.cpp`) is shared,
including address-zero, non-canonical hardware, indexed 64K wrap, stack,
IRQ/NMI register preservation and recursion, kernel-entry, SIO calling and
display-list loading checks, plus abnormal-DMA notifications. Its connections
read explicit CPU observations and machine services; the Windows adapter binds
them to the real CPU, simulator memory/ROM queries, and ANTIC. Observations and
flag/mapping queries remain live across synchronous failure callbacks, rather
than caching an instruction-wide snapshot. CPU mode definitions now share the
original byte-sized `cpumode.h` enum.

`Altirra_CPUVerifierAddressing`, `Altirra_CPUVerifierStack`,
`Altirra_CPUVerifierInterrupts`, `Altirra_CPUVerifierKernel`, and
`Altirra_CPUVerifierMachine` cover all original indexed-opcode lists, hardware
alias boundaries, emulation/native stack checks, IRQ/NMI bookkeeping and reset,
callback registration/reinitialization/shutdown, sorted kernel exceptions,
live callback changes, and actual ANTIC display-list enable/address state with
modular half-open SIO read ranges. Large ANTIC fixtures are heap-owned for the
Windows stack limit. PEA/PEI now check two-byte pushes, not pops; legal kernel
entry exceptions no longer skip independent SIO checks. Out-of-range exception
addresses cannot truncate and corrupt the sorted 16-bit set. Display-list
overlap retains the original console-only warning behavior.
Reentrant shutdown from either notifying SIO/ROM check stops the current
instruction's remaining checks before accessing released machine connections;
later observer entry points are harmless after shutdown. A per-initialization
lifetime token also permits synchronous deletion by the failure callback
(the real simulator's disable path) without reading the destroyed observer.
These verify the original observer. Main CPU execution is now shared too;
remaining debugger dependencies and full machine/application wiring are unfinished.

The complete breakpoint manager (`src/Altirra/source/bkptmanager.cpp`) is shared:
per-target PC and instruction breakpoints, primary-CPU global XPC matching,
24-bit read/write breakpoints, half-open ranges, 256-byte memory trap layers,
event policies, and noisy/silent interrupt selection. A Windows adapter binds
the original CPU/simulator API to actual machine methods; secondary targets
retain the existing debugger breakpoint interface. Memory traps run through
the real shared memory manager and pass through to RAM; debugger/ANTIC reads
do not spuriously report CPU access breakpoints.

`Altirra_BreakpointPC`, `Altirra_BreakpointInstructions`,
`Altirra_BreakpointTargets`, `Altirra_BreakpointMemory`, and
`Altirra_BreakpointRanges` cover ID allocation/reuse, aliases and XPC, ordinary
and instruction traps, detach/reattach, synchronous one-shot removal and
pending-ID reuse, shutdown, overlapping ranges, bank isolation, and overflow-
safe range clipping. A real `ATCoProc6502` executes an LDA before stopping at
the next PC through the production target breakpoint implementation; its
scheduler has a bounded deadline. Separate main-CPU execution tests are
described below.

Clearing the final instruction breakpoint now restores the correct addresses
on its own target. Reattachment restores retained breakpoint metadata; clearing
a range preserves the attribute-gate union of all banks. Dispatch snapshots
and generations make one-shot mutation and immediate ID reuse safe. Shutdown
invalidates the dispatch lifetime and releases target handlers and memory
layers, including repeated shutdown/reinitialization. Both platforms run
`Altirra_MainCPUBreakpoints` against the actual main CPU: setting every
breakpoint retains the correct internal count, and clearing all removes only
the breakpoint bits before ordinary PC breakpoints are restored.

The full disassembler (`src/Altirra/source/disasm.cpp`) is shared, retaining
the 6502/65C02/65C816 tables, Z80 prefix families, 8048/8051 and 6809 decoders,
operand spans, live/history references, labels, opcode lengths, anchor search,
and width prediction. The explicit formatter takes optional symbol/path-label
services without global machine state. The Windows adapter retains the legacy
entry points bound to the real debugger, CPU and simulator; portable callers
use the explicit services and existing debug-target interface. CPU submode
definitions now share `cpumode.h` with their original byte-sized values.

`Altirra_Disasm6502`, `Altirra_DisasmOtherCPUs`, `Altirra_DisasmContext`,
`Altirra_DisasmReferences`, `Altirra_DisasmSymbols`, and
`Altirra_DisasmPrediction` check instruction goldens, every opcode in the
6502-family width states, all base-byte values for other decoders and Z80
prefixes, register conversion, bank-wrapped instruction capture, live versus
historical indirect addresses, access widths, symbol flags, namespaces,
operand/comment bounds, and exhaustive valid P/E prediction combinations.
The reference fixture has actual separate 24-bit RAM backed by the production
memory manager; fat fixtures are heap-owned. Address-read comparisons do not
depend on the compiler's evaluation order for independent debug reads.

Regressions fix NMOS indirect JMP's page wrap, 65C816 indirect JMP's PBK,
8051 context selection and AJMP's post-instruction 2K page, wrapped branch
symbol requests, disabled bit-branch symbols, and long `cart:` PC prefixes.
These are debugger-component tests. Complete simulator/application integration
still remains to be ported before an Atari can boot.

The complete main CPU is shared in `cpu.cpp`, `cpumachine.inl`, `cpu6502.cpp`,
`cpu6502ill.cpp`, `cpu65c02.cpp`, and `cpu65c816.cpp`. This retains the original
cycle engine, all decoder tables including undocumented 6502 instructions,
65C816 emulation/native widths, accelerated execution, slow-bus retries,
interrupts, hooks, debugger steps, execution history, and snapshots. The CPU's
heat-map binding and live profiler collector are shared as well. Windows keeps
its legacy simulator-bound disassembly console adapter; the macOS adapter reads
the attached CPU memory bus through explicit disassembler services.

`Altirra_MainCPU6502`, `Altirra_MainCPU65C02`, `Altirra_MainCPU816`, and
`Altirra_MainCPUAcceleration` execute real programs through the production CPU,
memory manager, and schedulers. They check addressing and bank/page crossings,
cycle counts, decimal arithmetic, stack/control flow, read-modify-write bus
transactions, undocumented instructions, native widths, DMA, and slow handlers.
`Altirra_MainCPUHistory`, `Altirra_MainCPUSnapshot`, and `Altirra_MainCPUDebug`
check wraparound at `$FFFF`, 24 snapshot cut points with continued bus timing
and memory comparisons, actual breakpoint/hook dispatch, and IRQ/NMI entry/RTI.
`Altirra_MainCPUDiagnostics` verifies bus-backed reference formatting and native
macOS diagnostic output without generating CPU accesses or inventing a clock.
Large fixtures are heap-owned for Windows' default stack limit.

`Altirra_CPULiveProfiler` collects real executing CPU history in all five modes,
checks frame lengths and BASIC line memory, the two-scanline sampling cadence,
restart/end, and destruction with pending scheduler callbacks. Regressions fix
debugger P/E/D setters' decode selection and index truncation, history-map
indexing at `$FFFF`, the reported breakpoint count, and accelerated snapshot
phase restoration. History register copies use `memcpy` to avoid aliasing byte
storage as a DWORD. Profiler restart/destruction cancels pending events and
BASIC sampling keeps its original two-scanline interval on every update.

This is real main-CPU execution on macOS, still within component tests. The
complete simulator, device/application lifecycle, debugger command handling,
and booted-machine connection to the native AppKit UI remain unfinished.

The full Covox DAC and both device wrappers (`src/Altirra/source/covox.cpp`)
are shared. This includes mono/four-channel register decoding, configurable
memory overlays and write passthrough, scheduler-timestamped audio edges,
stereo/mono mixing transitions, enable-controller notifications, latched-output
snapshots, and SimCovox's actual PIA port-A connection. The original edge
renderer processes the generated audio; no alternate DAC algorithm is used.

`Altirra_CovoxMemory`, `Altirra_CovoxAudio`, `Altirra_CovoxSnapshot`,
`Altirra_CovoxDevice`, and `Altirra_SimCovox` cover exact-range/alias decoding,
page crossings and the end of the 16-bit bus, passthrough/consumed writes,
scheduled edge coalescing and sample goldens, 32-bit clock wrap, reset,
serialized snapshot continuation and mode rejection, factory settings,
controller replacement, and PIA callback release/reinitialization. A real
6502 program writes through the shared memory manager into Covox, with the
resulting audio edges checked against its actual instruction cycles.

Mono mixing now discards the duplicate right-channel edges after submitting
the left buffer for both outputs, preventing their replay on later stereo
frames. A mono write always updates all four DAC latches even when their
summed output stays unchanged. Controller replacement removes the prior
notification; POKEY-page passthrough policy is consistent before/after Init.
Invalid ranges/settings cannot wrap or silently alias the 16-bit bus.
The existing exclusive high bound reported to the simulator's HLE overlap
checks is preserved; it differs from the older interface-header comment.
Native device configuration UI and full machine wiring remain unfinished.

The full SlightSID expansion (`src/Altirra/source/slightsid.cpp`) is shared,
including its device factory, `$D500-D51F` register mapping, three oscillators,
ADSR rate tables, pulse/triangle/saw/noise combinations, sync/ring controls,
5:9 clock divider, box downsampling, and recursive LP/BP/HP filter. This keeps
the original scalar synthesizer and its documented approximate noise, analog
filter, and gain behavior. It does not substitute another SID implementation.

`Altirra_SlightSIDTiming`, `Altirra_SlightSIDEnvelopes`,
`Altirra_SlightSIDWaveforms`, `Altirra_SlightSIDFilters`, and
`Altirra_SlightSIDDevice` check divider remainders, phase and scheduler wrap,
elapsed-time writes, TEST reset/hold, attack/sustain/release boundaries, reset
with pending time, repeatable noise, combined waveforms, voice-three disable,
filter linearity, volume scaling, additive mixing and guarded sample buffers
through the original 1536-sample capacity. Factory tests verify actual memory
reads/writes, reserved register decoding, diagnostics and paired shutdown/reinit.
A real main 6502 executes STA/LDA through the production memory manager to
program SID registers and read the board's constant `$33` value.

Regressions fix an always-zero TEST-bit expression and stop phase advancement
while TEST is set, flush old elapsed time before resetting voice state, and
retain the logical-AND pulse/noise result when saw is also selected. The TEST
phase contract follows the [MOS 6581 datasheet](https://myoldcomputer.nl/Files/mos_6581_sid.pdf).
Complete simulator/audio-output/UI wiring remains unfinished.

The complete MMU (`src/Altirra/source/mmu.cpp`) is shared. Its ROM control,
PORTB bank tables, separate CPU/ANTIC banking, 256K main-RAM aliases, RAMBO and
Compy layouts, XEGS game ROM, hidden RAM, Axlon selectors/mirrors, linear 65C816
RAM, mode overrides and ROM-mapping observers retain the original algorithms.
The dependency on `simulator.h` was only for constants; the shared core now
includes `constants.h` directly. Configuring banked memory while mapping is
detached no longer forms a pointer into a null backing allocation.

`Altirra_MMUROM` checks all 256 PORTB values for every hardware trait set,
using actual layered ROM/RAM reads. `Altirra_MMUBanking` checks every PORTB
value in all seven extended-memory layouts, independent CPU/ANTIC reads,
writes to the correct backing bank, physical offsets, BASIC/self-test gating,
and 256K main-RAM aliases. These fixtures supply explicit ROM contents and
memory layers; they do not emulate the still-unported complete simulator.

`Altirra_MMUAxlon`, `Altirra_MMUHighMemory`, `Altirra_MMULifecycle`, and
`Altirra_MMUConnections` check all supported Axlon bank widths, low/high control
decoding and gating, full 24-bit linear RAM bounds, replacement/removal,
overrides across detached mappings, reinitialization, immediate/coherent ROM
notifications and self-removal. Actual 6502 code programs real PIA DDRB/PORTB
registers to switch two extended banks while ANTIC retains base RAM; actual
65C816 long instructions access the MMU's bank-one mapping. CPU timing comes
from the production scheduler and the memory paths are the production manager.
Full machine lifecycle, firmware boot and native application wiring remain
unfinished.
