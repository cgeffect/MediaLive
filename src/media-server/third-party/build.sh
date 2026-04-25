#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ZLM_SRC_DIR="${SCRIPT_DIR}/ZLMediaKit"
BUILD_DIR="${SCRIPT_DIR}/build/zlm"
INSTALL_DIR="${SCRIPT_DIR}/install/zlm"
TMP_DIR="${SCRIPT_DIR}/.tmp"

if [[ ! -d "${ZLM_SRC_DIR}" ]]; then
  echo "[third-party] ZLMediaKit source not found: ${ZLM_SRC_DIR}"
  exit 1
fi

fetch_dep_if_missing() {
  local dep_dir="$1"
  local git_url="$2"
  local git_commit="$3"

  if [[ -d "${dep_dir}/.git" ]]; then
    local current_commit
    current_commit="$(git -C "${dep_dir}" rev-parse HEAD 2>/dev/null || true)"
    if [[ "${current_commit}" == "${git_commit}" ]]; then
      return 0
    fi
  fi

  if [[ -d "${dep_dir}" ]] && [[ ! -d "${dep_dir}/.git" ]] && [[ -n "$(ls -A "${dep_dir}" 2>/dev/null)" ]]; then
    return 0
  fi

  local dep_name
  dep_name="$(basename "${dep_dir}")"
  local clone_dir="${TMP_DIR}/${dep_name}"

  echo "[third-party] fetching missing dependency ${dep_name} from ${git_url}"
  rm -rf "${clone_dir}" "${dep_dir}"
  mkdir -p "${TMP_DIR}"
  git clone --depth=1 "${git_url}" "${clone_dir}"
  git -C "${clone_dir}" fetch --depth=1 origin "${git_commit}"
  git -C "${clone_dir}" checkout "${git_commit}"
  mkdir -p "$(dirname "${dep_dir}")"
  mv "${clone_dir}" "${dep_dir}"
}

# Some downloaded source zips miss git submodule contents.
fetch_dep_if_missing "${ZLM_SRC_DIR}/3rdpart/ZLToolKit" "https://github.com/ZLMediaKit/ZLToolKit.git" "6e727443e6ca41af91f0cc2736b7a3bea971fedf"
fetch_dep_if_missing "${ZLM_SRC_DIR}/3rdpart/media-server" "https://github.com/ireader/media-server.git" "447e3b85f312f0558027d1f41c4334516c6ef643"
fetch_dep_if_missing "${ZLM_SRC_DIR}/3rdpart/jsoncpp" "https://github.com/open-source-parsers/jsoncpp.git" "69098a18b9af0c47549d9a271c054d13ca92b006"

echo "[third-party] configure ZLMediaKit static mk_api..."
cmake -S "${ZLM_SRC_DIR}" -B "${BUILD_DIR}" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="${INSTALL_DIR}" \
  -DBUILD_SHARED_LIBS=OFF \
  -DENABLE_API=ON \
  -DENABLE_API_STATIC_LIB=ON \
  -DENABLE_SERVER=OFF \
  -DENABLE_PLAYER=OFF \
  -DENABLE_TESTS=OFF \
  -DENABLE_WEBRTC=OFF \
  -DENABLE_SRT=OFF \
  -DENABLE_FFMPEG=OFF \
  -DENABLE_OPENSSL=OFF

echo "[third-party] build mk_api static lib..."
cmake --build "${BUILD_DIR}" --target mk_api -j "$(sysctl -n hw.logicalcpu)"

echo "[third-party] install headers + static lib..."
cmake --install "${BUILD_DIR}" --component Unspecified

mkdir -p "${INSTALL_DIR}/lib"
cp -f "${ZLM_SRC_DIR}/release/darwin/Release/"*.a "${INSTALL_DIR}/lib/" || true

if [[ -f "${INSTALL_DIR}/lib/libmk_api.a" ]]; then
  echo "[third-party] done: ${INSTALL_DIR}/lib/libmk_api.a"
else
  echo "[third-party] failed: static library not found"
  exit 1
fi
