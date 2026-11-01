# Test lab

A reproducible test corpus and test PACS. Everything runs in Docker, on Windows or
Linux, so nothing needs installing except Docker.

## Commands

Run these from this directory. In Git Bash on Windows, put `MSYS_NO_PATHCONV=1`
before each `docker compose run`.

```sh
# Generate the synthetic corpus and check it (77 MB)
docker compose run --rm --build tools python3 -I /lab/generate_corpus.py
docker compose run --rm tools python3 -I /lab/verify_corpus.py

# Download the public real-world files and check their checksums (37 MB; 590 MB with --large)
docker compose run --rm tools python3 -I /lab/fetch_public_data.py --large

# Start the test PACS, load the corpus and check its interfaces
docker compose up -d orthanc
docker compose run --rm tools python3 -I /lab/load_orthanc.py

# Stop the PACS. Add -v to delete its stored studies too.
docker compose down
```

Add `--large` to `generate_corpus.py` for the 512x512x1000 benchmark CT (about 520 MB).

## Test PACS

Orthanc 1.12.6 with DICOMweb, published on this machine's loopback interface only. The
ports avoid Orthanc's usual 4242 and 8042, in case another service already uses them.

| Setting | Value |
| --- | --- |
| DICOM | `127.0.0.1:14242`, called AE title `ORTHANC` |
| DICOMweb | `http://127.0.0.1:18042/dicom-web/` |
| Web interface | `http://127.0.0.1:18042/` |
| User | `qa`, password `qa-only` (test only) |

Starviewer is registered as C-MOVE destination `STARVIEWER`, at
`host.docker.internal:4006`. For retrieval to work, set Starviewer's local AE title to
`STARVIEWER` and keep its incoming port at the default of 4006.

## Corpus

`corpus/` is generated and not committed. `corpus/synthetic/expected.json` has the ground
truth for each case.

| Case | Checks |
| --- | --- |
| `ct-reference` | Distance (100.000 mm fiducials on instance 21), HU, laterality marker |
| `mr-orientations` | Sagittal, coronal and 20-degree oblique series, orientation labels, reference lines |
| `petct-suv` | Fusion alignment, SUVbw (sphere 8.0, background 1.0) |
| `us-multiframe` | Cine, ultrasound region calibration (20.00 mm between the dots) |
| `xa-multiframe` | Cine, imager pixel spacing, magnification 1.25 |
| `mg-monochrome1` | Inversion, laterality |
| `enhanced-ct` | Enhanced multi-frame geometry |
| `encapsulated-pdf` | Document display |
| `transfer-syntaxes/*` | Decoding: 14 variants, with lossless ones checked for exact round trip by another decoder |
| `broken/*` | Failing visibly: missing slices, truncated data, wrong pixel length, not DICOM |

In `corpus/public/`:

| Folder | Source | Licence |
| --- | --- | --- |
| `pydicom-test-files` | pydicom 2.4.4: 70 small files from many vendors, some deliberately malformed | MIT |
| `rider-petct` | [RIDER Lung PET-CT](https://doi.org/10.7937/K9/TCIA.2015.OFIP7TVM): GE PET-CT, one study | CC BY 3.0 |
| `rider-large-ct` | The same collection: 1,049 slices, for realistic benchmarks | CC BY 3.0 |

Credit TCIA and the collection if these files appear in anything published. They are
de-identified public data. Do not add real patient studies to this folder or to the test
PACS, because neither is protected.

## What the corpus does and does not prove

`verify_corpus.py` recomputes the ground truth from the files, using code separate from the
generator. The same author wrote both, though, so a shared misunderstanding of DICOM would
pass. Use the public files, RadiAnt and a clinical reviewer as independent checks before
relying on a phantom result.

Files generated with the same tool versions are byte-identical, so the checksum of the
corpus can be recorded with a test run. A new `ubuntu:24.04` image may bring different
encoder versions, which changes the lossy files.

Orthanc 1.12.6 refuses the deflated files over REST and C-STORE, although DCMTK, GDCM and
pydicom all read them. They can only be tested by opening them from disk.
