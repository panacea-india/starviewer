# Software requirements

Status: **draft.** These describe the behaviour internal users need from the existing
software, as a baseline. Nobody from the intended users has reviewed them. New features add
requirements here as they are built.

"Verification" points to [test-protocol.md](test-protocol.md) (`TP-`) and the
[corpus](testlab/README.md). "Status" stays *Not verified* until a recorded run passes on a
package built by CI.

## Intended use (provisional)

Internal review of DICOM studies by staff, on Windows and Linux, not for diagnosis or
treatment decisions. See the open decision in the
[roadmap](../ROADMAP.md#decisions-still-needed).

## Labelling

| ID | Requirement | Hazards | Verification | Status |
| --- | --- | --- | --- | --- |
| SR-LBL-01 | Non-CE builds show "Not for diagnostic use" in the main window title and About dialog. | H-10 | TP-01 | Not verified |

## Loading and decoding

| ID | Requirement | Hazards | Verification | Status |
| --- | --- | --- | --- | --- |
| SR-DEC-01 | Displays images in each transfer syntax of `transfer-syntaxes/`, with pixels matching the uncompressed source for lossless syntaxes. | H-05 | TP-03 | Not verified |
| SR-DEC-02 | Reports an error for a truncated file, a file with the wrong pixel length, or a file that is not DICOM, without crashing and without showing a partial image as complete. | H-05 | TP-04 | Not verified |
| SR-DEC-03 | Does not present a series with missing slices as continuous anatomy without warning. | H-05, H-04 | TP-04 | Not verified |
| SR-DEC-04 | Displays MONOCHROME1 images with the correct polarity. | H-06 | TP-05 | Not verified |
| SR-DEC-05 | Displays multi-frame images with every frame, in order. | H-05 | TP-05 | Not verified |
| SR-DEC-06 | Reports an object it cannot display, such as an unsupported type, rather than omitting it silently. | H-05 | TP-03, TP-05 | Not verified |

## Identification and orientation

| ID | Requirement | Hazards | Verification | Status |
| --- | --- | --- | --- | --- |
| SR-PAT-01 | Shows the patient's name and ID, and the study date, with the images. | H-01 | TP-02 | Not verified |
| SR-PAT-02 | Never mixes images of different patients in one view without warning. | H-01 | TP-06 | Not verified |
| SR-ORI-01 | Shows correct orientation labels in axial, sagittal, coronal and oblique views, and in MPR. | H-02 | TP-02, TP-07 | Not verified |
| SR-ORI-02 | Displays patient left on the viewer's right in a standard axial view of the reference CT. | H-02 | TP-02 | Not verified |
| SR-ORI-03 | Shows mammography laterality from the file. | H-02 | TP-05 | Not verified |

## Measurement

| ID | Requirement | Hazards | Verification | Status |
| --- | --- | --- | --- | --- |
| SR-MEA-01 | Measures distance in a CT to within one pixel of the true value (100.0 mm in the reference CT). | H-03 | TP-08 | Not verified |
| SR-MEA-02 | Measures distance using ultrasound region calibration (20.0 mm in the ultrasound case). | H-03 | TP-08 | Not verified |
| SR-MEA-03 | Reports HU values within 8 HU of the phantom's mean (100 and 1000 HU). | H-03 | TP-08 | Not verified |
| SR-MEA-04 | Reports SUVbw within 1% of the phantom's value (8.0 and 1.0). | H-03 | TP-09 | Not verified |
| SR-MEA-05 | Makes clear which calibration a measurement used, or that none was available. | H-03 | TP-08 | Not verified |

## MPR, 3D and fusion

| ID | Requirement | Hazards | Verification | Status |
| --- | --- | --- | --- | --- |
| SR-3D-01 | MPR and 3D views keep the phantom's geometry: spheres stay spherical and at their true positions. | H-02, H-03 | TP-07 | Not verified |
| SR-3D-02 | PET and CT in one frame of reference are shown aligned. | H-07 | TP-09 | Not verified |

## PACS

| ID | Requirement | Hazards | Verification | Status |
| --- | --- | --- | --- | --- |
| SR-PAC-01 | Queries by C-FIND and retrieves by C-MOVE, with every instance arriving. | H-04 | TP-10 | Not verified |
| SR-PAC-02 | Queries and retrieves through DICOMweb, with every instance arriving. | H-04 | TP-10 | Not verified |
| SR-PAC-03 | Sends a study by C-STORE. | H-04 | TP-10 | Not verified |
| SR-PAC-04 | Reports a failed or partial transfer to the user. | H-04 | TP-10 | Not verified |

## Export

| ID | Requirement | Hazards | Verification | Status |
| --- | --- | --- | --- | --- |
| SR-EXP-01 | Exports images to files and creates a DICOMDIR. | H-08 | TP-11 | Not verified |
| SR-EXP-02 | Removes the identifying tags when anonymised DICOMDIR creation is selected. | H-09 | TP-11 | Not verified |

## Installation

| ID | Requirement | Hazards | Verification | Status |
| --- | --- | --- | --- | --- |
| SR-INS-01 | The `.deb`, AppImage and NSIS packages install on a clean machine, and the application starts with no other software added. | | TP-01 | Not verified |
