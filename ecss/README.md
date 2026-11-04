# The ECSS package of MPSCring

MPSCring 1.0.10 (73bb6fe36fa58a356faf7c35864623147c53ed2c) documented after ECSS-E-ST-40C Rev.1 and ECSS-Q-ST-80C Rev.2, category B
proposed ([PAF/TAILORING.md](PAF/TAILORING.md#3-criticality-classification)). Written by the team of project MPSCRING in the box's
rms; this package is the source of its items, its links and its documents. The standards are cited, never copied.

## Customer and supplier

```text
   KYR (the platform, the customer)                          MPSCRING (the supplier)
   kyr_core 5.0.0: every write job, from any    RB/         TS/  DDF/  DJF/
   request thread to its environment's    ── SSS, IRD, SOW ─▶  SRS, ICD, SDD, SUM, API, SVS, SVR
   one writer, through a ring of 1024     ◀── the release 1.0.10, qualified at the QR, pinned in deps.txt
```

The requirements baseline is the customer's: Oliver Hughes writes it in MPSCRING from kyr_core's write queue ([RB/SSS.md](RB/SSS.md#5-requirements),
[RB/IRD.md](RB/IRD.md#5-interface-requirements), [RB/SOW.md](RB/SOW.md#2-work-to-be-performed)). Every software requirement derives
from it ([TS/SRS.md](TS/SRS.md#5-requirements)). In the rms, MPSCRING hangs under KYR and takes KYR's approved baseline as its input
MPSCRING-INPUT-KYR when KYR holds one.

Who links the library: kyr_core (the write queue), and through it every module that writes (kyr_drive, kyr_estate, kyr_rms), the box's
servers (kyr_server) and the platform's train (kyr); all pin 1.0.8, whose library is 1.0.10's (one header comment apart).

## Files

| File | What |
|---|---|
| `RB/` | the customer's: SSS, IRD, statement of work, their requirements (`requirements.csv`) |
| `TS/` | SRS, ICD, the requirements, assumptions and interfaces (`requirements.csv`) |
| `DDF/` | SDD, SUM, the components and configuration item, the decisions; `api/` the Doxygen reference (written by the loader) |
| `DJF/` | SVS, SVR, the cases, the results and the runs' logs (`runs/73bb6fe3/`, the inspection record among them) |
| `PAF/` | the tailoring |
| `diagrams/` | PlantUML: components, a slot's sequence, push and pop with their ordering, the claim and the stalled producer, deployment; each rendered to SVG beside it in the rms |
| `rms/` | the project, the cast, the links, the reviews (RIDs, change requests), the sources, the customer's files |
| `tools/package.py` | checks the package (every reference included) and writes the documents' tables |

## The documents

<!-- ecss:table documents -->
| ID | Title | DRD | Review | Path | Written by |
|---|---|---|---|---|---|
| MPSCRING-DOC-PKG | The ECSS package of MPSCring: what it holds and how it maps to the rms | - | SRR | [ecss/README.md](README.md) | Elena Russo |
| MPSCRING-DOC-TAIL | Tailoring statement and criticality classification | - | SRR | [ecss/PAF/TAILORING.md](PAF/TAILORING.md) | Elena Russo |
| MPSCRING-DOC-SSS | Software system specification (SSS): what the platform requires of the ring | ECSS-E-ST-40 Annex B | SRR | [ecss/RB/SSS.md](RB/SSS.md) | Oliver Hughes |
| MPSCRING-DOC-IRD | Interface requirements document (IRD) toward MPSCring | ECSS-E-ST-40 Annex C | SRR | [ecss/RB/IRD.md](RB/IRD.md) | Oliver Hughes |
| MPSCRING-DOC-SOW | Statement of work: the write queue's ring for the platform | - | SRR | [ecss/RB/SOW.md](RB/SOW.md) | Oliver Hughes |
| MPSCRING-DOC-SRS | Software requirements specification (SRS) | ECSS-E-ST-40 Annex D | PDR | [ecss/TS/SRS.md](TS/SRS.md) | Luca Romano |
| MPSCRING-DOC-ICD | Interface control document (ICD) | ECSS-E-ST-40 Annex E | PDR | [ecss/TS/ICD.md](TS/ICD.md) | Luca Romano |
| MPSCRING-DOC-SDD | Software design document (SDD) | ECSS-E-ST-40 Annex F | PDR,CDR | [ecss/DDF/SDD.md](DDF/SDD.md) | Emily Clarke |
| MPSCRING-DOC-API | API reference of mpscring.h (from the code, by Doxygen) | - | CDR | [ecss/DDF/api/mpscring.md](DDF/api/mpscring.md) | Emily Clarke |
| MPSCRING-DOC-SUM | Software user manual (SUM) | ECSS-E-ST-40 Annex H | CDR | [ecss/DDF/SUM.md](DDF/SUM.md) | Emily Clarke |
| MPSCRING-DOC-SVS | Software validation specification w.r.t. the TS (SVS) | ECSS-E-ST-40 Annex L | CDR | [ecss/DJF/SVS.md](DJF/SVS.md) | Davide Conti |
| MPSCRING-DOC-SVR | Software verification report (SVR) | ECSS-E-ST-40 Annex M | QR | [ecss/DJF/SVR.md](DJF/SVR.md) | Davide Conti |
<!-- /ecss:table -->

## The people

<!-- ecss:table cast -->
| Person | Roles | Duties |
|---|---|---|
| Elena Russo | project-admin, author | grants the team; writes the package overview and the tailoring; plans the reviews; raises the change requests on the documentation |
| Oliver Hughes | author, reviewer, approver | the customer's representative (KYR, the platform): writes the requirements baseline (SSS, IRD, SOW and their requirements); takes KYR's baseline as the input; closes the SRR, the PDR and the QR; approves the ICD and the PDR and QR baselines |
| Luca Romano | author | requirements and interfaces: the SRS, the ICD, the assumptions and the sources; derives the SRS from the requirements baseline |
| Emily Clarke | author | design and code: the components, the decisions, the SDD, the SUM and the API reference; files the code and the repository's documents into the space |
| Davide Conti | author | verification: the cases, the SVS and the SVR; records the release pipeline's runs at the release commit with their logs as evidence |
| James Whitfield | reviewer, author | independent reviewer: opens each review and raises the RIDs |
| Francesca Galli | approver | product assurance: approves the items, the RIDs' dispositions and the change requests; closes the CDR; approves the SRR and CDR baselines |
| Sarah Collins | config-master | configuration: freezes each review's baseline at the release commit |
<!-- /ecss:table -->

## References

An item's reference is `<path>#<anchor>`: a heading's slug (GitHub's rule) or lines `L<n>`, `L<n>-L<m>`, on the repository at
73bb6fe3; `customers/<repo>/<path>` is a customer's file at the commit `rms/customers.csv` names. A document's links are relative to the
document. Every file a reference names is in MPSCRING's space at that path: the code and the repository's documents in the inputs
drawer, the customer's files under `customers/`, this package in the documents drawer, the runs' logs in the evidence drawer.

```sh
ecss/tools/package.py check     # the CSVs, the links, the reviews, the results, every reference, the tables
ecss/tools/package.py tables    # rewrite the tables
```

## Open items

| Id | What | Closes when |
|---|---|---|
| OI-01 | Category B proposed, not confirmed by the customer | KYR's review confirms it; then declared in MPSCRING's rules |
| OI-02 | CR-001 and CR-002: the header's lock-free policy, the source's first comment | the next release carries them |
| OI-03 | CR-003: the TSan stage in every pipeline and a yielding stress producer, done on the mainline (9bf74d2) | the next release carries it |
| OI-04 | The platform pins 1.0.8; 1.0.10 is qualified (the same library) | the platform's next train moves the pin |
| OI-05 | The format stage was skipped on the build host (no clang-format 18) | a qualification run where it is installed |
