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
