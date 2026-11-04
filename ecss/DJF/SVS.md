# Software validation specification w.r.t. the TS (SVS)

MPSCRING-DOC-SVS. Written by Davide Conti. After the DRD of ECSS-E-ST-40C Rev.1 Annex L; agreed at the CDR. The cases are the rms items
`MPSCRING-VC-nnn` (`DJF/verification_cases.csv`); their table is written by `ecss/tools/package.py tables`.

## 1 Introduction

How MPSCring 1.0.10 is shown to meet its technical specification: the suites and gates of its release pipeline, grouped by what they
prove, and one inspection for what a test cannot prove (the sequence protocol, the memory ordering, the progress and its limit).

## 2 Applicable and reference documents

The SRS ([../TS/SRS.md](../TS/SRS.md#5-requirements)), the ICD ([../TS/ICD.md](../TS/ICD.md#53-interface-design)),
[utils/mpscring_project.sh](../../utils/mpscring_project.sh#L14-L35) (the suites as the pipeline declares them),
[README.md](../../README.md#build-system).

## 3 Terms

- profile: a kyr-build build profile (release, coverage, sanitize, tsan, audit); a run is recorded as `<profile>@<commit>`.
- stage: a step of `utils/run_pipeline.sh`, its log under `build/pipeline/` (kept as evidence in `DJF/runs/<commit>/`).

## 4 Validation approach

Every case runs in the pipeline at the release commit, on the build host, the way the repository builds it
(`./utils/deps.sh sysroot && eval "$(./utils/deps.sh env)" && ./utils/run_pipeline.sh`, with `GCC_BUILD_ENABLE_TSAN=1` for the TSan
stage, which the project file at 1.0.10 does not set: MPSCRING-RID-CDR-002). A case passes when its stage passes; the inspection
passes when every check of its list holds.

## 5 Validation cases

<!-- ecss:table cases -->
| ID | Title | Method | What runs | Criteria | Verifies |
|---|---|---|---|---|---|
| MPSCRING-VC-001 | Unit suite against the release library | test | tests/UTs/unit_tests.c built against the release library without the seams, with real (not NDEBUG) assertions (9 tests): capacity validation, the minimum capacity's full boundary, a NULL payload, FIFO on one thread, wrap-around, NULL and double-free safety, caller-owned storage, the exact size at every alignment, undersized storage refused. Pipeline stage unit-tests-release. | Every test passes. | SSS-002, SSS-003, SSS-004, IRD-002, SRS-001, SRS-002, SRS-004, SRS-005, SRS-006, SRS-007, SRS-009, SRS-016, SRS-017, SRS-018, SRS-024, SRS-025, IF-001, IF-003, IF-004, IF-005, IF-006, IF-007, IF-008, IF-009 |
| MPSCRING-VC-002 | Unit and integration suites under coverage, with the seams | test | tests/UTs/unit_tests.c (12 tests: the allocation failure, the lock-free rejection and the lost claim through the push hook among them) and tests/ITs/integration_tests.c (3 tests) on one instrumented object; gcovr gates lines and branches of app/mpscring.c at 100 %. Pipeline stage unit-tests. | Every test passes; lines and branches 100 %. | SSS-003, SSS-005, SSS-008, SRS-003, SRS-008, SRS-013, SRS-014, SRS-025, IF-005 |
| MPSCRING-VC-003 | Integration suite on the release archive | test | tests/ITs/integration_tests.c against the release archive (3 tests): a simple flow, the storage size round trip, six producer threads against one consumer through a deliberately small ring, each value checked. Pipeline stage integration. | Every test passes. | SSS-001, SSS-002, SSS-004, IRD-003, SRS-001, SRS-005, SRS-017, SRS-025, IF-003, IF-006, IF-007 |
| MPSCRING-VC-004 | AddressSanitizer, UBSan and LeakSanitizer | test | The unit suite (12), the integration suite (3) and a reduced stress (16 producers x 50,000 values) under ASan, UBSan and LSan. Pipeline stage sanitizers. | Every test passes; no sanitizer report. | SSS-008, SRS-025 |
| MPSCRING-VC-005 | ThreadSanitizer | test | The integration suite (3, 2,000 values a producer) and a reduced stress (16 producers x 2,000 values) under ThreadSanitizer, never mixed with ASan. Pipeline stage tsan (GCC_BUILD_ENABLE_TSAN=1, set by hand at 1.0.10). | Every test passes; no race reported. | SSS-001, SSS-008, IRD-003, IRD-004, SRS-008, SRS-010, SRS-011, SRS-025 |
| MPSCRING-VC-006 | Stress: 16 producers, 8,000,000 values, exactly once | test | tests/stress/stress_mt.c on the release archive: 16 producers push 500,000 encoded values each against one consumer through a ring of 4096; a bitmap indexed by (producer, sequence) aborts on a duplicate, an out-of-range value or a missing one. Pipeline stage stress-run. | STRESS TEST PASSED; every value seen once. | SSS-001, SSS-008, IRD-004, SRS-005, SRS-025, IF-008 |
| MPSCRING-VC-007 | ABI layout and exports | analysis | utils/check_abi.sh: app/mpscring.h against abi/layout.txt (eight functions and the constant), and the release shared object's exports against utils/mpscring.map (exactly eight functions, version node MPSCRING_1). Pipeline stages abi and abi-exports. | Both stages pass. | SSS-007, IRD-001, IRD-005, SRS-020, SRS-024, IF-001, IF-002, IF-010 |
| MPSCRING-VC-008 | The installed packages | test | utils/smoke_test_package.sh: the debs built, installed into a scratch root, tests/package/smoke.c compiled against the installed header and library alone and run (eight values in, eight out in order). Pipeline stages package and package-smoke. | The smoke program round-trips its values. | SSS-007, IRD-001, IRD-005, SRS-021, IF-010, IF-011 |
| MPSCRING-VC-009 | Hardening and the glibc ceiling | analysis | kyr-build's check_hardening on the release artifacts (RELRO, immediate binding, non-executable stack, canaries, fortify, no RPATH, versioned soname, every NEEDED on the allowlist, position-independent archive) and the glibc-ceiling stage. Pipeline stages package and glibc-ceiling. | Every hard check passes. | SSS-007, SRS-022, SRS-023 |
| MPSCRING-VC-010 | Static analysis and the warning gates | analysis | The audit profile's build (the whole warning set, -Werror and GCC's -fanalyzer) and kyr-build's gates legacy-names, pragmas, tables and warnings. Pipeline stages build:audit, legacy-names, pragmas, tables, warnings. | Every stage passes. | SRS-015, IF-012 |
| MPSCRING-VC-011 | Inspection of the sequence protocol, the memory ordering and the progress | inspection | A line-by-line inspection of push, pop, is_empty, storage_size and init_into against ISO/IEC 9899:2011 7.17 and Vyukov's protocol: the slots' initial sequences, the claim only on an equal sequence and a won CAS, the acquire before and the release after each side's access, the next lap's sequence on a pop, the bounded loop of push and the straight-line pop, the window of a stalled producer (SDD 5.3 and 5.4). | Every check of the inspection's list holds; the threaded suites under ThreadSanitizer corroborate it. | SSS-002, SSS-005, SSS-006, IRD-004, SRS-008, SRS-009, SRS-010, SRS-011, SRS-012, SRS-013, SRS-015, SRS-016, SRS-019, IF-012 |
<!-- /ecss:table -->

## 6 Validation environment

Linux on x86-64, GCC with C11, cmocka 2.0, gcovr 7.2, the pinned kyr-build 3.1.1 (from deps.txt, by SHA-256).
