#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
STATE_DIR="${SCRIPT_DIR}/state"

STREAM_ID="${1:-}"
if [[ -z "${STREAM_ID}" ]]; then
  echo "Usage: $0 <streamId>"
  exit 1
fi

PID_FILE="${STATE_DIR}/${STREAM_ID}.pid"

if [[ ! -f "${PID_FILE}" ]]; then
  echo "[encoder-pusher] stream not running: ${STREAM_ID}"
  exit 0
fi

PID="$(cat "${PID_FILE}")"
if kill -0 "${PID}" >/dev/null 2>&1; then
  kill "${PID}" || true
  echo "[encoder-pusher] stream stopped: ${STREAM_ID}, pid=${PID}"
else
  echo "[encoder-pusher] stream pid not alive: ${STREAM_ID}, pid=${PID}"
fi

rm -f "${PID_FILE}"
