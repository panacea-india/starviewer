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
| TP-10 | Configure the test PACS (DIMSE, then DICOMweb). Query, retrieve `ct-reference` and `petct-suv`, and send a study. Then stop Orthanc mid-retrieve. For DICOMweb, enter the login in the user and password fields, try a wrong password, and check that the password is not in the node lists, settings, database or log. Repeat against the internal PACS when available | Instance counts match the PACS, the partial retrieve is reported to the user, the wrong password is refused, and the password appears nowhere in plain text | SR-PAC-01 to SR-PAC-05 |
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
| 2 | 2026-11-01 | Windows 11 Home 10.0.22621, local build, not a clean machine | `3842fbcca` plus uncommitted phase0 fixes | None (local build) | TP-02, TP-04, TP-06, TP-08. The rest are partial; see [Run 2](#run-2) | None | A-25 to A-27 |
| 3 | 2026-11-01 | Windows 11 Home 10.0.22621, installed local build, not a clean machine | `3842fbcca` plus uncommitted phase0 fixes (the installer built after run 2) | None (local build) | TP-03, TP-09 (`petct-suv`). TP-05, TP-07 and TP-10 are partial; see [Run 3](#run-3) | None | A-28 to A-33 |
| 4 | 2026-11-01 | Windows 11 Home 10.0.22621, local build-tree executable, not a clean machine | `1821c890a` plus uncommitted `phase1` changes | None (local build) | The DICOMweb part of TP-10 (interrupted retrieve and re-retrieve). The rest of TP-10 is untested; see [Run 4](#run-4) | None | One finding, A-34; see [Run 4](#run-4) |
| 5 | 2026-11-01 | Windows 11 Home 10.0.22621, local build-tree executable, not a clean machine | `d48940883` plus uncommitted `phase1` changes, then `6d50df4b2` for the startup check | None (local build) | The DICOMweb login part of TP-10. The rest of TP-10 is untested; see [Run 5](#run-5) | None | Four findings, A-36 to A-39; see [Run 5](#run-5) |

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
Qt and Visual C++ runtime modules loaded from the package folder.

That installer was then run silently on this machine, into `C:\Program Files\Starviewer`. It
exited with 0 and added the Start menu shortcuts, the uninstall entry and the URL handler. The
installed application started with Qt and the SDK off PATH, loaded its modules from the install
folder and closed with exit code 0. The uninstaller removed the folder, shortcuts and uninstall
entry (A-26, A-27). The installer's interactive pages were not seen, and Qt is still installed on
this machine, so this does not replace TP-01.

Run 2's executable found no default hanging protocols, because a build-tree executable looks for
them in a folder that does not exist there. The installed application does load them. Run 2's
layouts, including anything in TP-07 and TP-09 that depends on a hanging protocol, may therefore
differ from what users get, and should be repeated on an installed package.

### Run 3

Run 3 used the installed application in `C:\Program Files\Starviewer`, started from the Start
menu shortcut, so it has the default hanging protocols that run 2's build-tree executable lacked.
The installer was `starviewer~2.0.0-devel~amd64-winnt.exe`, SHA-256
`359245dbea04c974b516a58cf7d23dfacf8901ad188cc841b6e42b4c99d71485`. I recomputed the hash, and
it matches the `.sha256` file beside it. It is the installer built after run 2, from `3842fbcca`
plus the uncommitted phase0 fixes, and was not rebuilt. It is a local build, not a CI package, and
the machine is not clean (Qt and the SDK are installed), so TP-01's clean-install check was not
run and the requirements stay *Not verified*. The corpus checksum was not recorded.

The installer was run interactively with the default folder, but a person clicked through it,
because Windows blocked Claude's input to the elevated installer. Its pages were therefore not
seen or checked. The uninstall entry still shows version 1.1.0 (A-26). Claude drove the
application by screenshots, clicks and typing, and used PowerShell scripts for the timings.

| Step | Result | Notes |
| --- | --- | --- |
| TP-03 | Passed | All 14 folders in `transfer-syntaxes` open and display, with no error. I compared a screenshot of each with `implicit-le`, in the same window, over an area of 729,000 pixels. `explicit-be`, `deflated`, `rle`, `jpeg-lossless`, `jpeg-lossless-sv1`, `jpeg-2000-lossless` and `jpeg-ls-lossless` each differ in 0 pixels, so all seven lossless variants match pixel for pixel. JPEG-LS now displays the phantom (A-01). The lossy ones differ slightly, as expected: `jpeg-2000-lossy` in 377,106 pixels with a largest difference of 8 grey levels, `jpeg-ls-near-lossless` in 285,068 with a largest of 2, and `jpeg-extended-12bit` in 367,775 with a largest of 10. `jpeg-baseline-8bit` (XA data), `jpeg-baseline-rgb` and `rle-rgb` (US data) are different images from `implicit-le`, so they were judged by eye only: the XA file shows a pale image like frame 1 of `xa-multiframe`, and the two RGB files show the 30-frame US grid with the moving disc. I did not look for a lossy marking at full size, so A-21 is not rechecked. The first readings for `deflated` and `rle` were spoiled by another window over the capture, and were redone |
| TP-05 | Partial | Both cines were played. In `us-multiframe` the disc advanced in equal steps with no jump and wrapped from the last position to the first at 2.30 s and 5.65 s, so 30 frames took 3.35 s. `xa-multiframe` repeated every 3.30 s; I could track its marker through only part of each cycle, so I cannot say every frame was shown. Both play at about 9 img/s, not at the file's frame time of 33.3 ms (US) and 66.7 ms (XA) (A-31). `mg-monochrome1` opens in a four-pane layout with the image in the second pane: a bright disc on a grey breast on a dark background, with an L marker at the top, so polarity is correct. `enhanced-ct` opens with 20 frames, and frame 11 shows the spheres, with the L block and the bone sphere on the viewer's right. `encapsulated-pdf` is listed as `QA-DOC-001`, and opening it wrote `DOC0000.dcm.pdf` beside the source file again (A-22) and handed it to Adobe Acrobat Reader. I was not allowed to view Acrobat, so I did not see the page. The extracted file's content stream reads "SYNTHETIC TEST REPORT" |
| TP-07 | Partial | `ct-reference` opens in MPR with all 100 slices. Axial slice 51 shows the soft-tissue sphere on the viewer's left and the bone sphere on the viewer's right, beside the L block, so the bone sphere is on patient left. Dragging in MPR rotated the reference line, and the oblique reformat shows the soft-tissue sphere round (about 81 by 77 screen pixels). The 3D viewer renders `ct-reference` as a cylinder with an orientation widget showing H and L. `mr-orientations` opens in the 2D viewer with three series in three panes and one empty pane: the coronal pane is labelled R, L, H and F, and the oblique series is labelled "Axial oblique 20 degrees", with AF at the top and PH at the bottom, R and L at the sides, and green reference lines on the other panes. The MR also opens in MPR and in 3D. I did not check the MR sphere positions against `expected.json`, and the sphere positions in 3D were not checked. 3D volume rendering of the real CT shows only a grey cylinder (A-32) |
| TP-09 | Passed for `petct-suv` | The blended fusion view exists and works in the installed package. It is not in the toolbar, the Visualization menu or the Layout popup. It is in the right-click menu of a 2D viewer pane (the patient browser menu), and appears only when a CT and a PET series share a frame of reference (A-33). Its entry is labelled "Fusion: 50% CT + 50% PT". It blends the pane that was clicked, with a CT/PT balance slider and the "PET Osirix" colour table, and it resets that pane to slice 1, where the CT shows no PET colour. In `petct-suv` the PET sphere lines up with the CT soft-tissue sphere. An elliptical ROI reads SUV (bw) mean 8.00 and max 8.00 in the sphere, and 1.00 and 1.00 in the background, so both are within 1% of 8.0 and 1.0. `rider-petct` opens in the same way, and slice 11 of 64 shows the blend. I did not judge its anatomy or alignment, and there is no ground truth |
| TP-10 | Partial | DICOMweb against the test Orthanc: see below. The DIMSE path was not repeated, and the PACS settings dialog was used only for DICOMweb |

#### DICOMweb against Orthanc

Docker Desktop was not running, so I started it, and its engine was ready after about 5 s.
`docker compose up -d` from `quality\testlab` created `starviewer-testlab-orthanc-1` (Orthanc
1.12.6, published on 127.0.0.1 ports 14242 and 18042). It already held 21 studies and 478
instances, including `ct-reference` with 100 and `petct-suv` with 150, so the corpus was not
reloaded. Orthanc's authentication and configuration were not changed.

- **Login:** the DICOMweb PACS was added in the settings dialog. With `http://127.0.0.1:18042/dicom-web/` as entered, its Test button reports "Host requires authentication". With `http://qa:qa-only@127.0.0.1:18042/dicom-web/` it reports "Test is correct", and I saved that entry. So the app has no login fields, but the login works in the URL (A-28), and then it shows in plain text in the node lists (A-29).
- **Query:** an unfiltered search lists 21 studies, which matches Orthanc, and a search on Patient ID also worked.
- **Retrieve:** `ct-reference` gave 100 instances and `petct-suv` gave 150, as the Operation List reported. On disk the study folders held 100 and 150 DICOM files, plus thumbnail PNGs (102 and 154 files in all).
- **Send:** Send to PACS on the retrieved `ct-reference` lists the DICOMweb entry, and the queue shows SENT with 100 files. The instances were already in Orthanc, so its counts could not show receipt. I therefore deleted that study from Orthanc over REST (20 studies and 378 instances left), and sent it again from Starviewer. Orthanc then held 21 studies and 478 instances, with `QA-CT-001` back as 1 series of 100 instances. The send works.
- **Interrupted retrieve:** Orthanc held no large study, so I uploaded `rider-large-ct` over REST (1,049 instances, 599 MB, 364 s). It is still in the test Orthanc, which now holds 22 studies and 1,527 instances. I started Retrieve on it, and a background script ran `docker compose stop -t 0 orthanc` once 150 new files had reached Starviewer's cache: 12:35:33.151, with 156 new files, and the container exited with code 137 (a hard stop, not a graceful one). After a spinner ("Operation in progress"), about 10 to 13 s later (read from screenshots, so approximate), a warning appeared: "Some images have been downloaded. However there have been some errors. Maybe those images are missing or corrupted in PACS", then a bold warning that 3D reconstructions and measurements may be wrong if a series is missing images, then "Details: Download ended prematurely for an unknown reason". Files kept arriving on disk after the stop, 278 when `docker` returned and 876 twenty seconds later, and the count was still 876 two minutes after that. The Local Database lists the study as "Series 3, 876 Images" with no marking, and it opens as 876 slices with no message (A-30). The expected 1,049 images are not shown anywhere. The Operation List entry was not read. The server was well ahead of the writer, so the stop did not interrupt the transfer at an early point.
- **Finish:** `docker compose stop` was run from `quality\testlab` at the end. The container was already stopped, and the network and Orthanc's stored data were left in place. Docker Desktop itself was left running.

`cool_wozniak` was not in Docker's container list at any point, and port 8042 was still held by process 16316, which I did not touch.

Not done in run 3: TP-01 on a clean machine, the installer's pages, the DIMSE path (C-MOVE and C-STORE), reading Acrobat's view of the PDF, the Operation List after the interrupted retrieve, `benchmark-ct-1000` (not generated), and RadiAnt (not requested). After Docker Desktop started, Starviewer's viewer was empty for a while, and I do not know why.

### Run 4

Run 4 repeats the interrupted DICOMweb retrieve of run 3 (A-30) on a build that has the fix for it.
It used `C:\sdk\sv-build\bin\starviewer.exe`, started with `C:\sdk\sdk-install\bin` and
`C:\Qt\5.15.2\msvc2019_64\bin` first on PATH. The executable was built at 15:55:22 on 2026-11-01,
after the last source edit (15:54:37), from `1821c890a` plus the uncommitted `phase1` changes
(`git status` listed 19 modified files and `test_localdatabasestudydal.cpp`). It is not a CI
package, the machine is not clean, and the requirements stay *Not verified*. The corpus checksum
was not recorded. Claude drove the interface by screenshots, clicks and typing.

Setup: Docker Desktop was not running, so I started it, and its engine answered at once.
`docker compose up -d` from `quality\testlab` started `starviewer-testlab-orthanc-1`, which
still held 22 studies and 1,527 instances, with `rider-large-ct` at 1,049 instances. The DICOMweb
PACS entry was already in the settings (the login is in its URL, see A-28 and A-29).

| Step | Result | Notes |
| --- | --- | --- |
| TP-10 (DICOMweb, interrupted retrieve) | Passed | The partial retrieve was reported, marked in the study list and the window title, and warned about again on opening. A second, clean retrieve of the whole study removed every mark. Details below. DIMSE, the internal PACS and the other TP-10 retrieves were not repeated |

1. **Before the test.** The Local Database still held run 3's `rider-large-ct` (patient
   `RIDER-1006487867`, study 157631) as "Series 3, 876 Images", with a plain description and no
   mark. It was stored before the fix, so this shows that old partial studies stay unmarked, as
   A-30 says. I deleted it with Delete in the study's context menu and confirmed the question; the
   list then held only `ct-reference` and `petct-suv`. A search of the DICOMweb PACS (all fields
   empty, after confirming the long-query question) listed 22 studies, with `rider-large-ct` last.
2. **The interrupted retrieve.** I started a background script that counted the files under
   `C:\Users\ayush\.starviewer\pacs\dicom` (baseline 250, from the two other studies) and ran
   `docker compose stop -t 0 orthanc` once 150 new files had arrived, then selected the study and
   clicked Retrieve. The log shows the retrieve starting at 17:18:11. The script reached its
   threshold at 17:18:14.781, with 156 new files, and `docker` returned at 17:18:16.058 with 266
   new files. The container exited with code 137. As in run 3, files kept arriving after the stop:
   799 new files 5 s later, and 922 at 10 s and again at 20 s, where it stayed. Starviewer
   logged "The connection to the PACS closed before the whole response arrived." at 17:18:22.378,
   about 6 s after the stop, and "922 instance(s) downloaded".
3. **Warning (text as shown).** A warning box titled "Starviewer", with the warning icon:
   > Only some of the images have been downloaded, because the connection to the PACS was lost.
   >
   > **Be aware that if a series is missing some images, 3D reconstructions may not be accurate and measurements on reconstructed planes may be wrong. Check the number of images in each series before using it for such purposes. Refer to the user guide for more information.**
   >
   > The study is marked as incomplete in the study lists. Retrieve it again to complete it.
   >
   > Details:The connection to the PACS closed before the whole response arrived.

   The bold paragraph is as run 3 showed it. The reason is now the real one, where run 3 said
   "unknown". There is no space after "Details:". The warning appeared about 11 s after Retrieve.
4. **List text.** In the Local Database list the patient row shows the description
   **"INCOMPLETE - Unspecified CT"** (the study's own description is "Unspecified CT"). The study
   holds one series, "Series 3", of **922 images**. The expected 1,049 are not shown anywhere,
   as the source says (A-30). The study row below the patient row has an empty description
   column, as it did before the test.
5. **Window title when the study is open.** Opened with View in the Local Database list:
   **"RIDER-1006487867 : 157631 - INCOMPLETE STUDY - Not for diagnostic use"**, with the slice
   counter at 1 of 922.
6. **Warning on opening.** A warning box titled "Starviewer", over the first image:
   > Only part of this study was retrieved, so some images are missing. 3D reconstructions may not be accurate and measurements on reconstructed planes may be wrong. Retrieve the study again to complete it.
7. **Operation List.** The interrupted job is listed as **ERROR**, direction Local, 1 series,
   **922 files**. (Run 3 did not read this entry.) The warning appeared before I opened the list.
8. **The second retrieve.** `docker compose start orthanc` brought Orthanc back with 22 studies
   and 1,527 instances, so its data survived the hard stop. I selected the study in the
   PACS results again and clicked Retrieve for the whole study. The log shows it from 17:19:27
   to 17:19:40, about 13 s, with "1049 instance(s) downloaded". There was no warning. On disk the
   study folder holds 1,049 files with 1,049 different names, plus 2 thumbnail PNGs. The
   Operation List now has a second line, **RETRIEVED**, with 1 series and **1,049 files**,
   under the ERROR line.
9. **Marks after the second retrieve.** The Local Database list shows the description as plain
   **"Unspecified CT"**, with "Series 3, **1049 Images**". Opened from a new, empty window (Ctrl+N,
   then Ctrl+L and View), the title is **"RIDER-1006487867 : 157631 - Not for diagnostic use"**,
   there is no warning, and the slice counter reads 1 of 1,049. So the list text, the title and
   the opening warning are all gone, and all 1,049 images arrived. I did not read the `State`
   column in the database itself, because no SQLite tool is installed here; the three marks
   shown to the user are the evidence.

**Finding (A-34).** I first opened the study with View in the window that was still showing the
partial study. The viewer did not reload it. The title stayed **"INCOMPLETE STUDY"**, the first
pane still read slice 1 of 922, and no warning was shown. The log shows "Merging equal patients"
at 17:20:06. It errs on the safe side, because the user still sees INCOMPLETE, but after a complete
retrieve a user who opens it in the same window sees the partial study and a warning title. Opening
in a new window gave the correct result. This is separate from the interrupted retrieve itself.

Review of this finding, from the source and not observed in the run: `Patient::operator +=` adds
only series whose UID is not already open. The new 1,049-image series has the same UID as the open
one, so it is dropped, and the window keeps the 922-image series and the old study's mark. The
panes were not scrolled to confirm this on screen.

Mistakes in the run, none of which touched the test data: asking the computer-use tool to open
"Starviewer" started the installed copy in `C:\Program Files\Starviewer` as a second instance,
because the first access grant named that path. I closed it within seconds. Its log lines show it
ran the cache cleanup ("No studies to delete") and could not bind the RIS port (11110) that the
test instance already held. The test instance's database and files were not changed by it.

Not done in run 4: the DIMSE path, the Operation List text for the interrupted job beyond its
status and file count, whether the missing 127 images (1,049 less 922) were at the end or spread
through the series, the Linux and installed-package builds, and the CI package. The test Orthanc and
Starviewer were left running.

### Run 5

Run 5 tests the DICOMweb login (A-28, A-29) against the test Orthanc at
`http://127.0.0.1:18042/dicom-web/`. Steps 1 to 10 used `C:\sdk\sv-build\bin\starviewer.exe` built
at 17:42 on 2026-11-01 from `d48940883` plus the uncommitted `phase1` changes. It is not a CI package,
the machine is not clean, and the requirements stay *Not verified*. Claude drove the interface in
Claude Desktop by screenshots, clicks and typing. I (Claude Code) then reviewed that report against the log and
registry, and ran the startup check below myself. That build differs from `6d50df4b2` in one thing:
the startup move of logins was then moved from the main window constructor to `main()`.

| Step | Result | Notes |
| --- | --- | --- |
| 1. Login moved at startup | Passed, see below | A generic credential `Starviewer/PACS/http://127.0.0.1:18042/dicom-web` exists with user `qa`. Its password was not read |
| 2. Node lists | Passed | The settings dialog and Query Studies both show `http://127.0.0.1:18042/dicom-web/`, with no login |
| 3. Editing the entry | Passed | User name `qa`; empty Password with "Saved. Leave blank to keep it."; "This address uses http, so the password is sent without encryption."; Test gave "Test is correct." |
| 4. Wrong password | Passed | "Host requires authentication" (A-37). Reset restored the placeholder |
| 5. No login | Passed | "Host requires authentication" with User name cleared. Without saving, the reopened entry still shows `qa`, so an explicit "no login" did not fall back on the saved one |
| 6. Query and retrieve | Passed | 22 studies listed. `QA-TS-implicit-le` shows RETRIEVED, 1 series, 10 files in the Operation List; Orthanc has 10 instances |
| 7. Send | Passed | The same study (Series 880, 10 images) shows SENT, 1 series, 10 files, with no error. It already existed in Orthanc |
| 8. Delete and re-add with a login in the URI | Passed | The credential went with the entry, with no confirmation question (A-38). After tabbing out, the URI lost the login, User name became `qa` and Password was masked. After OK the list had no login, the credential was back, and Test was correct |
| 9. Changed address | Passed | With the URI changed to `http://localhost:18042/dicom-web/` and Password blank, Test gave "A user name needs a password." The placeholder still said the password was saved (A-36) |
| 10. Plain text search | Passed, see below | No `qa-only` in the registry `PacsList` or `dicom.sdb`, and no new lines in the log |

**Corrections and additions from my review:**

- **The migration had already run.** The report found the login already gone at its setup step and could not
  say why. The log shows why: my own test run moved it, at 17:42:27 ("Moved the login of PACS
  http://127.0.0.1:18042/dicom-web/ to the secure store", then "Removed the login from 1 PACS addresses
  in the database"). `test_QApplicationMainWindow` creates real main windows, and the move was in their
  constructor, so a test run changed the real settings, database and Credential Manager (A-39).
  The before state is still known: earlier on the same day, before the change, I found the login in
  the registry `BaseUri` value and in `dicom.sdb`, and 11 lines of the log.
- **Log count.** The report counts 22 for `starviewer.log`. My count is 11 lines and 11 occurrences,
  the last at 17:19:27, before the 17:42 build. I did not find out why the report's count is double. Both
  counts stayed the same through the run, and both show that this build added none.
- **Startup check after moving the call to `main()`.** I wrote the login back into the registry value,
  rebuilt, and ran all tests: 117 suites, 4,822 passed, none failed or skipped. `test_PacsDevice` is covered
  only by `ctest`'s exit code, as its name is part of another suite's. The login was still in the
  registry afterwards, so tests no longer move it. Starting the executable once, without touching its
  interface, then logged the move at 20:08:52 and left the registry value without it, the credential with user
  `qa`, and the log at 11 lines. The database step could not be repeated, as there is no SQLite tool
  on this machine to put a login back; its code did not change, and `dicom.sdb` still has none.

Not done in run 5: steps 1 to 10 on the final build, WADO-URI and WADO-URI + DIMSE with a login, the
same-server redirect limit, HTTPS (A-03), DIMSE, the Linux and installed-package builds, and the CI package.
The DICOMweb entry is left configured with its login in Credential Manager, and the local
database now also holds `QA-TS-implicit-le`.

### Run 6

Run 6 tests C-GET, DICOM TLS and HTTPS with a CA file per PACS against the two test PACS of
`quality/testlab` (plain at 127.0.0.1, TLS at localhost). It used `C:\sdk\sv-build\bin\starviewer.exe`
built on 2026-11-02 from `b993ed31e` plus the uncommitted `phase1` changes, with OpenSSL 1.1.1w, started
by `C:\sdk\run-starviewer.bat`. Claude drove the interface in Claude Desktop, and I (Claude Code) checked
the report against the code. It is not a CI package, and the requirements stay *Not verified*.

| Step | Result | Notes |
| --- | --- | --- |
| 1. Dialog fields | Failed | The fields show and enable as designed for each PACS type, and TLS ticks and locks C-GET. Unticking TLS left C-GET ticked although it was never chosen. Fixed: the box now keeps the user's own choice |
| 2. C-GET | Not done | The test was wrong: it added a second entry with the AE title, address and port of the existing one, which Starviewer refuses (`This PACS already exists.`) |
| 3. Cancel a C-GET | Not done | Needs step 2 |
| 4. C-MOVE | Not done | |
| 5. DICOM TLS | Passed | Test, search, retrieve and view of QA-CT-001 over TLS |
| 6. TLS failures | Passed | A wrong CA, an address the certificate does not name, and no client certificate all fail Test with the TLS hint. A certificate without its key, and TLS without a CA, are refused on OK |
| 7. HTTPS DICOMweb | Failed | With the CA, search, retrieve and view worked (100 files). With the CA then cleared, search and Test still succeeded (A-45) |
| 8. Clean up | Passed | |

A-45 was reproduced by `test_EchoToWadoPacs` against the TLS test PACS before the fix and passes
after it. All suites passed on that build (4,834 tests, 4 lab tests skipped). Steps 1 to 4 and 7 were run again in run 7.

### Run 7

Run 7 repeats the steps of run 6 that failed or were not done, on a build with the fixes for A-45 and
the C-GET box, in the same way as run 6. I checked the protocol of each retrieve and the cancel in
the log.

| Step | Result | Notes |
| --- | --- | --- |
| 1. C-GET box | Passed | Unticking TLS leaves C-GET as the user chose it, ticked or not |
| 2. C-GET | Passed | Test passed and C-GET stayed saved. QA-CT-001 retrieved, 100 files, and displayed. The log shows no incoming connection, so it was C-GET |
| 3. Cancel a C-GET | Passed | Cancelled at 978 of 1049 files, and Starviewer stayed responsive. The study was already complete in the local database from an earlier run, so it was kept, as designed. Cancelling a study that is not yet local was not exercised |
| 4. C-MOVE | Passed | QA-US-001 retrieved, 1 file. The log shows Orthanc's incoming connection on port 4006, so it was C-MOVE |
| 5. HTTPS trust | Passed | In one session: no CA failed, the CA worked (QA-BRK-001 retrieved and displayed), the CA cleared failed again, and Test with no CA failed. Each failure said only `SSL handshake failed` (A-46) |
| 6. Clean up | Passed | The ORTHANC entry is as before. The local database keeps the retrieved studies |

Closing the viewer window ended Starviewer, which is its design: that window is the main one. Deleting a
PACS entry asks for no confirmation (A-47).

### Run 8

Run 8 tests the lossy marker, English only, the PACS login messages, the password placeholder,
the delete confirmation and the log after failed and cancelled connections, on a build from
`ee541d337` plus the changes later committed for A-09, A-20, A-21, A-36 to A-38 and A-44. It ran in
the same way as run 6, in two sessions: the first stopped when Edit on the ORTHANC entry asked to
delete it (A-48), and the second ran on a build with that fixed. I checked the log and a copy of
the local database afterwards. All suites passed on that build (4,871 tests, 6 lab tests skipped
without the lab variables; with them, the 7 tests of `test_EchoToWadoPacs` passed).

| Step | Result | Notes |
| --- | --- | --- |
| 1. Start-up and language | Passed | No Language entry, and the menus and settings are in English. The log viewer shows Catalan messages (A-50). The database upgrade to revision 9598 is in the log; the autotests made it, as they share the user's local database |
| 2. Lossy marker | Partly passed | "LOSSY 10.1:1" for JPEG 2000, also while scrolling and in sagittal and coronal views; "LOSSY 52.7:1" for JPEG baseline; "LOSSY 6.8:1" for JPEG-LS near-lossless; nothing for JPEG lossless, JPEG 2000 lossless, RLE and implicit little endian. The decompressed file that keeps Lossy Image Compression 01 was loaded, but the overlay was too small to read. The check after a restart was not possible, as a folder opened from disk is not added to the local database |
| 3. Missing login | Passed | "The PACS requires a login. Enter a user name and password in the PACS settings." |
| 4. Wrong password | Passed | "The PACS refused the login. Check the user name and password in the PACS settings." The right password then passed Test |
| 5. Password placeholder | Passed | Shown for the saved login; gone with another base URI or user name, and back when they match again |
| 6. Delete confirmation | Passed | "Delete the PACS https://localhost:18043/dicom-web/? Its saved login is deleted too, unless another PACS uses the same address." No kept the entry and Yes deleted it |
| 7. Failed connection log | Passed | The query error ran two sentences together (A-51). The log shows the connection error and no NULL key error. Delete asked "Delete the PACS NOBODY?" with no mention of a login |
| 8. Cancelled C-GET | Not done as designed | The C-GET of 1049 files ended about 0.2 s before the cancel, during the save to the local database. The list said CANCELLED, but the study was stored (A-49). No DUL error in the log, but no cancel during a transfer either |
| 9. Clean up | Passed | The ORTHANC entry is as before. The local database keeps the RIDER study and the synthetic QA-PT-001, opened by mistake |

The tester's file dialog allowed only one file to be chosen, so each folder opened its first file.
That was enough for the overlay checks. The decompressed file, the marker after reloading from the
local database, a cancel during a transfer and the A-51 message go to run 9.

### Baseline timings

| Date | Machine | Application and version | Data | First image (s) | All loaded (s) | Memory (MB) | MPR (s) | 3D (s) |
| --- | --- | --- | --- | ---: | ---: | ---: | ---: | ---: |
| 2026-11-01 | AMD Ryzen 7 5825U (8 cores, 16 threads), 15.4 GB RAM, AMD Radeon integrated graphics (driver 31.0.21924.1004), 512 GB NVMe SSD (SK hynix BC711), Windows 11 Home 10.0.22621, on AC power | Starviewer 2.0.0-devel, installed local build (installer SHA-256 `359245db`) | `rider-large-ct`, 1,049 slices, 530 MB, File > Open Files from a Directory | 3.79 | 3.79 | 1,104 | 0.40 | 1.96 |

Each figure is the median of three runs, restarting the application from the Start menu shortcut
before each run. Before each run the system was idle: total CPU was 0.2 to 0.6% (a 5 s average),
and 6.8 to 7.0 GB of RAM was free. Docker Desktop and browsers were closed; the Claude app, Tabby,
opencode and Dell background agents were open but idle, and the Claude window was minimized.

| Measure | Run 1 | Run 2 | Run 3 | Median |
| --- | ---: | ---: | ---: | ---: |
| `rider-large-ct`: first image, and all slices loaded (s) | 3.81 | 3.72 | 3.79 | 3.79 |
| Same, to the process being idle (CPU under 0.05 core for 4 s) (s) | 4.10 | 4.11 | 4.16 | 4.11 |
| Private bytes after loading (MB) | 1,106 | 1,103 | 1,104.1 | 1,104 |
| MPR open, to the first screen change (s) | 0.36 | 0.41 | 0.40 | 0.40 |
| MPR open, to the process being idle (s) | 0.79 | 0.76 | 0.78 | 0.78 |
| Private bytes after MPR (MB) | 1,467 | 1,464 | 1,465 | 1,465 |
| 3D volume rendering open, to the screen change (s) | 1.76 | 2.16 | 1.96 | 1.96 |
| 3D open, to the process being idle (s) | 1.80 | 2.03 | 2.30 | 2.03 |
| Private bytes after 3D (MB) | 2,754 | 2,751 | 2,751 | 2,751 |
| `rider-petct`: first image, and all loaded (s) | 1.30 | 1.33 | 1.34 | 1.33 |
| `rider-petct`: fusion open, to the last screen change (s) | 0.19 | 0.24 | 0.24 | 0.24 |
| Fusion open, to the process being idle (s) | 0.54 | 0.53 | 0.52 | 0.53 |
| Private bytes after fusion (MB) | 764 | 762 | 762 | 762 |
| `benchmark-ct-1000` | Not run: the file was not generated | | | |

How these were timed:

- A background PowerShell sampler captured the Starviewer window at about 12 Hz (median interval 0.082 s) and logged each sample where a 64 by 40 grey thumbnail changed. The process's CPU and private bytes (`PrivateMemorySize64`) were logged with each sample. Zero is the click on Select Folder in the folder dialog, or the moment a script invoked the MPR, 3D or fusion menu item. The **timing resolution is therefore about 0.08 s**, plus the delay of the script's own call, which was not measured. The "idle" figures are checked every 0.25 s.
- **The first image only appears once everything has loaded.** About 1.8 s after the click, the viewer shows its layout with empty panes. The images appear at 3.7 to 3.8 s, with a single screen change, when the process's private bytes have reached the loaded figure. So the first-image and all-loaded figures are the same measurement. I have no separate figure for the first slice appearing before the rest.
- The three counted runs read the files from a warm file cache. An earlier run that was not counted, after the data had first been read, reached idle at 12.15 s, so a first read of 530 MB from disk is slower than these figures.
- MPR and 3D were opened by a script, through the Visualization menu, and not by a click. Each produced a single screen change, so I have no figure for how long the content takes to settle.
- **Smoothness:** scroll and rotation were driven by a script and sampled at 12 Hz, so I could not see frame rates above about 12 per second, and the sampler shows stalls, not frame rate. Scrolling 100 wheel notches over 10 s moved the axial view to slice 101 of 1,049, with a screen change in 100 of 124 samples, a longest gap between changes of 0.18 s, and a peak process CPU of 0.77 core. Dragging the 3D view with 376 mouse moves over 6 s gave a screen change in 65 of 67 samples, with a longest gap of 0.18 s and a peak CPU of 0.58 core. I saw no stall longer than 0.18 s, but this is not a frame-rate figure, and what was rotating was the featureless grey cylinder of A-32.
- Private bytes after the 3D viewer opens (2.75 GB) are about five times the size of the data on disk (530 MB). I did not look into why.
