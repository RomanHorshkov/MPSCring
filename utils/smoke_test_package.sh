#!/usr/bin/env bash
set -euo pipefail

# Package smoke test: proves the .deb genuinely works for an external consumer — compiles and
# runs through the compiler's standard /usr include and multiarch library paths, never the
# repository build tree. This deliberately tests the installed PACKAGE.

START_DIR="$(pwd -P)"
cleanup() { cd -- "${START_DIR}"; }
trap cleanup EXIT

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
cd -- "${ROOT_DIR}"

WORK_DIR="$(mktemp -d)"
trap 'rm -rf "${WORK_DIR}"; cleanup' EXIT

if [[ ! -f /usr/local/include/mpscring.h ]]; then
    printf 'smoke_test_package: /usr/local/include/mpscring.h not found — install the .deb first\n' >&2
    exit 1
fi

cat > "${WORK_DIR}/smoke.c" <<'EOF'
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
EOF

gcc -std=c11 -Wall -Wextra -Werror \
    -I/usr/local/include \
    "${WORK_DIR}/smoke.c" \
    -L/usr/local/lib -Wl,-rpath,/usr/local/lib -lmpscring \
    -o "${WORK_DIR}/smoke"

"${WORK_DIR}/smoke"
