# Interface requirements document (IRD) toward MPSCring

MPSCRING-DOC-IRD. Written by Oliver Hughes, the customer's representative (KYR, the platform), from kyr_core 5.0.0 (aa7479c8). ECSS
makes the interface requirements the customer's output (ECSS-E-ST-40C Rev.1 5.2.4.3a), part of the requirements baseline it hands the
supplier, who answers them in its ICD ([../TS/ICD.md](../TS/ICD.md#53-interface-design)). Agreed at the SRR.

## 1 Introduction

kyr_core calls five functions of the ring: it creates one per environment, the submitters push job pointers, the writer checks
emptiness and pops batches, the stop drains and destroys. This document says what the platform requires of those calls.

## 2 Applicable and reference documents

The SSS ([SSS.md](SSS.md#5-requirements)); kyr_core 5.0.0: [the queue's state](../../customers/kyr_core/app/src/database/txn_queue/txn_queue.c#L95-L100), [the creation](../../customers/kyr_core/app/src/database/txn_queue/txn_queue.c#L164-L173),
[the push](../../customers/kyr_core/app/src/database/txn_queue/txn_queue.c#L340-L358), [the writer](../../customers/kyr_core/app/src/database/txn_queue/txn_queue.c#L655-L692), [the stop's drain](../../customers/kyr_core/app/src/database/txn_queue/txn_queue.c#L235-L256), [the destroy](../../customers/kyr_core/app/src/database/txn_queue/txn_queue.c#L276).

## 3 Terms

As in the SSS ([SSS.md](SSS.md#3-terms)).

## 4 The interface as the customer uses it

| Call (kyr_core 5.0.0) | Where | What the customer relies on |
|---|---|---|
| `q->ring = mpsc_ring_init(KYR_CORE_TXN_QUEUE_RING_CAPACITY)` | [txn_queue.c](../../customers/kyr_core/app/src/database/txn_queue/txn_queue.c#L164-L173) | a ring of 1024, or NULL, which fails the queue's start |
| `mpsc_ring_push(q->ring, job) == 0` | [txn_queue.c](../../customers/kyr_core/app/src/database/txn_queue/txn_queue.c#L343-L358) | 0 when the job is in; anything else: full, retry within the budget |
| `while(mpsc_ring_is_empty(q->ring) && ...)` | [txn_queue.c](../../customers/kyr_core/app/src/database/txn_queue/txn_queue.c#L659-L666) | from the writer's thread, whether a job is published at its next position |
| `while(n < 32 && mpsc_ring_pop(q->ring, &item) == 0)` | [txn_queue.c](../../customers/kyr_core/app/src/database/txn_queue/txn_queue.c#L679-L692) | 0 and the job pushed, until the ring answers empty |
| `mpsc_ring_destroy(&q->ring)` | [txn_queue.c](../../customers/kyr_core/app/src/database/txn_queue/txn_queue.c#L276) | everything released, the pointer set to NULL |

The customer does not use the caller-owned storage (`mpsc_ring_init_into`) nor the capacity query.

## 5 Interface requirements

<!-- ecss:table items doc=IRD -->
| ID | Title | Statement | Method | Source | Traces |
|---|---|---|---|---|---|
| MPSCRING-IRD-001 | A C interface of create, push, pop, is-empty and destroy | The supplier shall provide a C11 header declaring an opaque ring type and the calls to create a ring of a given capacity, push a pointer, pop a pointer into the caller's variable, tell from the consumer whether the ring is empty, and destroy the ring setting the caller's pointer to NULL. | inspection | [customers/kyr_core/app/src/database/txn_queue/txn_queue.c#L95-L97](../../customers/kyr_core/app/src/database/txn_queue/txn_queue.c#L95-L97); [customers/kyr_core/app/src/database/txn_queue/txn_queue.c#L167](../../customers/kyr_core/app/src/database/txn_queue/txn_queue.c#L167); [customers/kyr_core/app/src/database/txn_queue/txn_queue.c#L345](../../customers/kyr_core/app/src/database/txn_queue/txn_queue.c#L345); [customers/kyr_core/app/src/database/txn_queue/txn_queue.c#L662](../../customers/kyr_core/app/src/database/txn_queue/txn_queue.c#L662); [customers/kyr_core/app/src/database/txn_queue/txn_queue.c#L682](../../customers/kyr_core/app/src/database/txn_queue/txn_queue.c#L682); [customers/kyr_core/app/src/database/txn_queue/txn_queue.c#L276](../../customers/kyr_core/app/src/database/txn_queue/txn_queue.c#L276) | verified by VC-007, VC-008; source of SRS-018; satisfied by IF-002, IF-005, IF-006, IF-007, IF-008, IF-009 |
| MPSCRING-IRD-002 | Statuses as return values | Creation shall return NULL when it fails; push and pop shall return 0 on success and -1 when the ring is full or empty (or not there), with no other effect on a refusal and no errno to read. | test | [customers/kyr_core/app/src/database/txn_queue/txn_queue.c#L167-L173](../../customers/kyr_core/app/src/database/txn_queue/txn_queue.c#L167-L173); [customers/kyr_core/app/src/database/txn_queue/txn_queue.c#L345](../../customers/kyr_core/app/src/database/txn_queue/txn_queue.c#L345); [customers/kyr_core/app/src/database/txn_queue/txn_queue.c#L682](../../customers/kyr_core/app/src/database/txn_queue/txn_queue.c#L682) | verified by VC-001; source of SRS-003, SRS-006, SRS-007, SRS-018; satisfied by IF-005, IF-006, IF-007 |
| MPSCRING-IRD-003 | Many pushing threads, one popping thread at a time | Push shall be safe from any number of threads at the same time, with no lock; pop and the emptiness check shall be made by one thread at a time, the writer's, then the stopping thread's after the writer is joined. | test | [customers/kyr_core/app/src/database/txn_queue/txn_queue.c#L655-L692](../../customers/kyr_core/app/src/database/txn_queue/txn_queue.c#L655-L692); [customers/kyr_core/app/src/database/txn_queue/txn_queue.c#L214-L256](../../customers/kyr_core/app/src/database/txn_queue/txn_queue.c#L214-L256) | verified by VC-003, VC-005; source of SRS-008, SRS-010, SRS-011, SRS-017; satisfied by IF-006, IF-007, IF-008 |
| MPSCRING-IRD-004 | The job popped is the job pushed, its contents visible | A pointer popped shall be exactly the one pushed, and what the submitter wrote into the job before its push shall be visible to the writer after its pop: the hand-off shall order the producer's writes before the consumer's reads (release on push, acquire on pop). | test | [customers/kyr_core/app/src/database/txn_queue/txn_queue.c#L336-L345](../../customers/kyr_core/app/src/database/txn_queue/txn_queue.c#L336-L345); [customers/kyr_core/app/src/database/txn_queue/txn_queue.c#L682-L685](../../customers/kyr_core/app/src/database/txn_queue/txn_queue.c#L682-L685) | verified by VC-005, VC-006, VC-011; source of SRS-005, SRS-009, SRS-010, SRS-011; satisfied by IF-006, IF-007 |
| MPSCRING-IRD-005 | Linkage: versioned shared object and archive | The library shall be a shared object with a versioned soname and versioned symbols, exporting exactly its declared functions, and a static archive with the same functions. | test | [customers/kyr_core/deps.txt#L21-L22](../../customers/kyr_core/deps.txt#L21-L22); [customers/kyr_server/deps.txt#L27-L28](../../customers/kyr_server/deps.txt#L27-L28) | verified by VC-007, VC-008; source of SRS-020; satisfied by IF-010 |
<!-- /ecss:table -->

## 6 Open points toward the supplier

- The customer pins libmpscring 1.0.8 ([deps.txt](../../customers/kyr_core/deps.txt#L21-L22)); the supplier qualifies 1.0.10. The customer
  moves its pin when the QR shows the two are the same code.
- The customer annotates the hand-off for its thread-sanitizer builds ([txn_queue.c](../../customers/kyr_core/app/src/database/txn_queue/txn_queue.c#L36-L44)) because the library it links is not
  instrumented; the supplier's own TSan runs instrument it.
