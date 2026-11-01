# Hazard log

Status: **draft, written by an engineer without clinical review.** It lists what is known so
that it can be reviewed. Severity is provisional, and probability is left out because there
is no data to estimate it from yet. A clinician and a regulatory specialist must review this
log before it is relied on.

Severity is rated as if the software were used for clinical decisions, even though it is
labelled not for diagnostic use, because a label alone does not stop that use.

| Severity | Meaning |
| --- | --- |
| High | Could lead to wrong or delayed treatment |
| Medium | Could lead to a repeated exam, or a delay without lasting harm |
| Low | Inconvenience |

Control status: *Implemented* means the code has the control, *Verified* means a recorded
test passed, and *Planned* means the control does not exist yet.

| ID | Hazard | Possible cause | Possible harm | Severity | Controls | Control status | Requirements |
| --- | --- | --- | --- | --- | --- | --- | --- |
| H-01 | Images attributed to the wrong patient | Merged patients in the local database; missing or hidden patient annotation | Treatment based on another patient's images | High | Show patient identity on the images; warn on mixed patients | Unverified | SR-PAT-01, SR-PAT-02 |
| H-02 | Wrong laterality or orientation | Wrong orientation calculation; flip or rotate tool; MPR error; mirrored data | Treatment on the wrong side | High | Orientation labels; tests against the laterality marker | Unverified | SR-ORI-01, SR-ORI-02, SR-ORI-03, SR-3D-01 |
| H-03 | Wrong measurement | Wrong or missing calibration; imager spacing used as patient spacing; wrong SUV inputs; rounding | Wrong staging or dose planning | High | Phantom checks; show the calibration used | Partly planned (A-07) | SR-MEA-01 to SR-MEA-05, SR-3D-01 |
| H-04 | Missing images presented as a complete study | Partial C-MOVE or DICOMweb retrieval; gaps not reported; skipped slices | A finding is missed | High | Report partial transfers; warn on gaps | Unverified | SR-PAC-01 to SR-PAC-04, SR-DEC-03 |
| H-05 | Image silently wrong or incomplete | Unsupported or corrupt encoding; truncation; decoder error ignored | A finding is missed or invented | High | GDCM error handling patches; report unreadable files; tests with `broken/` and every transfer syntax | Partly implemented (A-01) | SR-DEC-01, SR-DEC-02, SR-DEC-03, SR-DEC-05, SR-DEC-06 |
| H-06 | Inverted or misleading greyscale | MONOCHROME1 handled wrongly; wrong VOI LUT or presentation state ignored | A finding is missed | High | Test polarity; warn when a presentation state cannot be applied | Partly planned (Phase 2) | SR-DEC-04 |
| H-07 | Misaligned fusion | Different frames of reference; patient moved between scans | Finding located in the wrong place | High | Fusion offered only in one frame of reference; show when registered (Phase 3) | Implemented, unverified | SR-3D-02 |
| H-08 | Exported image lacks needed context or is altered | Export drops annotations, scale or identifiers; lossy formats | Later misreading of the exported image | Medium | Test exports | Unverified | SR-EXP-01 |
| H-09 | Patient data exposed | Unencrypted DICOM; identifiers in logs, title bar, crash reports; incomplete anonymisation | Privacy breach | Medium | TLS (Phase 1); option to hide patient in the title; review logs and crash reports | Planned (A-03, A-06) | SR-EXP-02 |
| H-10 | Uncertified software relied on clinically | Users unaware of its status | Any of the above, without the expected safeguards | High | "Not for diagnostic use" label; internal rollout guidance | Implemented, unverified; title does not reach exports (A-10) | SR-LBL-01 |
| H-11 | Malicious DICOM file compromises the workstation | Memory errors in parsers and decoders. RadiAnt's CVE-2026-17264 shows that this class is real | Data breach, or loss of the workstation | Medium | Fuzzing in CI (Phase 1); keep libraries patched | Planned (A-05) | |
| H-12 | Viewer reports a defect to, or fetches updates from, the wrong organisation | Update checker and contact details still name the original project | Data sent outside; wrong instructions followed | Medium | Point these at this project, or disable them | Planned (A-06) | |
