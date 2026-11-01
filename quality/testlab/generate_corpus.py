#!/usr/bin/env python3
"""Generates the synthetic test corpus and records its ground truth.

Every value that a viewer should report (distances, HU, SUV, laterality) is known
exactly, so a test can compare what Starviewer shows with expected.json. The data
contains no patient information and is deterministic: the same tools produce the
same pixel data and UIDs.
"""

import argparse
import hashlib
import json
import math
import shutil
import subprocess
import tempfile
from pathlib import Path

import numpy as np
import pydicom
from pydicom.dataset import Dataset, FileMetaDataset
from pydicom.sequence import Sequence
from pydicom.uid import ExplicitVRLittleEndian

CT_STORAGE = "1.2.840.10008.5.1.4.1.1.2"
ENHANCED_CT_STORAGE = "1.2.840.10008.5.1.4.1.1.2.1"
MR_STORAGE = "1.2.840.10008.5.1.4.1.1.4"
PT_STORAGE = "1.2.840.10008.5.1.4.1.1.128"
US_MULTIFRAME_STORAGE = "1.2.840.10008.5.1.4.1.1.3.1"
XA_STORAGE = "1.2.840.10008.5.1.4.1.1.12.1"
MG_PRESENTATION_STORAGE = "1.2.840.10008.5.1.4.1.1.1.2"
ENCAPSULATED_PDF_STORAGE = "1.2.840.10008.5.1.4.1.1.104.1"

STUDY_DATE = "20260101"
INJECTION_TIME = "090000"
ACQUISITION_TIME = "100000"
F18_HALF_LIFE_SECONDS = 6586.2
DOSE_BQ = 370e6
WEIGHT_KG = 70.0
UPTAKE_SECONDS = 3600.0

# Phantom geometry in patient coordinates (LPS, mm). +x is patient left, +y is
# posterior and +z is superior.
BODY_RADIUS = 90.0
SOFT_SPHERE = {"center": (-40.0, -20.0, 125.0), "radius": 20.0, "hu": 100.0, "suv": 8.0}
BONE_SPHERE = {"center": (40.0, -20.0, 125.0), "radius": 10.0, "hu": 1000.0}
BACKGROUND_SUV = 1.0
MARKER_HU = 3000.0


def uid(*parts):
    # A 2.25 UID holds a decimal 128-bit value, here taken from a hash so that it is
    # deterministic. pydicom's generate_uid(prefix=None) ignores its entropy sources.
    digest = hashlib.sha256("/".join(str(p) for p in parts).encode()).hexdigest()
    return f"2.25.{int(digest[:32], 16)}"


class Writer:
    def __init__(self, root):
        self.root = Path(root)
        self.cases = {}

    def base(self, case, series, instance, sop_class, modality, patient_name, patient_id, description):
        ds = Dataset()
        ds.SpecificCharacterSet = "ISO_IR 100"
        ds.SOPClassUID = sop_class
        ds.SOPInstanceUID = uid(case, series, instance)
        ds.StudyDate = STUDY_DATE
        ds.SeriesDate = STUDY_DATE
        ds.ContentDate = STUDY_DATE
        ds.StudyTime = "080000"
        ds.SeriesTime = ACQUISITION_TIME
        ds.ContentTime = ACQUISITION_TIME
        ds.AccessionNumber = f"QA{int(hashlib.sha256(case.encode()).hexdigest(), 16) % 10**10:010d}"
        ds.Modality = modality
        ds.Manufacturer = "Starviewer test lab"
        ds.InstitutionName = "SYNTHETIC"
        ds.ReferringPhysicianName = ""
        ds.StudyDescription = f"SYNTHETIC {case}"
        ds.SeriesDescription = description
        ds.PatientName = patient_name
        ds.PatientID = patient_id
        ds.PatientBirthDate = "19700101"
        ds.PatientSex = "O"
        ds.StudyInstanceUID = uid(case, "study")
        ds.SeriesInstanceUID = uid(case, series, "series")
        ds.StudyID = "1"
        ds.SeriesNumber = abs(hash_int(series)) % 900 + 1
        ds.InstanceNumber = instance + 1
        return ds

    def save(self, ds, directory, name):
        directory = self.root / directory
        directory.mkdir(parents=True, exist_ok=True)
        meta = FileMetaDataset()
        meta.MediaStorageSOPClassUID = ds.SOPClassUID
        meta.MediaStorageSOPInstanceUID = ds.SOPInstanceUID
        meta.TransferSyntaxUID = ExplicitVRLittleEndian
        meta.ImplementationClassUID = "1.2.826.0.1.3680043.8.498.1"
        ds.file_meta = meta
        ds.is_little_endian = True
        ds.is_implicit_VR = False
        path = directory / name
        ds.save_as(path, write_like_original=False)
        return path


def hash_int(text):
    return int(hashlib.sha256(text.encode()).hexdigest()[:8], 16)


def set_pixels(ds, array, bits_stored, signed=False):
    ds.Rows, ds.Columns = array.shape[-2:] if array.ndim <= 3 and ds.get("SamplesPerPixel", 1) == 1 else array.shape[-3:-1]
    bits_allocated = array.dtype.itemsize * 8
    ds.BitsAllocated = bits_allocated
    ds.BitsStored = bits_stored
    ds.HighBit = bits_stored - 1
    ds.PixelRepresentation = 1 if signed else 0
    ds.PixelData = array.tobytes()


def monochrome(ds, photometric="MONOCHROME2"):
    ds.SamplesPerPixel = 1
    ds.PhotometricInterpretation = photometric


def axial_plane(ds, origin, spacing, thickness, position_z, frame_of_reference):
    ds.ImageOrientationPatient = [1, 0, 0, 0, 1, 0]
    ds.ImagePositionPatient = [origin[0], origin[1], position_z]
    ds.PixelSpacing = [spacing, spacing]
    ds.SliceThickness = thickness
    ds.SliceLocation = position_z
    ds.FrameOfReferenceUID = frame_of_reference
    ds.PositionReferenceIndicator = ""


def in_sphere(x, y, z, sphere):
    cx, cy, cz = sphere["center"]
    return (x - cx) ** 2 + (y - cy) ** 2 + (z - cz) ** 2 <= sphere["radius"] ** 2


def ct_slice_hu(columns, rows, spacing, origin, z, slice_index, noise_rng):
    xs = origin[0] + np.arange(columns) * spacing
    ys = origin[1] + np.arange(rows) * spacing
    x, y = np.meshgrid(xs, ys)
    hu = np.full((rows, columns), -1000.0)
    body = x ** 2 + y ** 2 <= BODY_RADIUS ** 2
    hu[body] = 0.0
    hu[in_sphere(x, y, z, SOFT_SPHERE)] = SOFT_SPHERE["hu"]
    hu[in_sphere(x, y, z, BONE_SPHERE)] = BONE_SPHERE["hu"]
    if noise_rng is not None:
        hu[body] += noise_rng.normal(0.0, 5.0, hu.shape)[body]
    # An L-shaped block on the patient's left and anterior side, on every slice.
    # It lets a tester check left/right and anterior/posterior at a glance.
    left = (x >= 55) & (x <= 75) & (y >= -75) & (y <= -35)
    foot = (x >= 55) & (x <= 85) & (y >= -40) & (y <= -35)
    hu[left & (x <= 60)] = MARKER_HU
    hu[foot] = MARKER_HU
    return hu


def add_distance_fiducials(hu, slice_index, fiducial_slice, row, columns):
    if slice_index == fiducial_slice:
        for column in columns:
            hu[row - 1:row + 2, column - 1:column + 2] = MARKER_HU


def write_ct_series(writer, directory, case, series, patient_name, patient_id, description,
                    size=256, slices=100, spacing=0.8, thickness=2.5, frame_of_reference=None,
                    noise=True, first_slice=0, skip_slices=()):
    origin = (-(size - 1) * spacing / 2.0, -(size - 1) * spacing / 2.0)
    frame_of_reference = frame_of_reference or uid(case, "frame")
    rng = np.random.default_rng(hash_int(case + series)) if noise else None
    paths = []
    for i in range(slices):
        index = first_slice + i
        z = index * thickness
        hu = ct_slice_hu(size, size, spacing, origin, z, index, rng)
        add_distance_fiducials(hu, index, 20, size // 2, (size // 2 - 62, size // 2 + 63))
        if index in skip_slices:
            continue
        ds = writer.base(case, series, index, CT_STORAGE, "CT", patient_name, patient_id, description)
        ds.ImageType = ["ORIGINAL", "PRIMARY", "AXIAL"]
        ds.KVP = 120
        monochrome(ds)
        axial_plane(ds, origin, spacing, thickness, z, frame_of_reference)
        ds.RescaleIntercept = -1024
        ds.RescaleSlope = 1
        ds.RescaleType = "HU"
        ds.WindowCenter = 40
        ds.WindowWidth = 400
        stored = np.clip(np.rint(hu + 1024), 0, 4095).astype(np.uint16)
        set_pixels(ds, stored, 12)
        paths.append(writer.save(ds, directory, f"CT{index:04d}.dcm"))
    return paths


def ct_truth(size=256, spacing=0.8, thickness=2.5):
    return {
        "pixel_spacing_mm": [spacing, spacing],
        "slice_spacing_mm": thickness,
        "fiducials": {
            "slice_instance_number": 21,
            "distance_mm": 125 * spacing,
            "orientation": "left-right",
        },
        "spheres": [
            {
                "label": "soft tissue, on patient right",
                "center_lps_mm": SOFT_SPHERE["center"],
                "radius_mm": SOFT_SPHERE["radius"],
                "mean_hu": SOFT_SPHERE["hu"],
                "volume_ml": 4.0 / 3.0 * math.pi * SOFT_SPHERE["radius"] ** 3 / 1000.0,
            },
            {
                "label": "bone, on patient left",
                "center_lps_mm": BONE_SPHERE["center"],
                "radius_mm": BONE_SPHERE["radius"],
                "mean_hu": BONE_SPHERE["hu"],
            },
        ],
        "laterality_marker": "L-shaped block on the patient's left and anterior side, shown on the viewer's right in a standard axial view",
    }


def generate_ct(writer):
    case = "ct-reference"
    write_ct_series(writer, case, case, "ct", "PHANTOM^CT", "QA-CT-001", "Reference CT")
    writer.cases[case] = {"modality": "CT", "instances": 100, **ct_truth()}


def generate_ct_large(writer):
    case = "benchmark-ct-1000"
    write_ct_series(writer, case, case, "ct", "BENCHMARK^CT", "QA-BENCH-001", "Benchmark CT 512x512x1000",
                    size=512, slices=1000, spacing=0.4, thickness=0.25, noise=True)
    writer.cases[case] = {
        "modality": "CT",
        "instances": 1000,
        "purpose": "Load, scroll, MPR and 3D timing. Its slices only reach z=250 mm, so the spheres are present.",
    }


def write_mr(writer, case, series, description, row_cosines, column_cosines, slices=40, size=256, spacing=1.0, thickness=4.0):
    # Deliberately the same for every series of the case.
    frame_of_reference = uid(case, "frame")
    row = np.array(row_cosines, dtype=float)
    column = np.array(column_cosines, dtype=float)
    normal = np.cross(row, column)
    center = np.array([0.0, 0.0, 125.0])
    for i in range(slices):
        offset = (i - (slices - 1) / 2.0) * thickness
        top_left = center + normal * offset - row * (size - 1) * spacing / 2.0 - column * (size - 1) * spacing / 2.0
        c, r = np.meshgrid(np.arange(size) * spacing, np.arange(size) * spacing)
        x = top_left[0] + c * row[0] + r * column[0]
        y = top_left[1] + c * row[1] + r * column[1]
        z = top_left[2] + c * row[2] + r * column[2]
        image = np.zeros((size, size), dtype=np.uint16)
        image[(x ** 2 + y ** 2 <= BODY_RADIUS ** 2) & (np.abs(z - 125.0) <= 110.0)] = 500
        image[in_sphere(x, y, z, SOFT_SPHERE)] = 1200
        image[in_sphere(x, y, z, BONE_SPHERE)] = 100
        ds = writer.base(case, series, i, MR_STORAGE, "MR", "PHANTOM^MR", "QA-MR-001", description)
        ds.ImageType = ["ORIGINAL", "PRIMARY", "M"]
        ds.ScanningSequence = "SE"
        ds.SequenceVariant = "NONE"
        ds.ScanOptions = ""
        ds.MRAcquisitionType = "2D"
        ds.EchoTime = 15
        ds.RepetitionTime = 500
        ds.MagneticFieldStrength = 1.5
        monochrome(ds)
        ds.ImageOrientationPatient = list(row_cosines) + list(column_cosines)
        ds.ImagePositionPatient = [float(v) for v in top_left]
        ds.PixelSpacing = [spacing, spacing]
        ds.SliceThickness = thickness
        ds.SpacingBetweenSlices = thickness
        ds.SliceLocation = float(offset)
        ds.FrameOfReferenceUID = frame_of_reference
        ds.PositionReferenceIndicator = ""
        ds.WindowCenter = 600
        ds.WindowWidth = 1200
        set_pixels(ds, image, 12)
        writer.save(ds, f"{case}/{series}", f"MR{i:04d}.dcm")


def generate_mr(writer):
    case = "mr-orientations"
    write_mr(writer, case, "sagittal", "Sagittal", (0, 1, 0), (0, 0, -1))
    write_mr(writer, case, "coronal", "Coronal", (1, 0, 0), (0, 0, -1))
    angle = math.radians(20)
    write_mr(writer, case, "oblique", "Axial oblique 20 degrees", (1, 0, 0), (0, math.cos(angle), math.sin(angle)))
    writer.cases[case] = {
        "modality": "MR",
        "series": ["sagittal", "coronal", "oblique"],
        "instances": 120,
        "purpose": "Orientation labels, reference lines and 3D cursor across planes. All three series share one frame of reference.",
    }


def generate_pet_ct(writer):
    case = "petct-suv"
    frame_of_reference = uid(case, "shared-frame")
    write_ct_series(writer, case + "/ct", case, "ct", "PHANTOM^PETCT", "QA-PT-001", "CT for attenuation correction",
                    frame_of_reference=frame_of_reference)
    decayed_dose = DOSE_BQ * 2.0 ** (-UPTAKE_SECONDS / F18_HALF_LIFE_SECONDS)
    weight_grams = WEIGHT_KG * 1000.0
    suv_to_bqml = decayed_dose / weight_grams
    size, slices, spacing, thickness = 128, 50, 4.0, 5.0
    origin = (-(size - 1) * spacing / 2.0, -(size - 1) * spacing / 2.0)
    slope = SOFT_SPHERE["suv"] * suv_to_bqml / 60000.0
    xs = origin[0] + np.arange(size) * spacing
    ys = origin[1] + np.arange(size) * spacing
    x, y = np.meshgrid(xs, ys)
    for i in range(slices):
        z = i * thickness
        suv = np.zeros((size, size))
        suv[x ** 2 + y ** 2 <= BODY_RADIUS ** 2] = BACKGROUND_SUV
        suv[in_sphere(x, y, z, SOFT_SPHERE)] = SOFT_SPHERE["suv"]
        stored = np.rint(suv * suv_to_bqml / slope).astype(np.uint16)
        ds = writer.base(case, "pt", i, PT_STORAGE, "PT", "PHANTOM^PETCT", "QA-PT-001", "PET static FDG")
        ds.ImageType = ["ORIGINAL", "PRIMARY"]
        ds.SeriesType = ["STATIC", "IMAGE"]
        ds.Units = "BQML"
        ds.CountsSource = "EMISSION"
        ds.CorrectedImage = ["DECY", "ATTN", "SCAT"]
        ds.DecayCorrection = "START"
        ds.AcquisitionDate = STUDY_DATE
        ds.AcquisitionTime = ACQUISITION_TIME
        ds.PatientWeight = WEIGHT_KG
        ds.PatientSize = 1.75
        ds.NumberOfSlices = slices
        ds.ImageIndex = i + 1
        drug = Dataset()
        drug.RadiopharmaceuticalStartTime = INJECTION_TIME
        drug.RadionuclideTotalDose = DOSE_BQ
        drug.RadionuclideHalfLife = F18_HALF_LIFE_SECONDS
        drug.RadionuclidePositronFraction = 0.97
        ds.RadiopharmaceuticalInformationSequence = Sequence([drug])
        monochrome(ds)
        axial_plane(ds, origin, spacing, thickness, z, frame_of_reference)
        ds.RescaleIntercept = 0
        ds.RescaleSlope = f"{slope:.10g}"[:16]
        ds.RescaleType = "BQML"
        set_pixels(ds, stored, 16)
        writer.save(ds, case + "/pt", f"PT{i:04d}.dcm")
    writer.cases[case] = {
        "modalities": ["CT", "PT"],
        "instances": 150,
        "purpose": "PET-CT fusion alignment and SUV. CT and PET share a frame of reference.",
        "suv": {
            "formula": "body weight, decay corrected to series time",
            "sphere_suvbw": SOFT_SPHERE["suv"],
            "background_suvbw": BACKGROUND_SUV,
            "sphere_center_lps_mm": SOFT_SPHERE["center"],
            "sphere_radius_mm": SOFT_SPHERE["radius"],
            "weight_kg": WEIGHT_KG,
            "dose_bq": DOSE_BQ,
            "uptake_seconds": UPTAKE_SECONDS,
            "half_life_seconds": F18_HALF_LIFE_SECONDS,
        },
        "ct": ct_truth(),
    }


def generate_us(writer):
    case = "us-multiframe"
    rows, columns, frames = 240, 320, 30
    video = np.zeros((frames, rows, columns, 3), dtype=np.uint8)
    y, x = np.mgrid[0:rows, 0:columns]
    for f in range(frames):
        cx = 60 + f * 6
        frame = video[f]
        frame[..., :] = (30 + (x + y) % 20)[..., None].astype(np.uint8)
        disc = (x - cx) ** 2 + (y - 120) ** 2 <= 20 ** 2
        frame[disc] = (220, 80, 40)
        frame[60:64, 100:104] = 255
        frame[60:64, 200:204] = 255
    ds = writer.base(case, "us", 0, US_MULTIFRAME_STORAGE, "US", "PHANTOM^US", "QA-US-001", "Cine with calibrated region")
    ds.ImageType = ["ORIGINAL", "PRIMARY", "ABDOM"]
    ds.SamplesPerPixel = 3
    ds.PhotometricInterpretation = "RGB"
    ds.PlanarConfiguration = 0
    ds.NumberOfFrames = frames
    ds.FrameIncrementPointer = (0x0018, 0x1063)
    ds.FrameTime = 33.3
    ds.CineRate = 30
    region = Dataset()
    region.RegionSpatialFormat = 1
    region.RegionDataType = 1
    region.RegionFlags = 0
    region.RegionLocationMinX0 = 0
    region.RegionLocationMinY0 = 0
    region.RegionLocationMaxX1 = columns - 1
    region.RegionLocationMaxY1 = rows - 1
    region.PhysicalUnitsXDirection = 3
    region.PhysicalUnitsYDirection = 3
    region.PhysicalDeltaX = 0.02
    region.PhysicalDeltaY = 0.02
    ds.SequenceOfUltrasoundRegions = Sequence([region])
    ds.Rows, ds.Columns = rows, columns
    ds.BitsAllocated = 8
    ds.BitsStored = 8
    ds.HighBit = 7
    ds.PixelRepresentation = 0
    ds.PixelData = video.tobytes()
    writer.save(ds, case, "US0000.dcm")
    writer.cases[case] = {
        "modality": "US",
        "instances": 1,
        "frames": frames,
        "frame_time_ms": 33.3,
        "fiducials": {"distance_mm": 20.0, "note": "The two white dots, 100 pixels apart at 0.2 mm per pixel from the ultrasound region."},
    }


def generate_xa(writer):
    case = "xa-multiframe"
    rows, columns, frames = 512, 512, 30
    video = np.zeros((frames, rows, columns), dtype=np.uint8)
    y, x = np.mgrid[0:rows, 0:columns]
    for f in range(frames):
        image = np.full((rows, columns), 180, dtype=np.uint8)
        vessel = np.abs(y - (256 + 80 * np.sin(x / 80.0))) < 6
        image[vessel & (x < 30 + f * 16)] = 40
        video[f] = image
    ds = writer.base(case, "xa", 0, XA_STORAGE, "XA", "PHANTOM^XA", "QA-XA-001", "Contrast run")
    ds.ImageType = ["ORIGINAL", "PRIMARY", "SINGLE PLANE"]
    monochrome(ds)
    ds.NumberOfFrames = frames
    ds.FrameIncrementPointer = (0x0018, 0x1063)
    ds.FrameTime = 66.7
    ds.ImagerPixelSpacing = [0.3, 0.3]
    ds.DistanceSourceToDetector = 1000
    ds.DistanceSourceToPatient = 800
    ds.PositionerPrimaryAngle = 0
    ds.PositionerSecondaryAngle = 0
    ds.Rows, ds.Columns = rows, columns
    ds.BitsAllocated = 8
    ds.BitsStored = 8
    ds.HighBit = 7
    ds.PixelRepresentation = 0
    ds.PixelData = video.tobytes()
    writer.save(ds, case, "XA0000.dcm")
    writer.cases[case] = {
        "modality": "XA",
        "instances": 1,
        "frames": frames,
        "imager_pixel_spacing_mm": [0.3, 0.3],
        "magnification": 1.25,
    }


def generate_mg(writer):
    case = "mg-monochrome1"
    rows, columns = 1280, 1024
    y, x = np.mgrid[0:rows, 0:columns]
    breast = (x / 800.0) ** 2 + ((y - 640) / 600.0) ** 2 <= 1.0
    # MONOCHROME1: a low stored value displays bright.
    image = np.full((rows, columns), 4000, dtype=np.uint16)
    image[breast] = 2200
    image[(x - 400) ** 2 + (y - 640) ** 2 <= 80 ** 2] = 600
    ds = writer.base(case, "mg", 0, MG_PRESENTATION_STORAGE, "MG", "PHANTOM^MG", "QA-MG-001", "L CC")
    ds.ImageType = ["DERIVED", "PRIMARY"]
    ds.PresentationIntentType = "FOR PRESENTATION"
    ds.ImageLaterality = "L"
    ds.Laterality = "L"
    ds.ViewPosition = "CC"
    ds.PatientOrientation = ["A", "R"]
    monochrome(ds, "MONOCHROME1")
    ds.ImagerPixelSpacing = [0.1, 0.1]
    ds.PixelSpacing = [0.1, 0.1]
    ds.WindowCenter = 2048
    ds.WindowWidth = 4096
    ds.RescaleIntercept = 0
    ds.RescaleSlope = 1
    ds.RescaleType = "US"
    set_pixels(ds, image, 12)
    writer.save(ds, case, "MG0000.dcm")
    writer.cases[case] = {
        "modality": "MG",
        "instances": 1,
        "laterality": "L",
        "photometric": "MONOCHROME1",
        "expected_display": "Bright disc in the middle of a grey breast on a dark background. An inverted image is a failure.",
        "disc_diameter_mm": 16.0,
    }


def generate_enhanced_ct(writer):
    case = "enhanced-ct"
    size, frames, spacing, thickness = 256, 20, 0.8, 2.5
    origin = (-(size - 1) * spacing / 2.0, -(size - 1) * spacing / 2.0)
    frame_of_reference = uid(case, "frame")
    volume = np.zeros((frames, size, size), dtype=np.uint16)
    per_frame = []
    for i in range(frames):
        index = 40 + i
        z = index * thickness
        volume[i] = np.clip(np.rint(ct_slice_hu(size, size, spacing, origin, z, index, None) + 1024), 0, 4095).astype(np.uint16)
        item = Dataset()
        position = Dataset()
        position.ImagePositionPatient = [origin[0], origin[1], z]
        item.PlanePositionSequence = Sequence([position])
        content = Dataset()
        content.DimensionIndexValues = [1, i + 1]
        content.StackID = "1"
        content.InStackPositionNumber = i + 1
        item.FrameContentSequence = Sequence([content])
        per_frame.append(item)
    ds = writer.base(case, "enhanced", 0, ENHANCED_CT_STORAGE, "CT", "PHANTOM^ENHANCED", "QA-ENH-001", "Enhanced CT")
    ds.ImageType = ["ORIGINAL", "PRIMARY", "VOLUME", "NONE"]
    ds.FrameOfReferenceUID = frame_of_reference
    ds.PositionReferenceIndicator = ""
    ds.AcquisitionNumber = 1
    ds.BurnedInAnnotation = "NO"
    ds.PresentationLUTShape = "IDENTITY"
    ds.LossyImageCompression = "00"
    monochrome(ds)
    ds.NumberOfFrames = frames
    measures = Dataset()
    measures.PixelSpacing = [spacing, spacing]
    measures.SliceThickness = thickness
    measures.SpacingBetweenSlices = thickness
    orientation = Dataset()
    orientation.ImageOrientationPatient = [1, 0, 0, 0, 1, 0]
    transform = Dataset()
    transform.RescaleIntercept = -1024
    transform.RescaleSlope = 1
    transform.RescaleType = "HU"
    voi = Dataset()
    voi.WindowCenter = 40
    voi.WindowWidth = 400
    shared = Dataset()
    shared.PixelMeasuresSequence = Sequence([measures])
    shared.PlaneOrientationSequence = Sequence([orientation])
    shared.PixelValueTransformationSequence = Sequence([transform])
    shared.FrameVOILUTSequence = Sequence([voi])
    ds.SharedFunctionalGroupsSequence = Sequence([shared])
    ds.PerFrameFunctionalGroupsSequence = Sequence(per_frame)
    ds.Rows, ds.Columns = size, size
    set_pixels(ds, volume, 12)
    ds.Rows, ds.Columns = size, size
    writer.save(ds, case, "ENH0000.dcm")
    writer.cases[case] = {
        "modality": "CT",
        "instances": 1,
        "frames": frames,
        "pixel_spacing_mm": [spacing, spacing],
        "slice_spacing_mm": thickness,
        "purpose": "Enhanced multi-frame geometry. The soft tissue and bone spheres appear in the middle frames.",
    }


def generate_pdf(writer):
    case = "encapsulated-pdf"
    text = b"BT /F1 18 Tf 72 720 Td (SYNTHETIC TEST REPORT) Tj ET"
    objects = [
        b"<< /Type /Catalog /Pages 2 0 R >>",
        b"<< /Type /Pages /Kids [3 0 R] /Count 1 >>",
        b"<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Contents 4 0 R /Resources << /Font << /F1 5 0 R >> >> >>",
        b"<< /Length %d >>\nstream\n%s\nendstream" % (len(text), text),
        b"<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>",
    ]
    pdf = b"%PDF-1.4\n"
    offsets = []
    for number, body in enumerate(objects, 1):
        offsets.append(len(pdf))
        pdf += b"%d 0 obj\n%s\nendobj\n" % (number, body)
    xref = len(pdf)
    pdf += b"xref\n0 %d\n0000000000 65535 f \n" % (len(objects) + 1)
    for offset in offsets:
        pdf += b"%010d 00000 n \n" % offset
    pdf += b"trailer << /Size %d /Root 1 0 R >>\nstartxref\n%d\n%%%%EOF\n" % (len(objects) + 1, xref)
    if len(pdf) % 2:
        pdf += b"\n"
    ds = writer.base(case, "doc", 0, ENCAPSULATED_PDF_STORAGE, "DOC", "PHANTOM^PDF", "QA-DOC-001", "Report")
    ds.BurnedInAnnotation = "YES"
    ds.DocumentTitle = "SYNTHETIC TEST REPORT"
    ds.ConceptNameCodeSequence = Sequence([])
    ds.MIMETypeOfEncapsulatedDocument = "application/pdf"
    ds.EncapsulatedDocument = pdf
    writer.save(ds, case, "DOC0000.dcm")
    writer.cases[case] = {"modality": "DOC", "instances": 1, "expected_text": "SYNTHETIC TEST REPORT"}


def run(*arguments):
    command = [str(a) for a in arguments]
    result = subprocess.run(command, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, text=True)
    if result.returncode != 0:
        raise RuntimeError(f"{' '.join(command)} failed: {result.stderr.strip()}")


# name: (transfer syntax UID, source kind, lossless, encoder command, decoder command)
TRANSFER_SYNTAXES = {
    "implicit-le": ("1.2.840.10008.1.2", "ct", True, ["dcmconv", "+ti"], None),
    "explicit-be": ("1.2.840.10008.1.2.2", "ct", True, ["dcmconv", "+tb"], None),
    "deflated": ("1.2.840.10008.1.2.1.99", "ct", True, ["dcmconv", "+td"], None),
    "rle": ("1.2.840.10008.1.2.5", "ct", True, ["dcmcrle"], ["dcmdrle"]),
    "jpeg-lossless-sv1": ("1.2.840.10008.1.2.4.70", "ct", True, ["dcmcjpeg", "+e1"], ["dcmdjpeg"]),
    "jpeg-lossless": ("1.2.840.10008.1.2.4.57", "ct", True, ["dcmcjpeg", "+el"], ["dcmdjpeg"]),
    "jpeg-extended-12bit": ("1.2.840.10008.1.2.4.51", "ct", False, ["dcmcjpeg", "+ee"], None),
    "jpeg-baseline-8bit": ("1.2.840.10008.1.2.4.50", "xa", False, ["dcmcjpeg", "+eb"], None),
    "jpeg-baseline-rgb": ("1.2.840.10008.1.2.4.50", "us", False, ["dcmcjpeg", "+eb"], None),
    "rle-rgb": ("1.2.840.10008.1.2.5", "us", True, ["dcmcrle"], ["dcmdrle"]),
    "jpeg-ls-lossless": ("1.2.840.10008.1.2.4.80", "ct", True, ["dcmcjpls", "+el"], ["dcmdjpls"]),
    "jpeg-ls-near-lossless": ("1.2.840.10008.1.2.4.81", "ct", False, ["dcmcjpls", "+en"], None),
    "jpeg-2000-lossless": ("1.2.840.10008.1.2.4.90", "ct", True, ["gdcmconv", "--j2k"], ["gdcmconv", "--raw"]),
    "jpeg-2000-lossy": ("1.2.840.10008.1.2.4.91", "ct", False, ["gdcmconv", "--j2k", "--lossy", "-r", "10"], None),
}


def retag(path, case, patient_name, patient_id):
    ds = pydicom.dcmread(path)
    ds.PatientName = patient_name
    ds.PatientID = patient_id
    ds.StudyDescription = f"SYNTHETIC {case}"
    ds.SeriesDescription = case
    ds.StudyInstanceUID = uid(case, "study")
    ds.SeriesInstanceUID = uid(case, "series")
    ds.SOPInstanceUID = uid(case, ds.InstanceNumber, path.name)
    ds.file_meta.MediaStorageSOPInstanceUID = ds.SOPInstanceUID
    ds.save_as(path, write_like_original=False)


def generate_transfer_syntaxes(writer):
    root = writer.root / "transfer-syntaxes"
    with tempfile.TemporaryDirectory() as temporary:
        temporary = Path(temporary)
        scratch = Writer(temporary)
        write_ct_series(scratch, "ct", "ts-source", "ct", "X", "X", "X", slices=10, first_slice=45)
        generate_xa(scratch)
        generate_us(scratch)
        sources = {
            "ct": sorted((temporary / "ct").glob("*.dcm")),
            "xa": sorted((temporary / "xa-multiframe").glob("*.dcm")),
            "us": sorted((temporary / "us-multiframe").glob("*.dcm")),
        }
        for name, (syntax, kind, lossless, encoder, decoder) in TRANSFER_SYNTAXES.items():
            case = f"ts-{name}"
            output = root / name
            output.mkdir(parents=True, exist_ok=True)
            count = 0
            for source in sources[kind]:
                copy = temporary / f"{name}-{source.name}"
                shutil.copy(source, copy)
                retag(copy, case, f"TS^{name.upper()}", f"QA-TS-{name}"[:64])
                target = output / source.name
                run(*encoder, copy, target)
                if not lossless:
                    # Lossy encoders rightly assign a new random instance UID. Fix it so
                    # that the corpus stays reproducible.
                    encoded = pydicom.dcmread(target)
                    encoded.SOPInstanceUID = uid(case, "lossy", source.name)
                    encoded.file_meta.MediaStorageSOPInstanceUID = encoded.SOPInstanceUID
                    encoded.save_as(target, write_like_original=False)
                found = pydicom.dcmread(target, stop_before_pixels=True).file_meta.TransferSyntaxUID
                if found != syntax:
                    raise RuntimeError(f"{name}: expected {syntax}, encoder produced {found}")
                if lossless:
                    check_lossless(copy, target, decoder, temporary)
                copy.unlink()
                count += 1
            writer.cases[f"transfer-syntaxes/{name}"] = {
                "transfer_syntax_uid": syntax,
                "source": kind,
                "lossless": lossless,
                "instances": count,
                "lossless_roundtrip_checked": lossless,
            }


def check_lossless(source, encoded, decoder, temporary):
    if decoder is None:
        decoded = encoded
    else:
        decoded = temporary / "decoded.dcm"
        run(*decoder, encoded, decoded)
    original = pydicom.dcmread(source).PixelData
    restored = pydicom.dcmread(decoded).PixelData
    if original != restored:
        # Big endian stores 16-bit words swapped.
        swapped = np.frombuffer(restored, dtype=np.uint16).byteswap().tobytes()
        if original != swapped:
            raise RuntimeError(f"{encoded}: lossless round trip changed the pixels")


def generate_broken(writer):
    root = writer.root / "broken"
    case = "broken/missing-slice"
    write_ct_series(writer, case, "broken-missing-slice", "ct", "BROKEN^MISSING SLICE", "QA-BRK-001",
                    "Slices 5 and 6 missing", slices=12, first_slice=45, skip_slices=(50, 51))
    with tempfile.TemporaryDirectory() as temporary:
        scratch = Writer(temporary)
        source = write_ct_series(scratch, "ct", "broken-source", "ct", "X", "X", "X", slices=1, first_slice=50)[0]
        truncated = root / "truncated-pixel-data"
        truncated.mkdir(parents=True, exist_ok=True)
        retag(source, "broken-truncated", "BROKEN^TRUNCATED", "QA-BRK-002")
        data = source.read_bytes()
        (truncated / "truncated.dcm").write_bytes(data[:len(data) // 2])
        mismatch = root / "pixel-length-mismatch"
        mismatch.mkdir(parents=True, exist_ok=True)
        retag(source, "broken-mismatch", "BROKEN^MISMATCH", "QA-BRK-003")
        ds = pydicom.dcmread(source)
        ds.Rows = ds.Rows * 2
        ds.save_as(mismatch / "mismatch.dcm", write_like_original=False)
    garbage = root / "not-dicom"
    garbage.mkdir(parents=True, exist_ok=True)
    (garbage / "garbage.dcm").write_bytes(hashlib.sha256(b"garbage").digest() * 64)
    writer.cases["broken"] = {
        "purpose": "Hazard H-05: the viewer must not silently show a wrong or incomplete image.",
        "missing-slice": "Slices at z=125.0 and z=127.5 mm are absent. The viewer should not present the gap as continuous anatomy without warning.",
        "truncated-pixel-data": "Must report an error rather than display a partial image as if complete.",
        "pixel-length-mismatch": "Must report an error rather than display a scrambled image.",
        "not-dicom": "Must be rejected without crashing.",
    }


def tool_versions():
    versions = {"pydicom": pydicom.__version__, "numpy": np.__version__}
    for tool in ("dcmconv", "gdcmconv"):
        result = subprocess.run([tool, "--version"], capture_output=True, text=True)
        versions[tool] = next((line for line in result.stdout.splitlines() if line.strip()), "unknown")
    return versions


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", default="/lab/corpus/synthetic")
    parser.add_argument("--large", action="store_true", help="also generate the 1000-slice benchmark CT (about 520 MB)")
    arguments = parser.parse_args()
    root = Path(arguments.out)
    if root.exists():
        shutil.rmtree(root)
    root.mkdir(parents=True)
    writer = Writer(root)
    for generator in (generate_ct, generate_mr, generate_pet_ct, generate_us, generate_xa, generate_mg,
                      generate_enhanced_ct, generate_pdf, generate_transfer_syntaxes, generate_broken):
        generator(writer)
        print(f"generated {generator.__name__.removeprefix('generate_')}", flush=True)
    if arguments.large:
        generate_ct_large(writer)
        print("generated ct_large", flush=True)
    expected = {"synthetic": True, "tools": tool_versions(), "cases": writer.cases}
    (root / "expected.json").write_text(json.dumps(expected, indent=2) + "\n")
    print(f"{len(writer.cases)} cases in {root}")


if __name__ == "__main__":
    main()
