#!/usr/bin/env bash
set -euo pipefail

repository_root=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
cd "$repository_root"

windows_include_pattern='^[[:space:]]*#[[:space:]]*include[[:space:]]*[<"](windows|windowsx|commctrl|commdlg|shellapi|shlobj|shlwapi|d3d[^/>]*|dxgi[^/>]*|xaudio[^/>]*|mmdeviceapi|audioclient|winsock[^/>]*|ws2tcpip|wrl)[.h>"/]'

include_violations=$(
  rg -n -i \
    --glob '!src/platform/**' \
    --glob '*.{h,hpp,c,cc,cpp,cxx,inl}' \
    "$windows_include_pattern" \
    src || true
)

name_violations=$(
  find src \
    -path 'src/platform' -prune -o \
    -type f -print |
    rg -i '(^|/)[^/]*(win32|windows|x64|amd64)[^/]*\.(h|hpp|c|cc|cpp|cxx|inl|asm|manifest)$' || true
)

if [[ -n "$include_violations" || -n "$name_violations" ]]; then
  if [[ -n "$include_violations" ]]; then
    printf '%s\n' 'Windows SDK includes found outside src/platform/Windows_x64:'
    printf '%s\n' "$include_violations"
  fi

  if [[ -n "$name_violations" ]]; then
    printf '%s\n' 'Windows/x64-specific filenames found outside src/platform/Windows_x64:'
    printf '%s\n' "$name_violations"
  fi

  exit 1
fi

printf '%s\n' 'Windows x64 platform boundary is clean.'
