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
| 2 | 2026-11-01 | Windows 11 Home 10.0.22621, local build, not a clean machine | `3842fbcca` plus uncommitted phase0 fixes | None (local build) | TP-02, TP-04, TP-06, TP-08. The rest are partial; see [Run 2](#run-2) | None | A-25 to A-27 |
| 3 | 2026-11-01 | Windows 11 Home 10.0.22621, installed local build, not a clean machine | `3842fbcca` plus uncommitted phase0 fixes (the installer built after run 2) | None (local build) | TP-03, TP-09 (`petct-suv`). TP-05, TP-07 and TP-10 are partial; see [Run 3](#run-3) | None | A-28 to A-33 |

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
