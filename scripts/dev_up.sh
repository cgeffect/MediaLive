#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
MEDIA_DIR="${ROOT_DIR}/render/media-server"
PUSHER_DIR="${ROOT_DIR}/render/encoder-pusher"
WEB_DIR="${ROOT_DIR}/web"
WEB_PID_FILE="${ROOT_DIR}/web/web.pid"
WEB_LOG="${ROOT_DIR}/web/web.log"
INPUT_VIDEO="${INPUT_VIDEO:-${ROOT_DIR}/render/encoder-pusher/test.mp4}"

if [[ -f "${ROOT_DIR}/CMakeLists.txt" ]]; then
  echo "[build] building from top-level CMake..."
  cmake -S "${ROOT_DIR}" -B "${ROOT_DIR}/build"
  cmake --build "${ROOT_DIR}/build" -j "$(sysctl -n hw.logicalcpu)"
fi

bash "${MEDIA_DIR}/run_server.sh"
sleep 1
bash "${PUSHER_DIR}/push_stream.sh" --stream-id stream1 --input "${INPUT_VIDEO}"

if [[ -f "${WEB_PID_FILE}" ]] && kill -0 "$(cat "${WEB_PID_FILE}")" >/dev/null 2>&1; then
  echo "[web] already running (pid=$(cat "${WEB_PID_FILE}"))"
else
  echo "[web] starting static web server on :8080"
  python3 -m http.server 8080 --directory "${WEB_DIR}" >"${WEB_LOG}" 2>&1 &
  echo $! > "${WEB_PID_FILE}"
fi

echo ""
echo "All services are up:"
echo "- Player page: http://127.0.0.1:8080"
echo "- FLV stream : http://127.0.0.1:8081/live/stream1.live.flv"
echo ""
echo "Check logs:"
echo "- ${MEDIA_DIR}/zlm.log"
echo "- ${PUSHER_DIR}/pusher.log"
echo "- ${WEB_LOG}"
