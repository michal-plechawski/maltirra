#!/usr/bin/env bash
set -euo pipefail

repository_root=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
cd "$repository_root"

windows_include_pattern='^[[:space:]]*#[[:space:]]*include[[:space:]]*[<"](windows|windowsx|commctrl|commdlg|shellapi|shlobj|shlwapi|d3d[^/>]*|dxgi[^/>]*|xaudio[^/>]*|mmdeviceapi|audioclient|winsock[^/>]*|ws2tcpip|wrl)[.h>"/]'
platform_include_pattern='^[[:space:]]*#[[:space:]]*include[[:space:]]*[<"][^>"]*(vd2/system/win32|platform[/\\]Windows_x64)[/\\]'

include_violations=$(
  rg -n -i \
    --glob '!src/platform/**' \
    --glob '*.{h,hpp,c,cc,cpp,cxx,inl}' \
    "$windows_include_pattern" \
    src || true
)

platform_include_violations=$(
  rg -n -i \
    --glob '!src/platform/**' \
    --glob '*.{h,hpp,c,cc,cpp,cxx,inl}' \
    "$platform_include_pattern" \
    src || true
)

name_violations=$(
  find src \
    -path 'src/platform' -prune -o \
    -type f -print |
    rg -i '(^|/)[^/]*(win32|windows|x64|amd64)[^/]*\.(h|hpp|c|cc|cpp|cxx|inl|asm|manifest)$' || true
)

if [[ -n "$include_violations" || -n "$platform_include_violations" || -n "$name_violations" ]]; then
  if [[ -n "$include_violations" ]]; then
    printf '%s\n' 'Windows SDK includes found outside src/platform/Windows_x64:'
    printf '%s\n' "$include_violations"
  fi

  if [[ -n "$platform_include_violations" ]]; then
    printf '%s\n' 'Platform-specific include paths found outside src/platform/Windows_x64:'
    printf '%s\n' "$platform_include_violations"
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
  '#include <vd2/system/int128.h>' \
  '#include <vd2/system/memory.h>' \
  '#include <vd2/system/refcount.h>' \
  '#include <vd2/system/seh.h>' \
  '#include <vd2/system/thread.h>' \
  '#include <vd2/system/time.h>' \
  '#include <vd2/system/unknown.h>' \
  'int main() { return 0; }' |
  "$compiler" -std=c++23 -Isrc/h -x c++ -fsyntax-only -

printf '%s\n' 'Windows x64 platform boundary is clean.'
