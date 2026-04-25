#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/../.." && pwd)"
LOG_FILE="${SCRIPT_DIR}/zlm.log"
PID_FILE="${SCRIPT_DIR}/zlm.pid"
TOP_BIN="${ROOT_DIR}/build/render/media-server/embedded_server"
LOCAL_BIN="${SCRIPT_DIR}/build/embedded_server"
BIN="${TOP_BIN}"
CONF="${SCRIPT_DIR}/conf/config.ini"

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
