#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ZLM_DIR="${SCRIPT_DIR}/ZLMediaKit"
BUILD_DIR="${ZLM_DIR}/build"

if [[ ! -d "${ZLM_DIR}" ]]; then
  echo "[media-server] cloning ZLMediaKit..."
  git clone --depth=1 https://github.com/ZLMediaKit/ZLMediaKit.git "${ZLM_DIR}"
fi

if [[ ! -x "${BUILD_DIR}/MediaServer" ]]; then
  echo "[media-server] building ZLMediaKit..."
  cmake -S "${ZLM_DIR}" -B "${BUILD_DIR}" -DCMAKE_BUILD_TYPE=Release
  cmake --build "${BUILD_DIR}" -j "$(sysctl -n hw.logicalcpu)"
fi

echo "[media-server] ZLMediaKit is ready"
