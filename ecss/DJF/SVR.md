# Software verification report (SVR)

MPSCRING-DOC-SVR. Written by Davide Conti from the release pipeline's run at 73bb6fe36fa58a356faf7c35864623147c53ed2c (v1.0.10) on
2026-11-04. After the DRD of ECSS-E-ST-40C Rev.1 Annex M; agreed at the QR. Every figure below is the run's own, its log kept in
[runs/73bb6fe3/](runs/73bb6fe3/pipeline.log) and in MPSCRING's evidence drawer, each result in the rms naming its log by SHA-256.

## 1 Introduction

The verification of MPSCring 1.0.10 against its SRS and ICD, case by case ([SVS.md](SVS.md#5-validation-cases)).

## 2 Applicable and reference documents

The SVS, the SRS ([../TS/SRS.md](../TS/SRS.md#5-requirements)), the ICD, the inspection record
([runs/73bb6fe3/inspection.md](runs/73bb6fe3/inspection.md#inspection-of-the-sequence-protocol-the-memory-ordering-and-the-progress-of-mpscring-1010)).

## 3 The run

| | |
|---|---|
| commit | 73bb6fe36fa58a356faf7c35864623147c53ed2c (tag v1.0.10) |
| how | a clean clone at the tag; `./utils/deps.sh sysroot`, the exports, `./utils/run_pipeline.sh`; `GCC_BUILD_ENABLE_TSAN=1` |
| host | Linux 7.0.0 x86-64, Intel Core i7-8750H, GCC 15.2.0; a host build (Build-Root: none) |
| tools | kyr-build 3.1.1 (sha256 91121e30...), cmocka 2.0.2, gcovr 7.2 |
| verdict | pipeline GREEN: 25 stages ok, 1 skipped ([pipeline.log](runs/73bb6fe3/pipeline.log#L68-L99)) |

## 4 Results

<!-- ecss:table results -->
| Case | Profile | Commit | Status | Summary | Evidence |
|---|---|---|---|---|---|
| MPSCRING-VC-001 | release | 73bb6fe3 | passed | 9 tests passed | `runs/73bb6fe3/unit-tests-release.log` |
| MPSCRING-VC-002 | coverage | 73bb6fe3 | passed | 15 tests passed (12 unit, 3 integration) · lines 100.0 % · branches 100.0 % | `runs/73bb6fe3/coverage-summary.json` |
| MPSCRING-VC-003 | release | 73bb6fe3 | passed | 3 tests passed | `runs/73bb6fe3/integration.log` |
| MPSCRING-VC-004 | sanitize | 73bb6fe3 | passed | 15 tests passed (12 unit, 3 integration) and the stress of 16 x 50000 values, no sanitizer report | `runs/73bb6fe3/sanitizers.log` |
| MPSCRING-VC-005 | tsan | 73bb6fe3 | passed | 3 tests passed and the stress of 16 x 2000 values (93.564 s, 342 values/s), no race reported | `runs/73bb6fe3/tsan.log` |
| MPSCRING-VC-006 | release | 73bb6fe3 | passed | 16 x 500000 = 8000000 values exactly once in 5.583 s, 1432946 values/s | `runs/73bb6fe3/stress-run.log` |
| MPSCRING-VC-007 | release | 73bb6fe3 | passed | abi and abi-exports passed: the 8 declared functions, all @@MPSCRING_1 | `runs/73bb6fe3/abi-exports.log` |
| MPSCRING-VC-008 | release | 73bb6fe3 | passed | smoke test passed against the installed packages | `runs/73bb6fe3/package-smoke.log` |
| MPSCRING-VC-009 | release | 73bb6fe3 | passed | check_hardening passed on the release artifacts, 20 ELF(s) within the glibc ceiling | `runs/73bb6fe3/glibc-ceiling.log` |
| MPSCRING-VC-010 | audit | 73bb6fe3 | passed | the audit build (-fanalyzer, -Werror) and the gates legacy-names, pragmas, tables and warnings passed: no warning in 24 logs | `runs/73bb6fe3/warnings.log` |
| MPSCRING-VC-011 | inspection | 73bb6fe3 | passed | inspection held: 12 of 12 checks | `runs/73bb6fe3/inspection.md` |
<!-- /ecss:table -->

Figures of the run:

- unit suite on the release library: 9 tests passed, the three seam tests compiled out ([unit-tests-release.log](runs/73bb6fe3/unit-tests-release.log#L25-L36));
- coverage of app/mpscring.c by the unit (12) and integration (3) suites: lines 139 of 139, branches 58 of 58, functions 18 of 18,
  100 % each ([coverage-summary.json](runs/73bb6fe3/coverage-summary.json#L1), [unit-tests.log](runs/73bb6fe3/unit-tests.log#L6-L48));
- stress on the release archive: 16 producers x 500,000 = 8,000,000 values through a ring of 4096 in 5.583 s, 1,432,946 values a second,
  each once ([stress-run.log](runs/73bb6fe3/stress-run.log#L1-L12));
- under ASan, UBSan and LSan: 12 + 3 tests and the stress of 16 x 50,000 values (0.381 s, 2,099,175 values a second, instrumented), no
  report ([sanitizers.log](runs/73bb6fe3/sanitizers.log#L1-L39));
- under ThreadSanitizer: 3 tests and the stress of 16 x 2,000 values, no race; the stress took 93.564 s, 342 values a second, its
  producers spinning on a full ring (MPSCRING-RID-CDR-001) ([tsan.log](runs/73bb6fe3/tsan.log#L1-L22));
- ABI: the header matches abi/layout.txt, the shared object exports exactly the 8 declared functions, all `@@MPSCRING_1`
  ([abi.log](runs/73bb6fe3/abi.log#L1), [abi-exports.log](runs/73bb6fe3/abi-exports.log#L1));
- warnings: none in 24 logs ([warnings.log](runs/73bb6fe3/warnings.log#L1)); pragmas, tables and legacy names: none.

## 5 What was not run, and why

- The format stage: clang-format 18 (the major the gate pins) is not installed on the build host ([pipeline.log](runs/73bb6fe3/pipeline.log#L25-L26));
  MPSCRING-RID-QR-001.
- The TSan stage does not run by itself at 1.0.10 (the project file does not set GCC_BUILD_ENABLE_TSAN); this run set it by hand
  (MPSCRING-RID-CDR-002, MPSCRING-CR-003).
- The debs of this run are development debs (Build-Root: none, [package.log](runs/73bb6fe3/package.log#L1)); the released debs were built
  in the build root, their SHA-256 in MPSCRING-CI-DEBS; MPSCRING-RID-QR-003.

## 6 Verification status

Every case passed at 73bb6fe3. Every software requirement and interface of the TS has at least one case, every case its run; the
qualification review moves the software requirements to verified on these runs. CR-001 and CR-002 change comments, not the code, and
stay open for the next release; CR-003 changes the stress test and the project file and is done on the mainline after the release
(9bf74d2).

<!-- ecss:table changes -->
| ID | Title | Impacts | Raised by | State | Why |
|---|---|---|---|---|---|
| MPSCRING-CR-001 | mpscring.h: both lock-free probes | SRS-014, IF-005, RID-PDR-001 | Luca Romano | approved | Accepted by the configuration control board for the next release. |
| MPSCRING-CR-002 | mpscring.c: mutex-free, not lock-free | SRS-013, RID-PDR-002 | Emily Clarke | approved | Accepted by the configuration control board for the next release. |
| MPSCRING-CR-003 | Run the TSan stage in every pipeline; let a waiting producer yield | VC-005, RID-CDR-001, RID-CDR-002 | Davide Conti | approved | Accepted; done after the release on the mainline by 9bf74d2 (the TSan stress then takes 0.05 s): it ships with the next release, not in 1.0.10. |
<!-- /ecss:table -->
