#!/usr/bin/env bash
# Run Rainfall bisect matrix: build each toggle variant, launch briefly, scrape [BISECT] lines.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
BUILD_DIR="${ROOT}/pc/build64"
EXE="${BUILD_DIR}/AnimalCrossing"
RUN_SECS="${RUN_SECS:-25}"
CMAKE_EXTRA=(-DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_PREFIX_PATH=/opt/homebrew/opt/sdl3 -DAC_USE_RAINFALL=ON)

mkdir -p "${BUILD_DIR}/rom"
if [[ ! -e "${BUILD_DIR}/rom/GAFE01_00.iso" && -f "${HOME}/Desktop/GAFE01_00.iso" ]]; then
  ln -sf "${HOME}/Desktop/GAFE01_00.iso" "${BUILD_DIR}/rom/GAFE01_00.iso"
fi

run_case() {
  local name="$1"
  shift
  echo ""
  echo "========== ${name} =========="
  cmake -S "${ROOT}/pc" -B "${BUILD_DIR}" "${CMAKE_EXTRA[@]}" "$@" >/dev/null
  cmake --build "${BUILD_DIR}" -j"$(sysctl -n hw.ncpu 2>/dev/null || nproc)" >/dev/null
  rm -f "${BUILD_DIR}/AnimalCrossing.log"
  (
    cd "${BUILD_DIR}"
    AC_RAINFALL_DIAG=1 ./AnimalCrossing </dev/null &
    pid=$!
    sleep "${RUN_SECS}"
    kill "${pid}" 2>/dev/null || true
    wait "${pid}" 2>/dev/null || true
  )
  local log="${BUILD_DIR}/AnimalCrossing.log"
  if [[ ! -f "${log}" ]]; then
    echo "FAIL: no log"
    return
  fi
  local lines
  lines="$(grep '\[BISECT\]' "${log}" | tail -3 || true)"
  if [[ -z "${lines}" ]]; then
    echo "WARN: no [BISECT] lines (game may not have reached gameplay)"
    tail -5 "${log}" | sed 's/^/  /'
  else
    echo "${lines}" | sed 's/^/  /'
  fi
  local last
  last="$(echo "${lines}" | tail -1)"
  if echo "${last}" | grep -q 'tri=0.*vtx=0.*draws=0'; then
    echo "  => BROKEN (no geometry reaching renderer)"
  elif echo "${last}" | grep -qE 'tri=[1-9]|vtx=[1-9]'; then
    if echo "${last}" | grep -qE 'draws=0'; then
      echo "  => PARTIAL (emu64 has geometry, Rainfall draw count zero)"
    else
      echo "  => OK (geometry + draws)"
    fi
  else
    echo "  => UNKNOWN (inspect log)"
  fi
}

run_case "baseline (all fixes on)" 
run_case "NO_FLUSH" -DAC_RAINFALL_BISECT_NO_FLUSH=ON
run_case "NO_FORCE_INCLUDE" -DAC_RAINFALL_BISECT_NO_FORCE_INCLUDE=ON
run_case "NO_TEXTURE_BIND" -DAC_RAINFALL_BISECT_NO_TEXTURE_BIND=ON
run_case "LEGACY_GXWGFIFO" -DAC_RAINFALL_BISECT_LEGACY_GXWGFIFO=ON

echo ""
echo "Done. Compare tri/vtx/draws across cases."
