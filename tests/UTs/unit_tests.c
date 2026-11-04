/* Project: https://github.com/RomanHorshkov */
/**
 * @file unit_tests.c
 * @brief Single-threaded correctness + edge-case unit tests for MPSCring.
 *
 * Concurrency correctness (the actual point of "multi-producer") is proven separately by
 * tests/stress/ under real contention and under ThreadSanitizer — these tests are the
 * single-threaded contract: capacity validation, storage-size arithmetic, full/empty
 * boundaries, the init_into vs init ownership distinction, and the lock-free-rejection path.
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mpscring.h"

/* Fault-injection hooks (mpsc_ring_test_set_allocators/set_lock_free_overrides/etc.) only
 * exist in a build compiled with MPSC_RING_TESTING (see mpscring.c) — a release/production
 * static or shared library never exports them. `build_UTs_release.sh` links this file against
 * exactly that release library, so anything calling those symbols must be compiled out
 * entirely on that path (matching SPSCring's own unit_tests.c convention exactly), or the
 * release-profile unit test binary fails to link. */
#ifdef MPSC_RING_TESTING
#    include <pthread.h>
#    include <stdint.h>
#    include <time.h>

#    include "mpscring_test_hooks.h"

static int g_calloc_fail_after = -1; /* -1 = never fail */
static int g_calloc_calls      = 0;

static void* _test_calloc(size_t count, size_t size)
{
    if(g_calloc_fail_after >= 0 && g_calloc_calls >= g_calloc_fail_after)
    {
        return NULL;
    }
    g_calloc_calls++;
    return calloc(count, size);
}

static void _reset_fault_injection(void)
{
    g_calloc_fail_after = -1;
    g_calloc_calls      = 0;
    mpsc_ring_test_reset_allocators();
}
#endif /* MPSC_RING_TESTING */

static void test_capacity_validation(void)
{
    assert(mpsc_ring_init(0) == NULL);
    assert(mpsc_ring_init(1) == NULL); /* one slot aliases published and free states */
    assert(mpsc_ring_init(3) == NULL); /* not a power of two */
    assert(mpsc_ring_init(6) == NULL); /* not a power of two */
    assert(mpsc_ring_storage_size(0) == 0u);
    assert(mpsc_ring_storage_size(1) == 0u);
    assert(mpsc_ring_storage_size(5) == 0u);
    /* A capacity so large that capacity * sizeof(slot) would overflow size_t: the storage-size
     * overflow guard must reject it (rather than silently returning a truncated byte count). */
    assert(mpsc_ring_storage_size((uint64_t)1 << 62) == 0u);

    unsigned char scratch[4096];
    assert(mpsc_ring_init_into(scratch, sizeof scratch, 1u) == NULL);
    printf("test_capacity_validation: PASS\n");
}

static void test_minimum_capacity_full_boundary(void)
{
    mpsc_ring_t* r = mpsc_ring_init(MPSC_RING_MIN_CAPACITY);
    assert(r != NULL);
    assert(mpsc_ring_capacity(r) == MPSC_RING_MIN_CAPACITY);

    assert(mpsc_ring_push(r, (void*)(intptr_t)11) == 0);
    assert(mpsc_ring_push(r, (void*)(intptr_t)22) == 0);
    assert(mpsc_ring_push(r, (void*)(intptr_t)33) == -1);

    void* out = NULL;
    assert(mpsc_ring_pop(r, &out) == 0);
    assert((intptr_t)out == 11);
    assert(mpsc_ring_pop(r, &out) == 0);
    assert((intptr_t)out == 22);
    assert(mpsc_ring_pop(r, &out) == -1);

    mpsc_ring_destroy(&r);
    printf("test_minimum_capacity_full_boundary: PASS\n");
}

static void test_null_payload_round_trip(void)
{
    mpsc_ring_t* r = mpsc_ring_init(2u);
    assert(r != NULL);
    assert(mpsc_ring_push(r, NULL) == 0);

    void* out = (void*)(intptr_t)1;
    assert(mpsc_ring_pop(r, &out) == 0);
    assert(out == NULL);

    mpsc_ring_destroy(&r);
    printf("test_null_payload_round_trip: PASS\n");
}

static void test_push_pop_fifo_single_thread(void)
{
    mpsc_ring_t* r = mpsc_ring_init(8);
    assert(r != NULL);
    assert(mpsc_ring_is_empty(r));

    for(intptr_t i = 1; i <= 8; ++i)
    {
        assert(mpsc_ring_push(r, (void*)i) == 0);
    }
    assert(mpsc_ring_push(r, (void*)99) != 0); /* full */
    assert(!mpsc_ring_is_empty(r));

    assert(mpsc_ring_pop(r, NULL) == 0);       /* discard-on-pop path (out_item == NULL), non-empty ring */
    assert(mpsc_ring_push(r, (void*)9) == 0);  /* backfill so the drain loop below still sees 8 items */

    /* Front item (1) was discarded above and 9 was appended, so the ring now holds 2..9 in order. */
    for(intptr_t i = 2; i <= 9; ++i)
    {
        void* out = NULL;
        assert(mpsc_ring_pop(r, &out) == 0);
        assert((intptr_t)out == i);      /* strict FIFO order */
    }
    assert(mpsc_ring_is_empty(r));
    assert(mpsc_ring_pop(r, NULL) != 0); /* empty */

    mpsc_ring_destroy(&r);
    printf("test_push_pop_fifo_single_thread: PASS\n");
}

static void test_wrap_around(void)
{
    mpsc_ring_t* r = mpsc_ring_init(4);
    assert(r != NULL);

    /* Push/pop many more times than capacity so the internal index genuinely wraps
     * (and, for the sequence scheme, laps the ring several times over). */
    for(int lap = 0; lap < 1000; ++lap)
    {
        for(intptr_t i = 0; i < 4; ++i)
        {
            assert(mpsc_ring_push(r, (void*)(i + 1)) == 0);
        }
        for(intptr_t i = 0; i < 4; ++i)
        {
            void* out = NULL;
            assert(mpsc_ring_pop(r, &out) == 0);
            assert((intptr_t)out == i + 1);
        }
    }
    assert(mpsc_ring_is_empty(r));
    mpsc_ring_destroy(&r);
    printf("test_wrap_around: PASS\n");
}

static void test_null_and_double_free_safety(void)
{
    assert(mpsc_ring_push(NULL, (void*)1) != 0);
    assert(mpsc_ring_pop(NULL, NULL) != 0);
    assert(mpsc_ring_is_empty(NULL) == 1);
    assert(mpsc_ring_capacity(NULL) == 0u);

    mpsc_ring_destroy(NULL); /* must not crash */
    mpsc_ring_t* r = NULL;
    mpsc_ring_destroy(&r);   /* destroying an already-NULL ring must not crash */

    r = mpsc_ring_init(2);
    assert(r != NULL);
    mpsc_ring_destroy(&r);
    assert(r == NULL);
    mpsc_ring_destroy(&r); /* double-destroy on the now-NULL local must be a no-op */
    printf("test_null_and_double_free_safety: PASS\n");
}

static void test_init_into_caller_owns_storage(void)
{
    const uint64_t cap  = 16u;
    const size_t   need = mpsc_ring_storage_size(cap);
    assert(need > 0u);

    unsigned char storage[4096];
    assert(need <= sizeof(storage));
    memset(storage, 0xAA, sizeof(storage)); /* poison, to prove init_into initializes what it needs */

    mpsc_ring_t* r = mpsc_ring_init_into(storage, sizeof(storage), cap);
    assert(r != NULL);
    assert(mpsc_ring_capacity(r) == cap);

    for(intptr_t i = 1; i <= (intptr_t)cap; ++i)
    {
        assert(mpsc_ring_push(r, (void*)i) == 0);
    }
    for(intptr_t i = 1; i <= (intptr_t)cap; ++i)
    {
        void* out = NULL;
        assert(mpsc_ring_pop(r, &out) == 0);
        assert((intptr_t)out == i);
    }

    /* destroy() on an init_into ring must free NOTHING — `storage` is a stack array; if
     * this called free() on it (or on any part of it) the test process would abort or
     * corrupt on return. Reaching the end of this function is itself the assertion. */
    mpsc_ring_destroy(&r);
    assert(r == NULL);
    printf("test_init_into_caller_owns_storage: PASS\n");
}

static void test_init_into_exact_size_at_every_alignment(void)
{
    const uint64_t cap  = 8u;
    const size_t   need = mpsc_ring_storage_size(cap);
    assert(need > 0u);

    /* Exercise every possible offset across two conventional 64-byte cache lines. The
     * returned ring may be aligned forward inside this malloc-owned arena, but the public
     * worst-case size must remain sufficient for every starting address. */
    unsigned char* arena = malloc(need + 128u);
    assert(arena != NULL);
    for(size_t offset = 0u; offset < 128u; ++offset)
    {
        unsigned char* storage = arena + offset;
        memset(storage, 0xA5, need);
        assert(mpsc_ring_init_into(storage, need - 1u, cap) == NULL);

        mpsc_ring_t* r = mpsc_ring_init_into(storage, need, cap);
        assert(r != NULL);
        assert(mpsc_ring_push(r, (void*)(intptr_t)(offset + 1u)) == 0);
        void* out = NULL;
        assert(mpsc_ring_pop(r, &out) == 0);
        assert((uintptr_t)out == offset + 1u);
        mpsc_ring_destroy(&r);
        assert(r == NULL);
    }
    free(arena);
    printf("test_init_into_exact_size_at_every_alignment: PASS\n");
}

static void test_init_into_rejects_undersized_storage(void)
{
    assert(mpsc_ring_init_into(NULL, 4096u, 64u) == NULL);            /* NULL storage, rejected up front */

    unsigned char scratch[4096];
    assert(mpsc_ring_init_into(scratch, sizeof scratch, 0u) == NULL); /* capacity 0 -> needed == 0 */
    assert(mpsc_ring_init_into(scratch, sizeof scratch, 3u) == NULL); /* not a power of two -> needed == 0 */

    const uint64_t cap     = 64u;
    const size_t   need    = mpsc_ring_storage_size(cap);
    unsigned char* storage = malloc(need - 1u); /* deliberately one byte short */
    assert(storage != NULL);

    mpsc_ring_t* r = mpsc_ring_init_into(storage, need - 1u, cap);
    assert(r == NULL);

    free(storage);
    printf("test_init_into_rejects_undersized_storage: PASS\n");
}

#ifdef MPSC_RING_TESTING
static void test_allocation_failure_path(void)
{
    mpsc_ring_test_set_allocators(NULL, _test_calloc, free);
    g_calloc_fail_after = 0; /* fail the very first calloc: the owned-storage allocation */
    g_calloc_calls      = 0;

    mpsc_ring_t* r = mpsc_ring_init(16);
    assert(r == NULL);

    _reset_fault_injection();
    printf("test_allocation_failure_path: PASS\n");
}

static void test_lock_free_rejection(void)
{
    mpsc_ring_test_set_lock_free_overrides(0, -1); /* force "enqueue_pos is not lock-free" */
    mpsc_ring_t* r = mpsc_ring_init(8);
    assert(r == NULL);
    mpsc_ring_test_reset_allocators();

    mpsc_ring_test_set_lock_free_overrides(-1, 0); /* force "slot sequence is not lock-free" */
    r = mpsc_ring_init(8);
    assert(r == NULL);
    mpsc_ring_test_reset_allocators();

    printf("test_lock_free_rejection: PASS\n");
}

/* mpsc_ring_push()'s claim CAS (atomic_compare_exchange_weak on enqueue_pos) only fails when a
 * DIFFERENT producer wins the SAME claim between our load and our CAS attempt — a branch that,
 * single-threaded, never fires on its own. tests/ITs' real multi-producer flow normally exercises
 * it, but that depends on genuine OS thread contention: on a resource-constrained CI runner (2
 * vCPUs) it can go un-hit often enough to fail the 100% branch-coverage gate even though the same
 * build passes reliably on a many-core dev box (reproduced locally with `taskset -c 0`). The hook
 * below forces it deterministically instead of hoping for lucky scheduling. */
static mpsc_ring_t* g_race_ring = NULL;

static void _steal_enqueue_pos(void)
{
    /* Called from INSIDE the outer push(), after it has already loaded its local `pos` snapshot
     * but before its CAS attempt. This is a raw position bump, not a full push() — see
     * mpsc_ring_test_steal_enqueue_pos()'s doc comment for why that distinction matters: a full
     * nested push() also publishes the slot, which would make the outer call take the
     * ALREADY-covered "someone published past us" branch instead of ever reaching the CAS. */
    mpsc_ring_test_steal_enqueue_pos(g_race_ring);
}

static void test_push_retries_when_enqueue_pos_races_underneath(void)
{
    mpsc_ring_test_steal_enqueue_pos(NULL); /* must not crash; nothing else to observe */

    mpsc_ring_t* r = mpsc_ring_init(4);
    assert(r != NULL);
    g_race_ring = r;

    int real_value = 42;
    mpsc_ring_test_set_push_hook(_steal_enqueue_pos);
    assert(mpsc_ring_push(r, &real_value) == 0); /* hook fires once, forces a CAS retry, then succeeds */

    /* No pop() here: the stolen position (index 0) was bumped past but never published, so
     * pop()'s strictly-sequential drain correctly reports "not ready" forever at that index — a
     * real competing producer always eventually publishes, so this stuck state can never occur
     * outside this synthetic single-threaded simulant. The push()'s return value above is the
     * whole point of this test (CAS-failure retry, then success); round-trip FIFO correctness is
     * already covered by every other single-threaded test in this file. */
    g_race_ring = NULL;
    mpsc_ring_destroy(&r);
    printf("test_push_retries_when_enqueue_pos_races_underneath: PASS\n");
}

/* ---- A producer stalled between its claim and its publish ---------------------------------------------------------------
 * The header's "NOT formally lock-free" paragraph, shown rather than written: one producer claims position 0 and stops inside
 * the window (the claim hook blocks it on a condition variable); three other producers claim and publish every other slot and
 * finish; the consumer sees nothing past the stalled slot however often it asks; the next push is refused at once (full), it
 * neither waits nor overwrites; once the stalled producer publishes, every item comes out exactly once, in the order the
 * positions were claimed; the ring goes on. Items are 1-based indices carried as pointers. */
#    define STALL_CAPACITY     16u
#    define STALL_PRODUCERS    3u
#    define STALL_PER_PRODUCER 5u                                          /* 3 x 5 = 15: every slot but the stalled one */
#    define STALL_ITEMS        (1u + STALL_PRODUCERS * STALL_PER_PRODUCER) /* item 0 is the stalled producer's */
#    define STALL_POLLS        10000u
#    define STALL_ITEM(id)     ((void*)(uintptr_t)((id) + 1u))
#    define STALL_ID(item)     ((size_t)((uintptr_t)(item) - 1u))

_Static_assert(STALL_ITEMS == STALL_CAPACITY, "the free producers fill every slot but the stalled one");

static mpsc_ring_t*    g_stall_ring;
static uint64_t        g_stall_claimed_at[STALL_ITEMS + 1u]; /* each item's position, written in the hook by its producer */
static pthread_mutex_t g_stall_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  g_stall_cond = PTHREAD_COND_INITIALIZER;
static int             g_stall_holding;                      /* item 0's producer is inside the window */
static int             g_stall_go;                           /* the test lets it publish */

static void _stall_claim_hook(uint64_t pos, void* item)
{
    const size_t id = STALL_ID(item);
    assert(id <= STALL_ITEMS);
    g_stall_claimed_at[id] = pos;
    if(id != 0u)
    {
        return;
    }
    pthread_mutex_lock(&g_stall_lock);
    g_stall_holding = 1;
    pthread_cond_broadcast(&g_stall_cond);
    while(!g_stall_go)
    {
        pthread_cond_wait(&g_stall_cond, &g_stall_lock);
    }
    pthread_mutex_unlock(&g_stall_lock);
}

static void* _stalled_producer(void* arg)
{
    (void)arg;
    return mpsc_ring_push(g_stall_ring, STALL_ITEM(0u)) == 0 ? NULL : arg;
}

static void* _free_producer(void* arg)
{
    const size_t p = (size_t)(uintptr_t)arg;
    for(size_t k = 0u; k < STALL_PER_PRODUCER; ++k)
    {
        if(mpsc_ring_push(g_stall_ring, STALL_ITEM(1u + p * STALL_PER_PRODUCER + k)) != 0)
        {
            return STALL_ITEM(p);
        }
    }
    return NULL;
}

static void test_stalled_producer_holds_back_the_consumer(void)
{
    g_stall_ring = mpsc_ring_init(STALL_CAPACITY);
    assert(g_stall_ring != NULL);
    mpsc_ring_test_set_claim_hook(_stall_claim_hook);

    /* One producer claims position 0 and stops between its claim and its publish. */
    pthread_t stalled;
    assert(pthread_create(&stalled, NULL, _stalled_producer, NULL) == 0);
    pthread_mutex_lock(&g_stall_lock);
    while(!g_stall_holding)
    {
        pthread_cond_wait(&g_stall_cond, &g_stall_lock);
    }
    pthread_mutex_unlock(&g_stall_lock);
    assert(g_stall_claimed_at[0] == 0u);

    /* The other producers are not held: they claim and publish the 15 other slots and finish. */
    pthread_t free_producers[STALL_PRODUCERS];
    for(size_t p = 0u; p < STALL_PRODUCERS; ++p)
    {
        assert(pthread_create(&free_producers[p], NULL, _free_producer, (void*)(uintptr_t)p) == 0);
    }
    for(size_t p = 0u; p < STALL_PRODUCERS; ++p)
    {
        void* failed = STALL_ITEM(p);
        assert(pthread_join(free_producers[p], &failed) == 0);
        assert(failed == NULL);
    }

    /* The bound: the ring is full behind the stalled slot, and the next push is refused at once; it neither waits nor overwrites. */
    assert(mpsc_ring_push(g_stall_ring, STALL_ITEM(STALL_ITEMS)) == -1);

    /* The consumer sees nothing past the stalled slot, though 15 items are published behind it, however often and long it asks. */
    const struct timespec pause = {.tv_sec = 0, .tv_nsec = 1000000L};
    for(unsigned i = 0u; i < STALL_POLLS; ++i)
    {
        void* out = NULL;
        assert(mpsc_ring_pop(g_stall_ring, &out) == -1);
        assert(out == NULL);
        assert(mpsc_ring_is_empty(g_stall_ring) == 1);
        if(i % 1000u == 0u)
        {
            assert(nanosleep(&pause, NULL) == 0);
        }
    }

    /* The stalled producer publishes: its push succeeds, and every item comes out once, in the order the positions were claimed. */
    pthread_mutex_lock(&g_stall_lock);
    g_stall_go = 1;
    pthread_cond_broadcast(&g_stall_cond);
    pthread_mutex_unlock(&g_stall_lock);
    void* stalled_failed = STALL_ITEM(0u);
    assert(pthread_join(stalled, &stalled_failed) == 0);
    assert(stalled_failed == NULL);
    mpsc_ring_test_set_claim_hook(NULL);

    unsigned delivered[STALL_ITEMS] = {0};
    for(uint64_t pos = 0u; pos < STALL_ITEMS; ++pos)
    {
        void* out = NULL;
        assert(mpsc_ring_pop(g_stall_ring, &out) == 0);
        const size_t id = STALL_ID(out);
        assert(id < STALL_ITEMS);
        assert(g_stall_claimed_at[id] == pos);
        delivered[id]++;
    }
    for(size_t id = 0u; id < STALL_ITEMS; ++id)
    {
        assert(delivered[id] == 1u);
    }
    assert(mpsc_ring_pop(g_stall_ring, NULL) == -1);
    assert(mpsc_ring_is_empty(g_stall_ring) == 1);

    /* And the ring goes on: a whole lap more, in order. */
    for(size_t id = 0u; id < STALL_CAPACITY; ++id)
    {
        assert(mpsc_ring_push(g_stall_ring, STALL_ITEM(id)) == 0);
    }
    for(size_t id = 0u; id < STALL_CAPACITY; ++id)
    {
        void* out = NULL;
        assert(mpsc_ring_pop(g_stall_ring, &out) == 0);
        assert(out == STALL_ITEM(id));
    }

    mpsc_ring_destroy(&g_stall_ring);
    printf("test_stalled_producer_holds_back_the_consumer: PASS\n");
}
#endif /* MPSC_RING_TESTING */

int main(void)
{
    test_capacity_validation();
    test_minimum_capacity_full_boundary();
    test_null_payload_round_trip();
    test_push_pop_fifo_single_thread();
    test_wrap_around();
    test_null_and_double_free_safety();
    test_init_into_caller_owns_storage();
    test_init_into_exact_size_at_every_alignment();
    test_init_into_rejects_undersized_storage();
#ifdef MPSC_RING_TESTING
    test_allocation_failure_path();
    test_lock_free_rejection();
    test_push_retries_when_enqueue_pos_races_underneath();
    test_stalled_producer_holds_back_the_consumer();
#endif

    printf("\nALL UNIT TESTS PASSED\n");
    return 0;
}
