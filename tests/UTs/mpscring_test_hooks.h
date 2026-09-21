/* Project: https://github.com/RomanHorshkov */
/**
 * @file mpscring_test_hooks.h
 * @brief Private allocator-injection seam for unit tests (mirrors SPSCring's pattern).
 *
 * Production builds never see this header — it's only pulled in under
 * MPSC_RING_TESTING, letting unit tests exercise the allocation-failure paths in
 * mpsc_ring_init() deterministically instead of hoping malloc actually fails.
 */
#ifndef MPSC_RING_TEST_HOOKS_H
#define MPSC_RING_TEST_HOOKS_H

#include <stddef.h>

#include "mpscring.h" /* mpsc_ring_t, for mpsc_ring_test_steal_enqueue_pos() below */

typedef void* (*mpsc_ring_test_aligned_alloc_fn)(size_t alignment, size_t size);
typedef void* (*mpsc_ring_test_calloc_fn)(size_t count, size_t size);
typedef void (*mpsc_ring_test_free_fn)(void* ptr);

void mpsc_ring_test_set_allocators(mpsc_ring_test_aligned_alloc_fn aligned_allocator, mpsc_ring_test_calloc_fn calloc_allocator,
                                   mpsc_ring_test_free_fn free_allocator);
void mpsc_ring_test_reset_allocators(void);

/** @brief Force mpsc_ring_init()'s lock-free checks to report a given verdict. Pass -1 to
 *  restore the real atomic_is_lock_free() result for that check. */
void mpsc_ring_test_set_lock_free_overrides(int enqueue_pos_is_lock_free, int slot_sequence_is_lock_free);

/**
 * @brief Fire once, synchronously, the moment mpsc_ring_push() has loaded its local
 *  enqueue-position snapshot but before it attempts the claim CAS — then self-clear (single-shot).
 *
 * This lets a single-threaded test deterministically force the "another producer already
 * advanced enqueue_pos past our stale snapshot" claim-CAS-failure branch, which by
 * construction otherwise only fires under genuine concurrent contention between real OS
 * threads — exactly the kind of thing that passes reliably on a many-core dev box but flakes
 * under a resource-constrained CI runner (reproduced locally with `taskset -c 0`; see the
 * coverage job's own comment in build_UTs.sh). A hook body typically calls
 * mpsc_ring_test_steal_enqueue_pos() on the same ring — see that function's own doc comment
 * for why a raw position bump, not a full nested push(), is the right simulant here.
 */
typedef void (*mpsc_ring_test_push_hook_fn)(void);
void mpsc_ring_test_set_push_hook(mpsc_ring_test_push_hook_fn hook);

/**
 * @brief Bump `ring`'s internal enqueue_pos by one WITHOUT publishing a slot — a raw simulant
 *  of "another producer already won the claim CAS", for use from a mpsc_ring_test_push_hook_fn.
 *
 * A full nested mpsc_ring_push() is the WRONG simulant here: it also completes the slot publish
 * (data + sequence store) before returning, so by the time the outer call inspects that slot it
 * sees `diff != 0` and takes the ALREADY-covered "someone published past us" branch instead of
 * ever reaching the claim CAS at all. A real competing producer can only be caught in the
 * narrow window between winning the CAS and publishing — which single-threaded code can only
 * fake by moving the position and nothing else. The position this steals is never published,
 * which is a deliberately synthetic, one-off gap: destroy the ring afterward rather than
 * reusing it.
 */
void mpsc_ring_test_steal_enqueue_pos(mpsc_ring_t* ring);

#endif /* MPSC_RING_TEST_HOOKS_H */
