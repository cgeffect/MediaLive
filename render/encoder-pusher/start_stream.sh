#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
STATE_DIR="${SCRIPT_DIR}/state"
mkdir -p "${STATE_DIR}"
BIN="${SCRIPT_DIR}/build/encoder_pusher"

STREAM_ID="${1:-}"
if [[ -z "${STREAM_ID}" ]]; then
  echo "Usage: $0 <streamId> [additional push_stream.sh args...]"
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
  bash "${SCRIPT_DIR}/build.sh"
fi

"${BIN}" --stream-id "${STREAM_ID}" "$@" >"${LOG_FILE}" 2>&1 &
PID=$!
echo "${PID}" > "${PID_FILE}"

echo "[encoder-pusher] stream started: ${STREAM_ID}, pid=${PID}, log=${LOG_FILE}"
