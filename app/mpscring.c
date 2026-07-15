/* Project: https://github.com/RomanHorshkov */
/*
 * Bounded lock-free MPSC ring buffer implementation (Vyukov bounded-queue algorithm).
 *
 * Implementation notes:
 * - Many producers, exactly one consumer.
 * - Capacity is a power of two; mask = size - 1.
 * - Each slot carries its own sequence counter (not just a shared tail), so concurrent
 *   producers claim DISTINCT slots via CAS on the shared enqueue position, without a lock:
 *     slot.sequence == pos            -> slot is free for whoever claims position `pos`
 *     slot.sequence == pos + 1        -> slot holds data published for position `pos`, unread
 *     slot.sequence == pos + mask + 1 -> slot has been read and is free again for wrap `pos + size`
 * - The consumer side needs no CAS at all (only one thread ever advances it), the same
 *   asymmetry SPSCring's single-producer side exploits.
 * - Memory ordering:
 *   Producer: acquire-load the claimed slot's sequence to synchronize with the previous
 *   reader's release; after the CAS wins, plain store the data, then release-store the
 *   slot's new sequence so the consumer's acquire-load of it happens-after this write.
 *   Consumer: acquire-load the slot's sequence, plain read data (already synchronized-with
 *   the producer's release), then release-store the slot's next-cycle sequence so a future
 *   producer's acquire-load happens-after this read.
 */

#include "mpscring.h"

#include <limits.h>    /* ULONG_MAX, ULLONG_MAX */
#include <stdatomic.h> /* C11 atomic operations and memory ordering */
#include <stddef.h>    /* size_t, offsetof */
#include <stdint.h>    /* uint64_t, SIZE_MAX */
#include <stdlib.h>    /* malloc, calloc, free */
#include <string.h>    /* memset */

#if defined(MPSC_REQUIRE_ALWAYS_LOCK_FREE)
#    if defined(__clang__) || defined(__GNUC__)
#        if UINT64_MAX == ULONG_MAX
#            if ATOMIC_LONG_LOCK_FREE != 2
#                error "MPSCring requires always-lock-free unsigned long atomics"
#            endif
#        elif UINT64_MAX == ULLONG_MAX
#            if ATOMIC_LLONG_LOCK_FREE != 2
#                error "MPSCring requires always-lock-free unsigned long long atomics"
#            endif
#        else
#            error "MPSCring does not recognize the uint64_t base type on this target"
#        endif
#    else
#        error "MPSC_REQUIRE_ALWAYS_LOCK_FREE requires GCC or Clang"
#    endif
#endif

/* Test builds replace the two allocation calls through this deliberately private seam
 * (mirrors SPSCring's pattern exactly). Production builds call the C library directly and
 * export no additional symbols. */
#ifdef MPSC_RING_TESTING
#    include "mpscring_test_hooks.h"

static mpsc_ring_test_aligned_alloc_fn mpsc_ring_aligned_allocator          = NULL; /* unused: init_into never mallocs */
static mpsc_ring_test_calloc_fn        mpsc_ring_calloc_allocator          = NULL;  /* set below */
static mpsc_ring_test_free_fn          mpsc_ring_free_allocator           = free;
static int                             mpsc_ring_enqueue_lock_free_override = -1;
static int                             mpsc_ring_slot_lock_free_override    = -1;

void mpsc_ring_test_set_allocators(mpsc_ring_test_aligned_alloc_fn aligned_allocator, mpsc_ring_test_calloc_fn calloc_allocator,
                                   mpsc_ring_test_free_fn free_allocator)
{
    mpsc_ring_aligned_allocator = aligned_allocator;
    mpsc_ring_calloc_allocator  = calloc_allocator;
    mpsc_ring_free_allocator    = free_allocator;
}

void mpsc_ring_test_reset_allocators(void)
{
    mpsc_ring_aligned_allocator          = NULL;
    mpsc_ring_calloc_allocator          = NULL;
    mpsc_ring_free_allocator            = free;
    mpsc_ring_enqueue_lock_free_override = -1;
    mpsc_ring_slot_lock_free_override    = -1;
}

void mpsc_ring_test_set_lock_free_overrides(int enqueue_pos_is_lock_free, int slot_sequence_is_lock_free)
{
    mpsc_ring_enqueue_lock_free_override = enqueue_pos_is_lock_free;
    mpsc_ring_slot_lock_free_override    = slot_sequence_is_lock_free;
}

static void* mpsc_ring_allocate_owned(size_t size)
{
    return mpsc_ring_calloc_allocator ? mpsc_ring_calloc_allocator(1u, size) : calloc(1u, size);
}

static void mpsc_ring_release_owned(void* ptr)
{
    mpsc_ring_free_allocator(ptr);
}
#else
#    define mpsc_ring_allocate_owned(size) calloc(1u, (size))
#    define mpsc_ring_release_owned(ptr) free((ptr))
#endif

#ifndef MPSC_CACHELINE
#    define MPSC_CACHELINE 64u /* typical L1 line; only affects layout, not correctness */
#endif

/**
 * @brief Sequence-number comparison without casting to a signed type.
 *
 * `diff` is `a - b` computed in uint64_t, which the C standard defines as wrapping modular
 * arithmetic (no UB, unlike signed overflow). The real difference between any two sequence
 * values this algorithm ever compares is always small in magnitude (bounded by the ring's
 * capacity), so a "negative" true difference wraps to a huge unsigned value near UINT64_MAX,
 * and a "non-negative" one stays small — comparing against UINT64_MAX/2 cleanly separates
 * the two for any capacity nowhere near 2^63 (the same trick used for TCP sequence numbers).
 * This replaces an earlier `(int64_t)a - (int64_t)b` form, which relied on an
 * implementation-defined (pre-C23) conversion and contradicted this API's own "wrap-safe"
 * claim.
 */
#define MPSC_SEQ_IS_NEGATIVE(diff) ((diff) > (UINT64_MAX / 2u))

/** @brief One ring element: a sequence counter (the Vyukov state machine) plus the payload. */
typedef struct mpsc_slot
{
    _Atomic uint64_t sequence;
    void*            data;
} mpsc_slot_t;

struct mpsc_ring
{
    mpsc_slot_t* buf;  /* Circular buffer of slots (read-only after init) */
    uint64_t     size; /* Ring size, MUST be power of 2 (read-only after init) */
    uint64_t     mask; /* mask = size - 1 for fast modulo (read-only after init) */
    int          owns_storage; /* 1 if mpsc_ring_init() malloc'd `storage_base`, 0 for init_into() */
    void*        storage_base; /* the original, possibly-unaligned block passed to init_into (or the
                                 * library's own malloc'd block) — this, not `this`, is what gets freed */

    /* Producers CAS this; the consumer only ever reads it via the slot sequences, never this
     * counter directly, so it needs no companion "consumer's own" field the way SPSCring's
     * head/tail pair does — the per-slot sequence IS the consumer-visible state. Its own
     * cache line keeps concurrent producer CAS traffic from bouncing the line the control
     * struct's read-mostly fields (buf/size/mask) live on. */
    _Alignas(MPSC_CACHELINE) _Atomic uint64_t enqueue_pos;

    /* Owned solely by the single consumer thread — exactly SPSCring's head field, plain
     * (non-atomic) reads/writes are correct because only one thread ever touches it; the
     * per-slot sequence's acquire/release pair is what actually synchronizes with producers.
     * Still gets its own cache line (like SPSCring's head/tail split) so the consumer
     * updating this can never bounce the line concurrent producers are CAS-ing enqueue_pos
     * on — false sharing would otherwise serialize the two sides against each other. */
    _Alignas(MPSC_CACHELINE) uint64_t dequeue_pos;
};

_Static_assert(offsetof(struct mpsc_ring, dequeue_pos) - offsetof(struct mpsc_ring, enqueue_pos) >= MPSC_CACHELINE,
               "mpsc_ring enqueue_pos and dequeue_pos must be on separate cache lines");

static uint64_t _align_up(uint64_t value, uint64_t alignment)
{
    return (value + (alignment - 1u)) & ~(alignment - 1u);
}

static int _enqueue_pos_is_lock_free(const struct mpsc_ring* ring)
{
#ifdef MPSC_RING_TESTING
    if(mpsc_ring_enqueue_lock_free_override >= 0)
    {
        return mpsc_ring_enqueue_lock_free_override != 0;
    }
#endif
    return atomic_is_lock_free(&ring->enqueue_pos) != 0;
}

static int _slot_sequence_is_lock_free(const mpsc_slot_t* slot)
{
#ifdef MPSC_RING_TESTING
    if(mpsc_ring_slot_lock_free_override >= 0)
    {
        return mpsc_ring_slot_lock_free_override != 0;
    }
#endif
    return atomic_is_lock_free(&slot->sequence) != 0;
}

size_t mpsc_ring_storage_size(uint64_t capacity)
{
    if((capacity == 0u) || ((capacity & (capacity - 1u)) != 0u))
    {
        return 0u;
    }
    if(capacity > (uint64_t)(SIZE_MAX / sizeof(mpsc_slot_t)))
    {
        return 0u;
    }

    /* Worst-case alignment slack on both sides: the ring struct may need bumping up to
     * _Alignof(mpsc_ring_t)-1 bytes within an arbitrarily-aligned `storage`, and the slot
     * array (placed right after the struct) may need its own bump up to
     * _Alignof(mpsc_slot_t)-1 bytes. Callers reserving exactly this many bytes are always
     * safe regardless of `storage`'s own starting alignment. */
    const uint64_t struct_slack = _Alignof(struct mpsc_ring) - 1u;
    const uint64_t slot_slack   = _Alignof(mpsc_slot_t) - 1u;
    const uint64_t total = (uint64_t)sizeof(struct mpsc_ring) + struct_slack + slot_slack + capacity * (uint64_t)sizeof(mpsc_slot_t);

    if(total > (uint64_t)SIZE_MAX)
    {
        return 0u;
    }
    return (size_t)total;
}

mpsc_ring_t* mpsc_ring_init_into(void* storage, size_t storage_size, uint64_t capacity)
{
    if(storage == NULL)
    {
        return NULL;
    }
    const size_t needed = mpsc_ring_storage_size(capacity);
    if(needed == 0u || storage_size < needed)
    {
        return NULL;
    }

    uintptr_t base = (uintptr_t)storage;
    uintptr_t ring_addr = _align_up((uint64_t)base, (uint64_t)_Alignof(struct mpsc_ring));
    struct mpsc_ring* ring = (struct mpsc_ring*)(void*)ring_addr;

    memset(ring, 0, sizeof(*ring));
    ring->size         = capacity;
    ring->mask         = capacity - 1u;
    ring->owns_storage = 0;
    ring->storage_base = storage;
    ring->dequeue_pos  = 0u;
    atomic_init(&ring->enqueue_pos, UINT64_C(0));

    if(!_enqueue_pos_is_lock_free(ring))
    {
        return NULL;
    }

    uintptr_t slots_addr = _align_up((uint64_t)(ring_addr + sizeof(struct mpsc_ring)), (uint64_t)_Alignof(mpsc_slot_t));
    ring->buf = (mpsc_slot_t*)(void*)slots_addr;

    if(!_slot_sequence_is_lock_free(&ring->buf[0]))
    {
        return NULL;
    }

    for(uint64_t i = 0u; i < capacity; ++i)
    {
        atomic_init(&ring->buf[i].sequence, i);
        ring->buf[i].data = NULL;
    }

    return ring;
}

mpsc_ring_t* mpsc_ring_init(uint64_t capacity)
{
    const size_t needed = mpsc_ring_storage_size(capacity);
    if(needed == 0u)
    {
        return NULL;
    }

    void* storage = mpsc_ring_allocate_owned(needed);
    if(!storage)
    {
        return NULL;
    }

    mpsc_ring_t* ring = mpsc_ring_init_into(storage, needed, capacity);
    if(!ring)
    {
        mpsc_ring_release_owned(storage);
        return NULL;
    }
    ring->owns_storage = 1;
    return ring;
}

int mpsc_ring_push(mpsc_ring_t* ring, void* item)
{
    if(ring == NULL)
    {
        return -1;
    }

    uint64_t pos = atomic_load_explicit(&ring->enqueue_pos, memory_order_relaxed);
    mpsc_slot_t* slot;

    for(;;)
    {
        slot = &ring->buf[pos & ring->mask];
        const uint64_t seq = atomic_load_explicit(&slot->sequence, memory_order_acquire);
        /* Unsigned modular difference (see MPSC_SEQ_IS_NEGATIVE): avoids casting to a signed
         * type, which is implementation-defined pre-C23 and, per the API's own "wrap-safe"
         * claim, should never rely on signed overflow behavior at all. */
        const uint64_t diff = seq - pos;

        if(diff == 0u)
        {
            /* This slot is free for position `pos` — try to claim it. On CAS failure `pos`
             * is updated in place to the current enqueue_pos by the intrinsic, so the loop
             * just retries with fresh information; no extra reload needed. */
            if(atomic_compare_exchange_weak_explicit(&ring->enqueue_pos, &pos, pos + 1u, memory_order_relaxed, memory_order_relaxed))
            {
                break;
            }
        }
        else if(MPSC_SEQ_IS_NEGATIVE(diff))
        {
            return -1; /* ring full: the slot this position would use isn't free yet */
        }
        else
        {
            /* Another producer already advanced enqueue_pos past our stale snapshot. */
            pos = atomic_load_explicit(&ring->enqueue_pos, memory_order_relaxed);
        }
    }

    slot->data = item;
    atomic_store_explicit(&slot->sequence, pos + 1u, memory_order_release);
    return 0;
}

int mpsc_ring_pop(mpsc_ring_t* ring, void** out_item)
{
    if(ring == NULL)
    {
        return -1;
    }

    const uint64_t pos  = ring->dequeue_pos;
    mpsc_slot_t*   slot = &ring->buf[pos & ring->mask];
    const uint64_t seq  = atomic_load_explicit(&slot->sequence, memory_order_acquire);
    const uint64_t diff = seq - (pos + 1u);

    if(MPSC_SEQ_IS_NEGATIVE(diff))
    {
        return -1; /* empty: the next slot to read hasn't been published yet */
    }

    if(out_item != NULL)
    {
        *out_item = slot->data;
    }

    ring->dequeue_pos = pos + 1u;
    /* Mark this slot free again for the NEXT lap around the ring (pos + size), not for an
     * immediate reuse at pos + 1 — that is what makes the sequence scheme correct: a
     * producer computing diff for the same slot one full lap later sees exactly 0. */
    atomic_store_explicit(&slot->sequence, pos + ring->mask + 1u, memory_order_release);
    return 0;
}

int mpsc_ring_is_empty(mpsc_ring_t* ring)
{
    if(ring == NULL)
    {
        return 1;
    }
    const uint64_t     pos  = ring->dequeue_pos;
    const mpsc_slot_t* slot = &ring->buf[pos & ring->mask];
    const uint64_t     seq  = atomic_load_explicit(&slot->sequence, memory_order_acquire);
    return MPSC_SEQ_IS_NEGATIVE(seq - (pos + 1u));
}

uint64_t mpsc_ring_capacity(const mpsc_ring_t* ring)
{
    if(ring == NULL)
    {
        return 0u;
    }
    return ring->size;
}

void mpsc_ring_destroy(mpsc_ring_t** ring)
{
    if(ring == NULL || *ring == NULL)
    {
        return;
    }
    if((*ring)->owns_storage)
    {
        mpsc_ring_release_owned((*ring)->storage_base);
    }
    *ring = NULL;
}
