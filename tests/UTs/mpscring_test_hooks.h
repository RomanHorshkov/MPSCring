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

typedef void* (*mpsc_ring_test_aligned_alloc_fn)(size_t alignment, size_t size);
typedef void* (*mpsc_ring_test_calloc_fn)(size_t count, size_t size);
typedef void (*mpsc_ring_test_free_fn)(void* ptr);

void mpsc_ring_test_set_allocators(mpsc_ring_test_aligned_alloc_fn aligned_allocator, mpsc_ring_test_calloc_fn calloc_allocator,
                                   mpsc_ring_test_free_fn free_allocator);
void mpsc_ring_test_reset_allocators(void);

/** @brief Force mpsc_ring_init()'s lock-free checks to report a given verdict. Pass -1 to
 *  restore the real atomic_is_lock_free() result for that check. */
void mpsc_ring_test_set_lock_free_overrides(int enqueue_pos_is_lock_free, int slot_sequence_is_lock_free);

#endif /* MPSC_RING_TEST_HOOKS_H */
