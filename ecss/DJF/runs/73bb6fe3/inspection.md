# Inspection of the sequence protocol, the memory ordering and the progress of MPSCring 1.0.10

MPSCRING-VC-011, by Davide Conti, on app/mpscring.c at 73bb6fe36fa58a356faf7c35864623147c53ed2c (v1.0.10), against ISO/IEC 9899:2011
7.17 (atomics), D. Vyukov's bounded queue and the progress conditions of wait-free synchronization. Every line number below is
app/mpscring.c's unless it names another file.

| # | Check | Where | Holds |
|---|---|---|---|
| 1 | Slot i starts with sequence i (free for position i); the payloads start NULL | L256-L260 | yes |
| 2 | enqueue_pos is written only by the claim's compare-and-swap (the position-stealing seam exists only under MPSC_RING_TESTING); dequeue_pos only by pop | L324, L345-L357, L381 | yes |
| 3 | A producer reads the slot's sequence with acquire before it decides, pairing with the consumer's release of the previous lap: the slot was read before it is reused | L313, L385 | yes |
| 4 | A producer stores only after observing the sequence equal to pos and winning the compare-and-swap of enqueue_pos from pos to pos + 1: no two producers own one position | L319-L327, L340 | yes |
| 5 | The compare-and-swap is relaxed on success and failure; the ordering is carried by the sequence's acquire before and release after, so the CAS only arbitrates ownership | L324, L313, L341 | yes |
| 6 | The publish is a release store of pos + 1, after the payload's plain store | L340-L341 | yes |
| 7 | Full is answered only when the slot's sequence is behind pos (the previous lap not yet read), by the unsigned modular difference against UINT64_MAX / 2, never a signed cast | L329-L332, L124 | yes |
| 8 | The consumer reads the slot's sequence with acquire and the payload only when it is pos + 1; empty otherwise, nothing touched | L366-L379 | yes |
| 9 | The consumer advances its plain position and frees the slot for the next lap with a release store of pos + capacity, after its read | L381-L385 | yes |
| 10 | is_empty reads the consumer's plain position and the slot's sequence with acquire: the consumer's own check, as the header says | L395-L398 | yes |
| 11 | pop and is_empty are straight-line code; push's one loop retries only after a lost claim (another producer advanced enqueue_pos, or a weak CAS failed spuriously) or a stale snapshot: lock-free among the producers, wait-free on the consumer's side; no lock, call or allocation in either | L310-L338, L359-L399 | yes |
| 12 | Between the claim and the publish there is one store and nothing else; a producer stalled there leaves its slot at pos, so the consumer answers empty at that position and every later position waits, none lost, none read early: not formally lock-free, as the header states (app/mpscring.h L15-L26) | L324-L341, L371-L374 | yes |

Corroboration (not proof): the integration suite and the stress under ThreadSanitizer report no race (tsan.log); the stress of 16
producers and 8,000,000 values delivers each once (stress-run.log); the unit suite forces the lost-claim branch through the push hook
(tests/UTs/unit_tests.c, test_push_retries_when_enqueue_pos_races_underneath).

Observed beside the checks, not against them: the header's lock-free policy (app/mpscring.h L36 and L58-L60) names only the per-slot
sequence, while the code probes enqueue_pos as well (L243-L254) and the README says both (MPSCRING-RID-PDR-001); the source's first
comment calls the implementation lock-free (L3) where the header says it is not (MPSCRING-RID-PDR-002).
