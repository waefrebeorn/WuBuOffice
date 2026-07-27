#!/usr/bin/env bash
# smoke.sh -- modular, fast test runner for WuBuOffice.
#
# Replaces the wasteful "run all 80 tests (+10-min CNN smoke) on every change"
# habit. Tests are tagged with module LABELS (see tests/CMakeLists.txt), so we
# can run just the module you touched.
#
# Usage:
#   ./tools/smoke.sh                 # fast: everything EXCEPT the slow ocr suite
#   ./tools/smoke.sh apps           # GUI-shell interactive suite (wubuos views + plugin ABI)
#   ./tools/smoke.sh spell           # only the spell module
#   ./tools/smoke.sh chart math draw # several modules at once
#   ./tools/smoke.sh ocr             # the full (slow) OCR/CNN battery
#   ./tools/smoke.sh all             # literally everything (long)
#
# It rebuilds the requested test targets first (incremental), then runs ctest
# filtered by label. Requires a configured build dir at ./build (or $BUILD_DIR).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$ROOT/build}"
cd "$ROOT"

if [ ! -d "$BUILD_DIR" ]; then echo "no build dir at $BUILD_DIR; run cmake -B build . first" >&2; exit 1; fi

MODE="${1:-fast}"

if [ "$MODE" = "fast" ]; then
  echo "== smoke: fast (all modules except 'ocr') =="
  cmake --build "$BUILD_DIR" -j"$(nproc)" >/dev/null
  ctest --test-dir "$BUILD_DIR" -LE ocr --output-on-failure
elif [ "$MODE" = "apps" ]; then
  echo "== smoke: GUI-shell interactive suite (apps_smoke) =="
  cmake --build "$BUILD_DIR" --target apps_smoke -j"$(nproc)" >/dev/null
  cmake --build "$BUILD_DIR" -j"$(nproc)" >/dev/null
  ctest --test-dir "$BUILD_DIR" -L apps --output-on-failure
elif [ "$MODE" = "all" ]; then
  echo "== smoke: ALL tests (including slow ocr) =="
  cmake --build "$BUILD_DIR" -j"$(nproc)" >/dev/null
  ctest --test-dir "$BUILD_DIR" --output-on-failure
elif [ "$MODE" = "ocr" ]; then
  echo "== smoke: ocr suite ONLY (slow) =="
  cmake --build "$BUILD_DIR" -j"$(nproc)" >/dev/null
  ctest --test-dir "$BUILD_DIR" -L ocr --output-on-failure
else
  echo "== smoke: modules: $* =="
  # build the matching test executables, then run by label
  cmake --build "$BUILD_DIR" -j"$(nproc)" >/dev/null
  LABELS=("$@")
  # ctest -L takes a regex; join labels with |
  RE=$(IFS='|'; echo "${LABELS[*]}")
  ctest --test-dir "$BUILD_DIR" -L "$RE" --output-on-failure
fi
