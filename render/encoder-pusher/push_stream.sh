#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
LOG_FILE="${SCRIPT_DIR}/pusher.log"
PID_FILE="${SCRIPT_DIR}/pusher.pid"
BIN="${SCRIPT_DIR}/build/encoder_pusher"

STREAM_ID="stream1"
HOST="127.0.0.1"
PORT="1935"
APP="live"
INPUT_FILE=""
FPS="30"
WIDTH="1280"
HEIGHT="720"
AUDIO_FREQ="1000"

while [[ $# -gt 0 ]]; do
  case "$1" in
    --stream-id)
      STREAM_ID="$2"; shift 2 ;;
    --host)
      HOST="$2"; shift 2 ;;
    --port)
      PORT="$2"; shift 2 ;;
    --app)
      APP="$2"; shift 2 ;;
    --input)
      INPUT_FILE="$2"; shift 2 ;;
    --fps)
      FPS="$2"; shift 2 ;;
    --width)
      WIDTH="$2"; shift 2 ;;
    --height)
      HEIGHT="$2"; shift 2 ;;
    --audio-freq)
      AUDIO_FREQ="$2"; shift 2 ;;
    *)
      echo "Unknown arg: $1"
      echo "Usage: $0 [--stream-id stream1] [--host 127.0.0.1] [--port 1935] [--app live] [--input /path/to/file.mp4]"
      exit 1 ;;
  esac
done

if [[ -f "${PID_FILE}" ]] && kill -0 "$(cat "${PID_FILE}")" >/dev/null 2>&1; then
  echo "[encoder-pusher] already running (pid=$(cat "${PID_FILE}"))"
  exit 0
fi

if [[ ! -x "${BIN}" ]]; then
  bash "${SCRIPT_DIR}/build.sh"
fi

CMD=(
  "${BIN}"
  --stream-id "${STREAM_ID}"
  --host "${HOST}"
  --port "${PORT}"
  --app "${APP}"
  --fps "${FPS}"
  --width "${WIDTH}"
  --height "${HEIGHT}"
  --audio-freq "${AUDIO_FREQ}"
)

if [[ -n "${INPUT_FILE}" ]]; then
  if [[ ! -f "${INPUT_FILE}" ]]; then
    echo "[encoder-pusher] input not found: ${INPUT_FILE}"
    exit 1
  fi
  CMD+=(--input "${INPUT_FILE}")
fi

echo "[encoder-pusher] starting C++ pusher..."
"${CMD[@]}" >"${LOG_FILE}" 2>&1 &
PID=$!
echo "${PID}" > "${PID_FILE}"

echo "[encoder-pusher] started pid=${PID}, log=${LOG_FILE}"
