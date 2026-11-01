# Roadmap: competitive analysis and phases

Goal: bring Starviewer in line with the market-leading desktop DICOM viewers,
OsiriX and Horos on macOS, and RadiAnt and Sante DICOM Viewer on Windows.

This document follows [PHASE0.md](PHASE0.md), which covers the reproducible
build and test baseline. It was written on 2026-10-31 against branch `phase0`
(commit `72ff3c8db`).

## How this was assessed

- **Starviewer** was assessed by reading the source. Nothing was exercised
  interactively, so "Yes" means the feature exists in the code, not that it was
  validated with real studies.
- **Competitors** were assessed from vendor pages and, for Horos, secondary
  sources. Nobody has run them side by side with Starviewer yet.
- **"Not listed"** means the sources did not mention the feature. It does not
  mean the product lacks it. The Horos column has the most of these, because
  its website could not be reached and no complete feature list was found.
- Effort figures are rough estimates, intended for ordering work rather than
  for commitments.

## Summary

1. **Starviewer is closer to parity than expected on core viewing and PACS.**
   It already has hanging protocols, thick slab, PET-CT fusion with SUV,
   DICOMweb, DICOM print, DICOMDIR creation and multi-monitor support.
2. **Network security is the most serious functional gap.** The application has
   no DICOM TLS, and the Windows package has no HTTPS backend for WADO.
3. **Several everyday DICOM objects are not displayed.** Structured reports and
   DICOM video have no viewer, and the GSPS and key image note loaders are
   stubs. JPEG-LS files do not decode, and show as blank images with no error.
4. **The advanced 3D tools are behind OsiriX.** Curved MPR, 3D measurement,
   registration, sculpting, STL export and endoscopy are all missing.
5. **macOS and receiving pushed studies are real gaps but low priority** for
   internal users, so they are deferred.
6. **Certification comes later, but the practices start now.** Records are hard
   to recreate afterwards (see
   [Certification-ready engineering](#certification-ready-engineering)).
7. **The GPL shapes the open core design.** Commercial modules most likely have to
   be separate programs (see [Open core](#open-core)).

## Comparison

### Product and platform

| | Starviewer | OsiriX MD | Horos | RadiAnt | Sante DICOM Viewer Pro |
| --- | --- | --- | --- | --- | --- |
| Windows | Yes (CI) | No | No | Yes, including ARM64 | Yes |
| macOS | **No CI or package** | Yes, Intel and Apple Silicon | Yes | No | No |
| Linux | Yes (CI) | No | No | No | No |
| Licence | GPL-3.0+ | Commercial | LGPL-3.0 | Commercial | Commercial |
| Price | Free | From $69.99/month | Free | Paid, price not listed | $280 per user, $1,500 per site |
| Regulatory claim | None for this fork (see below) | FDA Class II; CE Class IIa | Not confirmed; described as research and non-diagnostic | None on vendor pages | None on vendor pages |
| Latest activity | This branch | Current; supports macOS 27 | Last GitHub release April 2024 | 2026.1, August 2026 | v10.x, site copyright 2026 |

### Viewing

| | Starviewer | OsiriX MD | Horos | RadiAnt | Sante Pro |
| --- | --- | --- | --- | --- | --- |
| Series and study comparison | Yes | Yes | Not listed | Yes | Yes |
| Cross-reference lines | Yes | Yes | Not listed | Yes | Yes |
| Hanging protocols | Yes, XML only, no editor | Yes, customisable | Not listed | Not listed | Not listed |
| Multi-monitor | Yes | Yes | Not listed | Multiple windows | Dual monitor |
| Cine | Yes | Yes | Yes | Yes | Not listed |
| Image filters (sharpen, smooth) | **No** | Yes | Not listed | Yes | Yes |
| Encapsulated PDF | Yes | Yes | Not listed | Yes | Yes |
| Structured reports | **Thumbnail only** | Yes | Not listed | Yes | Yes |
| DICOM video (MPEG-2/4) | **No** | Yes | Not listed | Yes | Not listed |
| Presentation states (GSPS) | **Loader is a stub** | Not listed | Not listed | Not listed | Not listed |
| Key images | **KO loader is a stub** | Yes | Not listed | Not listed | Not listed |
| DICOM overlays | Yes | Not listed | Not listed | Yes | Yes |
| Tag browser | Yes | Yes | Not listed | Yes | Yes |
| Interface languages | 3 | 11 | Not listed | 30+ | Not listed |
| Customisable shortcuts | No editor found | Yes | Not listed | Yes | Not listed |

### Measurement and analysis

| | Starviewer | OsiriX MD | Horos | RadiAnt | Sante Pro |
| --- | --- | --- | --- | --- | --- |
| Distance, angle, Cobb angle | Yes | Yes | Not listed | Yes | Yes |
| 2D ROI statistics | Yes | Yes | Not listed | Yes | Yes |
| Ultrasound region calibration | Yes | Yes | Not listed | Yes | Yes |
| Manual calibration | **Not found** | Not listed | Not listed | Yes | Not listed |
| SUV | Yes | Yes, 2D and 3D | Yes | Yes | Not listed |
| Volume ROI | **No** | Yes | Not listed | Not listed | Yes |
| Time-intensity curves | **Not found** | Yes | Not listed | Yes | Not listed |
| DSA | Playground extension, excluded from CE builds | Not listed | Not listed | Yes | Subtraction filter |
| RTSTRUCT display | **No** | Yes | Not listed | Not listed | Not listed |

### MPR and 3D

| | Starviewer | OsiriX MD | Horos | RadiAnt | Sante Pro |
| --- | --- | --- | --- | --- | --- |
| Orthogonal MPR | Yes | Yes | Yes | Yes | Yes |
| Oblique MPR | Yes | Yes | Not listed | Yes | Not listed |
| Thick slab MIP, MinIP, average | Yes | Yes | Yes | Yes | Not listed |
| Curved MPR | **No** | Yes | Not listed | Not listed | Not listed |
| Volume rendering | Yes | Yes | Yes | Yes | Yes |
| Surface or isosurface rendering | Yes | Yes | Yes | Not listed | Yes |
| 3D MIP | Yes | Yes | Yes | Yes | Yes |
| GPU acceleration | Through `vtkSmartVolumeMapper` | Yes | Not listed | NVIDIA only | Not listed |
| 3D clipping | Clipping box | Sculpting | Not listed | Not listed | Not listed |
| 3D measurement | **No** | Yes | Not listed | Not listed | Not listed |
| STL export | **No** | Yes | Not listed | Yes | "3D models" in the 3D Pro edition |
| Virtual endoscopy | **No** | Yes | Secondary source only | Not listed | Not listed |
| Fusion | PET/NM with CT, same frame of reference only | PET-CT, PET-MR, SPECT-CT | PET-CT, SPECT-CT | Any modalities or protocols | Not listed |
| Image registration | **No** | 3D rigid | Not listed | Not listed | Not listed |
| 4D | Phases in the data model and 2D viewer | Cardiac CT and MR | Not listed | Not listed | Not listed |

### DICOM and networking

| | Starviewer | OsiriX MD | Horos | RadiAnt | Sante Pro |
| --- | --- | --- | --- | --- | --- |
| C-FIND, C-MOVE | Yes | Yes | Not listed | Yes | Yes |
| C-GET | **No** | Yes | Not listed | Not listed | Yes |
| C-STORE send | Yes | Yes | Not listed | Through add-on | Yes |
| Accept unsolicited push | **No**, receives only during its own retrieve | Yes | Not listed | Yes | Yes |
| DICOM TLS | **No** | Not listed | Not listed | Not listed | Yes |
| Worklist | **No** | Yes | Not listed | Not listed | Separate product |
| DICOMweb | QIDO, WADO-RS, STOW and WADO-URI; no authentication found | WADO | Not listed | Not listed | Not listed |
| JPEG, JPEG Lossless, RLE | Yes | Yes | Not listed | Yes | Not listed |
| JPEG 2000 | Yes, through GDCM | Yes | Yes | Yes | Yes |
| JPEG-LS | **Broken** (see Phase 1) | Yes | Not listed | Yes | Not listed |
| Enhanced multi-frame | Yes: CT, MR, tomosynthesis | Not listed | Not listed | Not listed | Not listed |
| URL scheme and command line | Yes | Yes, plus XML-RPC | Not listed | Yes | Command line |
| RIS integration | Yes, listener | Yes | Not listed | Not listed | Not listed |

### Data, export and extensibility

| | Starviewer | OsiriX MD | Horos | RadiAnt | Sante Pro |
| --- | --- | --- | --- | --- | --- |
| Local database | Yes, SQLite | Yes, SQLite | Not listed | Yes | Yes |
| Automatic cleanup | Yes | Yes | Not listed | Not listed | Not listed |
| ZIP import | **No** | Not listed | Not listed | Yes, including encrypted | Not listed |
| Image export | PNG, JPEG, BMP, TIFF | TIFF, JPEG, PNG | Not listed | JPEG, BMP | JPEG, TIFF, BMP, PNG |
| Movie export | **No** | QuickTime, MPEG-4 | Not listed | MP4, WMV | WMV |
| Save captures as DICOM | Yes, secondary capture | Yes | Not listed | No for MPR, VR and fusion | Not listed |
| Standard printing or PDF | **Not found** | Yes | Not listed | Yes | Yes |
| DICOM print | Yes | Yes | Not listed | Not listed | Yes |
| CD/DVD with viewer | DICOMDIR burning; can copy a configured folder, such as a viewer, onto the disc | Yes | Not listed | Separate edition | Yes, plus disc robots |
| Anonymisation | Tags, PS3.15 basic profile; only when creating a DICOMDIR | Yes | Not listed | Add-on | Yes, including burned-in text |
| DICOM tag editor | **No** | Yes | Not listed | Not listed | Yes, plus hex editor |
| Reporting | **No** | DICOM PDF, Word, Pages | Cloud service, conflicting sources | Not listed | Attach PDF reports |
| Auto-routing | **No** | Yes | Not listed | Not listed | Not listed |
| Mini PACS server | **No** | Server mode | Not listed | Not listed | Yes |
| Plugins | **Compile-time extensions only** | Plugin SDK | Not listed | Not listed | Not listed |
| AI segmentation | Unmerged `segmentation-ai` branch | Not listed | Not listed | Not listed | Not listed |

## Positioning and constraints

Decided on 2026-10-31:

| Topic | Decision | Consequence |
| --- | --- | --- |
| Users | Internal users for now | No market launch to plan for; feedback comes from real internal workflows |
| Certification | Not now, but later | Build to certification-ready practices from the start |
| Business model | Open core | The GPL shapes the architecture (see [Open core](#open-core)) |
| Language | English only | No translation work (see below) |
| macOS | Low priority | Moved to [Later](#later-low-priority) |
| Receiving pushed studies | Low priority | Moved to [Later](#later-low-priority) |

The target is a dependable internal viewer on Windows and Linux that closes the
security, display and advanced 3D gaps. Matching everything OsiriX has
accumulated is not a goal.

New user-facing strings still go through `tr()`, so translation stays possible.
The `ca_ES` and `es_ES` translations stay in the repository but are no longer
maintained, and packages ship English only.

Two things keep macOS cheap to add later: new code stays portable, with no
Windows-only or Linux-only APIs outside isolated platform layers, and no new
QtXmlPatterns usage is added.

## Phases

| Phase | Theme | Rough effort (engineer-weeks) |
| ---: | --- | ---: |
| 0.5 | Close out Phase 0 | 2-3 |
| 1 | Network security and decoding | 5-8 |
| 2 | Display every everyday DICOM object | 5-7 |
| 3 | Advanced MPR and 3D | 14-20 |
| 4 | Remaining viewing parity | 5-7 |
| 5 | Workflow, data tools and AI | 10-16 |
| | **Total** | **41-61** |
| Later | macOS, Qt 6, receiving pushed studies, C-GET, worklist | 11-16 |

Certification-ready practices add overhead on top of these figures, probably
15-25% once established, with more at the start while the foundations are set up.

### Phase 0.5: Close out Phase 0

Everything after this builds on the assumption that the baseline works.

- Install the `.deb`, AppImage and NSIS packages on clean machines of the kind
  internal users have.
- Build a test corpus of real or public de-identified studies covering CT, MR,
  PET-CT, US, XA, MG, multi-frame and every supported transfer syntax.
- Exercise the main flows: open from disc, query and retrieve against a test
  PACS such as Orthanc and the internal PACS, MPR, 3D, fusion, measurement,
  export and printing.
- Hands-on benchmark against RadiAnt on the same studies, and correct this
  document where the desk research was wrong.
- Set up the foundations in
  [Certification-ready engineering](#certification-ready-engineering).
- Open the PR from `phase0` into `devel`.

**Exit:** packages install and work on clean machines, findings are triaged,
baseline timings for load, scroll and 3D are recorded, and the foundations are
in place.

**Status:** the test corpus, test PACS and foundation records are in
[quality/](quality/README.md). Installing, the main flows and the benchmark are
still to run, using [test-protocol.md](quality/test-protocol.md).

### Phase 1: Network security and decoding

- **Windows OpenSSL:** build it into the SDK, restoring WADO over HTTPS and
  enabling DICOM TLS support in DCMTK.
- **DICOM TLS:** add it to PACS configuration and all DIMSE operations, on
  Windows and Linux. The SDK enables OpenSSL on Linux, but the application never
  uses it.
- **DICOMweb authentication:** add basic and OAuth 2 bearer tokens, with
  credentials in the operating system's secure store. None was found in the WADO
  request classes.
- **JPEG-LS:** fix, and report undecodable frames to the user (A-01). The files
  show as blank images with no error. `main.cpp:203` registers only the JPEG and
  RLE decoders, and `volumepixeldatareaderfactory.cpp` sends only JPEG 2000 and
  segmented palettes to GDCM.
- **Wrong-length pixel data:** reject the file, and check for an out-of-bounds
  read (A-20).
- **Update checker:** move it to HTTPS and make sending version and OS optional
  (`applicationupdatechecker.cpp`).
- **Malformed input:** fuzz the DICOM parsers and decoders in CI and pin
  dependency updates. RadiAnt's 2026.1 release fixed an out-of-bounds write
  triggered by a malicious DICOM file (CVE-2026-17264), and Starviewer has
  comparable exposure.
- **Backlog fix:** make `autotests` report failures correctly, so 256 failures
  cannot wrap to exit code 0.

**Exit:** TLS and authenticated DICOMweb work against Orthanc and the internal
PACS, every transfer syntax in the corpus decodes, and fuzzing runs in CI.

### Phase 2: Display every everyday DICOM object

- **Structured reports:** render them, instead of showing only a thumbnail.
- **Presentation states:** implement GSPS (`presentationstatefillerstep.cpp`).
- **Key images:** implement key image notes (`keyimagenotefillerstep.cpp`).
- **DICOM video:** play MPEG-2 and MPEG-4 objects. Choosing a decoder is a
  licence and patent decision as well as a technical one, because it affects
  open core distribution later.

DCMTK includes SR and presentation state modules (`dcmsr`, `dcmpstat`), so check
that the SDK builds them before writing new parsing code.

**Exit:** each object type in the corpus displays correctly on Windows and
Linux, with golden-image tests. A GSPS or key image that cannot be applied warns
the user rather than silently showing the image without it.

### Phase 3: Advanced MPR and 3D

In priority order:

1. Rigid registration, which fusion across different acquisitions depends on.
   ITK is already in the SDK.
2. General multi-modality fusion, such as MR T1/T2 with DWI. Today fusion is
   offered only for PET or NM with CT in the same frame of reference
   (`patientbrowsermenu.cpp:112`), and that limit should only be relaxed
   together with registration.
3. Curved MPR.
4. 3D measurement.
5. Volume ROI, with 3D SUV statistics.
6. STL export, using VTK's writer.
7. Sculpting and saved views.
8. Movie export from 3D, including flythrough.
9. RTSTRUCT display and gantry tilt correction (`imageplane.cpp:280` has a
   workaround rather than a real correction).
10. Virtual endoscopy and 4D volume rendering, if internal users ask for them.

Measurement and registration errors can mislead a reader, so these features
carry the most risk in the roadmap. Keep their calculations in tested,
UI-independent classes, and show when an image is registered rather than
original.

**Exit:** items 1-6 are shipped and validated with internal users, including
measurement accuracy checks against known phantoms.

### Phase 4: Remaining viewing parity

- **Measurement:** manual calibration, for images with no pixel spacing.
- **Image tools:** sharpen and smooth filters; promote DSA from the playground
  into a supported extension; time-intensity curves, building on
  `PolylineTemporalROITool`.
- **Input and output:** ZIP import; MP4 export; standard printing and PDF.
- **Usability:** customisable shortcuts, a hanging protocol editor, and
  performance work against the Phase 0.5 baseline.

**Exit:** load and scroll timings are no worse than RadiAnt's on the agreed
benchmark, or the difference is accepted and recorded, and the items internal
users rank highest are shipped.

### Phase 5: Workflow, data tools and AI

- **Editing and anonymisation:** DICOM tag editor; batch anonymisation with
  burned-in text removal and a choice of de-identification profiles. Today
  anonymisation only runs when creating a DICOMDIR.
- **Routing:** auto-routing rules.
- **Reporting:** simple reports saved as DICOM PDF.
- **Extension boundary:** a documented API for separate-process modules (see
  [Open core](#open-core)).
- **AI segmentation:** review the 2025 `segmentation-ai` branch, which adds
  TensorFlow and Torch back ends in a playground extension, and rebuild it behind
  the extension boundary. Keep it marked as research, because AI functions can
  raise a product's regulatory class.

**Exit:** a study can be edited, de-identified, routed and reported on from
inside the application, and one module runs against the documented boundary.

### Later (low priority)

Not scheduled. Pick these up when internal users need them.

- **macOS:** native Apple Silicon build, `macos.yml` workflow, signed and
  notarized `.dmg`, and testing of VTK's OpenGL rendering, which Apple has
  deprecated. `InstallTargets.cmake` and `Installers.cmake` have no macOS bundle
  or disk image rules today. The 2018 `mac-bundle-improvements` branch may have
  reusable work.
- **Qt 6:** no longer urgent without macOS, but Qt 5 is ageing, so review it
  yearly as a maintenance and security risk. It requires removing QtXmlPatterns,
  used only for schema validation in `customwindowlevelsreader` and
  `dicomdumpdefaulttagsreader`.
- **Receiving pushed studies:** a background Store SCP, not only during retrieval
  (`retrievedicomfilesfrompacs.cpp`), and any mini PACS mode built on it.
- **C-GET:** for sites and firewalls where C-MOVE is impractical.
- **Worklist:** a modality worklist client.

## Certification-ready engineering

The aim is that certification later means completing a record that already
exists, not recreating one. Design records in particular are hard to write after
the fact. This is engineering context; a regulatory specialist should review it
before it is relied on, ideally early in Phase 1.

| Practice | What to do now | Standard it prepares for |
| --- | --- | --- |
| Quality records | Keep requirements, design decisions, reviews and releases in the repository and issue tracker | ISO 13485 |
| Traceability | Give each requirement an ID, and link it to its design, code and tests through issues and commits | IEC 62304 |
| Safety classification | Assume at least Class B until a specialist confirms it | IEC 62304 |
| Architecture | Document modules and interfaces in the repository, and record major decisions | IEC 62304 |
| Third-party software | Generate a software bill of materials in CI, listing version, licence and known issues for each library | IEC 62304, IEC 81001-5-1 |
| Risk management | Keep a hazard log from Phase 0.5: wrong patient, laterality or orientation, wrong measurement, missing images, silent decode failure | ISO 14971 |
| Verification | Tests for each requirement, golden-image tests for decoding and display, phantom checks for measurement | IEC 62304 |
| Change control | Protected `devel`, required review and CI, tagged releases, retained build artifacts | IEC 62304 |
| Problem tracking | Record defects and known anomalies per release, including internal user reports | IEC 62304 |
| Security | Threat model, TLS by default, secure credential storage, vulnerability monitoring, no patient data in logs | IEC 81001-5-1 |
| Usability | Record the use scenarios and usability findings from internal users | IEC 62366 |
| DICOM | Maintain a DICOM conformance statement | DICOM PS3.2 |

Code rules that follow from this:

- Keep clinical logic (decoding, geometry, measurement, SUV) in UI-independent,
  unit-tested classes.
- Fail visibly. Report an image that cannot be displayed correctly, rather than
  showing it in a degraded form without warning.
- Keep experimental work behind a build option, as playground extensions are
  today, so it stays out of a future certified configuration.
- Build all release packages in CI only, from a tagged commit.

**Until certified:**

- Label builds as not for diagnostic use. This roadmap assumes internal users are
  not making diagnostic or treatment decisions with it. If they will, get
  regulatory advice first, because internal use can still be regulated.
- Do not ship the CE build or CE marking. Any certification from the original
  project belongs to its legal manufacturer, and its status was not checked. The
  MDR work in `release-2.0` is useful as reference. `STARVIEWER_CE` also needs a
  deployment repository that is not in this codebase.

## Open core

This needs legal review before any commercial module is built. The main
constraint comes from the existing licence:

- **The existing code is GPL-3.0+,** with copyright held by the Universitat de
  Girona and the Institut de Diagnòstic per la Imatge. We cannot relicense it
  ourselves.
- **Code linked into the application generally has to be GPL when distributed.**
  That rules out the common open core design of proprietary in-process plugins,
  unless the copyright holders grant an exception.
- **Separate programs are the likely route.** Commercial modules can run as
  separate processes, communicating through a documented interface such as DICOM,
  DICOMweb or a local API. They need a real arm's-length design, so settle the
  boundary before Phase 5 builds on it.
- **Internal use is not distribution,** so these obligations mostly start at the
  first external release. Sharing with outside contractors or partner
  organisations may count as distribution, so check before doing so.

Recommended split, to be confirmed:

- **Open core:** viewing, DICOM networking, security, measurement and everything
  needed for a safe, usable viewer. These must remain GPL anyway.
- **Commercial modules, as separate processes:** AI, enterprise administration
  and deployment, and routing and integration services. Support and certified
  builds can also be sold.

Other actions:

- Generate licence information in the software bill of materials from Phase 0.5,
  so incompatible dependencies are caught when they are added.
- Decide on a contributor agreement before accepting outside contributions.
- Confirm rights to the "Starviewer" name before any external release.

## Decisions still needed

| Decision | Recommendation | Affects |
| --- | --- | --- |
| Do internal users make diagnostic or treatment decisions with it? | Assume not; get advice if they do | Labelling, rollout |
| Commercial module boundary | Separate processes, reviewed by a lawyer | Phase 5 |
| Video decoder | Choose with licence and patent review | Phase 2 |
| Product name | Confirm rights or rename before any external release | Branding, packaging |
| Safety class | Assume Class B or higher until a specialist confirms | Depth of verification |

## Sources

Starviewer findings come from this repository. Competitor information was read on
2026-10-31:

- OsiriX: [technical sheet](https://www.osirix-viewer.com/resources/technical-sheet/),
  [OsiriX MD](https://www.osirix-viewer.com/osirix/osirix-md/)
- Horos: [GitHub repository](https://github.com/horosproject/horos),
  [GitHub releases API](https://api.github.com/repos/horosproject/horos/releases?per_page=3),
  [Radiology Cafe](https://www.radiologycafe.com/radiology-trainees/dicom-viewers/),
  [SourceForge](https://sourceforge.net/software/product/Horos/),
  [virtual bronchoscopy paper](https://pmc.ncbi.nlm.nih.gov/articles/PMC7857369/),
  [Rosetta Check](https://rosettacheck.com/apps/org.horosproject.horos) (community
  source). [horosproject.org](https://horosproject.org/) refused connections.
- RadiAnt: [home page](https://www.radiantviewer.com/),
  [features](https://www.radiantviewer.com/dicom-viewer-manual/radiant_dicom_viewer_features.html),
  [products](https://www.radiantviewer.com/products/)
- Sante: [Sante DICOM Viewer Pro](https://www.santesoft.com/win/sante-dicom-viewer-pro/sante-dicom-viewer-pro.html)
- Qt on Apple Silicon: [Qt forum](https://forum.qt.io/topic/143072/create-a-native-arm64-app),
  [Qt mailing list](https://lists.qt-project.org/pipermail/interest/2022-April/038107.html)
