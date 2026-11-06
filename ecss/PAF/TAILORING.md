# Tailoring statement and criticality classification

MPSCRING-DOC-TAIL. Written by Elena Russo, the supplier's project manager. A proposal agreed at the SRR; the category is the customer's
to confirm (MPSCRING-RID-SRR-001).

## 1 Purpose

MPSCring is documented after ECSS-E-ST-40C Rev.1 and ECSS-Q-ST-80C Rev.2, tailored for what it is: one C source file of 421 lines and
its header, a tier 1 library of the platform with no dependency but the C library. The standards are cited, never copied.

## 2 Starting point

The tailoring of the platform's other libraries (uuid7, kyr_lmdb, SPSCring: their ecss/PAF/TAILORING.md) applies as written for the
reviews and the document set, with the differences of section 4.

## 3 Criticality classification

Proposed: **B**. ECSS-Q-ST-80C Rev.2 6.2.2 gives a software's category from the consequences of its failure on the system that uses it;
the system here is the box, as its customer KYR describes it ([../RB/SSS.md](../RB/SSS.md#4-general-description)).

- Every durable write of the box crosses the ring: kyr_core's write queue carries each job of the drive, the estate and the rms from the
  request thread to its environment's one writer (kyr_drive, kyr_estate and kyr_rms all write through kyr_core_txn_queue_submit()).
- A defect of the ring loses a job (its submitter waits for ever, its write is never made: one environment's writes stall behind it),
  duplicates one (a write runs twice) or hands over a torn pointer (the writer runs a stranger's job or the box crashes). Those are the
  owner's records and their integrity, the box's mission: a critical failure, category B. Not C, because the damage is not one
  connection's (as with SPSCring) but the store's whole write path; not A, because the box endangers no life and its store's own
  atomic commits keep what was committed before the defect.
- The ring is not formally lock-free (MPSCRING-SRS-013): a stalled producer delays delivery. The customer accepts it (MPSCRING-SSS-005)
  under an assumption it keeps (MPSCRING-ASM-002); category B is why it is stated as a requirement, inspected and not left implicit.
- The team of MPSCRING (nine people in the rms) holds the independence category B asks: the author of every item is never its
  approver, the configuration manager who freezes a baseline never approves it, and the verification is recorded by a verification
  engineer who wrote no requirement and no design.

## 4 Differences

| Clauses | Disposition | Why |
|---|---|---|
| E-40 5.2 (requirements baseline) | performed by the customer | KYR writes the SSS, the IRD and the statement of work (RB/), from kyr_core's code |
| E-40 5.3 (management plans: SDP, SRevP, SCMP) | covered by the package | the reviews are the rms's, each baseline frozen by one person and approved by another; the configuration is git and the rms baselines; no separate plan |
| E-40 5.4 to 5.6 (TS, design, validation) | performed | SRS, ICD, SDD, SUM, API reference, SVS |
| E-40 5.8 (verification) | performed, with an inspection | SVR, the release pipeline's run at the release commit as evidence; the sequence protocol and the memory ordering inspected line by line (category B: what a test cannot prove is inspected) |
| E-40 5.7 (acceptance) | the customer's | KYR accepts at the QR and pins the release |
| Q-80 7 (product quality) | by the pipeline's gates | 100 % lines and branches, zero warnings, the hardening check, the sanitizers and ThreadSanitizer |

## 5 What the package claims

The SRR, PDR, CDR and QR are held in the rms (project MPSCRING), each closed by its baseline at the release commit 73bb6fe3 (1.0.10);
every claim on the code is a reference to its lines at that commit.
