#!/usr/bin/env bash
set -euo pipefail

repository_root=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
cd "$repository_root"

windows_include_pattern='^[[:space:]]*#[[:space:]]*include[[:space:]]*[<"](windows|windowsx|commctrl|commdlg|shellapi|shlobj|shlwapi|d3d[^/>]*|dxgi[^/>]*|xaudio[^/>]*|mmdeviceapi|audioclient|winsock[^/>]*|ws2tcpip|wrl)[.h>"/]'
platform_include_pattern='^[[:space:]]*#[[:space:]]*include[[:space:]]*[<"][^>"]*(vd2/system/win32|platform[/\\]Windows_x64)[/\\]'
compiler_intrinsic_pattern='^[[:space:]]*#[[:space:]]*include[[:space:]]*[<"](intrin|emmintrin|immintrin|xmmintrin)[.]h[>"]'
architecture_impl_pattern='(__m(64|128|256)|_mm[0-9]*_|_Interlocked|__shift(left|right)128)'

include_violations=$(
  git grep -n -i -E \
    "$windows_include_pattern" \
    -- src ':!src/platform/**' || true
)

platform_include_violations=$(
  git grep -n -i -E \
    "$platform_include_pattern" \
    -- src ':!src/platform/**' || true
)

compiler_intrinsic_violations=$(
  git grep -n -i -E \
    "$compiler_intrinsic_pattern" \
    -- src ':!src/platform/**' ':!src/h/vd2/system/intrin.h' || true
)

architecture_impl_violations=$(
  git grep -n -E \
    "$architecture_impl_pattern" \
    -- src ':!src/platform/**' \
    ':!src/Shared/altirra.natvis' \
    ':!src/h/vd2/system/atomic.h' \
    ':!src/h/vd2/system/int128.h' \
    ':!src/h/vd2/system/math.h' || true
)

name_violations=$(
  find src \
    -path 'src/platform' -prune -o \
    -type f -print |
    grep -E -i '(^|/)[^/]*(win32|windows|x64|amd64)[^/]*\.(h|hpp|c|cc|cpp|cxx|inl|asm|manifest)$' || true
)

if [[ -n "$include_violations" || -n "$platform_include_violations" || -n "$compiler_intrinsic_violations" || -n "$architecture_impl_violations" || -n "$name_violations" ]]; then
  if [[ -n "$include_violations" ]]; then
    printf '%s\n' 'Windows SDK includes found outside src/platform/Windows_x64:'
    printf '%s\n' "$include_violations"
  fi

  if [[ -n "$platform_include_violations" ]]; then
    printf '%s\n' 'Platform-specific include paths found outside src/platform/Windows_x64:'
    printf '%s\n' "$platform_include_violations"
  fi

  if [[ -n "$compiler_intrinsic_violations" ]]; then
    printf '%s\n' 'Compiler intrinsic headers bypassing the platform abstraction:'
    printf '%s\n' "$compiler_intrinsic_violations"
  fi

  if [[ -n "$architecture_impl_violations" ]]; then
    printf '%s\n' 'Architecture-specific implementation found outside src/platform/Windows_x64:'
    printf '%s\n' "$architecture_impl_violations"
  fi

  if [[ -n "$name_violations" ]]; then
    printf '%s\n' 'Windows/x64-specific filenames found outside src/platform/Windows_x64:'
    printf '%s\n' "$name_violations"
  fi

  exit 1
fi

compiler=${CXX:-clang++}
printf '%s\n' \
  '#include <vd2/system/atomic.h>' \
  '#include <vd2/system/event.h>' \
  '#include <vd2/system/file.h>' \
  '#include <vd2/system/memory.h>' \
  '#include <vd2/system/refcount.h>' \
  '#include <vd2/system/seh.h>' \
  '#include <vd2/system/thread.h>' \
  '#include <vd2/system/time.h>' \
  '#include <vd2/system/unknown.h>' \
  '#include <vd2/system/vecmath.h>' \
  'int main() { return 0; }' |
  "$compiler" -std=c++23 -Isrc/h -x c++ -fsyntax-only -

printf '%s\n' 'Windows x64 platform boundary is clean.'
