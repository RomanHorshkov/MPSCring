/* Project: https://github.com/RomanHorshkov */
/**
 * @file mpscring.h
 * @brief Bounded, lock-free multi-producer / single-consumer ring buffer API.
 *
 * The single-producer/single-consumer sibling of this library, SPSCring, only ever needs
 * one thread to own the tail — this one relaxes that to MANY concurrent producer threads,
 * which means the tail can no longer be a plain relaxed load+store: two producers could
 * observe the same slot as free and both write into it. Instead this uses the well-known
 * Vyukov bounded MPMC queue algorithm (a per-slot sequence counter, not just a tail
 * pointer) — a long-established, widely implemented, provably-correct design, not a new
 * invention. The single-consumer side keeps SPSCring's simplicity: only one thread ever
 * pops, so the head side needs no CAS at all, exactly the same asymmetry SPSCring exploits.
 *
 * Constraints:
 * - MANY producer threads, exactly ONE consumer thread.
 * - Capacity must be a power of two.
 * - Elements stored are `void*` (opaque to this library — it carries pointers, it does not
 *   know or care what they point to; the DB_app write-queue that consumes this library
 *   stores pointers to caller-owned job structs, for example, but this library has zero
 *   knowledge of "jobs" or "transactions").
 * - Indices are 64-bit and wrap-safe when used through this API.
 * - Initialization rejects targets where the sequence-counter atomic is not lock-free.
 *
 * Correctness:
 * - Each slot carries its own sequence number (not just "is this slot occupied") so
 *   concurrent producers can each atomically claim a DISTINCT slot via compare-and-swap on
 *   the shared tail counter, without ever taking a lock.
 * - Full: every slot the next producer would claim is not yet vacated by the consumer.
 * - Empty: the slot the consumer would next read has not yet been published by a producer.
 *
 * Threading:
 * - `mpsc_ring_init` and `mpsc_ring_destroy` are not thread-safe.
 * - `mpsc_ring_push` is safe from any number of concurrent producer threads.
 * - `mpsc_ring_pop` is consumer-only — calling it from more than one thread concurrently is
 *   undefined behavior (this is MPSC, not MPMC, even though the underlying slot algorithm
 *   is generally an MPMC design).
 * - `mpsc_ring_is_empty` is a snapshot and may change concurrently.
 *
 * Lock-free target policy:
 * - `mpsc_ring_init`/`mpsc_ring_init_into` check the per-slot sequence atomics with C11
 *   `atomic_is_lock_free` and return NULL if the target can't provide them lock-free.
 *
 * Allocation policy — two ways to get a ring, one control:
 * - `mpsc_ring_init(capacity)` is the convenience path: the library malloc's its own
 *   storage and `mpsc_ring_destroy()` frees it. Fine for tests and callers that don't
 *   care where the bytes come from.
 * - `mpsc_ring_init_into(storage, storage_size, capacity)` is the embedded/NASA-style
 *   path: the CALLER owns the memory — a static buffer sized at compile time via
 *   `mpsc_ring_storage_size(capacity)`, a slab from a larger arena, whatever the owning
 *   layer's allocation policy is. This library never calls malloc on that path, and
 *   `mpsc_ring_destroy()` on a ring built this way releases nothing (there is nothing
 *   for it to own) — the caller's storage outlives or is freed by whoever allocated it.
 *   This is the intended path for anything wanting static/startup-time-only allocation
 *   with no allocation surprises later in the process's life.
 */
#ifndef MPSC_RING_H
#define MPSC_RING_H

#include <stddef.h>
#include <stdint.h>

/**
 * @brief Opaque ring buffer handle.
 */
typedef struct mpsc_ring mpsc_ring_t;

/**
 * @brief Bytes of storage `mpsc_ring_init_into` needs for a ring of the given capacity.
 *
 * Callers that want to own the memory (a static array, an arena slot, ...) compute this
 * once — typically at compile time, since `capacity` is normally a compile-time constant
 * for a statically-sized system — and reserve exactly that many bytes.
 *
 * @param capacity Number of elements the ring will hold. Must be a power of two.
 * @return Required byte count, or 0 if `capacity` is 0 or not a power of two.
 */
size_t mpsc_ring_storage_size(uint64_t capacity);

/**
 * @brief Create a ring buffer inside caller-owned memory — no allocation by this library.
 *
 * @param storage      Caller-owned memory, at least `storage_size` bytes. Ordinary buffer
 *                      alignment is enough (a plain static array, a stack buffer, or a
 *                      malloc'd block all qualify) — `mpsc_ring_storage_size()` already
 *                      includes slack for this function to align the ring's internal
 *                      structures within `storage` itself; the caller does not need to
 *                      know or reason about this library's internal alignment needs.
 * @param storage_size Size of `storage` in bytes; must be >= `mpsc_ring_storage_size(capacity)`.
 * @param capacity     Number of elements the ring will hold. Must be a power of two.
 * @return Pointer to the ring (aliasing the start of `storage`) on success, or NULL on
 *         invalid arguments, undersized storage, or a target without a lock-free
 *         sequence-counter atomic.
 *
 * @note `mpsc_ring_destroy()` on a ring returned from this function frees nothing — the
 *       caller's storage is the caller's to release, on whatever schedule it likes.
 */
mpsc_ring_t* mpsc_ring_init_into(void* storage, size_t storage_size, uint64_t capacity);

/**
 * @brief Create and initialize a ring buffer, library-owned storage.
 *
 * Convenience wrapper over `mpsc_ring_init_into`: mallocs exactly `mpsc_ring_storage_size(
 * capacity)` bytes and remembers it owns them, so `mpsc_ring_destroy()` frees them.
 *
 * @param capacity Number of elements the ring can hold. Must be a power of two.
 * @return Pointer to a new ring on success, or NULL on invalid capacity, allocation
 *         failure, or a target without a lock-free sequence-counter atomic.
 *
 * @note `capacity` must fit into platform allocation limits (i.e. `mpsc_ring_storage_size(
 *       capacity)` must fit in `size_t`).
 */
mpsc_ring_t* mpsc_ring_init(uint64_t capacity);

/**
 * @brief Push an element into the ring (any producer thread).
 *
 * @param ring Ring buffer instance.
 * @param item Opaque pointer to store. NULL is a valid, storable value.
 * @return 0 on success, -1 if the ring is full or the ring pointer is NULL.
 *
 * @note Safe to call concurrently from any number of producer threads.
 */
int mpsc_ring_push(mpsc_ring_t* ring, void* item);

/**
 * @brief Pop an element from the ring (consumer-side).
 *
 * @param ring     Ring buffer instance.
 * @param out_item Output location for the value. May be NULL to discard.
 * @return 0 on success, -1 if the ring is empty or the ring pointer is NULL.
 *
 * @note Must be called only by the single consumer thread.
 */
int mpsc_ring_pop(mpsc_ring_t* ring, void** out_item);

/**
 * @brief Check whether the ring is empty.
 *
 * @param ring Ring buffer instance.
 * @return 1 if empty, 0 otherwise. Returns 1 if `ring` is NULL.
 */
int mpsc_ring_is_empty(mpsc_ring_t* ring);

/**
 * @brief Return the fixed capacity of the ring.
 *
 * @param ring Ring buffer instance.
 * @return Capacity, or 0 if `ring` is NULL.
 */
uint64_t mpsc_ring_capacity(const mpsc_ring_t* ring);

/**
 * @brief Destroy a ring buffer and set the caller's pointer to NULL.
 *
 * Frees the underlying storage IFF the ring was created via `mpsc_ring_init()` (library-
 * owned). A ring created via `mpsc_ring_init_into()` owns nothing to free — this call
 * still zeroes the caller's pointer, but the caller's storage is untouched.
 *
 * @param ring Pointer to the ring pointer. Safe to pass NULL. Not thread-safe — call only
 *             once every producer and the consumer are done with the ring.
 */
void mpsc_ring_destroy(mpsc_ring_t** ring);

#endif /* MPSC_RING_H */
