#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
: "${EMXX:=em++}"
mkdir -p web/public/wasm
"$EMXX" src/core/thermal3d.cpp src/cli/browser_bridge.cpp -Isrc -Ithird_party \
  -std=c++17 -O2 -fexceptions --no-entry -sDISABLE_EXCEPTION_CATCHING=0 \
  -sMODULARIZE=1 -sEXPORT_ES6=1 -sENVIRONMENT=web,worker,node \
  -sEXPORTED_FUNCTIONS='["_thermal_compare"]' -sEXPORTED_RUNTIME_METHODS='["ccall"]' \
  -sINITIAL_MEMORY=33554432 -sALLOW_MEMORY_GROWTH=1 -sMAXIMUM_MEMORY=67108864 \
  -o web/public/wasm/thermal.mjs
node tools/browser-manifest.mjs
