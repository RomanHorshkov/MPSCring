#include <assert.h>
#include <stdio.h>
#include <mpscring.h>

int main(void)
{
    mpsc_ring_t* ring = mpsc_ring_init(8);
    assert(ring != NULL);
    for (intptr_t i = 1; i <= 8; ++i) {
        assert(mpsc_ring_push(ring, (void*)i) == 0);
    }
    for (intptr_t i = 1; i <= 8; ++i) {
        void* out = NULL;
        assert(mpsc_ring_pop(ring, &out) == 0);
        assert((intptr_t)out == i);
    }
    assert(mpsc_ring_is_empty(ring));
    mpsc_ring_destroy(&ring);
    assert(ring == NULL);
    printf("smoke test: installed package round-trips correctly\n");
    return 0;
}
