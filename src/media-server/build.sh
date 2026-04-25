#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/../.." && pwd)"
THIRD_PARTY_BUILD_SCRIPT="${ROOT_DIR}/src/third-party/build.sh"
BUILD_DIR="${SCRIPT_DIR}/build"

bash "${THIRD_PARTY_BUILD_SCRIPT}"

cmake -S "${SCRIPT_DIR}" -B "${BUILD_DIR}" -DCMAKE_BUILD_TYPE=Release
cmake --build "${BUILD_DIR}" -j "$(sysctl -n hw.logicalcpu)"

echo "[media-server] binary ready: ${BUILD_DIR}/embedded_server"
