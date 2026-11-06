# Software design document (SDD)

MPSCRING-DOC-SDD. Written by Emily Clarke from [app/mpscring.c](../../app/mpscring.c#L1-L421) at 73bb6fe3 (1.0.10). After the DRD of
ECSS-E-ST-40C Rev.1 Annex F; the architecture agreed at the PDR, the detail at the CDR. The function reference is generated from the
code ([api/mpscring.md](api/mpscring.md)); the figures are PlantUML in [../diagrams/](../diagrams/component.puml), each rendered to SVG
beside its source.

## 1 Introduction

The design of the ring: one control block, slots that each carry a sequence, producers that claim positions by compare-and-swap, one
consumer that needs none, storage the caller or the library owns, test seams that never ship.

## 2 Applicable and reference documents

The SRS ([../TS/SRS.md](../TS/SRS.md#5-requirements)), the ICD ([../TS/ICD.md](../TS/ICD.md#53-interface-design)),
[README.md](../../README.md#implementation-choices), ISO/IEC 9899:2011 7.17, D. Vyukov's bounded MPMC queue.

## 3 Terms

As in the SRS ([../TS/SRS.md](../TS/SRS.md#3-terms)).

## 4 Software design overview

### 4.1 Static architecture

![Components](../diagrams/component.svg)

Figure 4-1 ([component.puml](../diagrams/component.puml)). The library is one translation unit; its components are the parts of that
unit each with one job:

<!-- ecss:table items doc=SDD kind=component -->
| ID | Title | Statement | Method | Source | Traces |
|---|---|---|---|---|---|
| MPSCRING-CMP-STATE | The control block and the slots | struct mpsc_ring: the slot array, the capacity and its mask (read-only after creation), who owns the storage and where it began, then the producers' enqueue_pos (_Atomic uint64_t) and the consumer's dequeue_pos (plain uint64_t), each aligned on its own cache line, the separation proven by a _Static_assert; mpsc_slot_t: an _Atomic uint64_t sequence and the void* payload. | - | [app/mpscring.c#L126-L159](../../app/mpscring.c#L126-L159) | satisfies SRS-004, SRS-009, SRS-015, SRS-016; justified by DEC-001, DEC-004, DEC-006, DEC-009 |
| MPSCRING-CMP-STORAGE | Storage and placement | mpsc_ring_storage_size() (the control block, the worst-case slack of both alignments, the slots) and mpsc_ring_init_into(): the control block aligned up inside the caller's storage, the lock-free probe of enqueue_pos, the slots aligned after the block, the probe of a slot's sequence, every slot's sequence set to its index. | - | [app/mpscring.c#L161-L263](../../app/mpscring.c#L161-L263) | satisfies SRS-001, SRS-002, SRS-009, SRS-014; justified by DEC-003, DEC-005, DEC-007 |
| MPSCRING-CMP-LIFE | Owned storage and teardown | mpsc_ring_init(): calloc of exactly mpsc_ring_storage_size() bytes, mpsc_ring_init_into() over them, the ownership recorded, the bytes freed on any failure; mpsc_ring_destroy(): the storage freed only when owned, the caller's pointer cleared. | - | [app/mpscring.c#L265-L287](../../app/mpscring.c#L265-L287); [app/mpscring.c#L410-L421](../../app/mpscring.c#L410-L421) | satisfies SRS-003, SRS-018; justified by DEC-005 |
| MPSCRING-CMP-PRODUCER | The producers' claim and publish | mpsc_ring_push(): enqueue_pos read relaxed; the slot's sequence read with acquire; equal to pos: a weak compare-and-swap of enqueue_pos claims the position; behind: the ring is full; ahead: another producer moved on, enqueue_pos is read again; after the claim the payload is stored and the sequence released at pos + 1. | - | [app/mpscring.c#L289-L343](../../app/mpscring.c#L289-L343) | satisfies SRS-005, SRS-006, SRS-008, SRS-010, SRS-012, SRS-013; justified by DEC-001, DEC-008, DEC-010 |
| MPSCRING-CMP-CONSUMER | The consumer's take | mpsc_ring_pop() and mpsc_ring_is_empty(): the consumer's plain dequeue_pos, the slot's sequence read with acquire, published when equal to pos + 1; pop reads the payload, advances dequeue_pos and releases the sequence at pos + capacity, freeing the slot for the next lap. | - | [app/mpscring.c#L359-L399](../../app/mpscring.c#L359-L399) | satisfies SRS-005, SRS-007, SRS-011, SRS-012, SRS-017, SRS-019; justified by DEC-002 |
| MPSCRING-CMP-QUERY | The capacity | mpsc_ring_capacity(): the fixed size, 0 for NULL, from any thread. | - | [app/mpscring.c#L401-L408](../../app/mpscring.c#L401-L408) | satisfies SRS-018 |
| MPSCRING-CMP-SEAMS | The test seams (tests only) | Under MPSC_RING_TESTING only: the allocator replaced, the two lock-free verdicts overridden, a single-shot hook fired between push's snapshot of enqueue_pos and its claim, and mpsc_ring_test_steal_enqueue_pos() moving the position without publishing, so that a single-threaded test reaches the lost-claim branch; without the switch none of it is compiled or exported. | - | [app/mpscring.c#L51-L105](../../app/mpscring.c#L51-L105); [app/mpscring.c#L299-L308](../../app/mpscring.c#L299-L308); [app/mpscring.c#L345-L357](../../app/mpscring.c#L345-L357); [tests/UTs/mpscring_test_hooks.h#L1-L60](../../tests/UTs/mpscring_test_hooks.h#L1-L60) | satisfies SRS-025 |
<!-- /ecss:table -->

### 4.2 Delivery

![Delivery and linkage](../diagrams/deployment.svg)

Figure 4-2 ([deployment.puml](../diagrams/deployment.puml)). The configuration item delivered:

<!-- ecss:table items doc=SDD kind=configuration-item -->
| ID | Title | Statement | Method | Source | Traces |
|---|---|---|---|---|---|
| MPSCRING-CI-DEBS | libmpscring 1.0.10 and libmpscring-dev 1.0.10 | The release's two Debian packages, built in the build root by kyr-build's lib/deb.sh from 73bb6fe3 and published with the release: libmpscring_1.0.10_amd64.deb SHA-256 3d517ac7fd93416b19e4c44b2e334cd312a8a33a8d0d0e32bfba768b510fbd23, libmpscring-dev_1.0.10_amd64.deb SHA-256 025cecd055fcf136c72c0c7441cdea8a9608a1fc4756d9c5154c539ad7f46032. The qualification run on the build host (Build-Root: none) built development debs of other bytes; its package smoke test ran on those. | - | [utils/mpscring_project.sh#L37-L44](../../utils/mpscring_project.sh#L37-L44); [ecss/DJF/runs/73bb6fe3/package.log#L1](../DJF/runs/73bb6fe3/package.log#L1) | satisfies SRS-020, SRS-021, SRS-022, SRS-023, SRS-024, SSS-007 |
<!-- /ecss:table -->

The repository's own figures: [the ABI gate](../../docs/diagrams/abi_exports.puml) and [the tooling](../../docs/diagrams/tooling_sequence.puml).

## 5 Software components design

### 5.1 The control block and the storage

`struct mpsc_ring` ([app/mpscring.c](../../app/mpscring.c#L133-L156)): `buf`, `size` and `mask` are written once by the creation;
`owns_storage` and `storage_base` say what destroy frees; `enqueue_pos` (the producers', atomic) and `dequeue_pos` (the consumer's,
plain) are each `_Alignas(MPSC_CACHELINE)`, the separation proven at compile time ([app/mpscring.c](../../app/mpscring.c#L158-L159)). A
slot is an atomic sequence and a `void*` ([app/mpscring.c](../../app/mpscring.c#L126-L131)).

`mpsc_ring_storage_size` ([app/mpscring.c](../../app/mpscring.c#L188-L217)) adds to the block and the slots the worst-case slack of
both alignments, so `mpsc_ring_init_into` ([app/mpscring.c](../../app/mpscring.c#L219-L263)) can align the block up inside any buffer
and the slots after it. `mpsc_ring_init` ([app/mpscring.c](../../app/mpscring.c#L265-L287)) is a `calloc` of that size and a call to
`mpsc_ring_init_into`, the ownership recorded: one control for both modes.

### 5.2 A slot's sequence

![A slot's sequence](../diagrams/slot_states.svg)

Figure 5-1 ([slot_states.puml](../diagrams/slot_states.puml)). Slot i starts at sequence i. For the position pos it serves, the sequence
reads pos (free), pos + 1 (published) or pos + capacity (read, free for the next lap). A producer for pos + capacity that finds the
sequence behind it answers full; the consumer at pos that finds it behind pos + 1 answers empty. Capacity 1 is refused: its one slot's
published state (pos + 1) would read as free for the next position.

### 5.3 The memory ordering

![Push and pop](../diagrams/push_pop_sequence.svg)

Figure 5-2 ([push_pop_sequence.puml](../diagrams/push_pop_sequence.puml)). The slot's sequence carries every ordering; enqueue_pos only
decides ownership:

- a producer reads the sequence with acquire ([app/mpscring.c](../../app/mpscring.c#L313)), pairing with the consumer's release of the
  previous lap ([app/mpscring.c](../../app/mpscring.c#L385)): the slot was read before it is reused;
- it claims with a relaxed compare-and-swap of enqueue_pos ([app/mpscring.c](../../app/mpscring.c#L324)), stores the payload and
  publishes with a release store of pos + 1 ([app/mpscring.c](../../app/mpscring.c#L340-L341));
- the consumer reads the sequence with acquire ([app/mpscring.c](../../app/mpscring.c#L368)), pairing with that release: the payload,
  and whatever the submitter wrote into the job before its push, is visible; it reads the payload, advances its plain position and frees
  the slot with a release store of pos + capacity ([app/mpscring.c](../../app/mpscring.c#L376-L385)).

Sequences and positions are compared by their unsigned modular difference against UINT64_MAX / 2 (MPSC_SEQ_IS_NEGATIVE,
[app/mpscring.c](../../app/mpscring.c#L111-L124)): no signed conversion, wrap-safe for any capacity far below 2^63.

### 5.4 The claim and the stalled producer

![The claim and the stalled producer](../diagrams/mpsc_reservation.svg)

Figure 5-3 ([mpsc_reservation.puml](../diagrams/mpsc_reservation.puml)). A producer loops ([app/mpscring.c](../../app/mpscring.c#L310-L338)):
a sequence equal to its position, it tries the claim; behind it, the ring is full and push answers -1; ahead of it, another producer
moved on and enqueue_pos is read again. A lost claim means another producer's won (or a weak compare-and-swap failed spuriously): the
producers are lock-free among themselves. pop and is_empty are straight-line: the consumer is wait-free.

The ring as a whole is not lock-free: between its claim and its publish a producer holds a position whose slot reads free, so the
consumer answers empty at that position and every later published job waits; producers answer full once the ring wraps to it. Nothing
is lost or read early; delivery resumes at the publish. The window holds one store and a release, no call. The header says so
([app/mpscring.h](../../app/mpscring.h#L15-L26)); the customer accepts it (MPSCRING-SSS-005) and keeps its producers running through it
(MPSCRING-ASM-002); kyr_core's writer, woken by each submitter after its own push, sleeps again while the head of the ring is unpublished.

### 5.5 The test seams

Under `MPSC_RING_TESTING` only ([app/mpscring.c](../../app/mpscring.c#L51-L105)): the allocator replaced, the two lock-free verdicts
overridden, a single-shot hook between push's snapshot and its claim ([app/mpscring.c](../../app/mpscring.c#L299-L308)) and a raw bump of
enqueue_pos without a publish ([app/mpscring.c](../../app/mpscring.c#L345-L357)): together they let one thread reach the lost-claim
branch that otherwise only real contention does. Without the switch none of it is compiled or exported.

## 6 Decisions

<!-- ecss:table items doc=SDD kind=decision -->
| ID | Title | Statement | Method | Source | Traces |
|---|---|---|---|---|---|
| MPSCRING-DEC-001 | Vyukov's bounded queue, per-slot sequences | Each slot carries a sequence that says free for a position, published for it, or freed for the next lap; producers claim positions by compare-and-swap on one shared position. | - | [README.md#implementation-choices](../../README.md#implementation-choices); [app/mpscring.c#L5-L14](../../app/mpscring.c#L5-L14) | - |
| MPSCRING-DEC-002 | One consumer, no compare-and-swap on its side | Only one thread pops, so the consumer's position is a plain field and pop is straight-line code; the MPMC generality of the algorithm is not used. | - | [README.md#implementation-choices](../../README.md#implementation-choices); [app/mpscring.c#L149-L155](../../app/mpscring.c#L149-L155) | - |
| MPSCRING-DEC-003 | Power-of-two capacity, at least two | The capacity must be a power of two (a mask addresses the slot) and at least 2 (one slot aliases published and free). | - | [app/mpscring.c#L190-L197](../../app/mpscring.c#L190-L197) | - |
| MPSCRING-DEC-004 | 64-bit positions, an unsigned comparison | Positions and sequences are uint64_t, compared by their modular difference against UINT64_MAX / 2 (MPSC_SEQ_IS_NEGATIVE), the trick of TCP sequence numbers; it replaced a signed cast. | - | [app/mpscring.c#L111-L124](../../app/mpscring.c#L111-L124) | - |
| MPSCRING-DEC-005 | Two allocation modes, one control | mpsc_ring_init_into() builds the ring in the caller's memory and never allocates; mpsc_ring_init() is a calloc of mpsc_ring_storage_size() bytes and a call to it; mpsc_ring_storage_size() includes the worst-case slack of both alignments. | - | [README.md#implementation-choices](../../README.md#implementation-choices); [app/mpscring.c#L203-L210](../../app/mpscring.c#L203-L210) | - |
| MPSCRING-DEC-006 | The two positions on separate cache lines | enqueue_pos and dequeue_pos are each _Alignas(MPSC_CACHELINE). | - | [app/mpscring.c#L142-L159](../../app/mpscring.c#L142-L159) | - |
| MPSCRING-DEC-007 | Refuse lock-backed atomics | Creation probes enqueue_pos and a slot's sequence with atomic_is_lock_free and refuses the ring; the project's builds also define MPSC_REQUIRE_ALWAYS_LOCK_FREE for a compile-time refusal. | - | [README.md#lock-free-target-support](../../README.md#lock-free-target-support); [app/mpscring.c#L243-L254](../../app/mpscring.c#L243-L254) | - |
| MPSCRING-DEC-008 | A relaxed compare-and-swap | The claim's compare-and-swap on enqueue_pos is relaxed on success and failure; the ordering is carried by the slot's sequence, acquired before the claim and released after the store. | - | [app/mpscring.c#L313](../../app/mpscring.c#L313); [app/mpscring.c#L324](../../app/mpscring.c#L324) | - |
| MPSCRING-DEC-009 | An opaque void* payload, NULL storable | The ring stores void* and knows nothing of what it points to; NULL is stored and popped like any value. | - | [app/mpscring.h#L31-L34](../../app/mpscring.h#L31-L34) | - |
| MPSCRING-DEC-010 | Accept the claim-to-publish window | The ring is not made formally lock-free: a producer stalled between its claim and its publish holds back delivery; the window is a store and a release, no call, and the header says so. | - | [app/mpscring.h#L15-L26](../../app/mpscring.h#L15-L26) | - |
<!-- /ecss:table -->

## 7 Requirements to design

Every software requirement is satisfied by a component or the configuration item (column Traces of 4.1 and 4.2); the components' own
traces to the decisions are the links justified-by.
