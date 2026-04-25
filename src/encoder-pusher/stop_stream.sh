#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
STATE_DIR="${SCRIPT_DIR}/state"

stop_one_stream() {
  local stream_id="$1"
  local pid_file="${STATE_DIR}/${stream_id}.pid"

  if [[ ! -f "${pid_file}" ]]; then
    echo "[encoder-pusher] stream not running: ${stream_id}"
    return 0
  fi

  local pid
  pid="$(cat "${pid_file}")"
  if kill -0 "${pid}" >/dev/null 2>&1; then
    kill "${pid}" || true
    echo "[encoder-pusher] stream stopped: ${stream_id}, pid=${pid}"
  else
    echo "[encoder-pusher] stream pid not alive: ${stream_id}, pid=${pid}"
  fi

  rm -f "${pid_file}"
}

STREAM_ID="${1:-}"
if [[ -z "${STREAM_ID}" ]]; then
  if [[ ! -d "${STATE_DIR}" ]]; then
    echo "[encoder-pusher] no stream state"
    exit 0
  fi

  shopt -s nullglob
  pid_files=("${STATE_DIR}"/*.pid)
  if [[ ${#pid_files[@]} -eq 0 ]]; then
    echo "[encoder-pusher] no running streams"
    exit 0
  fi

  for pid_file in "${pid_files[@]}"; do
    stream_id="$(basename "${pid_file}" .pid)"
    stop_one_stream "${stream_id}"
  done
  exit 0
fi

stop_one_stream "${STREAM_ID}"
