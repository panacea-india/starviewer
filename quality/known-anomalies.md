# Known anomalies

Open defects and findings. Add new ones as they are found, including from internal users.
Close them by recording what fixed them. Never reuse an ID.

Severity follows [hazard-log.md](hazard-log.md). *Confirmed* means it was observed, and
*Suspected* means it comes from reading code and nobody has reproduced it yet.

| ID | Anomaly | Severity | Evidence | Hazard | Plan |
| --- | --- | --- | --- | --- | --- |
| A-01 | JPEG-LS files may not decode. `main/main.cpp:203` registers only the JPEG and RLE decoders, and `core/volumepixeldatareaderfactory.cpp:146` sends only JPEG 2000 and segmented palettes to GDCM | High | Suspected. Test with `transfer-syntaxes/jpeg-ls-*` | H-05 | Phase 1 |
| A-02 | The packages have never been installed on a clean machine, and the application has never been used interactively with studies | High | Confirmed gap | All | TP-01 onwards, Phase 0.5 |
| A-03 | The Windows package has no OpenSSL, so WADO over HTTPS does not work. There is no DICOM TLS or DICOMweb authentication on any platform | High | Confirmed from the build and source | H-09 | Phase 1 |
| A-04 | `autotests` returns the sum of failures as its exit code, which wraps modulo 256, so 256 failures would report success | Medium | Confirmed from source | All | Phase 1 |
| A-05 | No fuzzing and no vulnerability monitoring of third-party libraries | Medium | Confirmed gap | H-11 | Phase 1 |
| A-06 | Support email, web address and organisation constants still name the original project (`starviewerapplication.h`). The update checker uses plain HTTP and sends the version and OS | Medium | Confirmed from source | H-09, H-12 | Phase 1 |
| A-07 | No manual calibration for images with no pixel spacing | Medium | Suspected; not found in source | H-03 | Phase 4 |
| A-08 | The vendored Breakpad and Easylogging++ have no licence text in this repository | Low | Confirmed | | Before any external release |
| A-09 | Packages do not ship English only yet. They bundle the Catalan and Spanish translations and follow the system locale, although these translations are no longer maintained. New strings, including the diagnostic-use label, have no translation and appear in English | Medium | Confirmed from `InstallTargets.cmake` and `applicationtranslationsloader.cpp` | H-10 | Phase 1 |
| A-10 | The not-for-diagnostic label appears in the window title and About dialog only. It does not appear on exported images, prints or DICOM captures | Medium | Confirmed by design | H-10 | Decide once internal use is settled |
| A-11 | Change control is documented but not enforced: `devel` protection and required review are not confirmed, and there is no tagged-release workflow | Medium | Confirmed gap | All | Before the first internal release |
| A-12 | The Linux CI artifact is 1.1 GB against 56 MB for Windows, so packages may carry debug information | Low | Observed on run 40439029170; the cause is not investigated | | Check with the first download |
| A-13 | Root `Dockerfile` is stale: Ubuntu 18.04, with an SDK from `trueta.udg.edu` | Low | Confirmed | | Remove or replace |
| A-14 | `sdk_download.sh` still has Qt 5.12.6 downloads, which CI does not use. The About dialog has a hard-coded copyright year of 2019 | Low | Confirmed from source | | Clean up when touched |
| A-15 | The inherited DICOM conformance statement has not been checked against this fork | Medium | Confirmed gap | | Update after TP-10 and Phase 1 |
| A-16 | Orthanc 1.12.6 refuses the deflated test files over REST and C-STORE, so DICOM retrieval of deflated data is untested. This is a limit of the test PACS, not of Starviewer | Low | Confirmed | H-05 | Test from disk; revisit with the internal PACS |
