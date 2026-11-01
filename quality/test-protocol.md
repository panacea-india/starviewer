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
GPU and tester for each run. Nothing has been run yet.

| Run | Date | Platform | Commit | CI run | Steps passed | Steps failed | New anomalies |
| --- | --- | --- | --- | --- | --- | --- | --- |
| | | | | | | | |

### Baseline timings

| Date | Machine | Application and version | Data | First image (s) | All loaded (s) | Memory (MB) | MPR (s) | 3D (s) |
| --- | --- | --- | --- | ---: | ---: | ---: | ---: | ---: |
| | | | | | | | | |
