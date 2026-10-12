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
These remain component tests: main CPU execution and simulator/application
wiring still need to be ported before a machine can boot on macOS.

CPU profiling's complete history-analysis layer is shared in
`src/Altirra/source/profilebuilder.cpp`: sessions and ownership, instruction,
function, call-graph, basic-block and BASIC-line profiling, auxiliary counters,
automatic frame boundaries, frame merging, and inclusive call-graph statistics.
The original live `ATCPUProfiler` collector remains in the Windows subtree
until the main CPU is ported; the shared builder processes actual history-entry
formats without a fabricated CPU or simulator. CPU status flags now live in
the shared `cpuflags.h`, with their original values retained.

`Altirra_ProfileBuilder`, `Altirra_ProfileCounters`, `Altirra_ProfileFrames`,
and `Altirra_ProfileCallGraph` verify all five analysis modes, overlapping
history batches, global addresses, taken/untaken branches and page crossings,
redundant operations, call/return and IRQ contexts, PC/function/VBlank frame
boundaries, discarded intervals, and exact exclusive/inclusive cycle totals.
Session reuse and self-move retain correct ownership. Merging now preserves
separate main/interrupt rows at the same PC; BASIC-line and call-graph address
records explicitly initialize their context instead of exposing allocator bits.
These are deterministic history-analysis tests, not CPU execution tests; main
instruction execution and its debugger/simulator integration remain unfinished.

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
Main CPU execution, its remaining debugger dependencies, and the complete
simulator/application integration still remain to be ported.

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
later observer entry points are harmless after shutdown.
These verify the original observer, not an executing substitute CPU. Main CPU
execution, remaining debugger dependencies, and full machine/application
wiring are still unfinished.
