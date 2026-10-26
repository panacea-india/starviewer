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
  libkf5threadweaver-dev extra-cmake-modules \
  qtbase5-gtk-platformtheme qttranslations5-l10n qtwayland5 squashfs-tools \
  xvfb xauth
```

(The last three Qt packages and `squashfs-tools` are only needed to build
the packages; `qtbase5-gtk-platformtheme` provides the bundled `qgtk3`
plugin and `qtwayland5` the bundled Wayland plugins.)

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

CI runs the same steps in `.github/workflows/linux.yml`. On GitHub's
`ubuntu-24.04` runner an uncached SDK build takes about 46 minutes, the
Starviewer build about 12 minutes, and the tests a few seconds. The workflow
then rebuilds with `USE_QTCONF=ON` and packages a `.deb` (CPack) and an
AppImage (`.github/scripts/make-appimage.sh`), uploaded as workflow
artifacts.

CI caches each SDK library separately (`.github/scripts/sdk-library.sh`). A
library's cache key covers the shared SDK scripts, its own `config/`, `libs/`
and `sdk/patches/` files, and those of the libraries it is built against
(GDCM against VTK; ITK against DCMTK, VTK and GDCM). So a GDCM patch rebuilds
GDCM and ITK only, and an ITK patch rebuilds ITK only. Changing the shared
scripts (`config/setup.sh`, `libs/cmakebuild.sh`, `sdk_*.sh`) rebuilds
everything. When adding a library or a dependency between libraries, update
`DEPENDENCIES` in that script and the per-library steps in both workflows.

## Building on Windows (verified in CI: Windows Server 2022, MSVC 19.44)

`.github/workflows/windows.yml` is the reference. It uses Visual Studio 2022
(x64), Qt 5.15.2 `msvc2019_64`, MSYS2 (the `MSYS` environment with
`path-type: inherit`, so MSVC, Qt and CMake stay on `PATH`) and jom. The SDK
scripts run in that MSYS2 shell, built `RelWithDebInfo`, and include
ThreadWeaver and ECM (`LIBS="dcmtk vtk gdcm itk ecm threadweaver"`).

Things that matter when reproducing it by hand:

- Keep `SDK_BASE_PREFIX` short (CI uses `D:/sdk`): VTK and ITK exceed the
  Windows path length limit otherwise.
- Give the SDK scripts the POSIX form of that path (`cygpath -u`). MSYS2's tar
  reads `D:/...` as a remote host.
- Remove or shadow MSYS2's `/usr/bin/link.exe`, which hides MSVC's linker.
- Run the tests with `sdk-install/bin` and Qt's `bin` on `PATH`.
  `.github/scripts/list-missing-dlls.sh` names any DLL the tests cannot load
  (ctest only reports exit code `0xc0000135`).

On GitHub's `windows-2022` runner an uncached SDK build takes about 66
minutes, the Starviewer build about 13 minutes, and the tests about 7
seconds. With the SDK cached, a run takes about 15 minutes. The workflow
then rebuilds with `USE_QTCONF=ON` and packages an NSIS installer (CPack),
uploaded as a workflow artifact.

## Test baseline

`autotests` (registered as a single CTest test):

| Suites | Test cases | Passed | Failed | Skipped |
| -----: | ---------: | -----: | -----: | ------: |
|    111 |      4,757 |  4,757 |      0 |       0 |

The same baseline passes in Linux CI. On Windows `autotests` also passes
(ctest reports only the overall result, which is 0 failures). The application starts under Xvfb and stays up without crashing. It has not
yet been exercised interactively with studies.

## Packaging

Both workflows end by producing installable packages and uploading them as
workflow artifacts: `starviewer~<version>~<arch>-<os>.deb` and
`starviewer~<version>~<arch>.AppImage` on Linux,
`starviewer~<version>~<arch>-winnt.exe` (NSIS) on Windows. `COMPILE.mkd`
describes how to build them by hand.

The existing CPack configuration (`starviewer/CMake/Installers.cmake`) was
kept; its install rules (`starviewer/CMake/InstallTargets.cmake`) were
rewritten for the current SDK:

- The old rules were written for a Qt 5.12.6 installed under a custom
  prefix, with hardcoded sonames (`*.so.5.12.6`, ICU 56). They now work with
  a Qt installation prefix and with a system Qt (Debian multiarch layout,
  any 5.15.x sonames), detected at configure time from `QT_DIR`.
- The old rules bundled VTK, GDCM and ThreadWeaver from the SDK with
  hardcoded file lists and versions. VTK, DCMTK and ITK are built static and
  need no runtime files; the shared libraries the SDK installs are now
  bundled with globs, so the rules follow library version changes. On Linux
  ThreadWeaver comes from the `libkf5threadweaver5` package instead of being
  bundled.
- On Windows the OpenSSL runtime DLLs (needed by QtNetwork for WADO over
  HTTPS) come from the Qt installation, because DCMTK is built without
  OpenSSL there and the SDK does not provide them.
- The DEB dependency list was replaced with the system libraries directly
  needed by the bundled Qt and plugins on Ubuntu 24.04 (the old list named
  libraries that no longer exist in 24.04, such as `libffi7`).
- NSIS shortcuts to the PDF manuals and the DEB "How to access IFU" menu
  entry are only created when the manuals were built; with
  `BUILD_PDF_MANUALS=OFF` they pointed at files that do not exist.

Package builds are configured with `USE_QTCONF=ON`, which embeds a qt.conf
so that Qt finds plugins and translations next to the installed executable.
Because that hides the development Qt plugins from a build tree, the
workflows test first on a `USE_QTCONF=OFF` build and only then reconfigure
and relink for packaging.

The AppImage is packed with `appimagetool` 1.9.1
(`.github/scripts/make-appimage.sh`). It installs the same tree as the DEB
under `AppDir/opt/starviewer` and uses an AppRun that adds the bundled
`lib` directory to the loader path.

The packages have not been installed or exercised on a real machine yet;
they are verified to build in CI only.

## Fixes made in Phase 0

- Bundled Breakpad: include `<stdint.h>` (GCC 13).
- SDK patches for GCC 13: `<cstdint>` in ITK 5.3.0 and VTK 9.2.5
  (`sdk/patches/itk/`, `sdk/patches/vtk/gcc13-include-cstdint.patch`).
- SDK scripts exited with status 0 when a patch failed to apply, when a
  library failed to configure, compile or install, and when a download failed
  checksum verification. They now exit with status 1. (This hid a real VTK
  build failure during Phase 0.)
- Inside an enclosing git repository (such as the CI workspace), `git apply`
  skipped the files of git-format patches and still succeeded. The SDK
  scripts now stop git from looking above the source directory and fail if
  any file of a patch is skipped. SDK patches should be plain unified diffs
  (`--- a/path`, `+++ b/path`) like the existing ones.
- Removed the Travis and AppVeyor configurations, which depended on a
  precompiled SDK hosted at trueta.udg.edu.
- The packaging install rules (`starviewer/CMake/InstallTargets.cmake`)
  still described Starviewer 0.13's SDK (bundled VTK 6.1 and ThreadWeaver,
  Qt 5.12.6 under a custom prefix, hardcoded sonames, ICU 56). They were
  rewritten; see "Packaging" above.
- Windows: ITK's internal Eigen was silently not installed; `libs/itk.sh`
  now installs it explicitly and fails if it is missing.
- Windows: `bzero` replaced with `memset` in the PACS query and send code.
- Windows: DCMTK's support libraries (zlib, libiconv) are copied into
  `sdk-install/dcmtk-support`, because DCMTK's package config refers to them
  by absolute path and the SDK build directory is not kept.
- Windows: GDCM did not install `vtkgdcm-9.2.dll`; a patch gives
  `vtk_module_build` GDCM's `bin` and `lib` destinations
  (`sdk/patches/gdcm/vtkgdcmInstallDestinations.patch`), and `libs/gdcm.sh`
  fails if the library is missing.

## Still open in Phase 0

- The packages produced by CI have been built, but not installed and
  exercised on a real machine (the application itself has not been exercised
  interactively with studies either).
- Only the plain (non-CE) build is packaged. The CE variant, which needs the
  deployment repository and PDF manuals, is not.

## Backlog found during Phase 0

- `autotests` returns the sum of failures as its exit code, which wraps
  modulo 256 (256 failures would report success).
- `src/core/applicationupdatechecker.cpp` checks for updates over plain HTTP
  and sends the version and OS.
- QtXmlPatterns, used by Starviewer, does not exist in Qt 6.
- The GSPS presentation state loader is a stub
  (`src/core/presentationstatefillerstep.cpp`).
- The `segmentation-ai` branch (2025) has not been reviewed.
- Windows: DCMTK is built without OpenSSL (`WITH_OPENSSLINC` is unset), so
  DICOM TLS is not available there.
- CI uses actions that target Node.js 20, which GitHub has deprecated
  (`actions/cache`, `actions/checkout`, `ilammy/msvc-dev-cmd`).
- Packaging: the NSIS installer is only built in CI, never run. The WiX
  configuration in `Installers.cmake` is kept but no workflow uses it.
- Packaging: the `appimagetool` download in `make-appimage.sh` is not
  checksum-verified, and appimagetool fetches the type2 runtime at package
  time. The AppImage also only runs on systems with a glibc as new as the
  build system's (ubuntu-24.04).
- Packaging: `CPACK_DEBIAN_PACKAGE_DEPENDS` names Ubuntu 24.04 packages;
  other distributions need their own list.
- A change to VTK or to the shared SDK scripts still rebuilds most of the
  SDK (about 66 minutes on Windows, 46 on Linux). Publishing a prebuilt
  patched SDK as a CI artifact would also let developers skip the SDK build.
