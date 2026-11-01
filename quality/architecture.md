# Architecture

Status: **overview from reading the build files and source.** It records the structure as it
is, not a target design.

## Modules

Dependencies point downwards, as declared by `target_link_libraries`.

```text
starviewer (src/main)            program entry, DCMTK decoder registration, crash handling
  |
  +-- extensions                 q2dviewer, mpr2d, q3dviewer, pdf, dicomprint
  |                              playground/*: experimental, needs ENABLE_PLAYGROUND_EXTENSIONS
  +-- interface (src/interface)  main window, menus, extension hosting
  |     |
  +-- inputoutput                PACS (DIMSE, DICOMweb), local database, DICOMDIR, RIS listener
  |     |
  +-- core (src/core)            data model, reading and decoding, viewers, tools, measurement
        |
        +-- DCMTK, VTK, GDCM, ITK (patched SDK), Qt, ThreadWeaver
```

`src/crashreporter` and `src/starviewersapwrapper` are separate executables.
`src/thirdparty` holds the vendored libraries listed in [soup.md](soup.md).

## Boundaries that matter for safety

| Concern | Where | Note |
| --- | --- | --- |
| Choosing a decoder | `core/volumepixeldatareaderfactory.cpp`, `main/main.cpp` | See A-01 |
| Image geometry | `core/imageplane.cpp` | Gantry tilt has a workaround, not a correction |
| Measurement, SUV | `core/` | Not all separated from UI code. Check when changing them |
| PACS | `inputoutput/` | No TLS or DICOMweb authentication yet |
| Settings | `*settings.cpp` in each module | Includes PACS defaults and the local AE title |

## Build variants

| Option | Effect | Status for this fork |
| --- | --- | --- |
| `STARVIEWER_CE` | CE-marked build of the original project | Do not ship. Needs a deployment repository that is not here |
| `STARVIEWER_LITE` | Reduced build | Not built in CI |
| `STARVIEWER_BETA` | Beta notice in the menu bar | Not used by CI |
| `ENABLE_PLAYGROUND_EXTENSIONS` | Experimental extensions | Keep out of any future certified configuration |

## Decisions

| ID | Decision | Why |
| --- | --- | --- |
| AD-01 | Build DCMTK, VTK, GDCM and ITK from source with `sdk/patches`, never from distribution packages | The patches change behaviour, including error reporting for corrupt files. See [PHASE0.md](../PHASE0.md) |
| AD-02 | Build release packages only in CI, from tagged commits | Every package can be traced to its source and SDK |
| AD-03 | Commercial modules run as separate processes behind a documented interface, pending legal review | The existing code is GPL-3.0-or-later. See the [open core notes](../ROADMAP.md#open-core) |
| AD-04 | The not-for-diagnostic label goes in the window title and About dialog, on all non-CE builds | The title stays visible on every main window. The beta notice was not reused because it is opt-in and describes a preview for radiologists |
| AD-05 | Keep quality records in the repository, as Markdown | They change with the code and are reviewed through the same pull requests |
