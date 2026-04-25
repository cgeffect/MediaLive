#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
LOG_FILE="${SCRIPT_DIR}/zlm.log"
PID_FILE="${SCRIPT_DIR}/zlm.pid"
BIN="${SCRIPT_DIR}/build/embedded_server"
CONF="${SCRIPT_DIR}/conf/config.ini"

if [[ ! -x "${BIN}" ]]; then
  bash "${SCRIPT_DIR}/build.sh"
fi

if [[ -f "${PID_FILE}" ]] && kill -0 "$(cat "${PID_FILE}")" >/dev/null 2>&1; then
  echo "[media-server] already running (pid=$(cat "${PID_FILE}"))"
  exit 0
fi

if [[ ! -f "${CONF}" ]]; then
  echo "[media-server] config missing: ${CONF}"
  exit 1
fi

echo "[media-server] starting embedded_server..."
"${BIN}" "${CONF}" >"${LOG_FILE}" 2>&1 &
PID=$!
echo "${PID}" > "${PID_FILE}"

sleep 1
if kill -0 "${PID}" >/dev/null 2>&1; then
  echo "[media-server] started pid=${PID}, log=${LOG_FILE}"
else
  echo "[media-server] failed to start, check ${LOG_FILE}"
  exit 1
fi
