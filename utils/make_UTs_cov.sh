#!/usr/bin/env bash
set -euo pipefail

# SET THE SCRIPT TO GO INTO DESIRED FOLDER AND COME BACK FROM WHERE LAUNCHED.
START_DIR="$(pwd -P)"
cleanup() { cd -- "$START_DIR"; }
trap cleanup EXIT

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd -- "$ROOT_DIR"

BUILD_DIR="${ROOT_DIR}/build/UTs"
RESULT_DIR="${ROOT_DIR}/tests/results/UTs"

mkdir -p "$BUILD_DIR" "$RESULT_DIR"

# Clean previous coverage data (otherwise gcov/gcovr can fail with stamp mismatches).
rm -f "${BUILD_DIR}"/*.gcda "${BUILD_DIR}"/*.gcno "${BUILD_DIR}"/*.gcov 2>/dev/null || true

# Coverage is measured across UTs AND ITs against the SAME instrumented mpscring.o (linked
# into both binaries, run back to back — gcov accumulates counts additively across multiple
# runs of the same .gcno/.gcda pair). UTs alone cannot reach 100%: mpsc_ring_push's CAS-retry
# branch (another producer having already advanced enqueue_pos past a stale snapshot) is, by
# construction, only reachable under real concurrent contention between producers — a
# single-threaded unit test cannot trigger it deterministically without faking internal state.
# The integration test's real multi-producer flow exercises exactly that path.
gcc -std=c11 -O0 -g --coverage -DMPSC_RING_TESTING -Iapp -Itests/UTs \
    -c app/mpscring.c -o "${BUILD_DIR}/mpscring.o"

# Build unit test objects.
UT_CFLAGS=(-std=c11 -O0 -g --coverage -D_GNU_SOURCE -DMPSC_RING_TESTING -Iapp -Itests/UTs)
for src in tests/UTs/*.c; do
  out="${BUILD_DIR}/$(basename "${src%.c}").o"
  gcc "${UT_CFLAGS[@]}" -c "$src" -o "$out"
done

# Build integration test objects (public API only, no test hooks).
IT_CFLAGS=(-std=c11 -O0 -g --coverage -D_GNU_SOURCE -Iapp -Itests/ITs)
for src in tests/ITs/*.c; do
  out="${BUILD_DIR}/it_$(basename "${src%.c}").o"
  gcc "${IT_CFLAGS[@]}" -c "$src" -o "$out"
done

# Link the UT binary (mpscring.o + tests/UTs/*.o) and the IT binary (mpscring.o +
# tests/ITs/*.o) SEPARATELY, but both against the SAME mpscring.o — so both runs' .gcda
# output accumulates onto the one set of counters for app/mpscring.c.
UT_OBJECTS=("${BUILD_DIR}/mpscring.o")
for src in tests/UTs/*.c; do UT_OBJECTS+=("${BUILD_DIR}/$(basename "${src%.c}").o"); done
gcc --coverage "${UT_OBJECTS[@]}" -o "${BUILD_DIR}/ut" -lcmocka -pthread

IT_OBJECTS=("${BUILD_DIR}/mpscring.o")
for src in tests/ITs/*.c; do IT_OBJECTS+=("${BUILD_DIR}/it_$(basename "${src%.c}").o"); done
gcc --coverage "${IT_OBJECTS[@]}" -o "${BUILD_DIR}/it" -lcmocka -pthread

# Run both to generate fresh (additively-merged) .gcda files.
"${BUILD_DIR}/ut"
"${BUILD_DIR}/it"

# generate coverage
if ! command -v gcovr >/dev/null 2>&1; then
    echo "gcovr not found."
    exit 1
fi

# --gcov-ignore-parse-errors: gcov's own per-arc counters are plain (non-atomic) increments,
# not thread-safe — under the IT binary's real concurrent producer/consumer execution, two
# threads incrementing the same counter can race and occasionally produce a corrupted value
# gcov reports as negative (documented upstream: https://gcc.gnu.org/bugzilla/show_bug.cgi?id=68080).
# This does not affect the actual mpscring.o build (coverage instrumentation is test-only) or
# the correctness of the library; it only affects how gcovr tolerates a noisy counter sample.
gcovr -r "${ROOT_DIR}" \
    --object-directory "${BUILD_DIR}" \
    --gcov-ignore-parse-errors=negative_hits.warn \
    --exclude 'tests/' \
    --html --html-details \
    -o "${RESULT_DIR}/UTs_coverage.html"
# --gcov-ignore-parse-errors: gcov's own per-arc counters are plain (non-atomic) increments,
# not thread-safe — under the IT binary's real concurrent producer/consumer execution, two
# threads incrementing the same counter can race and occasionally produce a corrupted value
# gcov reports as negative (documented upstream: https://gcc.gnu.org/bugzilla/show_bug.cgi?id=68080).
# This does not affect the actual mpscring.o build (coverage instrumentation is test-only) or
# the correctness of the library; it only affects how gcovr tolerates a noisy counter sample.
gcovr -r "${ROOT_DIR}" \
    --object-directory "${BUILD_DIR}" \
    --gcov-ignore-parse-errors=negative_hits.warn \
    --exclude 'tests/' \
    --xml \
    -o "${RESULT_DIR}/UTs_coverage.xml"
# --gcov-ignore-parse-errors: gcov's own per-arc counters are plain (non-atomic) increments,
# not thread-safe — under the IT binary's real concurrent producer/consumer execution, two
# threads incrementing the same counter can race and occasionally produce a corrupted value
# gcov reports as negative (documented upstream: https://gcc.gnu.org/bugzilla/show_bug.cgi?id=68080).
# This does not affect the actual mpscring.o build (coverage instrumentation is test-only) or
# the correctness of the library; it only affects how gcovr tolerates a noisy counter sample.
gcovr -r "${ROOT_DIR}" \
    --object-directory "${BUILD_DIR}" \
    --gcov-ignore-parse-errors=negative_hits.warn \
    --exclude 'tests/' \
    --json-summary \
    --fail-under-line 100 \
    --fail-under-branch 100 \
    -o "${RESULT_DIR}/coverage-summary.json"

printf '100%% line and branch coverage gate passed; report ready: %s\n' "${RESULT_DIR}/UTs_coverage.html"
