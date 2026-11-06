# Statement of work: the write queue's ring for the platform

MPSCRING-DOC-SOW. Written by Oliver Hughes for the customer (KYR, the platform), addressed to the supplier of MPSCring (the team of
project MPSCRING). Part of the requirements baseline (ECSS-E-ST-40C Rev.1 5.2), agreed at the SRR.

## 1 Scope

The supplier develops, verifies, delivers and maintains MPSCring, the bounded multi-producer single-consumer ring the platform's write
queue hands every write job through, to the requirements of the SSS ([SSS.md](SSS.md#5-requirements)) and the IRD
([IRD.md](IRD.md#5-interface-requirements)).

## 2 Work to be performed

1. Keep the library and its interface: app/mpscring.h and app/mpscring.c, their ABI gated (abi/layout.txt).
2. Specify it: an SRS and an ICD derived from this requirements baseline, after ECSS-E-ST-40C Rev.1 Annexes D and E; say plainly what
   progress the ring guarantees and what it does not.
3. Design it: an SDD after Annex F, the decisions recorded, an API reference generated from the code, a user manual after Annex H.
4. Verify it: an SVS after Annex L, the release pipeline of kyr-build's profiles at the release commit, the thread sanitizer included;
   an SVR after Annex M with the evidence.
5. Hold the reviews SRR, PDR, CDR and QR with the customer, each closed by a baseline frozen by the supplier's configuration manager
   and approved by another person.
6. Deliver the Debian packages libmpscring and libmpscring-dev through the platform's release train, pinned by SHA-256.

## 3 Deliverables

| Deliverable | When |
|---|---|
| The requirements baseline agreed (this document, the SSS, the IRD) | SRR |
| SRS, ICD, SDD (architecture) | PDR |
| SDD (detailed), API reference, SUM, SVS; the first runs | CDR |
| SVR; the release's packages | QR |

## 4 Tailoring and category

The supplier proposes the tailoring and the category ([../PAF/TAILORING.md](../PAF/TAILORING.md#3-criticality-classification)); the
customer confirms the category from the platform's analysis.

## 5 Acceptance

The customer accepts a release at the QR when every case of the SVS has passed at the release commit, and takes it into the platform
by pinning it after the platform's own pipeline passes with it.
