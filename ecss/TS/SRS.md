# Software requirements specification (SRS)

MPSCRING-DOC-SRS. Written by Luca Romano from the code, the README and the suites of MPSCring 1.0.10 (73bb6fe3), each requirement
derived from the customer's requirements baseline ([../RB/SSS.md](../RB/SSS.md#5-requirements), [../RB/IRD.md](../RB/IRD.md#5-interface-requirements)).
After the DRD of ECSS-E-ST-40C Rev.1 Annex D; agreed at the PDR. The table in section 5 is written from `TS/requirements.csv` by
`ecss/tools/package.py tables`.

## 1 Introduction

This SRS specifies MPSCring, the bounded multi-producer single-consumer ring of pointers, as software of category B
([../PAF/TAILORING.md](../PAF/TAILORING.md#3-criticality-classification)). It is the first part of the technical specification; the
second is the ICD ([ICD.md](ICD.md#53-interface-design)).

## 2 Applicable and reference documents

- ECSS-E-ST-40C Rev.1, ECSS-Q-ST-80C Rev.2; ISO/IEC 9899:2011 7.17 (the C11 atomics).
- D. Vyukov, Bounded MPMC queue (1024cores.net); M. Herlihy, Wait-free synchronization (1991).
- The repository: [README.md](../../README.md#api-contract), [app/mpscring.h](../../app/mpscring.h#L1-L74), [app/mpscring.c](../../app/mpscring.c#L1-L22).
- The requirements baseline (RB/) and the ICD.

## 3 Terms

- position: the 64-bit index of a hand-off; enqueue_pos the producers' next one (atomic), dequeue_pos the consumer's (plain).
- sequence: a slot's 64-bit counter; equal to pos, free for pos; pos + 1, published for pos; pos + capacity, read and free for the next lap.
- claim: a producer's winning compare-and-swap of enqueue_pos from pos to pos + 1; publish: its release store of pos + 1 into the slot.
- lock-free: some thread completes its call in a bounded number of steps whatever the others do; wait-free: every thread does.

## 4 Software overview

### 4.1 Function

MPSCring hands pointers from any number of threads to one thread through a fixed array of slots, without a lock. Producers claim
distinct positions by compare-and-swap on one shared position; each slot's sequence tells free, published and read apart; the consumer
needs no compare-and-swap. Each side reads a slot's sequence with acquire before touching the slot and publishes it with release after.

### 4.2 Environment

Linux, C11, a 64-bit target with lock-free 64-bit atomics (x86-64, aarch64). Tier 1 of the platform: no dependency but the C library
([deps.txt](../../deps.txt#L1-L9)). Built and tested under the pinned kyr-build 3.1.1.

### 4.3 Constraints

One consumer at a time (MPSCRING-ASM-001); producers that publish promptly (MPSCRING-ASM-002): the ring is not formally lock-free, a
producer stalled between its claim and its publish holds back delivery (MPSCRING-SRS-013).

## 5 Requirements

### 5.1 General provisions

Each requirement is an rms item `MPSCRING-SRS-nnn` of kind requirement with its rationale, criteria, method and sources, derived from a
requirement of the RB (the column Traces: "derives from"), satisfied by interfaces and components and verified by the cases of the SVS.
The assumptions are items `MPSCRING-ASM-nnn`.

### 5.2 Functional and non-functional requirements

<!-- ecss:table items doc=SRS kind=requirement -->
| ID | Title | Statement | Method | Source | Traces |
|---|---|---|---|---|---|
| MPSCRING-SRS-001 | The storage a ring needs | mpsc_ring_storage_size(capacity) shall return the bytes mpsc_ring_init_into() needs for a ring of that capacity, the control block, the slots and the worst-case alignment slack of both included, and 0 when the capacity is below 2, not a power of two, or too large for size_t. | test | [app/mpscring.h#L89-L100](../../app/mpscring.h#L89-L100); [app/mpscring.c#L188-L217](../../app/mpscring.c#L188-L217) | derives from SSS-003; verified by VC-001, VC-003; satisfied by IF-003, CMP-STORAGE |
| MPSCRING-SRS-002 | A ring in the caller's memory | mpsc_ring_init_into(storage, storage_size, capacity) shall return an empty ring placed inside storage when storage is not NULL and storage_size is at least mpsc_ring_storage_size(capacity), whatever storage's alignment, and NULL otherwise; it shall never allocate, and mpsc_ring_destroy() on such a ring shall free nothing. | test | [app/mpscring.h#L102-L121](../../app/mpscring.h#L102-L121); [app/mpscring.c#L219-L263](../../app/mpscring.c#L219-L263) | derives from SSS-003; verified by VC-001; satisfied by IF-004, IF-009, CMP-STORAGE; assumed by ASM-003 |
| MPSCRING-SRS-003 | A ring in the library's memory | mpsc_ring_init(capacity) shall allocate exactly mpsc_ring_storage_size(capacity) zeroed bytes, build the ring inside them and own them, returning NULL (and freeing them) when the capacity is invalid, the allocation fails or the ring cannot be built; mpsc_ring_destroy() shall free them. | test | [app/mpscring.h#L123-L136](../../app/mpscring.h#L123-L136); [app/mpscring.c#L265-L287](../../app/mpscring.c#L265-L287) | derives from SSS-003, IRD-002; verified by VC-002; satisfied by IF-005, IF-009, CMP-LIFE; assumed by ASM-003 |
| MPSCRING-SRS-004 | Capacity: a power of two, at least two | The capacity shall be a power of two and at least MPSC_RING_MIN_CAPACITY (2); a ring of capacity N shall accept N pushes from empty and refuse the next one. | test | [app/mpscring.h#L81-L82](../../app/mpscring.h#L81-L82); [app/mpscring.c#L188-L197](../../app/mpscring.c#L188-L197) | derives from SSS-003; verified by VC-001; satisfied by IF-001, CMP-STATE |
| MPSCRING-SRS-005 | Each job once, in the order of the claims | The values popped shall be the values pushed, each exactly once, in the order the producers' claims of enqueue_pos were made. | test | [app/mpscring.c#L289-L387](../../app/mpscring.c#L289-L387) | derives from SSS-001, IRD-004; verified by VC-001, VC-003, VC-006; satisfied by IF-006, IF-007, CMP-PRODUCER, CMP-CONSUMER |
| MPSCRING-SRS-006 | A full ring refuses a push at once | mpsc_ring_push() shall return -1 and store nothing when the slot of the next position has not been freed by the consumer; on a NULL ring it shall return -1. | test | [app/mpscring.h#L138-L147](../../app/mpscring.h#L138-L147); [app/mpscring.c#L329-L332](../../app/mpscring.c#L329-L332) | derives from SSS-002, IRD-002; verified by VC-001; satisfied by IF-006, CMP-PRODUCER |
| MPSCRING-SRS-007 | An empty ring refuses a pop at once | mpsc_ring_pop() shall return -1 and leave the caller's variable untouched when the next slot has not been published; a NULL output pointer shall discard the value popped; on a NULL ring it shall return -1. | test | [app/mpscring.h#L149-L158](../../app/mpscring.h#L149-L158); [app/mpscring.c#L359-L374](../../app/mpscring.c#L359-L374) | derives from SSS-004, IRD-002; verified by VC-001; satisfied by IF-007, CMP-CONSUMER; assumed by ASM-001 |
| MPSCRING-SRS-008 | Each producer claims a distinct position | A producer shall store into the slot of position pos only after observing that slot's sequence equal to pos and winning a compare-and-swap of enqueue_pos from pos to pos + 1; no two producers shall ever claim the same position. | inspection | [app/mpscring.c#L296-L338](../../app/mpscring.c#L296-L338) | derives from SSS-001, SSS-002, IRD-003; verified by VC-002, VC-005, VC-011; justified by SRC-VYUKOV; satisfied by IF-006, CMP-PRODUCER |
| MPSCRING-SRS-009 | The per-slot sequence protocol | Slot i shall start with sequence i; a sequence equal to pos shall mean free for position pos, pos + 1 published for pos, and pos + capacity read and free for the next lap; push shall set pos + 1 after its store, pop pos + capacity after its read. | inspection | [app/mpscring.c#L5-L14](../../app/mpscring.c#L5-L14); [app/mpscring.c#L256-L260](../../app/mpscring.c#L256-L260); [app/mpscring.c#L341](../../app/mpscring.c#L341); [app/mpscring.c#L385](../../app/mpscring.c#L385) | derives from SSS-001, IRD-004; verified by VC-001, VC-011; justified by SRC-VYUKOV; satisfied by CMP-STATE, CMP-STORAGE |
| MPSCRING-SRS-010 | The producer publishes with release | mpsc_ring_push() shall load the slot's sequence with acquire before it claims, store the value after the claim, and publish it with a release store of the sequence. | inspection | [app/mpscring.c#L310-L343](../../app/mpscring.c#L310-L343) | derives from IRD-003, IRD-004; verified by VC-005, VC-011; justified by SRC-C11; satisfied by IF-006, CMP-PRODUCER |
| MPSCRING-SRS-011 | The consumer reads after acquire | mpsc_ring_pop() shall load the slot's sequence with acquire before it reads the value, and free the slot with a release store of the next lap's sequence after the read. | inspection | [app/mpscring.c#L359-L387](../../app/mpscring.c#L359-L387) | derives from IRD-003, IRD-004; verified by VC-005, VC-011; justified by SRC-C11; satisfied by IF-007, CMP-CONSUMER; assumed by ASM-001 |
| MPSCRING-SRS-012 | Progress: lock-free producers, a wait-free consumer | mpsc_ring_push() shall take no lock, make no system call and allocate nothing, and shall retry its claim only when another producer's claim succeeded or moved enqueue_pos past it (lock-free among producers); mpsc_ring_pop() and mpsc_ring_is_empty() shall finish in a bounded number of their own steps (wait-free). | inspection | [app/mpscring.c#L289-L343](../../app/mpscring.c#L289-L343); [app/mpscring.c#L359-L399](../../app/mpscring.c#L359-L399) | derives from SSS-002, SSS-004; verified by VC-011; justified by SRC-HERLIHY; satisfied by IF-006, IF-007, CMP-PRODUCER, CMP-CONSUMER; assumed by ASM-002 |
| MPSCRING-SRS-013 | The limit of progress: a stalled producer delays, never loses | A producer stalled between its claim and its publish shall delay the delivery of every later position until it publishes, the consumer answering empty meanwhile and the producers answering full once the ring wraps to the stalled slot; it shall never cause a value to be lost, duplicated or read before it is written. The ring is therefore not formally lock-free. | inspection | [app/mpscring.h#L15-L26](../../app/mpscring.h#L15-L26); [app/mpscring.c#L324-L341](../../app/mpscring.c#L324-L341) | derives from SSS-005; verified by VC-002, VC-011; justified by SRC-VYUKOV, SRC-HERLIHY; satisfied by CMP-PRODUCER; assumed by ASM-002 |
| MPSCRING-SRS-014 | Lock-free counters or no ring | mpsc_ring_init() and mpsc_ring_init_into() shall return NULL when enqueue_pos or a slot's sequence is not lock-free on the target (atomic_is_lock_free); a build defining MPSC_REQUIRE_ALWAYS_LOCK_FREE shall fail to compile on a target whose 64-bit atomics are not always lock-free. | test | [app/mpscring.c#L33-L49](../../app/mpscring.c#L33-L49); [app/mpscring.c#L166-L186](../../app/mpscring.c#L166-L186); [app/mpscring.c#L243-L254](../../app/mpscring.c#L243-L254); [README.md#lock-free-target-support](../../README.md#lock-free-target-support) | derives from SSS-002; verified by VC-002; justified by SRC-C11; satisfied by IF-004, IF-005, IF-012, CMP-STORAGE; assumed by ASM-004 |
| MPSCRING-SRS-015 | Producers and consumer never share a cache line | enqueue_pos (the producers') and dequeue_pos (the consumer's) shall sit at least MPSC_CACHELINE (64) bytes apart, checked at compile time. | inspection | [app/mpscring.c#L133-L159](../../app/mpscring.c#L133-L159) | derives from SSS-002; verified by VC-010, VC-011; satisfied by IF-012, CMP-STATE |
| MPSCRING-SRS-016 | Wrap-safe positions | Positions and sequences shall be 64-bit counters compared by their unsigned modular difference against UINT64_MAX / 2, never through a signed conversion; at 10^6 hand-offs a second they wrap after about 584,000 years. | inspection | [app/mpscring.c#L111-L124](../../app/mpscring.c#L111-L124) | derives from SSS-001; verified by VC-001, VC-011; satisfied by CMP-STATE; assumed by ASM-004 |
| MPSCRING-SRS-017 | The emptiness check is the consumer's | mpsc_ring_is_empty() shall return 1 when the slot of the consumer's next position is not published and 0 otherwise, and 1 for a NULL ring; it shall be called from the consumer thread only, since it reads the consumer's plain position. | test | [app/mpscring.h#L160-L170](../../app/mpscring.h#L160-L170); [app/mpscring.c#L389-L399](../../app/mpscring.c#L389-L399) | derives from SSS-004, IRD-003; verified by VC-001, VC-003; satisfied by IF-008, CMP-CONSUMER; assumed by ASM-001 |
| MPSCRING-SRS-018 | Every call tolerates NULL | With a NULL ring, push and pop shall return -1, is_empty 1 and capacity 0; destroy shall accept NULL and a pointer to NULL; NULL shall be a value like any other, stored and popped. | test | [app/mpscring.h#L138-L190](../../app/mpscring.h#L138-L190); [app/mpscring.c#L401-L421](../../app/mpscring.c#L401-L421) | derives from IRD-001, IRD-002; verified by VC-001; justified by SRC-README; satisfied by IF-002, IF-008, IF-009, CMP-LIFE, CMP-QUERY |
| MPSCRING-SRS-019 | The consumer may change hands across a synchronisation | The consumer's role may pass from one thread to another when the second's first take is ordered after the first's last one by a synchronisation (a thread join), the consumer's position being plain memory owned by whichever thread is the consumer. | analysis | [app/mpscring.c#L149-L155](../../app/mpscring.c#L149-L155); [app/mpscring.h#L45-L56](../../app/mpscring.h#L45-L56) | derives from SSS-006; verified by VC-011; satisfied by CMP-CONSUMER; assumed by ASM-001 |
| MPSCRING-SRS-020 | The ABI is gated | abi/layout.txt shall describe the public ABI of app/mpscring.h (eight functions and the constant MPSC_RING_MIN_CAPACITY); the shared object shall export exactly the eight declared functions as <name>@@MPSCRING_1; a changed line of the layout shall need a new major, a new public name a new minor. | analysis | [abi/layout.txt#L1-L12](../../abi/layout.txt#L1-L12); [utils/mpscring.map#L1-L16](../../utils/mpscring.map#L1-L16); [README.md#build-profiles--hardening](../../README.md#build-profiles--hardening) | derives from IRD-005; verified by VC-007; justified by SRC-README; satisfied by IF-010, CI-DEBS |
| MPSCRING-SRS-021 | Two packages, reproducible | The library shall be delivered as libmpscring (the shared object and its soname link) and libmpscring-dev (the header, the static archive, the linker link), the second depending on the first at the same version, built to the same bytes from the same commit. | test | [utils/mpscring_project.sh#L37-L44](../../utils/mpscring_project.sh#L37-L44); [README.md#build-system](../../README.md#build-system) | derives from SSS-007; verified by VC-008; satisfied by IF-011, CI-DEBS |
| MPSCRING-SRS-022 | A hardened release | The release shared object shall pass kyr-build's hardening check: full RELRO, immediate binding, a non-executable stack, stack canaries, fortified calls, no RPATH, a versioned soname. | test | [README.md#build-profiles--hardening](../../README.md#build-profiles--hardening) | derives from SSS-007; verified by VC-009; satisfied by CI-DEBS |
| MPSCRING-SRS-023 | Nothing linked but the C library | At run time the library shall depend on the C library alone; deps.txt shall name only build and test tools. | inspection | [deps.txt#L1-L9](../../deps.txt#L1-L9) | derives from SSS-007; verified by VC-009; satisfied by IF-010, CI-DEBS |
| MPSCRING-SRS-024 | Test seams never shipped | The allocation, lock-free, push-hook and position-stealing seams shall be compiled only with MPSC_RING_TESTING, in the coverage and sanitizer runs, and never in a shipped artifact; the release library shall export no test symbol. | inspection | [app/mpscring.c#L51-L105](../../app/mpscring.c#L51-L105); [app/mpscring.c#L345-L357](../../app/mpscring.c#L345-L357); [utils/mpscring_project.sh#L11-L12](../../utils/mpscring_project.sh#L11-L12) | derives from SSS-007, SSS-008; verified by VC-001, VC-007; satisfied by IF-012, CI-DEBS |
| MPSCRING-SRS-025 | Verified under every profile that runs the suites | The unit suite shall pass against the release library; the unit and integration suites together shall cover 100 % of the lines and branches of app/mpscring.c; both shall pass under ASan, UBSan and LSan, the integration suite and a reduced stress under ThreadSanitizer, and the full stress (16 producers, 8,000,000 values) on the release archive. | test | [utils/mpscring_project.sh#L14-L35](../../utils/mpscring_project.sh#L14-L35); [README.md#build-system](../../README.md#build-system) | derives from SSS-008; verified by VC-001, VC-002, VC-003, VC-004, VC-005, VC-006; satisfied by CMP-SEAMS |
<!-- /ecss:table -->

### 5.3 Assumptions

<!-- ecss:table items doc=SRS kind=assumption -->
| ID | Title | Statement | Method | Source | Traces |
|---|---|---|---|---|---|
| MPSCRING-ASM-001 | One consumer at a time | The caller guarantees that at any moment at most one thread pops or checks emptiness, and that a change of consumer is ordered by a synchronisation. | - | [app/mpscring.h#L45-L56](../../app/mpscring.h#L45-L56) | assumption for SRS-007, SRS-011, SRS-017, SRS-019 |
| MPSCRING-ASM-002 | Producers publish promptly | No producer blocks, sleeps or is cancelled between its claim and its publish; the platform cancels no thread and kyr_core's submitters hold no lock in the hand-off. | - | [app/mpscring.h#L15-L26](../../app/mpscring.h#L15-L26); [customers/kyr_core/app/src/database/txn_queue/txn_queue.c#L343-L358](../../customers/kyr_core/app/src/database/txn_queue/txn_queue.c#L343-L358) | assumption for SRS-012, SRS-013 |
| MPSCRING-ASM-003 | Creation and destruction while the ring is quiet | The caller creates and destroys a ring only while no thread uses it. | - | [app/mpscring.h#L45-L47](../../app/mpscring.h#L45-L47); [customers/kyr_core/app/src/database/txn_queue/txn_queue.c#L276](../../customers/kyr_core/app/src/database/txn_queue/txn_queue.c#L276) | assumption for SRS-002, SRS-003 |
| MPSCRING-ASM-004 | A 64-bit target with lock-free 64-bit atomics | The library runs on a 64-bit target (x86-64, aarch64) whose 64-bit atomics are lock-free, with capacities far below 2^63. | - | [app/mpscring.c#L33-L49](../../app/mpscring.c#L33-L49); [app/mpscring.c#L111-L124](../../app/mpscring.c#L111-L124) | assumption for SRS-014, SRS-016 |
<!-- /ecss:table -->

## 6 Validation requirements

Each requirement states its criteria and method; its cases are the items `MPSCRING-VC-nnn` ([../DJF/SVS.md](../DJF/SVS.md#5-validation-cases)),
the release pipeline's stages at the release commit and one inspection.

## 7 Traceability

Up: each requirement derives from the RB (column Traces). Down: the interfaces of the ICD and the components of the SDD that satisfy
it, the cases that verify it. Sources: the literature and the standards each relies on.

<!-- ecss:table sources -->
| ID | Title | What it is | Reference |
|---|---|---|---|
| MPSCRING-SRC-E40 | ECSS-E-ST-40C Rev.1, Space engineering: Software | The engineering standard the package follows: the requirements baseline (5.2), the technical specification (5.4), the design (5.5), the validation (5.6), the delivery and acceptance (5.7), the verification (5.8) and the DRDs of its annexes. Cited, never copied. | ECSS-E-ST-40C Rev.1 |
| MPSCRING-SRC-Q80 | ECSS-Q-ST-80C Rev.2, Space product assurance: Software product assurance | The product assurance standard: the software criticality categories (6.2.2) the tailoring argues from. Cited, never copied. | ECSS-Q-ST-80C Rev.2 |
| MPSCRING-SRC-C11 | ISO/IEC 9899:2011, 7.17 Atomics | The C11 memory model: atomic objects, memory_order_relaxed, memory_order_acquire and memory_order_release, atomic_compare_exchange_weak_explicit and its spurious failures, the synchronizes-with relation, atomic_is_lock_free and the ATOMIC_*_LOCK_FREE macros. | ISO/IEC 9899:2011 clause 7.17; [app/mpscring.c#L15-L21](../../app/mpscring.c#L15-L21) |
| MPSCRING-SRC-VYUKOV | Vyukov, Bounded MPMC queue | D. Vyukov, Bounded MPMC queue, 1024cores.net (https://www.1024cores.net/home/lock-free-algorithms/queues/bounded-mpmc-queue): the per-slot sequence, the claim by compare-and-swap on one shared position, and the author's own note that a producer stalled between claim and publish blocks the consumer, so the queue is not formally lock-free. | [README.md#implementation-choices](../../README.md#implementation-choices); [app/mpscring.h#L6-L26](../../app/mpscring.h#L6-L26) |
| MPSCRING-SRC-HERLIHY | Herlihy, Wait-free synchronization (1991) | M. Herlihy, Wait-free synchronization, ACM Transactions on Programming Languages and Systems 13(1), 1991: wait-freedom and lock-freedom, the progress conditions SRS-012 and SRS-013 use. | ACM TOPLAS 13(1), 1991 |
| MPSCRING-SRC-README | The repository's README at 1.0.10 | README.md of MPSCring at 73bb6fe3: the API contract, the implementation choices, the lock-free target support, the build system, the profiles and hardening. | [README.md#api-contract](../../README.md#api-contract); [README.md#implementation-choices](../../README.md#implementation-choices) |
| MPSCRING-SRC-CORE | kyr_core 5.0.0, the customer's use | kyr_core at aa7479c8 (v5.0.0): the write queue, one ring of 1024 per environment, any request thread submitting a job within a 250 ms admission budget, the environment's one writer taking batches of 32 and sleeping on its condition while the ring is empty, the stop joining the writer and draining from the stopping thread; every module's durable write goes through it (kyr_drive, kyr_estate, kyr_rms). | [customers/kyr_core/app/src/database/txn_queue/txn_queue.c#L306-L372](../../customers/kyr_core/app/src/database/txn_queue/txn_queue.c#L306-L372); [customers/kyr_core/app/src/database/txn_queue/txn_queue.c#L655-L703](../../customers/kyr_core/app/src/database/txn_queue/txn_queue.c#L655-L703) |
<!-- /ecss:table -->
