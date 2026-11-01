# Quality records

These records let a later certification build on what already exists, instead of
recreating it (see [Certification-ready engineering](../ROADMAP.md#certification-ready-engineering)).
They are not a certified quality system, and no regulatory specialist has reviewed them yet.

**Starviewer is not certified. This fork must not be used for diagnosis or treatment
decisions.**

| Record | Contents | Prepares for |
| --- | --- | --- |
| [requirements.md](requirements.md) | What the software must do, with IDs and how each is verified | IEC 62304 |
| [hazard-log.md](hazard-log.md) | What could harm a patient, and the controls | ISO 14971 |
| [architecture.md](architecture.md) | Modules, boundaries, and design decisions | IEC 62304 |
| [soup.md](soup.md) | Third-party software, and how the bill of materials is produced | IEC 62304, IEC 81001-5-1 |
| [known-anomalies.md](known-anomalies.md) | Open defects and findings | IEC 62304 |
| [test-protocol.md](test-protocol.md) | Phase 0.5 test steps, results and benchmark | IEC 62304 |
| [testlab/](testlab/README.md) | Test corpus and test PACS | IEC 62304 |
| [DICOM conformance statement](<../starviewer/doc/DICOM Conformance Statement.fodt>) | Inherited from the original project and not yet reviewed for this fork | DICOM PS3.2 |

## How changes are made

1. **Issue:** each change starts from an issue, or from an entry in one of these records.
2. **Branch and pull request:** no direct commits to `devel`. The
   [pull request template](../.github/pull_request_template.md) asks for the requirements,
   hazards and tests the change touches.
3. **Review and CI:** a person other than the author reviews, and the Linux and Windows
   workflows pass.
4. **Records:** a change that alters behaviour updates the records in the same pull request.
5. **Release:** packages are built only by CI, from a tagged commit, with the bill of
   materials. The tag, CI run and test results are recorded together.

Steps 2, 3 and 5 are not enforced yet. See A-11 in [known-anomalies.md](known-anomalies.md).

## IDs

IDs are never reused, including after an entry is removed.

| Prefix | Record |
| --- | --- |
| `SR-` | Software requirement |
| `H-` | Hazard |
| `A-` | Anomaly |
| `AD-` | Architecture decision |
| `TP-` | Test protocol step |

Mention these IDs in commits, pull requests and code comments where they apply, so that
links can be followed in both directions.

## Clinical code rules

- Keep clinical logic (decoding, geometry, measurement, SUV) in UI-independent classes with
  unit tests.
- Fail visibly. Report an image that cannot be displayed correctly, rather than showing it
  degraded without warning.
- Keep experimental work behind a build option, as playground extensions are today.
- Never write patient data to logs, test fixtures or issues.
