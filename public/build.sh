#!/usr/bin/env bash
# ─────────────────────────────────────────────────────────────────────────────
# build.sh — Compile C++ engine to WebAssembly using Emscripten
#
# Prerequisites:
#   1. Install Emscripten SDK:
#        git clone https://github.com/emscripten-core/emsdk.git
#        cd emsdk && ./emsdk install latest && ./emsdk activate latest
#        source ./emsdk_env.sh
#
#   2. Run this script from the project root:
#        chmod +x build.sh && ./build.sh
# ─────────────────────────────────────────────────────────────────────────────
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRC="$SCRIPT_DIR/src/engine.cpp"
OUT="$SCRIPT_DIR/public/engine.js"

echo "╔══════════════════════════════════════════════════╗"
echo "║    Algorithm Flight Recorder — WASM Build         ║"
echo "╚══════════════════════════════════════════════════╝"
echo ""

# Check for emcc
if ! command -v emcc &>/dev/null; then
  echo "❌  emcc not found."
  echo "    Install Emscripten: https://emscripten.org/docs/getting_started/downloads.html"
  echo "    Then run: source emsdk/emsdk_env.sh"
  exit 1
fi

echo "✓  emcc found: $(emcc --version | head -1)"
echo "→  Compiling $SRC …"
echo ""

emcc "$SRC" \
  -o "$OUT" \
  -sWASM=1 \
  -sEXPORTED_FUNCTIONS='["_run_algorithm","_get_steps_json","_get_history_json","_free_result","_malloc","_free"]' \
  -sEXPORTED_RUNTIME_METHODS='["ccall","cwrap","UTF8ToString","stringToUTF8","lengthBytesUTF8"]' \
  -sALLOW_MEMORY_GROWTH=1 \
  -sMODULARIZE=1 \
  -sEXPORT_NAME="AlgoEngineWASM" \
  -sUSE_SQLITE3=1 \
  -O2 \
  -std=c++17

echo ""
echo "✅  Build complete!"
echo "    Output: $OUT"
echo "    WASM:   ${OUT%.js}.wasm"
echo ""
echo "Next: Update index.html to load engine.js instead of engine.sim.js"
echo "      and call AlgoEngineWASM() to get the module, then use ccall/cwrap."
