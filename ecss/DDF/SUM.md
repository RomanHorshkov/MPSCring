# Software user manual (SUM)

MPSCRING-DOC-SUM. Written by Emily Clarke for a programmer who links MPSCring 1.0.10. After the DRD of ECSS-E-ST-40C Rev.1 Annex H;
agreed at the CDR. Each call in full: [api/mpscring.md](api/mpscring.md).

## 1 Introduction

MPSCring hands pointers from any number of producer threads to exactly one consumer thread, without a lock. It is not formally
lock-free: read section 4.6 before you rely on its liveness.

## 2 Applicable and reference documents

The ICD ([../TS/ICD.md](../TS/ICD.md#53-interface-design)); [README.md](../../README.md#api-contract).

## 3 Getting the library

Install `libmpscring` and `libmpscring-dev` (the platform pins them in its deps.txt), include `<mpscring.h>`, link `-lmpscring`. From
the source: `./utils/deps.sh sysroot && eval "$(./utils/deps.sh env)" && ./utils/run_pipeline.sh` ([README.md](../../README.md#build-profiles--hardening)).

## 4 Using the ring

### 4.1 Create it once, before the threads share it

```c
mpsc_ring_t* ring = mpsc_ring_init(1024);   /* a power of two, at least MPSC_RING_MIN_CAPACITY (2); the library owns the memory */
if(!ring) { /* invalid capacity, no memory, or no lock-free 64-bit atomics: refuse your component's start */ }
```

Or in memory you own, sized once at start:

```c
size_t need = mpsc_ring_storage_size(1024);       /* 0: invalid capacity */
void* arena = my_arena_take(need);                 /* any alignment will do */
mpsc_ring_t* ring = mpsc_ring_init_into(arena, need, 1024);
```

### 4.2 The producers

```c
if(mpsc_ring_push(ring, job) != 0) {
    /* full: nothing stored; retry later or refuse the work (kyr_core retries every 0.2 ms for 250 ms) */
}
```

Any number of threads may push at once. NULL is a value like any other.

### 4.3 The consumer

```c
void* job;
while(mpsc_ring_pop(ring, &job) == 0) {
    /* run job */
}
```

Only one thread may pop or call `mpsc_ring_is_empty`. The role may pass to another thread if the passing is ordered (a join).

### 4.4 Sleeping when there is nothing

Check `mpsc_ring_is_empty(ring)` from the consumer under your own mutex before waiting on your condition; have every producer signal
after its push (kyr_core's writer does exactly this).

### 4.5 Tear it down

Stop every producer and the consumer first, then `mpsc_ring_destroy(&ring)`: the library's memory freed (yours untouched for
`mpsc_ring_init_into`), `ring` set to NULL.

### 4.6 The stalled producer

A producer preempted between its claim and its publish holds back every job behind it: pop answers empty and, once the ring has wrapped,
push answers full, until it runs on. Nothing is lost. Do not cancel or block a producer inside push, and do not take the ring for
lock-free (MPSCRING-SRS-013).

### 4.7 A complete program

The package's own smoke test, built against the installed packages alone ([tests/package/smoke.c](../../tests/package/smoke.c#L1-L24)).

## 5 Errors and what to do

| Answer | Means | Do |
|---|---|---|
| `mpsc_ring_init` / `mpsc_ring_init_into` NULL | capacity below 2 or not a power of two, storage NULL or too small, no memory, or a target whose 64-bit atomics take a lock | check the capacity and the size; a lock-backed target cannot run this ring |
| `mpsc_ring_storage_size` 0 | an invalid capacity | check the capacity |
| `mpsc_ring_push` -1 | full, or the ring pointer is NULL | the value was not stored: retry within your budget or refuse it |
| `mpsc_ring_pop` -1 | empty (or the next position is not published yet), or the ring pointer is NULL | nothing to take: wait for a producer's signal |
| a build error "MPSCring requires always-lock-free..." | `MPSC_REQUIRE_ALWAYS_LOCK_FREE` on a target without them | build for a 64-bit target with lock-free atomics |

## 6 What not to do

- Pop, or call `mpsc_ring_is_empty`, from two threads at once: the consumer's position is plain memory.
- Destroy while a thread uses the ring.
- Use a capacity of 1: it is refused, one slot cannot tell published from free.
