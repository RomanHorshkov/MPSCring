/* Project: https://github.com/RomanHorshkov */
/**
 * @file stress_mt.c
 * @brief Multi-producer stress/correctness test — the actual point of "MPSC."
 *
 * N producer threads push unique, checksummable values concurrently while 1 consumer
 * thread drains as fast as it can. Correctness is proven by a consumer-owned bitmap indexed
 * by (producer_id, sequence): every consumed item's bit is tested-and-set, so a duplicate
 * delivery is caught the instant it happens (not just inferred from a mismatched total), an
 * out-of-range or malformed item is rejected directly, and reaching the expected total count
 * without ever hitting a duplicate is what proves every one of the TOTAL_EXPECTED distinct
 * (producer, sequence) pairs was delivered exactly once — a sum/count check alone cannot rule
 * out two independent corruptions (a lost item and a duplicated one) whose effects on the sum
 * happen to cancel out; a bitmap has no such blind spot; also reports throughput, mirroring
 * this project's other stress tests (uuid7/tests/stress).
 *
 * Run this under the `sanitize` build profile (ASan/UBSan/TSan) — that is the actual
 * correctness bar for lock-free code, not just "the counts matched once."
 */
#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "mpscring.h"

#ifndef RING_CAPACITY
#    define RING_CAPACITY 4096u
#endif
#ifndef N_PRODUCERS
#    define N_PRODUCERS 16u
#endif
#ifndef N_PER_PRODUCER
#    define N_PER_PRODUCER 500000u /* override with -DN_PER_PRODUCER=<n> for a faster TSan pass */
#endif
#define TOTAL_EXPECTED ((uint64_t)N_PRODUCERS * N_PER_PRODUCER)

static mpsc_ring_t*     g_ring;
static _Atomic uint64_t g_produced_sum;
static _Atomic uint64_t g_consumed_sum;
static _Atomic uint64_t g_consumed_count;

/* One bit per (producer_id, sequence) pair, owned solely by the single consumer thread (no
 * atomics needed on the bitmap itself — exactly the same single-writer reasoning the ring's
 * own consumer side relies on). Sized at compile time from the (possibly -D-overridden)
 * totals above. */
static uint8_t g_seen[(TOTAL_EXPECTED + 7u) / 8u];

static uint64_t _now_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * UINT64_C(1000000000) + (uint64_t)ts.tv_nsec;
}

/* Encode (producer_id, sequence) into one nonzero uintptr_t so a checksum mismatch or a
 * count mismatch both independently prove corruption/loss/duplication happened — a lost or
 * duplicated item can't hide behind a coincidentally-matching sum. */
static void* _encode(uint32_t producer_id, uint32_t seq)
{
    uint64_t v = ((uint64_t)producer_id << 32) | (uint64_t)(seq + 1u);
    return (void*)(uintptr_t)v;
}

/* @return 1 if the bit was ALREADY set (duplicate), 0 if this is the first time. */
static int _bit_test_and_set(uint64_t index)
{
    const uint64_t byte = index / 8u;
    const uint8_t  mask = (uint8_t)(1u << (index % 8u));
    const int      was_set = (g_seen[byte] & mask) != 0;
    g_seen[byte] |= mask;
    return was_set;
}

static void* producer_main(void* arg)
{
    uint32_t id = (uint32_t)(uintptr_t)arg;
    for(uint32_t i = 0; i < N_PER_PRODUCER; ++i)
    {
        void*    item = _encode(id, i);
        uint64_t val  = (uint64_t)(uintptr_t)item;
        atomic_fetch_add_explicit(&g_produced_sum, val, memory_order_relaxed);
        while(mpsc_ring_push(g_ring, item) != 0)
        {
            /* ring momentarily full: the consumer is a single thread and this is a stress
             * test by design, so producers legitimately spin-wait here. */
        }
    }
    return NULL;
}

int main(void)
{
    g_ring = mpsc_ring_init(RING_CAPACITY);
    if(!g_ring)
    {
        fprintf(stderr, "mpsc_ring_init failed\n");
        return 1;
    }

    pthread_t producers[N_PRODUCERS];
    const uint64_t t0 = _now_ns();

    for(uint32_t i = 0; i < N_PRODUCERS; ++i)
    {
        if(pthread_create(&producers[i], NULL, producer_main, (void*)(uintptr_t)i) != 0)
        {
            fprintf(stderr, "pthread_create failed at producer %u\n", i);
            return 1;
        }
    }

    while(atomic_load_explicit(&g_consumed_count, memory_order_relaxed) < TOTAL_EXPECTED)
    {
        void* item;
        if(mpsc_ring_pop(g_ring, &item) == 0)
        {
            const uint64_t v             = (uint64_t)(uintptr_t)item;
            const uint32_t producer_id   = (uint32_t)(v >> 32);
            const uint32_t seq_plus_one  = (uint32_t)(v & 0xFFFFFFFFu);

            if(producer_id >= N_PRODUCERS || seq_plus_one == 0u || seq_plus_one > N_PER_PRODUCER)
            {
                fprintf(stderr, "CORRUPTION: malformed/out-of-range item (producer_id=%u seq+1=%u)\n", producer_id,
                        seq_plus_one);
                return 1;
            }

            const uint32_t seq   = seq_plus_one - 1u;
            const uint64_t index = (uint64_t)producer_id * N_PER_PRODUCER + seq;
            if(_bit_test_and_set(index))
            {
                fprintf(stderr, "CORRUPTION: duplicate delivery (producer_id=%u seq=%u)\n", producer_id, seq);
                return 1;
            }

            atomic_fetch_add_explicit(&g_consumed_sum, v, memory_order_relaxed);
            atomic_fetch_add_explicit(&g_consumed_count, 1u, memory_order_relaxed);
        }
    }
    const uint64_t t1 = _now_ns();

    for(uint32_t i = 0; i < N_PRODUCERS; ++i)
    {
        pthread_join(producers[i], NULL);
    }

    const uint64_t produced_sum   = atomic_load_explicit(&g_produced_sum, memory_order_relaxed);
    const uint64_t consumed_sum   = atomic_load_explicit(&g_consumed_sum, memory_order_relaxed);
    const uint64_t consumed_count = atomic_load_explicit(&g_consumed_count, memory_order_relaxed);
    const double   elapsed_s      = (double)(t1 - t0) / 1e9;
    const double   ops_per_s      = (double)consumed_count / elapsed_s;

    printf("MPSCring stress: %u producers x %u items = %lu total\n", N_PRODUCERS, N_PER_PRODUCER, TOTAL_EXPECTED);
    printf("  consumed_count=%lu (expected %lu)\n", consumed_count, TOTAL_EXPECTED);
    printf("  produced_sum=%lu consumed_sum=%lu (match: %s)\n", produced_sum, consumed_sum,
           (produced_sum == consumed_sum) ? "yes" : "NO — CORRUPTION/LOSS/DUPLICATION");
    printf("  elapsed=%.3fs  throughput=%.0f ops/s\n", elapsed_s, ops_per_s);

    /* Reaching TOTAL_EXPECTED without ever hitting the duplicate-rejection path above is
     * itself the exactly-once proof: every consume tested-and-set a bit in a space of exactly
     * TOTAL_EXPECTED distinct (producer, sequence) values, any duplicate would have aborted
     * immediately, so the only way to reach this count is one bit set per distinct value. */
    assert(consumed_count == TOTAL_EXPECTED);
    assert(produced_sum == consumed_sum);
    assert(mpsc_ring_is_empty(g_ring));

    mpsc_ring_destroy(&g_ring);
    printf("\nSTRESS TEST PASSED (exactly-once proven via per-item bitmap, not just count/sum)\n");
    return 0;
}
