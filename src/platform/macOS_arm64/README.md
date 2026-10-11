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
ANTIC, full GTIA frame orchestration, and simulator integration are still in
the Windows-only subtree.

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
