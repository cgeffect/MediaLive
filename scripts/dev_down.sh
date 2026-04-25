#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

stop_by_pid_file() {
  local name="$1"
  local pid_file="$2"

  if [[ -f "${pid_file}" ]]; then
    local pid
    pid="$(cat "${pid_file}")"
    if kill -0 "${pid}" >/dev/null 2>&1; then
      kill "${pid}" || true
      echo "[${name}] stopped pid=${pid}"
    else
      echo "[${name}] pid file exists but process not running"
    fi
    rm -f "${pid_file}"
  else
    echo "[${name}] not running"
  fi
}

stop_by_pid_file "web" "${ROOT_DIR}/web/web.pid"
stop_by_pid_file "encoder-pusher" "${ROOT_DIR}/render/encoder-pusher/pusher.pid"
if [[ -d "${ROOT_DIR}/render/encoder-pusher/state" ]]; then
  for stream_pid in "${ROOT_DIR}/render/encoder-pusher/state/"*.pid; do
    [[ -e "${stream_pid}" ]] || continue
    stop_by_pid_file "encoder-pusher-stream" "${stream_pid}"
  done
fi
stop_by_pid_file "media-server" "${ROOT_DIR}/render/media-server/zlm.pid"
