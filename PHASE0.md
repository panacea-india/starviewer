# Phase 0: Reproducible build and test baseline

Goal of Phase 0: get the abandoned codebase building from source on a current
toolchain, with its own patched SDK, a passing test baseline, and CI that
keeps it that way. No functional changes.

## Branch

`phase0` is `release-2.0` (the MDR certification work and the latest clinical
fixes) with `devel` merged in (dependency upgrade to VTK 9.2.5, ITK 5.3.0,
DCMTK 3.6.7, GDCM 3.0.21, CTest and C++17). The only merge conflicts were in
`sdk/`, which was taken from `devel`; `SDK_BUILD_TYPE` and
`STARVIEWER_BUILD_TYPE` keep `release-2.0`'s `RelWithDebInfo` default.

## Why the SDK must be built from source

Starviewer is linked against its own patched builds of DCMTK, VTK, GDCM and
ITK (`sdk/patches/`). The patches change behaviour, not just compilation:

- DCMTK: floating point pixel representation (`EPR_float32`), required by
  `vtkdcmtkimagereader.cpp`.
- GDCM: make the reader rethrow on error, throw on `bad_alloc`, and wrap
  reads in try/catch, so that corrupt or truncated files are reported instead
  of being displayed silently wrong.
- VTK: faster corner annotation and text mapping.

Distribution packages of these libraries do not carry the patches. Do not
build Starviewer against them, even when it happens to compile.

## Building on Linux (verified on Ubuntu 24.04, GCC 13.3)

System packages (Qt and ThreadWeaver are not patched, so packages are fine):

```sh
sudo apt-get install build-essential cmake git rsync wget xsltproc \
  libssl-dev zlib1g-dev libgl1-mesa-dev libglu1-mesa-dev libxt-dev \
  qtbase5-dev qttools5-dev qtdeclarative5-dev libqt5xmlpatterns5-dev \
  libqt5opengl5-dev libqt5sql5-sqlite \
  qml-module-qtquick2 qml-module-qtquick-controls qml-module-qtquick-controls2 \
  qml-module-qtquick-layouts qml-module-qtquick-window2 qml-module-qtqml-models2 \
  libkf5threadweaver-dev extra-cmake-modules xvfb xauth
```

Patched SDK (downloads are checksum-verified; takes over an hour):

```sh
export SDK_BASE_PREFIX=$HOME/starviewer-sdk QTDIR=/usr LIBS="dcmtk vtk gdcm itk"
cd sdk/scripts/unix
./sdk_download.sh && ./sdk_extract.sh > /dev/null && ./sdk_build.sh
```

Starviewer:

```sh
cmake -S starviewer -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DSDK_INSTALL_DIR=$SDK_BASE_PREFIX/sdk-install -DQT_DIR=/usr \
  -DBUILD_PDF_MANUALS=OFF
cmake --build build --parallel
```

Tests:

```sh
cd build
LD_LIBRARY_PATH=$SDK_BASE_PREFIX/sdk-install/lib \
  xvfb-run -a -s "-screen 0 1280x1024x24" ctest --output-on-failure
```

CI runs the same steps in `.github/workflows/linux.yml` and caches the SDK
keyed on `sdk/patches/` and `sdk/scripts/unix/`.

## Test baseline

`autotests` (registered as a single CTest test):

| Suites | Test cases | Passed | Failed | Skipped |
| -----: | ---------: | -----: | -----: | ------: |
|    111 |      4,757 |  4,757 |      0 |       0 |

The application starts under Xvfb and stays up without crashing. It has not
yet been exercised interactively with studies.

## Fixes made in Phase 0

- Bundled Breakpad: include `<stdint.h>` (GCC 13).
- SDK patches for GCC 13: `<cstdint>` in ITK 5.3.0 and VTK 9.2.5
  (`sdk/patches/itk/`, `sdk/patches/vtk/gcc13-include-cstdint.patch`).
- SDK scripts exited with status 0 when a patch failed to apply, when a
  library failed to configure, compile or install, and when a download failed
  checksum verification. They now exit with status 1. (This hid a real VTK
  build failure during Phase 0.)
- Removed the Travis and AppVeyor configurations, which depended on a
  precompiled SDK hosted at trueta.udg.edu.

## Still open in Phase 0

- Windows CI (MSVC). The SDK scripts support Windows through MSYS2; this
  needs to be brought up on a GitHub Windows runner.
- Packaging (Linux `.deb`/AppImage, Windows installer) from CI.
- `COMPILE.mkd` describes Starviewer 0.13 on Debian 8 and should be replaced
  by the steps above.

## Backlog found during Phase 0

- `autotests` returns the sum of failures as its exit code, which wraps
  modulo 256 (256 failures would report success).
- `src/core/applicationupdatechecker.cpp` checks for updates over plain HTTP
  and sends the version and OS.
- QtXmlPatterns, used by Starviewer, does not exist in Qt 6.
- The GSPS presentation state loader is a stub
  (`src/core/presentationstatefillerstep.cpp`).
- The `segmentation-ai` branch (2025) has not been reviewed.
