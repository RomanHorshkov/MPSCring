#!/usr/bin/env bash
# shellcheck shell=bash
# mpscring_project.sh — MPSCring's declarations for the pinned kyr-build (every other script in utils/ is its wrapper): the library,
# the suites, the debs and what the ABI gate treats as public. Declarations only.
KYR_LIB="mpscring"
KYR_LIB_SOURCES=(app/mpscring.c)
KYR_LIB_HEADER="app/mpscring.h"
KYR_LIB_MAP="utils/mpscring.map"
KYR_LIB_CPPFLAGS=(-I"${ROOT_DIR}/app" -DMPSC_REQUIRE_ALWAYS_LOCK_FREE)
KYR_LIB_LDLIBS=()
# The test-only allocation, lock-free and claim-window seams, in the coverage, sanitizer and ThreadSanitizer runs only, never in a
# shipped library.
KYR_TEST_HOOKS_CPPFLAGS=(-DMPSC_RING_TESTING -I"${ROOT_DIR}/tests/UTs")
KYR_TEST_HOOKS_RUNS=(coverage sanitize tsan)

KYR_UT_SUITES=(ut)
KYR_SUITE_UT_SOURCES=('tests/UTs/*.c')
KYR_SUITE_UT_CPPFLAGS=(-D_GNU_SOURCE -I"${ROOT_DIR}/tests/UTs")
KYR_SUITE_UT_LDLIBS=(-lcmocka -pthread)
KYR_IT_SUITES=(it)
KYR_SUITE_IT_SOURCES=('tests/ITs/*.c')
KYR_SUITE_IT_CPPFLAGS=(-D_GNU_SOURCE -I"${ROOT_DIR}/tests/ITs")
KYR_SUITE_IT_CPPFLAGS_TSAN=(-DIT_PER_PRODUCER=2000u)
KYR_SUITE_IT_LDLIBS=(-lcmocka -pthread)
# Coverage across the unit AND the integration suite on one instrumented object: the threaded flow counts toward the gate.
KYR_COVERAGE_SUITES=(ut it)
KYR_UT_FAIL_UNDER_LINE=100
KYR_UT_FAIL_UNDER_BRANCH=100
# The coverage badge (.github/badges/coverage.svg, the README's Coverage line, and its endpoint JSON) from the gated run's summary.
KYR_PIPELINE_EXTRA=("post|coverage-badge|[[ ! -f tests/results/UTs/coverage-summary.json ]] || python3 build/sysroot/share/kyr-build/coverage_badge.py tests/results/UTs/coverage-summary.json .github/badges/coverage.json .github/badges/coverage.svg")
# The large-scale proof against the release archive; a reduced run under the sanitizers.
KYR_STRESS_SUITES=(stress)
KYR_SUITE_STRESS_SOURCES=(tests/stress/stress_mt.c)
KYR_SUITE_STRESS_CPPFLAGS=(-D_GNU_SOURCE)
KYR_SUITE_STRESS_CPPFLAGS_SANITIZE=(-DN_PER_PRODUCER=50000u)
KYR_SUITE_STRESS_CPPFLAGS_TSAN=(-DN_PER_PRODUCER=2000u)
KYR_SUITE_STRESS_LDLIBS=(-pthread)
KYR_SANITIZE_SUITES=(ut it stress)
# The unit suite too: its stalled-producer test runs four threads through the claim window.
KYR_TSAN_SUITES=(ut it stress)
GCC_BUILD_ENABLE_TSAN=1 # the catalog, sourced first, sets 0 when unset: the tsan stage runs in every pipeline

# libmpscring and libmpscring-dev, both replacing the former single package "mpscring".
KYR_PKG="libmpscring"
KYR_PKG_DESCRIPTION="Bounded multi-producer single-consumer ring buffer library"
KYR_DEB_HEADERS=(app/mpscring.h)
KYR_DEB_RUNTIME_FIELDS=("Conflicts: mpscring" "Replaces: mpscring")
KYR_DEB_DEV_FIELDS=("Conflicts: mpscring" "Replaces: mpscring")
KYR_DEB_DEV_DESCRIPTION="Development files for libmpscring (header, static library, linker symlink)"
KYR_DEB_PC=0

# The public ABI: app/mpscring.h; the linker map's node is MPSCRING_<major>.
KYR_ABI_NAME="MPSCring"
KYR_ABI_INCLUDE_DIR="app"
KYR_ABI_UMBRELLA="mpscring.h"
KYR_ABI_TYPE_PREFIX="mpsc_ring_"
KYR_ABI_FUNC_PREFIX="mpsc_ring_"
KYR_ABI_CONST_PREFIX="MPSC_RING_"
KYR_ABI_NODE="MPSCRING"

