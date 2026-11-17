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

## Test PACS with TLS

A second Orthanc with HTTPS and DICOM TLS, with its own storage. It requires a client
certificate. Its certificates are throwaway ones, in `tls/`, which git ignores.

```
docker compose run --rm tools sh /lab/make_tls_certs.sh
docker compose --profile tls up -d orthanc-tls
docker compose --profile tls run --rm tools sh /lab/load_orthanc_tls.sh
```

| Setting | Value |
| --- | --- |
| DICOM TLS | `localhost:12762`, called AE title `ORTHANC` |
| DICOMweb | `https://localhost:18043/dicom-web/` |
| Trust | `tls/ca.pem`. `tls/other-ca.pem` signed nothing, so it must be refused |
| Client certificate | `tls/client.pem`, private key `tls/client.key` |

The server certificate names `localhost` and not `127.0.0.1`, so connecting by
`127.0.0.1` must fail the host name check. It holds only the synthetic corpus, and Orthanc
rejects its `broken` and `deflated` files.

## OAuth mock

Orthanc has no OAuth of its own, so `oauth_mock.py` stands in front of the test PACS. It needs only Python 3
and the test PACS running. It issues bearer tokens at `/oauth/token` (client credentials grant), and forwards every
other request to Orthanc, with Orthanc's test login added, only when it carries a valid token. Without one it answers
401, as RFC 6750 says.

```sh
python3 -I oauth_mock.py --ttl 60                # add --refresh to also issue refresh tokens
```

| Setting | Value |
| --- | --- |
| DICOMweb (needs a token) | `http://127.0.0.1:18044/dicom-web/` |
| Token endpoint | `http://127.0.0.1:18044/oauth/token` |
| Client ID and secret | `starviewer-qa` and `qa-only-secret` (test only) |
| `POST /mock/revoke` | Invalidates every token, to see how a client recovers from a 401 |
| `GET /mock/stats` | Counts of tokens issued, and of requests accepted and refused |

`test_EchoToWadoPacsOAuth` runs against it when these variables are set:
`STARVIEWER_TESTLAB_OAUTH_URI` (the DICOMweb URI above) and `STARVIEWER_TESTLAB_OAUTH_TOKEN_URI` (the token endpoint).
It is not run by Docker: start the mock on the host, with Orthanc published on `127.0.0.1:18042`.

## Retrieve test

`test_RetrieveDICOMFilesFromPACS` is skipped unless these are set, and retrieves one study
with C-GET into a temporary directory. The header of the test lists all of its variables.
`STARVIEWER_TESTLAB_CACHE` keeps the files, but the count then includes any other study in
that directory, so use an empty one for each study.

```
STARVIEWER_TESTLAB_PORT=14242 STARVIEWER_TESTLAB_ADDRESS=127.0.0.1 \
STARVIEWER_TESTLAB_STUDY=<Study Instance UID> STARVIEWER_TESTLAB_FILES=<number of files> \
  autotests -testsToExecute test_RetrieveDICOMFilesFromPACS
```

`test_EchoToWadoPacs` checks that DICOMweb over HTTPS trusts only the CA file given for the PACS,
including after a connection made with it in the same process. It is skipped unless these are set:

```
STARVIEWER_TESTLAB_HTTPS_URI=https://localhost:18043/dicom-web/ STARVIEWER_TESTLAB_CA=<path to tls/ca.pem> \
  autotests -testsToExecute test_EchoToWadoPacs
```

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
| `presentation-states` | Grayscale softcopy presentation states in the study of `ct-reference`: Annotations (a line between the fiducial dots of image 20, a circle on the soft tissue sphere of image 50, and a rectangle at 25% to 75% of image 70 in display units), and one with a shutter, a rotation and a missing image, which must give a warning |
| `structured-reports` | Enhanced SR documents in the study of `ct-reference`: a final report (complete and verified) and a draft (partial, unverified) whose text has markup in it, which must be shown as text. Both have a measurement and a reference to image 50 |
| `videos` | DICOM video (MPEG-4 AVC/H.264 in one fragment). `video-undecodable` has bytes that no decoder can play, and must give a notice. `generate_corpus.py --video file.mp4` (or `.mpg`) adds `video-real` with a real video, which must play on a machine with decoders |
| `key-objects` | Key Object Selection Documents in the study of `ct-reference`: Of Interest (images 20 and 45), Rejected for Quality Reasons (60), and For Teaching with one missing image (10 and a CT that does not exist), which must give a warning |
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
