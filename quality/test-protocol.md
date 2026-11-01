# Phase 0.5 test protocol

Run this on packages built by CI, once per platform. Set up the [test lab](testlab/README.md)
first. Record each run in [Results](#results), and add anything unexpected to
[known-anomalies.md](known-anomalies.md).

Open `corpus/synthetic/expected.json` alongside: it has the exact expected values.

## Steps

| ID | Step | Pass when | Requirements |
| --- | --- | --- | --- |
| TP-01 | Install the package on a clean machine and start it. Check the title bar and the About dialog | It installs and starts without adding software, and both show "Not for diagnostic use" | SR-INS-01, SR-LBL-01 |
| TP-02 | Open `ct-reference` from disk in the 2D viewer | Patient `PHANTOM^CT`, ID and date are shown. The labels read R on the left and L on the right, and A at the top. The L-shaped block is on the viewer's right and towards the top | SR-PAT-01, SR-ORI-01, SR-ORI-02 |
| TP-03 | Open each folder in `transfer-syntaxes/`, one at a time. Compare `CT0050.dcm` or frame 1 with `implicit-le`, at the same window | Every folder displays. Lossless ones look identical. Lossy ones look close and are marked as lossy, if that is shown. Record which ones fail, especially JPEG-LS (A-01) | SR-DEC-01, SR-DEC-06 |
| TP-04 | Open each folder in `broken/` | Truncated, wrong-length and not-DICOM files give an error, with no crash and no partial image shown as complete. `missing-slice` warns about the gap, or the gap is otherwise evident | SR-DEC-02, SR-DEC-03 |
| TP-05 | Open `us-multiframe`, `xa-multiframe`, `mg-monochrome1`, `enhanced-ct` and `encapsulated-pdf`. Play the cines | Every frame plays in order. In the mammogram the disc is bright, the breast grey and the background dark, with L laterality. The enhanced CT shows 20 frames with the spheres in the middle ones. The PDF shows its text | SR-DEC-04, SR-DEC-05, SR-DEC-06, SR-ORI-03 |
| TP-06 | Open `ct-reference`, then add `mr-orientations` from another patient | The application does not combine them silently under one patient | SR-PAT-02 |
| TP-07 | Open `ct-reference` in MPR and 3D, then `mr-orientations` in the 2D viewer with reference lines | Spheres are round and at the expected positions, the bone sphere is on patient left, and labels are correct in every plane, including the oblique series | SR-ORI-01, SR-3D-01 |
| TP-08 | Measure the fiducials on instance 21 of `ct-reference`, the dots in `us-multiframe`, and HU with an ROI inside each sphere | CT distance is 100.0 mm to within one pixel (0.8 mm). Ultrasound distance is 20.0 mm to within one pixel (0.2 mm). Mean HU is within 8 of 100 and 1000. The calibration used is evident | SR-MEA-01, SR-MEA-02, SR-MEA-03, SR-MEA-05 |
| TP-09 | Open `petct-suv` in the PET-CT fusion view, then repeat with `rider-petct`. Read SUV in the sphere and the background | The PET sphere lines up with the CT soft tissue sphere. SUVbw is within 1% of 8.0 and 1.0. RIDER looks anatomically plausible and aligned, with no ground truth | SR-MEA-04, SR-3D-02 |
| TP-10 | Configure the test PACS (DIMSE, then DICOMweb). Query, retrieve `ct-reference` and `petct-suv`, and send a study. Then stop Orthanc mid-retrieve. Repeat against the internal PACS when available | Instance counts match the PACS, and the partial retrieve is reported to the user | SR-PAC-01 to SR-PAC-04 |
| TP-11 | Export images from `ct-reference`, and create a DICOMDIR with and without anonymisation. Print to DICOM print if available | Files open correctly. `dcmdump` shows no patient name or ID in the anonymised copy | SR-EXP-01, SR-EXP-02 |

## Benchmark against RadiAnt

Use the same machine, with no other heavy work running, and restart both applications between
series. Take the median of three timed runs. Use a stopwatch or screen recording, and record
the machine's CPU, memory, GPU and disk.

| Data | Measure |
| --- | --- |
| `rider-large-ct` (1,049 real slices) | Open from disk until the first image shows and until all slices are loaded; memory afterwards |
| `rider-large-ct` | Scroll smoothness through the series, by eye and by frame rate if shown |
| `rider-large-ct` | MPR open time, 3D volume rendering open time, and rotation smoothness |
| `transfer-syntaxes/*` | Whether each one opens, in both applications |
| `rider-petct` | Fusion open time |
| `benchmark-ct-1000` (`--large`) | Same as `rider-large-ct`, at 512x512x1000 |

Also record which [ROADMAP](../ROADMAP.md) entries were wrong for RadiAnt or Starviewer,
and correct them.

## Results

Record the commit, CI run, package file name and SHA-256, corpus checksum, operating system,
GPU and tester for each run.

| Run | Date | Platform | Commit | CI run | Steps passed | Steps failed | New anomalies |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 1 | 2026-10-31 | Linux, clean `ubuntu:24.04` container, Xvfb, Mesa software rendering | `f9cc1c38c` | 41260260302 | See below | TP-01, TP-03, TP-04 | A-17, A-19 to A-23 |
| 2 | 2026-11-01 | Windows 11 Home 10.0.22621, local build, not a clean machine | `3842fbcca` plus uncommitted phase0 fixes | None (local build) | TP-02, TP-04, TP-06, TP-08. The rest are partial; see [Run 2](#run-2) | None | A-25 |

Run 1 used the `.deb` with SHA-256 starting `7a3163e6`, tested by Claude driving the interface
with scripted input. Software rendering says nothing about performance. A person still has to
repeat this on a real desktop.

| Step | Result | Notes |
| --- | --- | --- |
| TP-01 | **Failed** | Did not start until libraries were added (A-17). The AppImage failed too (A-19). The title bar shows the label; the About dialog was not checked |
| TP-02 | Passed | Patient, ID, date and labels correct, with the block on the viewer's right and towards the top |
| TP-03 | **Failed** | JPEG-LS displays blank (A-01). The other 6 lossless CT variants match `implicit-le` on screen, pixel for pixel. The lossy CT variants and RGB folders display, and were judged by eye only |
| TP-04 | **Failed** | Wrong-length file displays with no warning (A-20). Truncated and not-DICOM files give "no patient data that can be loaded", with no crash. `missing-slice` shows "Slices 5 and 6 missing" |
| TP-05 | Partial | Mammogram polarity correct. Enhanced CT shows 20 frames. Cines were not played, and the PDF was not opened (A-22) |
| TP-06 | Partial | Opening another patient replaced the first, with no mixing. Adding studies by other routes was not tried |
| TP-07 | Partial | MPR opened by default with correct labels. The oblique series shows AF and PH labels and reference lines. 3D and geometry were not checked |
| TP-08, TP-09, TP-11 | Not run | |
| TP-10 | Partial | C-FIND and C-MOVE of `ct-reference` retrieved all 100 instances and displayed them, with the PACS settings written directly to the configuration file. C-STORE, DICOMweb, mid-transfer failure and the settings dialog were not tested |

No crash occurred in any of the 28 cases opened, including `pydicom-test-files`. Windows is
checked only by inspecting the installer (A-18).

The JPEG 2000 lossy case was 500:1 in this run, too coarse to judge. The generator now uses
10:1, with a separate decoder measuring an RMS error of 2.4 stored values, but that file has
not yet been opened in the viewer.

### Run 2

Run 2 used `C:\sdk\sv-build\bin\starviewer.exe`, built locally from `3842fbcca` with the
uncommitted phase0 fixes for A-01 and A-20, with `C:\sdk\sdk-install\bin` and
`C:\Qt\5.15.2\msvc2019_64\bin` first on PATH. It is not a CI package and the machine is not
clean, so TP-01's install check was not run. Claude drove the interface by screenshots, with
clicks and typing.

| Step | Result | Notes |
| --- | --- | --- |
| TP-01 | Partial | Title bar and About dialog both show "Not for diagnostic use"; version 2.0.0-devel. Install on a clean machine not tested |
| TP-02 | Passed | PHANTOM^CT and QA-CT-001 shown. R on the left, L on the right, A at the top. The L-shaped block is on the viewer's right and towards the top. The exact date was not read |
| TP-03 | Partial | `jpeg-ls-lossless` `CT0050.dcm` now displays the phantom, not a blank image (A-01). It was not compared pixel for pixel with `implicit-le`, and the other folders were not opened |
| TP-04 | Passed | `truncated-pixel-data` and `not-dicom` give "no patient data that can be loaded", with no crash. `pixel-length-mismatch` gives "Error loading data" and shows no image (A-20). `missing-slice` loads and shows "Slices 5 and 6 missing" |
| TP-06 | Passed | Opening `mr-orientations` (QA-MR-001) after another patient replaced it. The two were not mixed. Other routes for adding studies were not tried |
| TP-05 | Partial | `mg-monochrome1` shows a bright disc on a grey breast with a dark background, so polarity is correct. The laterality text was not read at full size. `us-multiframe` opens as 30 frames in a grid and the red disc moves steadily from frame 1 to 30. `xa-multiframe` opens at 30 frames, 512x512; frame 1 is a white background with a black curve, which is the data's own min-max window (pixel values 40 to 180, confirmed with `dcm2pnm`). Neither cine was played in motion. `enhanced-ct` opens and the spheres are in the middle frames (frame 11 of 20 checked; the bone sphere is on patient left). `encapsulated-pdf` lists "SYNTHETIC TEST REPORT" and double-clicking hands it to Adobe Acrobat; its text was not read, because Acrobat is outside what this session could see |
| TP-07 | Partial | `ct-reference` opened from the local database with all 100 slices. In MPR, axial slice 48 shows two round spheres, the 1000 HU one on the viewer's right (labelled L), so on patient left. The 3D viewer renders a cylinder with the correct height-to-width proportion and an orientation widget showing H and L; the L-block appears on the L side. Sphere positions in 3D and the coronal and sagittal labels were not checked in detail. `mr-orientations` and its oblique series were not opened in this run |
| TP-08 | Passed | Distance tool between the two fiducials on slice 21 of `ct-reference`: about 100.2 mm (limit 100.0 ± 0.8; the last digit was hard to read on screen). `us-multiframe` dots: 20.00 mm (limit ± 0.2). Elliptical ROI on slice 49: soft-tissue sphere mean 99.73 HU (SD 4.93), bone sphere mean about 999.5 HU (SD 5.56), both within 8. Distances and HU are labelled with units on the overlay, so the calibration is evident. Measurements were placed by mouse, so each carries about 1 screen pixel of placement error |
| TP-09 | Partial | `petct-suv` opens from the local database with PET (128x128, inverse grey) and CT side by side, with slices linked. At z = 125 mm the PET sphere sits at the same offset from the disc centre as the CT soft-tissue sphere, so they are aligned. An ROI inside the PET sphere gives SUV (bw) mean 8.00 and max 8.00; one in the background gives 1.00 and 1.00. I found no blended fusion view in the 2D viewer toolbar or Visualization menu, so that part is not checked. `rider-petct` was not tried |
| TP-11 | Partial | Create DICOMDIR from the local database for `ct-reference` on the hard disk: with a short path it wrote 102 files (100 images, DICOMDIR, README), `dcmftest` accepts the DICOMDIR and the images keep `PHANTOM^CT` and `QA-CT-001`. With "Anonymize" ticked, the copy has 102 files, patient name `Anonymous`, ID `1`, no birth date, sex, accession, institution or description, a new study UID, and a text search of every file finds no `PHANTOM`, `QA-CT-001` or accession. The first attempt, into a folder under the session's temporary directory with a path of about 250 characters, failed with "Error creating DICOMDIR... folder is empty" and wrote nothing; I did not confirm the cause, but a Windows path-length limit is plausible and worth a follow-up. Export of images and DICOM print were not tested |
| TP-10 | Partial | Settings dialog: local AE title set to `STARVIEWER` (port 4006), DIMSE PACS `ORTHANC` at `127.0.0.1:14242`. Its Test button reports the PACS is correct. An unfiltered C-FIND lists the studies. C-MOVE of `ct-reference` (1 series, 100 files) and `petct-suv` (2 series, 150 files) both show RETRIEVED. With Orthanc stopped, a retrieve gives "Unable to connect to PACS ORTHANC to retrieve images from study 1 of patient PHANTOM^CT", so an unreachable PACS is reported. The mid-transfer stop was not reached: the 150-file retrieve finished before Orthanc stopped. C-STORE and DICOMweb were not tested |

The source has a fusion view reached through the patient browser menu
(`patientbrowsermenufusionitem.cpp`), not the toolbar, so the blended view in TP-09 is untested
rather than missing. The requirements stay *Not verified*, because this is not a CI package.

After the run, `cpack -G NSIS` on the same tree gave an installer with SHA-256 starting
`359245db`. Its staged files started and closed cleanly with Qt and the SDK off PATH, with all
Qt and Visual C++ runtime modules loaded from the package folder. Qt is still installed on this
machine, and the installer itself was not run, so this does not replace TP-01.

### Baseline timings

| Date | Machine | Application and version | Data | First image (s) | All loaded (s) | Memory (MB) | MPR (s) | 3D (s) |
| --- | --- | --- | --- | ---: | ---: | ---: | ---: | ---: |
| | | | | | | | | |
