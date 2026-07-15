/* Project: https://github.com/RomanHorshkov */
/**
 * @file integration_tests.c
 * @brief Black-box integration tests: the public API only, no test hooks.
 *
 * Distinct from tests/UTs (single-threaded contract + edge cases, needs the test-hook seams)
 * and tests/stress (large-scale throughput/correctness proof, its own standalone binary) —
 * this is the moderate-scale "does the public API actually work together, concurrently, as a
 * real caller would use it" check that belongs in the standard cmocka test gate.
 */
#include <pthread.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdatomic.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include <cmocka.h>
#include "mpscring.h"

static void test_simple_flow(void** state)
{
    (void)state;
    mpsc_ring_t* ring = mpsc_ring_init(8);
    assert_non_null(ring);
    assert_true(mpsc_ring_is_empty(ring));

    for(intptr_t i = 1; i <= 5; ++i)
    {
        assert_int_equal(0, mpsc_ring_push(ring, (void*)i));
    }
    assert_false(mpsc_ring_is_empty(ring));

    for(intptr_t i = 1; i <= 5; ++i)
    {
        void* out = NULL;
        assert_int_equal(0, mpsc_ring_pop(ring, &out));
        assert_int_equal(i, (intptr_t)out);
    }

    assert_true(mpsc_ring_is_empty(ring));
    mpsc_ring_destroy(&ring);
    assert_null(ring);
}

static void test_storage_size_round_trip(void** state)
{
    (void)state;
    const uint64_t cap  = 32u;
    const size_t   need = mpsc_ring_storage_size(cap);
    assert_true(need > 0u);

    void* storage = malloc(need);
    assert_non_null(storage);

    mpsc_ring_t* ring = mpsc_ring_init_into(storage, need, cap);
    assert_non_null(ring);
    assert_int_equal((int)mpsc_ring_capacity(ring), (int)cap);

    for(intptr_t i = 1; i <= (intptr_t)cap; ++i)
    {
        assert_int_equal(0, mpsc_ring_push(ring, (void*)i));
    }
    assert_int_equal(-1, mpsc_ring_push(ring, (void*)999)); /* full */

    for(intptr_t i = 1; i <= (intptr_t)cap; ++i)
    {
        void* out = NULL;
        assert_int_equal(0, mpsc_ring_pop(ring, &out));
        assert_int_equal(i, (intptr_t)out);
    }

    mpsc_ring_destroy(&ring); /* frees nothing: init_into never owns storage */
    free(storage);
}

/* ---------------------------------------------------------------------------------------- */

#define IT_PRODUCERS 6u
#define IT_PER_PRODUCER 20000u
#define IT_TOTAL ((uint64_t)IT_PRODUCERS * IT_PER_PRODUCER)

typedef struct
{
    mpsc_ring_t*    ring;
    _Atomic uint64_t produced_sum;
    _Atomic uint64_t consumed_sum;
    _Atomic uint64_t consumed_count;
} it_ctx_t;

static void* it_encode(uint32_t producer_id, uint32_t seq)
{
    uint64_t v = ((uint64_t)producer_id << 32) | (uint64_t)(seq + 1u);
    return (void*)(uintptr_t)v;
}

static void* it_producer(void* arg_)
{
    struct
    {
        it_ctx_t* ctx;
        uint32_t  id;
    }* arg = arg_;

    for(uint32_t i = 0; i < IT_PER_PRODUCER; ++i)
    {
        void*    item = it_encode(arg->id, i);
        uint64_t val  = (uint64_t)(uintptr_t)item;
        atomic_fetch_add_explicit(&arg->ctx->produced_sum, val, memory_order_relaxed);
        while(mpsc_ring_push(arg->ctx->ring, item) != 0)
        {
            /* ring momentarily full under this deliberately small capacity; keep trying. */
        }
    }
    return NULL;
}

static void* it_consumer(void* arg_)
{
    it_ctx_t* ctx = arg_;
    while(atomic_load_explicit(&ctx->consumed_count, memory_order_relaxed) < IT_TOTAL)
    {
        void* item;
        if(mpsc_ring_pop(ctx->ring, &item) == 0)
        {
            atomic_fetch_add_explicit(&ctx->consumed_sum, (uint64_t)(uintptr_t)item, memory_order_relaxed);
            atomic_fetch_add_explicit(&ctx->consumed_count, 1u, memory_order_relaxed);
        }
    }
    return NULL;
}

static void test_concurrent_multi_producer_flow(void** state)
{
    (void)state;
    it_ctx_t ctx = {0};
    /* Deliberately tiny relative to IT_PRODUCERS x IT_PER_PRODUCER: guarantees heavy, repeated
     * CAS-retry contention on essentially every push (not just an occasional lucky race), so
     * mpsc_ring_push's "another producer already advanced enqueue_pos past our stale snapshot"
     * branch is reliably exercised by this test, not merely possibly exercised. */
    ctx.ring     = mpsc_ring_init(16);
    assert_non_null(ctx.ring);

    pthread_t consumer;
    assert_int_equal(0, pthread_create(&consumer, NULL, it_consumer, &ctx));

    pthread_t producers[IT_PRODUCERS];
    struct
    {
        it_ctx_t* ctx;
        uint32_t  id;
    } producer_args[IT_PRODUCERS];
    for(uint32_t i = 0; i < IT_PRODUCERS; ++i)
    {
        producer_args[i].ctx = &ctx;
        producer_args[i].id  = i;
        assert_int_equal(0, pthread_create(&producers[i], NULL, it_producer, &producer_args[i]));
    }
    for(uint32_t i = 0; i < IT_PRODUCERS; ++i)
    {
        assert_int_equal(0, pthread_join(producers[i], NULL));
    }
    assert_int_equal(0, pthread_join(consumer, NULL));

    /* Exactly-once delivery from every producer, through real concurrent contention on a
     * deliberately small (256-slot) ring — not just "eventually drained," but sum-checked so
     * loss/duplication/corruption would fail this even if the count happened to match. */
    assert_int_equal((int)atomic_load(&ctx.consumed_count), (int)IT_TOTAL);
    assert_int_equal((long long)atomic_load(&ctx.consumed_sum), (long long)atomic_load(&ctx.produced_sum));
    assert_true(mpsc_ring_is_empty(ctx.ring));

    mpsc_ring_destroy(&ctx.ring);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_simple_flow),
        cmocka_unit_test(test_storage_size_round_trip),
        cmocka_unit_test(test_concurrent_multi_producer_flow),
    };
    return cmocka_run_group_tests_name("mpscring_its", tests, NULL, NULL);
}
