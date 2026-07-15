#!/usr/bin/env bash
# Build and run the public integration suite and a reduced stress workload under TSan.
set -euo pipefail

START_DIR="$(pwd -P)"
cleanup() { cd -- "${START_DIR}"; }
trap cleanup EXIT

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build/tsan"
cd -- "${ROOT_DIR}"

# shellcheck source=/dev/null
source "${SCRIPT_DIR}/gcc_build_profiles.sh"
mkdir -p "${BUILD_DIR}"

TSAN_CPPFLAGS=(
    "${CPPFLAGS_TSAN[@]}"
    -DMPSC_REQUIRE_ALWAYS_LOCK_FREE
    -D_GNU_SOURCE
    -Iapp
    -DIT_PER_PRODUCER=2000
)
TSAN_CFLAGS=("${CFLAGS_TSAN[@]}" -fno-pie)
TSAN_LDFLAGS=("${LDFLAGS_TSAN[@]}" -no-pie)

gcc "${TSAN_CPPFLAGS[@]}" "${TSAN_CFLAGS[@]}" \
    -c app/mpscring.c -o "${BUILD_DIR}/mpscring_it.o"
gcc "${TSAN_CPPFLAGS[@]}" "${TSAN_CFLAGS[@]}" -Itests/ITs \
    -c tests/ITs/integration_tests.c -o "${BUILD_DIR}/integration_tests.o"
gcc "${BUILD_DIR}/mpscring_it.o" "${BUILD_DIR}/integration_tests.o" \
    -o "${BUILD_DIR}/it_tsan" "${TSAN_LDFLAGS[@]}" -lcmocka -pthread

gcc "${TSAN_CPPFLAGS[@]}" "${TSAN_CFLAGS[@]}" -DN_PER_PRODUCER=2000 \
    app/mpscring.c tests/stress/stress_mt.c \
    -o "${BUILD_DIR}/stress_tsan" "${TSAN_LDFLAGS[@]}" -pthread

run_tsan() {
    timeout --signal=TERM 300 env TSAN_OPTIONS="halt_on_error=1:history_size=7" "$@"
}

run_tsan "${BUILD_DIR}/it_tsan"
run_tsan "${BUILD_DIR}/stress_tsan"
printf 'ThreadSanitizer integration + stress gates passed\n'
