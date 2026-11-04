# Software system specification (SSS): what the platform requires of the ring

MPSCRING-DOC-SSS. Written by Oliver Hughes, the customer's representative (KYR, the platform), from the customer's own code: kyr_core
5.0.0 (aa7479c8), whose write queue every module of the box writes through. Part of the requirements baseline the platform gives the
supplier (ECSS-E-ST-40C Rev.1 5.2), agreed at the SRR. The interface the customer requires is in the IRD
([IRD.md](IRD.md#5-interface-requirements)), the work in the statement of work ([SOW.md](SOW.md#2-work-to-be-performed)).

## 1 Introduction

The box keeps every record in kyr_lmdb environments, each written by one writer thread at a time. Any request thread that has to write
(a file of the drive, a house of the estate, an item of the rms, a line of a security log) submits a job to kyr_core's write queue; the
queue hands the job to its environment's one writer thread, which commits jobs in batches. Between the request threads and the writer
sits one ring per environment: many producers, one consumer. This document states what the platform needs of that ring, in the
platform's words; the supplier answers it in its technical specification ([../TS/SRS.md](../TS/SRS.md#5-requirements)).

## 2 Applicable and reference documents

- ECSS-E-ST-40C Rev.1, ECSS-Q-ST-80C Rev.2: cited, never copied.
- kyr_core 5.0.0: [the submit](../../customers/kyr_core/app/src/database/txn_queue/txn_queue.c#L306-L372), [the writer](../../customers/kyr_core/app/src/database/txn_queue/txn_queue.c#L655-L703), [the stop](../../customers/kyr_core/app/src/database/txn_queue/txn_queue.c#L214-L256),
  [the sizes](../../customers/kyr_core/app/src/database/txn_queue/txn_queue.c#L46-L72), [the thread-sanitizer annotations](../../customers/kyr_core/app/src/database/txn_queue/txn_queue.c#L36-L44),
  [the queue's contract](../../customers/kyr_core/app/include/kyr/core/database/txn_queue.h#L160-L176), [the pins](../../customers/kyr_core/deps.txt#L21-L22).
- The modules writing through it: [kyr_drive](../../customers/kyr_drive/app/src/database/repo/repo_file.c#L229-L230),
  [kyr_estate](../../customers/kyr_estate/app/src/store.c#L141-L145), [kyr_rms](../../customers/kyr_rms/app/src/module/boot.c#L94).
- The statement of work ([SOW.md](SOW.md#1-scope)) and the IRD ([IRD.md](IRD.md#1-introduction)).

## 3 Terms

- submitter: a request thread calling kyr_core_txn_queue_submit(); one of the ring's producers.
- writer: the one thread of an environment that commits its jobs; the ring's consumer.
- job: the submitter's structure (the function to run, its context, the result), handed over by pointer.
- admission: the submitter getting a completion slot and a ring slot within 250 ms; after it, the job is the queue's.

## 4 General description

A submitter takes a completion slot, fills the job, pushes its pointer into the environment's ring, retrying every 0.2 ms while the
ring is full until its 250 ms budget runs out (then the write is refused, INTERNAL), and signals the writer. The writer sleeps on its
condition while the ring is empty, then takes up to 32 jobs, runs each in a child transaction of one parent, commits the parent and
answers each submitter. At the stop the writer is joined and the stopping thread takes what is left, answering each job INTERNAL. The
rings (1024 each) are made at the queue's start and released at its stop; nothing is allocated on the request path. Under its own
thread-sanitizer builds the customer annotates the hand-off (release before the push, acquire after the pop), the library it links
being uninstrumented.

What a defect of the ring does on the box: a lost job leaves its submitter waiting for ever (the wait after admission has no bound) and
its write never made; a duplicated job runs a write twice; a torn pointer runs a stranger's job or crashes the box. Every durable write
of every module crosses the ring (the basis of the category, [../PAF/TAILORING.md](../PAF/TAILORING.md#3-criticality-classification)).

## 5 Requirements

The customer's requirements on the software; each is an rms item of MPSCRING written by Oliver Hughes, derived by the supplier into the
SRS (the column Traces) and verified by the supplier's cases.

<!-- ecss:table items doc=SSS -->
| ID | Title | Statement | Method | Source | Traces |
|---|---|---|---|---|---|
| MPSCRING-SSS-001 | Every admitted write job reaches its writer, once | The software shall carry each write job a submitter admits (a pointer to the caller's job) from the submitting thread to its environment's one writer thread, each job delivered exactly once and none lost, whatever the number of threads submitting at once. | test | [customers/kyr_core/app/src/database/txn_queue/txn_queue.c#L340-L358](../../customers/kyr_core/app/src/database/txn_queue/txn_queue.c#L340-L358); [customers/kyr_core/app/src/database/txn_queue/txn_queue.c#L679-L692](../../customers/kyr_core/app/src/database/txn_queue/txn_queue.c#L679-L692); [customers/kyr_drive/app/src/database/repo/repo_file.c#L229-L230](../../customers/kyr_drive/app/src/database/repo/repo_file.c#L229-L230); [customers/kyr_estate/app/src/store.c#L141-L145](../../customers/kyr_estate/app/src/store.c#L141-L145) | verified by VC-003, VC-005, VC-006; justified by SRC-CORE; source of SRS-005, SRS-008, SRS-009, SRS-016 |
| MPSCRING-SSS-002 | Many submitters at once, none waiting on another | Any number of request threads shall hand jobs over at the same time without a lock and without waiting on one another; a hand-off on a full ring shall refuse at once, so that the submitter retries within its admission budget (250 ms, every 0.2 ms) and then refuses the write. | test | [customers/kyr_core/app/src/database/txn_queue/txn_queue.c#L58-L63](../../customers/kyr_core/app/src/database/txn_queue/txn_queue.c#L58-L63); [customers/kyr_core/app/src/database/txn_queue/txn_queue.c#L343-L358](../../customers/kyr_core/app/src/database/txn_queue/txn_queue.c#L343-L358); [customers/kyr_core/app/include/kyr/core/database/txn_queue.h#L160-L176](../../customers/kyr_core/app/include/kyr/core/database/txn_queue.h#L160-L176) | verified by VC-001, VC-003, VC-011; justified by SRC-CORE; source of SRS-006, SRS-008, SRS-012, SRS-014, SRS-015 |
| MPSCRING-SSS-003 | A bounded ring per environment, allocated once | Each environment's ring shall hold a number of jobs fixed when the queue starts (1024), every slot usable, its memory taken once at the start; nothing shall be allocated on the request path. | test | [customers/kyr_core/app/src/database/txn_queue/txn_queue.c#L46-L50](../../customers/kyr_core/app/src/database/txn_queue/txn_queue.c#L46-L50); [customers/kyr_core/app/src/database/txn_queue/txn_queue.c#L68-L72](../../customers/kyr_core/app/src/database/txn_queue/txn_queue.c#L68-L72); [customers/kyr_core/app/src/database/txn_queue/txn_queue.c#L164-L173](../../customers/kyr_core/app/src/database/txn_queue/txn_queue.c#L164-L173) | verified by VC-001, VC-002; source of SRS-001, SRS-002, SRS-003, SRS-004 |
| MPSCRING-SSS-004 | The writer takes batches without waiting | The environment's writer shall take up to 32 jobs at a time, each take answering at once whether there was one, and shall learn from its own thread, before it sleeps on its condition, whether a job is waiting. | test | [customers/kyr_core/app/src/database/txn_queue/txn_queue.c#L655-L692](../../customers/kyr_core/app/src/database/txn_queue/txn_queue.c#L655-L692) | verified by VC-001, VC-003; justified by SRC-CORE; source of SRS-007, SRS-012, SRS-017 |
| MPSCRING-SSS-005 | A preempted submitter delays, never loses | The software may delay the delivery of the jobs behind a submitter preempted inside its own hand-off, but shall never lose, duplicate or tear a job because of it; delivery shall resume when that submitter runs on. | analysis | [customers/kyr_core/app/src/database/txn_queue/txn_queue.c#L343-L365](../../customers/kyr_core/app/src/database/txn_queue/txn_queue.c#L343-L365) | verified by VC-002, VC-011; justified by SRC-CORE; source of SRS-013 |
| MPSCRING-SSS-006 | The writer's role passes to the stopping thread | At the queue's stop the writer thread is joined and the stopping thread drains what is left, so that no submitter waits for ever: the ring shall accept its consumer changing from one thread to another across a thread join. | analysis | [customers/kyr_core/app/src/database/txn_queue/txn_queue.c#L214-L229](../../customers/kyr_core/app/src/database/txn_queue/txn_queue.c#L214-L229); [customers/kyr_core/app/src/database/txn_queue/txn_queue.c#L235-L256](../../customers/kyr_core/app/src/database/txn_queue/txn_queue.c#L235-L256); [customers/kyr_core/app/src/database/txn_queue/txn_queue.c#L695-L701](../../customers/kyr_core/app/src/database/txn_queue/txn_queue.c#L695-L701) | verified by VC-011; justified by SRC-CORE; source of SRS-019 |
| MPSCRING-SSS-007 | Delivered as the platform pins its libraries | The software shall be delivered as the Debian packages libmpscring and libmpscring-dev, tier 1 of the platform (no platform dependency of its own), built reproducibly and pinned by the platform by version and SHA-256 in deps.txt. | test | [customers/kyr_core/deps.txt#L21-L22](../../customers/kyr_core/deps.txt#L21-L22); [customers/kyr_server/deps.txt#L27-L28](../../customers/kyr_server/deps.txt#L27-L28); [customers/kyr/deps.txt#L23](../../customers/kyr/deps.txt#L23); [customers/kyr_drive/deps.txt#L17-L18](../../customers/kyr_drive/deps.txt#L17-L18); [customers/kyr_estate/deps.txt#L18-L19](../../customers/kyr_estate/deps.txt#L18-L19); [customers/kyr_rms/deps.txt#L18-L19](../../customers/kyr_rms/deps.txt#L18-L19) | verified by VC-007, VC-008, VC-009; source of SRS-021, SRS-022, SRS-023, SRS-024; satisfied by IF-011, CI-DEBS |
| MPSCRING-SSS-008 | Qualified under the platform's build profiles | Each release the platform pins shall be built and tested under the platform's kyr-build profiles (release, sanitizers, thread sanitizer, coverage), its pipeline green at the release commit. | test | [customers/kyr_core/app/src/database/txn_queue/txn_queue.c#L36-L44](../../customers/kyr_core/app/src/database/txn_queue/txn_queue.c#L36-L44); [customers/kyr_core/deps.txt#L21-L22](../../customers/kyr_core/deps.txt#L21-L22) | verified by VC-002, VC-004, VC-005, VC-006; source of SRS-024, SRS-025 |
| MPSCRING-SRC-CORE | kyr_core 5.0.0, the customer's use | kyr_core at aa7479c8 (v5.0.0): the write queue, one ring of 1024 per environment, any request thread submitting a job within a 250 ms admission budget, the environment's one writer taking batches of 32 and sleeping on its condition while the ring is empty, the stop joining the writer and draining from the stopping thread; every module's durable write goes through it (kyr_drive, kyr_estate, kyr_rms). | - | [customers/kyr_core/app/src/database/txn_queue/txn_queue.c#L306-L372](../../customers/kyr_core/app/src/database/txn_queue/txn_queue.c#L306-L372); [customers/kyr_core/app/src/database/txn_queue/txn_queue.c#L655-L703](../../customers/kyr_core/app/src/database/txn_queue/txn_queue.c#L655-L703) | - |
<!-- /ecss:table -->

## 6 From the requirements baseline down

How each customer requirement is answered: the software requirements derived from it, the interfaces and components that satisfy them,
and the cases that verify them.

<!-- ecss:table trace -->
| Customer requirement | Software requirements | Interfaces and components | Cases |
|---|---|---|---|
| SSS-001 Every admitted write job reaches its writer, once | SRS-005, SRS-008, SRS-009, SRS-016 | CMP-CONSUMER, CMP-PRODUCER, CMP-STATE, CMP-STORAGE, IF-006, IF-007 | VC-001, VC-002, VC-003, VC-005, VC-006, VC-011 |
| SSS-002 Many submitters at once, none waiting on another | SRS-006, SRS-008, SRS-012, SRS-014, SRS-015 | CMP-CONSUMER, CMP-PRODUCER, CMP-STATE, CMP-STORAGE, IF-004, IF-005, IF-006, IF-007, IF-012 | VC-001, VC-002, VC-003, VC-005, VC-010, VC-011 |
| SSS-003 A bounded ring per environment, allocated once | SRS-001, SRS-002, SRS-003, SRS-004 | CMP-LIFE, CMP-STATE, CMP-STORAGE, IF-001, IF-003, IF-004, IF-005, IF-009 | VC-001, VC-002, VC-003 |
| SSS-004 The writer takes batches without waiting | SRS-007, SRS-012, SRS-017 | CMP-CONSUMER, CMP-PRODUCER, IF-006, IF-007, IF-008 | VC-001, VC-003, VC-011 |
| SSS-005 A preempted submitter delays, never loses | SRS-013 | CMP-PRODUCER | VC-002, VC-011 |
| SSS-006 The writer's role passes to the stopping thread | SRS-019 | CMP-CONSUMER | VC-011 |
| SSS-007 Delivered as the platform pins its libraries | SRS-021, SRS-022, SRS-023, SRS-024 | CI-DEBS, IF-010, IF-011, IF-012 | VC-001, VC-007, VC-008, VC-009 |
| SSS-008 Qualified under the platform's build profiles | SRS-024, SRS-025 | CI-DEBS, CMP-SEAMS, IF-012 | VC-001, VC-002, VC-003, VC-004, VC-005, VC-006, VC-007 |
| IRD-001 A C interface of create, push, pop, is-empty and destroy | SRS-018 | CMP-LIFE, CMP-QUERY, IF-002, IF-005, IF-006, IF-007, IF-008, IF-009 | VC-001, VC-007, VC-008 |
| IRD-002 Statuses as return values | SRS-003, SRS-006, SRS-007, SRS-018 | CMP-CONSUMER, CMP-LIFE, CMP-PRODUCER, CMP-QUERY, IF-002, IF-005, IF-006, IF-007, IF-008, IF-009 | VC-001, VC-002 |
| IRD-003 Many pushing threads, one popping thread at a time | SRS-008, SRS-010, SRS-011, SRS-017 | CMP-CONSUMER, CMP-PRODUCER, IF-006, IF-007, IF-008 | VC-001, VC-002, VC-003, VC-005, VC-011 |
| IRD-004 The job popped is the job pushed, its contents visible | SRS-005, SRS-009, SRS-010, SRS-011 | CMP-CONSUMER, CMP-PRODUCER, CMP-STATE, CMP-STORAGE, IF-006, IF-007 | VC-001, VC-003, VC-005, VC-006, VC-011 |
| IRD-005 Linkage: versioned shared object and archive | SRS-020 | CI-DEBS, IF-010 | VC-007, VC-008 |
<!-- /ecss:table -->

## 7 Verification

Each requirement is verified through the supplier's cases (the SVS); the customer accepts the release at the QR on the recorded runs
of the release pipeline at the release commit.
