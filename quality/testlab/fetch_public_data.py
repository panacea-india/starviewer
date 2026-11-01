#!/usr/bin/env python3
"""Downloads the public real-world files of the corpus and checks them against the lock file.

Synthetic data has exact ground truth but no vendor quirks, so the corpus also has real
files. They are not committed. Downloads are untrusted data: only files with a .dcm
name are kept, under generated paths, and nothing from them is executed.
"""

import argparse
import hashlib
import io
import json
import shutil
import sys
import tarfile
import urllib.request
import zipfile
from pathlib import Path

LOCK_FILE = Path(__file__).resolve().parent / "public-data.lock.json"
NBIA_IMAGE = "https://services.cancerimagingarchive.net/nbia-api/services/v1/getImage?SeriesInstanceUID="
RIDER = {
    "source": "TCIA, RIDER Lung PET-CT",
    "doi": "https://doi.org/10.7937/K9/TCIA.2015.OFIP7TVM",
    "licence": "CC BY 3.0",
}

SOURCES = {
    "pydicom-test-files": {
        "source": "pydicom 2.4.4 test files, from many vendors, some deliberately malformed",
        "url": "https://github.com/pydicom/pydicom/archive/refs/tags/v2.4.4.tar.gz",
        "licence": "MIT (see the LICENSE file of pydicom for the origin of individual files)",
    },
    "rider-petct/ct": {
        **RIDER,
        "patient": "RIDER-1002189179",
        "url": NBIA_IMAGE + "1.3.6.1.4.1.9328.50.17.215396350040428026963589891044491388543",
    },
    "rider-petct/pt": {
        **RIDER,
        "patient": "RIDER-1002189179",
        "url": NBIA_IMAGE + "1.3.6.1.4.1.9328.50.17.142988316909724707487240522957927897313",
    },
    "rider-large-ct": {
        **RIDER,
        "patient": "RIDER-1006487867",
        "url": NBIA_IMAGE + "1.3.6.1.4.1.9328.50.17.249836005616032823097408901947312961772",
        "large": True,
    },
}


def download(url):
    request = urllib.request.Request(url, headers={"User-Agent": "starviewer-testlab"})
    with urllib.request.urlopen(request, timeout=600) as response:
        return response.read()


def dicom_members(data):
    """Yields (name, bytes) for the DICOM files of a tar.gz or zip archive."""
    if zipfile.is_zipfile(io.BytesIO(data)):
        with zipfile.ZipFile(io.BytesIO(data)) as archive:
            for member in archive.infolist():
                if not member.is_dir() and member.filename.lower().endswith(".dcm"):
                    yield member.filename, archive.read(member)
    else:
        with tarfile.open(fileobj=io.BytesIO(data), mode="r:gz") as archive:
            for member in archive:
                in_test_files = "/data/test_files/" in member.name
                if member.isfile() and in_test_files and member.name.lower().endswith(".dcm"):
                    yield member.name, archive.extractfile(member).read()


def fetch(name, source, root):
    target = root / name
    if target.exists():
        shutil.rmtree(target)
    target.mkdir(parents=True)
    hashes = []
    # The archive's order and metadata can change between downloads, so the lock is
    # on the sorted hashes of the files. Paths come from the hash too: never the archive.
    for _, content in dicom_members(download(source["url"])):
        digest = hashlib.sha256(content).hexdigest()
        hashes.append(digest)
        (target / f"{digest[:32]}.dcm").write_bytes(content)
    hashes.sort()
    return {
        "files": len(hashes),
        "bytes": sum(path.stat().st_size for path in target.iterdir()),
        "sha256": hashlib.sha256("\n".join(hashes).encode()).hexdigest(),
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", default="/lab/corpus/public")
    parser.add_argument("--large", action="store_true", help="also fetch the 1049-slice CT (about 530 MB)")
    parser.add_argument("--update-lock", action="store_true", help="record what was downloaded instead of checking it")
    arguments = parser.parse_args()
    root = Path(arguments.out)
    lock = json.loads(LOCK_FILE.read_text()) if LOCK_FILE.exists() else {}
    failed = False
    for name, source in SOURCES.items():
        if source.get("large") and not arguments.large:
            continue
        found = fetch(name, source, root)
        print(f"{name}: {found['files']} files, {found['bytes'] / 1e6:.1f} MB, {found['sha256'][:16]}", flush=True)
        if arguments.update_lock:
            lock[name] = found
        elif lock.get(name, {}).get("sha256") != found["sha256"]:
            print(f"FAIL  {name} differs from the lock file")
            failed = True
    if arguments.update_lock:
        LOCK_FILE.write_text(json.dumps(lock, indent=2, sort_keys=True) + "\n")
    (root / "SOURCES.json").write_text(json.dumps(SOURCES, indent=2) + "\n")
    sys.exit(1 if failed else 0)


if __name__ == "__main__":
    main()
