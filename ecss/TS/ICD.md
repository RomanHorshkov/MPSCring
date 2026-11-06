# Interface control document (ICD)

MPSCRING-DOC-ICD. Written by Luca Romano from [app/mpscring.h](../../app/mpscring.h#L1-L192), [abi/layout.txt](../../abi/layout.txt#L1-L12)
and [utils/mpscring.map](../../utils/mpscring.map#L1-L16) at 73bb6fe3 (1.0.10). After the DRD of ECSS-E-ST-40C Rev.1 Annex E; agreed at
the PDR, approved by the customer. The function-by-function reference is generated from the header ([../DDF/api/mpscring.md](../DDF/api/mpscring.md)).

## 1 Introduction

The second part of the technical specification: the library's interface toward kyr_core and any consumer, answering the customer's
IRD ([../RB/IRD.md](../RB/IRD.md#5-interface-requirements)).

## 2 Applicable and reference documents

The SRS ([SRS.md](SRS.md#5-requirements)); the IRD; [README.md](../../README.md#api-contract) and [README.md](../../README.md#build-system).

## 3 Terms

As in the SRS ([SRS.md](SRS.md#3-terms)).

## 4 Software overview

See the SRS ([SRS.md](SRS.md#4-software-overview)).

## 5 Requirements and design

### 5.1 General provisions

Each interface item is an rms item `MPSCRING-IF-nnn` of kind interface; it satisfies the customer's interface requirements
(`MPSCRING-IRD-nnn`) and the software requirements, and is verified by cases of its own.

### 5.2 Interface requirements

Software to software only: one C header, a shared object and an archive, two Debian packages. Every call that can fail answers -1 (push,
pop), NULL (init, init_into) or 0 (storage_size); nothing sets errno. The ABI is gated: `utils/check_abi.sh` compares the header with
abi/layout.txt (eight functions and the constant) and the exports with utils/mpscring.map.

### 5.3 Interface design

<!-- ecss:table items doc=ICD -->
| ID | Title | Statement | Method | Source | Traces |
|---|---|---|---|---|---|
| MPSCRING-IF-001 | MPSC_RING_MIN_CAPACITY | #define MPSC_RING_MIN_CAPACITY UINT64_C(2): the smallest capacity the per-slot sequence algorithm supports; part of the ABI (abi/layout.txt). | inspection | [app/mpscring.h#L81-L82](../../app/mpscring.h#L81-L82); [abi/layout.txt#L12](../../abi/layout.txt#L12) | satisfies SRS-004; verified by VC-001, VC-007 |
| MPSCRING-IF-002 | The ring handle | typedef struct mpsc_ring mpsc_ring_t: an opaque type, held by pointer; its layout is the library's. | inspection | [app/mpscring.h#L84-L87](../../app/mpscring.h#L84-L87) | satisfies IRD-001, SRS-018; verified by VC-007 |
| MPSCRING-IF-003 | mpsc_ring_storage_size() | size_t mpsc_ring_storage_size(uint64_t capacity): the bytes mpsc_ring_init_into() needs, alignment slack included, or 0 for an invalid capacity. A function, not a constant expression. | test | [app/mpscring.h#L89-L100](../../app/mpscring.h#L89-L100) | satisfies SRS-001; verified by VC-001, VC-003 |
| MPSCRING-IF-004 | mpsc_ring_init_into() | mpsc_ring_t* mpsc_ring_init_into(void* storage, size_t storage_size, uint64_t capacity): a ring inside the caller's storage, never allocating, or NULL for invalid arguments, undersized storage or a target without lock-free atomics; destroy frees nothing. Not thread-safe. | test | [app/mpscring.h#L102-L121](../../app/mpscring.h#L102-L121) | satisfies SRS-002, SRS-014; verified by VC-001 |
| MPSCRING-IF-005 | mpsc_ring_init() | mpsc_ring_t* mpsc_ring_init(uint64_t capacity): a ring in storage the library allocates and owns, or NULL for an invalid capacity, a failed allocation or a target without lock-free atomics. Not thread-safe. | test | [app/mpscring.h#L123-L136](../../app/mpscring.h#L123-L136) | satisfies IRD-001, IRD-002, SRS-003, SRS-014; verified by VC-001, VC-002 |
| MPSCRING-IF-006 | mpsc_ring_push() | int mpsc_ring_push(mpsc_ring_t* ring, void* item): 0 when item is stored, -1 when the ring is full or NULL; safe from any number of concurrent producer threads; NULL is a storable item. | test | [app/mpscring.h#L138-L147](../../app/mpscring.h#L138-L147) | satisfies IRD-001, IRD-002, IRD-003, IRD-004, SRS-005, SRS-006, SRS-008, SRS-010, SRS-012; verified by VC-001, VC-003 |
| MPSCRING-IF-007 | mpsc_ring_pop() | int mpsc_ring_pop(mpsc_ring_t* ring, void** out_item): 0 when a value is taken (written to *out_item unless out_item is NULL), -1 when the ring is empty or NULL; the single consumer thread only. | test | [app/mpscring.h#L149-L158](../../app/mpscring.h#L149-L158) | satisfies IRD-001, IRD-002, IRD-003, IRD-004, SRS-005, SRS-007, SRS-011, SRS-012; verified by VC-001, VC-003 |
| MPSCRING-IF-008 | mpsc_ring_is_empty() and mpsc_ring_capacity() | int mpsc_ring_is_empty(mpsc_ring_t*): 1 when nothing is published at the consumer's next position (1 for NULL), the consumer thread only. uint64_t mpsc_ring_capacity(const mpsc_ring_t*): the fixed capacity, 0 for NULL, any thread. | test | [app/mpscring.h#L160-L178](../../app/mpscring.h#L160-L178) | satisfies IRD-001, IRD-003, SRS-017, SRS-018; verified by VC-001, VC-006 |
| MPSCRING-IF-009 | mpsc_ring_destroy() | void mpsc_ring_destroy(mpsc_ring_t** ring): free the storage when the library owns it (mpsc_ring_init), free nothing for mpsc_ring_init_into, and set *ring to NULL; NULL and a NULL *ring are no-ops. Not thread-safe. | test | [app/mpscring.h#L180-L190](../../app/mpscring.h#L180-L190) | satisfies IRD-001, SRS-002, SRS-003, SRS-018; verified by VC-001 |
| MPSCRING-IF-010 | Library linkage | libmpscring.so.1 (soname major 1) exports exactly the eight functions of app/mpscring.h as <name>@@MPSCRING_1 (utils/mpscring.map); libmpscring.a carries the same functions; a consumer includes <mpscring.h>. | test | [utils/mpscring.map#L1-L16](../../utils/mpscring.map#L1-L16); [abi/layout.txt#L1-L12](../../abi/layout.txt#L1-L12) | satisfies IRD-005, SRS-020, SRS-023; verified by VC-007, VC-008 |
| MPSCRING-IF-011 | The packages | libmpscring_<version>_<arch>.deb: /usr/local/lib/libmpscring.so.<version> and its soname link; libmpscring-dev_<version>_<arch>.deb: /usr/local/include/mpscring.h, /usr/local/lib/libmpscring.a and the libmpscring.so link, depending on libmpscring (= <version>); both replace the former single package mpscring. | test | [utils/mpscring_project.sh#L37-L44](../../utils/mpscring_project.sh#L37-L44); [README.md#build-system](../../README.md#build-system) | satisfies SSS-007, SRS-021; verified by VC-008 |
| MPSCRING-IF-012 | Build-time switches | MPSC_REQUIRE_ALWAYS_LOCK_FREE (the project's builds define it: a target without always-lock-free 64-bit atomics fails to compile); MPSC_CACHELINE (64 by default: the layout only, never the behaviour); MPSC_RING_TESTING (the test seams, never in a shipped build). | inspection | [app/mpscring.c#L33-L49](../../app/mpscring.c#L33-L49); [app/mpscring.c#L107-L109](../../app/mpscring.c#L107-L109); [utils/mpscring_project.sh#L9-L12](../../utils/mpscring_project.sh#L9-L12) | satisfies SRS-014, SRS-015, SRS-024; verified by VC-010, VC-011 |
<!-- /ecss:table -->

### 5.4 The calls and their threads

| Call | Thread | Progress | With NULL |
|---|---|---|---|
| `mpsc_ring_storage_size` | any | wait-free (arithmetic) | - |
| `mpsc_ring_init_into`, `mpsc_ring_init` | any, before the ring is shared | bounded (init_into allocates nothing) | NULL |
| `mpsc_ring_push` | any number of producers at once | lock-free among producers; refuses at once when full | -1 |
| `mpsc_ring_pop` | the one consumer | wait-free; answers empty while the next position is unpublished | -1 |
| `mpsc_ring_is_empty` | the one consumer | wait-free | 1 |
| `mpsc_ring_capacity` | any | wait-free | 0 |
| `mpsc_ring_destroy` | any, while no thread uses the ring | frees owned storage only | no-op |

### 5.5 What the library uses

The C library only: `calloc`, `free`, `memset` and the C11 atomics.
