#!/usr/bin/env python3
"""Checks the generated corpus against expected.json without using the generator's code.

This guards the test data itself: if a phantom is wrong, a viewer test against it
means nothing. Values are read back from the DICOM files and recomputed.
"""

import argparse
import json
import math
import shutil
import subprocess
import sys
from pathlib import Path

import numpy as np
import pydicom

failures = []


def check(condition, message):
    print(("ok    " if condition else "FAIL  ") + message)
    if not condition:
        failures.append(message)


def close(found, expected, tolerance):
    return abs(float(found) - float(expected)) <= tolerance


def read_series(directory):
    datasets = [pydicom.dcmread(path) for path in sorted(Path(directory).glob("*.dcm"))]
    datasets.sort(key=lambda ds: float(ds.ImagePositionPatient[2]))
    return datasets


def voxel(ds, lps):
    origin = [float(v) for v in ds.ImagePositionPatient]
    spacing = [float(v) for v in ds.PixelSpacing]
    return round((lps[1] - origin[1]) / spacing[0]), round((lps[0] - origin[0]) / spacing[1])


def verify_ct(root, truth):
    series = read_series(root / "ct-reference")
    check(len(series) == truth["instances"], f"CT has {truth['instances']} slices")
    fiducials = truth["fiducials"]
    ds = next(s for s in series if s.InstanceNumber == fiducials["slice_instance_number"])
    hu = ds.pixel_array * float(ds.RescaleSlope) + float(ds.RescaleIntercept)
    row = hu[ds.Rows // 2]
    columns = np.flatnonzero(row >= 2900)
    groups = np.split(columns, np.flatnonzero(np.diff(columns) > 1) + 1)
    centers = [g.mean() for g in groups]
    # The laterality block does not reach the centre row, so only the fiducials are here.
    check(len(centers) == 2, "CT distance fiducials found")
    distance = (centers[1] - centers[0]) * float(ds.PixelSpacing[1])
    check(close(distance, fiducials["distance_mm"], 0.01), f"CT fiducial distance {distance:.3f} mm")
    positions = [float(s.ImagePositionPatient[2]) for s in series]
    check(all(close(b - a, truth["slice_spacing_mm"], 1e-6) for a, b in zip(positions, positions[1:])), "CT slice spacing is regular")
    for sphere in truth["spheres"]:
        center = sphere["center_lps_mm"]
        ds = min(series, key=lambda s: abs(float(s.ImagePositionPatient[2]) - center[2]))
        hu = ds.pixel_array * float(ds.RescaleSlope) + float(ds.RescaleIntercept)
        r, c = voxel(ds, center)
        patch = hu[r - 3:r + 4, c - 3:c + 4]
        check(close(patch.mean(), sphere["mean_hu"], 8.0), f"CT {sphere['label']} mean {patch.mean():.1f} HU")
    # Column index increases towards patient left in this orientation.
    middle = series[len(series) // 2]
    hu = middle.pixel_array - 1024.0
    high = np.argwhere(hu >= 2900)
    check(high[:, 1].mean() > middle.Columns / 2, "CT laterality block is on patient left (higher column index)")
    check(high[:, 0].mean() < middle.Rows / 2, "CT laterality block is anterior (lower row index)")


def verify_pet(root, truth):
    suv = truth["suv"]
    series = read_series(root / "petct-suv" / "pt")
    check(len(series) == 50, "PET has 50 slices")
    center = suv["sphere_center_lps_mm"]
    ds = min(series, key=lambda s: abs(float(s.ImagePositionPatient[2]) - center[2]))
    bqml = ds.pixel_array * float(ds.RescaleSlope) + float(ds.RescaleIntercept)
    drug = ds.RadiopharmaceuticalInformationSequence[0]
    decayed = float(drug.RadionuclideTotalDose) * 2.0 ** (-suv["uptake_seconds"] / float(drug.RadionuclideHalfLife))
    factor = float(ds.PatientWeight) * 1000.0 / decayed
    r, c = voxel(ds, center)
    check(close(bqml[r, c] * factor, suv["sphere_suvbw"], 0.01), f"PET sphere SUVbw {bqml[r, c] * factor:.3f}")
    check(close(bqml[ds.Rows // 2, ds.Columns // 2] * factor, suv["background_suvbw"], 0.01), "PET background SUVbw")
    ct = read_series(root / "petct-suv" / "ct")
    check(ct[0].FrameOfReferenceUID == ds.FrameOfReferenceUID, "PET and CT share a frame of reference")
    check(ct[0].StudyInstanceUID == ds.StudyInstanceUID, "PET and CT are in one study")
    check(len({s.SeriesInstanceUID for s in series}) == 1, "PET slices are in one series")
    check(len({s.SOPInstanceUID for s in series}) == len(series), "PET slices have unique instance UIDs")


def verify_other(root, cases):
    us = pydicom.dcmread(next((root / "us-multiframe").glob("*.dcm")))
    region = us.SequenceOfUltrasoundRegions[0]
    frame = us.pixel_array[0]
    white = np.flatnonzero(frame[61].min(axis=1) == 255)
    distance = (white[-1] - white[0] - 3) * float(region.PhysicalDeltaX) * 10.0
    check(close(distance, cases["us-multiframe"]["fiducials"]["distance_mm"], 0.01), f"US fiducial distance {distance:.2f} mm")
    check(int(us.NumberOfFrames) == 30, "US has 30 frames")

    xa = pydicom.dcmread(next((root / "xa-multiframe").glob("*.dcm")))
    magnification = float(xa.DistanceSourceToDetector) / float(xa.DistanceSourceToPatient)
    check(close(magnification, cases["xa-multiframe"]["magnification"], 1e-6), "XA magnification")

    mg = pydicom.dcmread(next((root / "mg-monochrome1").glob("*.dcm")))
    check(mg.PhotometricInterpretation == "MONOCHROME1", "MG is MONOCHROME1")
    pixels = mg.pixel_array
    check(pixels[640, 400] < pixels[640, 700] < pixels[0, 1000], "MG disc stored lowest, so it displays brightest")

    enhanced = pydicom.dcmread(next((root / "enhanced-ct").glob("*.dcm")))
    frames = enhanced.PerFrameFunctionalGroupsSequence
    check(len(frames) == int(enhanced.NumberOfFrames) == 20, "Enhanced CT has 20 frames with positions")
    z = [float(f.PlanePositionSequence[0].ImagePositionPatient[2]) for f in frames]
    check(all(close(b - a, 2.5, 1e-6) for a, b in zip(z, z[1:])), "Enhanced CT frame spacing")
    check(enhanced.pixel_array.shape == (20, 256, 256), "Enhanced CT pixel array shape")

    document = pydicom.dcmread(next((root / "encapsulated-pdf").glob("*.dcm")))
    check(document.EncapsulatedDocument.startswith(b"%PDF-"), "Encapsulated document is a PDF")
    check(b"SYNTHETIC TEST REPORT" in document.EncapsulatedDocument, "PDF contains the expected text")


def verify_transfer_syntaxes(root, cases):
    for key, case in cases.items():
        if not key.startswith("transfer-syntaxes/"):
            continue
        files = sorted((root / key).glob("*.dcm"))
        check(len(files) == case["instances"], f"{key}: {len(files)} files")
        syntaxes = {pydicom.dcmread(f, stop_before_pixels=True).file_meta.TransferSyntaxUID for f in files}
        check(syntaxes == {case["transfer_syntax_uid"]}, f"{key}: transfer syntax {case['transfer_syntax_uid']}")


def verify_key_objects(root, cases):
    case = cases["key-objects"]
    ct_uids = {str(ds.SOPInstanceUID): int(ds.InstanceNumber) - 1 for ds in read_series(root / "ct-reference")}
    study_uid = str(read_series(root / "ct-reference")[0].StudyInstanceUID)
    for name, truth in case["documents"].items():
        ds = pydicom.dcmread(root / "key-objects" / f"{name}.dcm")
        check(ds.Modality == "KO" and str(ds.StudyInstanceUID) == study_uid, f"{name}: a KO in the study of the reference CT")
        check(ds.ConceptNameCodeSequence[0].CodeMeaning == truth["title"], f"{name}: title {truth['title']}")
        referenced = [str(i.ReferencedSOPSequence[0].ReferencedSOPInstanceUID) for i in ds.ContentSequence if i.ValueType == "IMAGE"]
        found = sorted(ct_uids[u] for u in referenced if u in ct_uids)
        check(found == truth["images"], f"{name}: refers to CT images {truth['images']}")
        check(len(referenced) - len(found) == truth.get("missing_images", 0), f"{name}: {truth.get('missing_images', 0)} missing image(s)")


def verify_presentation_states(root, cases):
    case = cases["presentation-states"]
    ct_uids = {str(ds.SOPInstanceUID): int(ds.InstanceNumber) - 1 for ds in read_series(root / "ct-reference")}
    study_uid = str(read_series(root / "ct-reference")[0].StudyInstanceUID)
    for name, truth in case["documents"].items():
        ds = pydicom.dcmread(root / "presentation-states" / f"{name}.dcm")
        check(ds.Modality == "PR" and str(ds.StudyInstanceUID) == study_uid, f"{name}: a PR in the study of the reference CT")
        referenced = [str(i.ReferencedSOPInstanceUID) for s in ds.ReferencedSeriesSequence for i in s.ReferencedImageSequence]
        found = sorted(ct_uids[u] for u in referenced if u in ct_uids)
        check(found == truth["images"], f"{name}: refers to CT images {truth['images']}")
        check(len(referenced) - len(found) == truth.get("missing_images", 0), f"{name}: {truth.get('missing_images', 0)} missing image(s)")
    # The drawings sit on what they mark: the fiducial dots and the sphere of the reference CT
    ds = pydicom.dcmread(root / "presentation-states" / "gsps-annotations.dcm")
    ct = {int(d.InstanceNumber) - 1: d for d in read_series(root / "ct-reference")}
    line = ds.GraphicAnnotationSequence[0].GraphicObjectSequence[0].GraphicData
    pixels = ct[20].pixel_array
    check(pixels[int(line[1]), int(line[0])] == pixels[int(line[3]), int(line[2])] == max(pixels.max(), 0), "gsps line ends on the fiducial dots")
    circle = ds.GraphicAnnotationSequence[1].GraphicObjectSequence[0].GraphicData
    sphere = ct[50].pixel_array
    # Stored values are HU + 1024, with noise of 5 HU: the sphere is at about 1124 and the body around it at about 1024
    edge_inside = int(sphere[int(circle[1]), int(circle[0] + (circle[2] - circle[0]) - 3)])
    edge_outside = int(sphere[int(circle[1]), int(circle[0] + (circle[2] - circle[0]) + 3)])
    check(edge_inside > 1080 and edge_outside < 1070, "gsps circle follows the edge of the soft tissue sphere")


def verify_structured_reports(root, cases):
    case = cases["structured-reports"]
    study_uid = str(read_series(root / "ct-reference")[0].StudyInstanceUID)
    dsr2html = shutil.which("dsr2html")
    for name, truth in case["documents"].items():
        path = root / "structured-reports" / f"{name}.dcm"
        ds = pydicom.dcmread(path)
        check(ds.Modality == "SR" and str(ds.StudyInstanceUID) == study_uid, f"{name}: an SR in the study of the reference CT")
        check(ds.ConceptNameCodeSequence[0].CodeMeaning == truth["title"], f"{name}: title {truth['title']}")
        check((ds.CompletionFlag == "COMPLETE" and ds.VerificationFlag == "VERIFIED") == truth["final"], f"{name}: final is {truth['final']}")
        if dsr2html:
            result = subprocess.run([dsr2html, str(path)], capture_output=True, text=True)
            check(result.returncode == 0 and "Findings" in result.stdout, f"{name}: DCMTK renders it")
            check("<script>" not in result.stdout, f"{name}: the markup in the text is escaped")


def encapsulated_fragments(data):
    """The fragments of encapsulated pixel data, without the basic offset table (the first item)."""
    items = []
    position = 0
    while position + 8 <= len(data) and data[position:position + 4] == b"\xfe\xff\x00\xe0":
        length = int.from_bytes(data[position + 4:position + 8], "little")
        items.append(data[position + 8:position + 8 + length])
        position += 8 + length
    return items[1:]


def verify_videos(root, cases):
    for name, truth in cases["videos"]["documents"].items():
        ds = pydicom.dcmread(root / "videos" / f"{name}.dcm")
        syntax = str(ds.file_meta.TransferSyntaxUID)
        check(syntax.startswith("1.2.840.10008.1.2.4.") and 100 <= int(syntax.rsplit(".", 1)[1]) <= 108, f"{name}: a video transfer syntax")
        fragments = encapsulated_fragments(ds.PixelData)
        check(len(fragments) == 1 and 0 <= len(fragments[0]) - truth["bytes"] <= 1, f"{name}: one fragment with the {truth['bytes']} bytes")


def verify_registration_pair(root, cases):
    truth = cases["registration-pair"]
    fixed = {int(d.InstanceNumber): d for d in read_series(root / "ct-reference")}
    moved = {int(d.InstanceNumber): d for d in read_series(root / "registration-pair")}
    check(len(moved) == truth["instances"], "registration pair has 100 slices")
    first = min(fixed)
    shift = [float(m) - float(f) for m, f in zip(moved[first].ImagePositionPatient, fixed[first].ImagePositionPatient)]
    check(all(close(a, b, 1e-4) for a, b in zip(shift, truth["true_shift_mm"])), f"registration pair is shifted by {truth['true_shift_mm']} mm")
    check(str(moved[first].FrameOfReferenceUID) != str(fixed[first].FrameOfReferenceUID), "registration pair has its own frame of reference")
    check(str(moved[first].StudyInstanceUID) == str(fixed[first].StudyInstanceUID), "registration pair is in the study of the reference CT")
    sample = 50
    check(bool((moved[sample].pixel_array.astype(int) + fixed[sample].pixel_array.astype(int) == 4095).all()), "registration pair has the contrast inverted")


def verify_broken(root):
    missing = read_series(root / "broken" / "missing-slice")
    positions = [float(s.ImagePositionPatient[2]) for s in missing]
    gaps = [b - a for a, b in zip(positions, positions[1:])]
    check(len(missing) == 10 and math.isclose(max(gaps), 7.5), "Missing-slice series has a 7.5 mm gap")
    check((root / "broken" / "truncated-pixel-data" / "truncated.dcm").is_file(), "Truncated file exists")
    mismatch = pydicom.dcmread(root / "broken" / "pixel-length-mismatch" / "mismatch.dcm")
    expected_bytes = mismatch.Rows * mismatch.Columns * 2
    check(len(mismatch.PixelData) != expected_bytes, "Mismatch file has inconsistent pixel length")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", default="/lab/corpus/synthetic")
    arguments = parser.parse_args()
    root = Path(arguments.root)
    expected = json.loads((root / "expected.json").read_text())
    cases = expected["cases"]
    verify_ct(root, cases["ct-reference"])
    verify_pet(root, cases["petct-suv"])
    verify_other(root, cases)
    verify_transfer_syntaxes(root, cases)
    verify_key_objects(root, cases)
    verify_presentation_states(root, cases)
    verify_structured_reports(root, cases)
    verify_registration_pair(root, cases)
    verify_videos(root, cases)
    verify_broken(root)
    print(f"\n{'FAILED: ' + str(len(failures)) if failures else 'All checks passed'}")
    sys.exit(1 if failures else 0)


if __name__ == "__main__":
    main()
