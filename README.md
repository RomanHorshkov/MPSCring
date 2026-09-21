# MPSCring

[![Quality](https://github.com/RomanHorshkov/MPSCring/actions/workflows/quality.yml/badge.svg?branch=master)](https://github.com/RomanHorshkov/MPSCring/actions/workflows/quality.yml?query=branch%3Amaster)
[![Security](https://github.com/RomanHorshkov/MPSCring/actions/workflows/security.yml/badge.svg?branch=master)](https://github.com/RomanHorshkov/MPSCring/actions/workflows/security.yml?query=branch%3Amaster)
[![Release](https://github.com/RomanHorshkov/MPSCring/actions/workflows/release.yml/badge.svg?branch=master)](https://github.com/RomanHorshkov/MPSCring/actions/workflows/release.yml?query=branch%3Amaster)
![Coverage](.github/badges/coverage.svg)
[![Version](https://img.shields.io/github/v/tag/RomanHorshkov/MPSCring?label=version)](https://github.com/RomanHorshkov/MPSCring/tags)
[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)

Multi-Producer Single-Consumer (MPSC) ring buffer implemented in C11 with mutex-free
semantics for many producer threads and exactly one consumer thread. NOT formally lock-free
(see "Implementation choices" below) — a fast, CAS-based, mutex-free design, but Vyukov's own
description of the underlying algorithm is explicit that a stalled producer can block the
consumer's progress. The data type is `void*` (opaque to this library) — sibling to
[SPSCring](https://github.com/RomanHorshkov/SPSCring) (which is single-producer, `int`-typed,
and intended for socket file descriptors); this one exists for the case where many threads
need to hand work to one dedicated consumer thread concurrently, e.g. many request-handling
threads submitting a database write to one dedicated writer thread.

Public API documentation is in `app/mpscring.h`.

## API contract

The following behaviors are part of the public interface and must remain stable:

- `mpsc_ring_push(NULL, ...)` returns `-1`.
- `mpsc_ring_pop(NULL, ...)` returns `-1`.
- `mpsc_ring_is_empty(NULL)` returns `1`.
- `mpsc_ring_capacity(NULL)` returns `0`.
- `mpsc_ring_destroy(NULL)` and `mpsc_ring_destroy(&NULL)` are no-ops.
- Any number of producer threads may call `mpsc_ring_push` concurrently. Exactly one
  consumer thread may call `mpsc_ring_pop` — concurrent `pop` calls are undefined behavior
  (this is MPSC, not MPMC, even though the underlying per-slot algorithm generalizes to MPMC).
- Capacity is fixed after initialization, must be a power of two, and must be at least 2.
- The ring stores `void*` values; it has no knowledge of what they point to.
- `mpsc_ring_init(capacity)` owns and frees its own storage. `mpsc_ring_init_into(storage,
  storage_size, capacity)` uses caller-owned storage and never allocates — `mpsc_ring_destroy`
  on a ring built this way frees nothing.
- `mpsc_ring_storage_size(capacity)` returns the exact byte count `init_into` needs; callers
  reserving that many bytes succeed regardless of their buffer's own starting alignment
  (the function accounts for worst-case internal alignment slack).

## Implementation choices

- Many producers, exactly one consumer — a deliberately narrower contract than a full MPMC
  queue, because the consumer side needs no CAS at all this way (the same asymmetry
  SPSCring's single-producer side exploits): only one thread ever advances the dequeue
  position, so a plain (non-atomic) counter is correct there.
- The producer side cannot use SPSCring's plain relaxed-load-then-release-store tail
  update — two producers could both observe the same slot as free and both write into it.
  Instead each slot carries its own sequence counter (the well-established
  [Vyukov bounded MPMC queue](https://www.1024cores.net/home/lock-free-algorithms/queues/bounded-mpmc-queue)
  design — a long-published, widely implemented, provably-correct algorithm, not a new
  invention): a producer reads the shared `enqueue_pos`, checks that the target slot's
  sequence says "free for this position," and CAS-claims the position before writing.
- Capacity must be a power of two and at least 2; masking is used only for indexing. A
  one-slot queue cannot encode distinct published/free sequence states and is rejected.
- Indices are 64-bit and monotonic, same wrap-time reasoning as SPSCring (irrelevant for
  any real workload — see SPSCring's README for the exact math, it applies identically here).
- Two allocation modes, one control: `mpsc_ring_init` (library-owned, malloc-backed, the
  convenient default) and `mpsc_ring_init_into` (caller-owned storage — a static array sized
  from a deployment-specific bound or a runtime-sized arena slot validated with
  `mpsc_ring_storage_size`; this function is not a C constant expression — this library never
  calls malloc on that path). The latter is the
  intended path for callers that want static/startup-time-only allocation with no allocation
  surprises later in the process's life.

## Lock-free target support

MPSCring does not silently use lock-backed atomic operations. At initialization, it checks
both the shared `enqueue_pos` counter and a representative per-slot `sequence` counter with
C11 `atomic_is_lock_free`; if either check fails, `mpsc_ring_init`/`mpsc_ring_init_into`
return `NULL` before doing anything else. Same policy as SPSCring, same
`MPSC_REQUIRE_ALWAYS_LOCK_FREE` compile-time opt-in for a build-time (not just runtime) target
check on GCC/Clang.

## Build system

Builds are driven by scripts under `utils/`, mirroring SPSCring's:

- `utils/build_libs.sh [profile …]` builds the static and shared libraries per profile into
  `build/<profile>/` (release gated by `check_hardening.sh`).
- `utils/build_UTs_release.sh` builds `tests/UTs/unit_tests.c` (single-threaded contract:
  capacity validation, storage-size arithmetic, full/empty boundaries, the two allocation
  modes, fault-injection on the allocator and the lock-free checks — the fault-injection tests
  are compiled out entirely, not just skipped, when `MPSC_RING_TESTING` isn't defined, so this
  release-profile run genuinely link-tests the shipped library, not a test-instrumented one)
  and runs it with real (non-`NDEBUG`) assertions.
- `utils/build_UTs.sh` measures line/branch coverage across BOTH `tests/UTs` and
  `tests/ITs` against one shared instrumented object (100% gate on both dimensions) — UTs
  alone cannot reach `mpsc_ring_push`'s CAS-retry branch, which is only reachable under real
  producer contention; `tests/ITs/integration_tests.c` (a small, deliberately-contentious
  multi-producer flow) is what exercises it.
- `utils/build_ITs.sh` runs public-API black-box integration tests, distinct from white-box UTs.
- `utils/build_sanitizer_tests.sh` runs UTs, ITs, and reduced stress under ASan/UBSan/LSan.
- `tests/stress/stress_mt.c` is the large-scale concurrency proof: N producer threads pushing
  concurrently against 1 consumer thread. Correctness is a consumer-owned bitmap indexed by
  `(producer_id, sequence)`, tested-and-set on every consume — a duplicate delivery aborts the
  run immediately (a checksum/count pair alone cannot rule out two corruptions whose effects
  on the sum cancel out; a bitmap has no such blind spot). Run it under the `sanitize` profile
  (ASan/UBSan) and, separately, under ThreadSanitizer with a reduced `-DN_PER_PRODUCER=<small>`
  (TSan's instrumentation overhead makes the full-size run impractically slow, not incorrect)
  — a correctness claim about concurrent code that hasn't been run under TSan at least once
  isn't really a correctness claim yet.
- `utils/build_tsan_tests.sh` separately runs ITs and reduced stress under ThreadSanitizer;
  TSan is never mixed with ASan.
- `utils/build_deb.sh` builds the release deb + `SHA256SUMS`; `utils/smoke_test_package.sh`
  compiles and runs a tiny program against ONLY the installed package (`/usr/include` and the
  Debian multiarch library directory), proving the shipped artifact works standalone, not just
  that the source builds; `utils/build_stress.sh` + `utils/run_stress.sh` build and run the
  stress test (results under `tests/results/stress/`); `utils/run_pipeline.sh` runs build,
  release UTs, atomic-counter coverage, ITs, sanitizers, stress, optional local TSan, and
  packaging.

## Continuous integration

Three workflows, a connected graph rather than independent races on every push:

- **Quality** (`.github/workflows/quality.yml`) — `build` fans out to strict GCC/Clang
  compilation, release UTs plus the 100% atomic-counter coverage gate, and threaded
  integration/stress. Integration/stress gates both ASan/UBSan/LSan and TSan; every branch
  must pass before package installation and smoke testing. Also reusable by releases.
- **Security** (`.github/workflows/security.yml`) — CodeQL + GCC's `-fanalyzer`, on push/PR
  plus a weekly schedule.
- **Release** (`.github/workflows/release.yml`) — requires Quality to pass, validates the
  pushed tag matches `VERSION`, builds the hardened package, attaches a build-provenance
  attestation, and uploads to the GitHub Release.

## Dependencies

- GCC or Clang with C11 support (build)
- `fakeroot` and `dpkg-deb` (packaging)

## License

MIT, see `LICENSE`.

## Build profiles & hardening

Same profile catalog as every sibling library in this workspace — `utils/gcc_build_profiles.sh`
(synced from `Utils/compilation/`, never edited locally); artifacts land in `build/<profile>/`;
`utils/check_hardening.sh` gates every release artifact. See `SPSCring/README.md` for the
full profile table (debug/audit/sanitize/release/native/extreme) — identical here.
