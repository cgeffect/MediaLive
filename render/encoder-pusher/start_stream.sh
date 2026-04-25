#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/../.." && pwd)"
STATE_DIR="${SCRIPT_DIR}/state"
mkdir -p "${STATE_DIR}"
TOP_BIN="${ROOT_DIR}/build/render/encoder-pusher/encoder_pusher"
LOCAL_BIN="${SCRIPT_DIR}/build/encoder_pusher"
BIN="${TOP_BIN}"

STREAM_ID="${1:-}"
if [[ -z "${STREAM_ID}" ]]; then
  echo "Usage: $0 <streamId> [encoder_pusher args...]"
  exit 1
fi
shift || true

PID_FILE="${STATE_DIR}/${STREAM_ID}.pid"
LOG_FILE="${STATE_DIR}/${STREAM_ID}.log"

if [[ -f "${PID_FILE}" ]] && kill -0 "$(cat "${PID_FILE}")" >/dev/null 2>&1; then
  echo "[encoder-pusher] stream already running: ${STREAM_ID} (pid=$(cat "${PID_FILE}"))"
  exit 0
fi

if [[ ! -x "${BIN}" ]]; then
  BIN="${LOCAL_BIN}"
fi

if [[ ! -x "${BIN}" ]]; then
  if [[ -f "${ROOT_DIR}/CMakeLists.txt" ]]; then
    cmake -S "${ROOT_DIR}" -B "${ROOT_DIR}/build"
    cmake --build "${ROOT_DIR}/build" -j "$(sysctl -n hw.logicalcpu)"
  else
    bash "${SCRIPT_DIR}/build.sh"
  fi
fi

if [[ ! -x "${BIN}" ]]; then
  if [[ -x "${TOP_BIN}" ]]; then
    BIN="${TOP_BIN}"
  elif [[ -x "${LOCAL_BIN}" ]]; then
    BIN="${LOCAL_BIN}"
  fi
fi

"${BIN}" --stream-id "${STREAM_ID}" "$@" >"${LOG_FILE}" 2>&1 &
PID=$!
echo "${PID}" > "${PID_FILE}"

echo "[encoder-pusher] stream started: ${STREAM_ID}, pid=${PID}, log=${LOG_FILE}"
