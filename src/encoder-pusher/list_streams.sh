#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
STATE_DIR="${SCRIPT_DIR}/state"

if [[ ! -d "${STATE_DIR}" ]]; then
  echo "[encoder-pusher] no stream state"
  exit 0
fi

shopt -s nullglob
for pid_file in "${STATE_DIR}"/*.pid; do
  stream_id="$(basename "${pid_file}" .pid)"
  pid="$(cat "${pid_file}")"
  if kill -0 "${pid}" >/dev/null 2>&1; then
    echo "${stream_id} running pid=${pid}"
  else
    echo "${stream_id} stale pid=${pid}"
  fi
done
