#!/usr/bin/env python3
"""Loads the corpus into the lab's Orthanc and checks the interfaces Starviewer uses.

Files are uploaded unchanged over REST so that each keeps its transfer syntax. The
checks then use C-ECHO and C-FIND (DIMSE), and QIDO-RS and WADO-RS (DICOMweb).
Deliberately broken files are not uploaded: they are for opening from disk. Neither
are deflated files: they are valid (DCMTK, GDCM and pydicom all read them), but Orthanc
1.12.6 refuses them over both REST and C-STORE, so a viewer can only open them from disk.
"""

import argparse
import base64
import json
import subprocess
import sys
import urllib.error
import urllib.request
from pathlib import Path

AUTHORIZATION = "Basic " + base64.b64encode(b"qa:qa-only").decode()


def request(url, data=None, accept=None):
    req = urllib.request.Request(url, data=data, headers={"Authorization": AUTHORIZATION})
    if accept:
        req.add_header("Accept", accept)
    with urllib.request.urlopen(req, timeout=60) as response:
        return response.status, response.read(), response.headers.get("Content-Type", "")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", default="/lab/corpus/synthetic")
    parser.add_argument("--orthanc", default="orthanc")
    arguments = parser.parse_args()
    base = f"http://{arguments.orthanc}:8042"
    root = Path(arguments.root)

    # Another local service could answer on the same address, so check it is this one.
    system = json.loads(request(f"{base}/system")[1])
    if system.get("Name") != "Starviewer test lab":
        sys.exit(f"Not the test lab's Orthanc: {system}")
    print(f"Orthanc {system.get('Version')}")

    uploaded = rejected = 0
    for path in sorted(root.rglob("*.dcm")):
        parts = path.relative_to(root).parts
        if parts[0] == "broken" or parts[:2] == ("transfer-syntaxes", "deflated"):
            continue
        try:
            request(f"{base}/instances", path.read_bytes())
            uploaded += 1
        except urllib.error.HTTPError as error:
            rejected += 1
            print(f"rejected {path}: {error}")
    print(f"uploaded {uploaded}, rejected {rejected}")

    failures = []

    def check(condition, message):
        print(("ok    " if condition else "FAIL  ") + message)
        if not condition:
            failures.append(message)

    check(rejected == 0, "Orthanc accepted every file")
    statistics = json.loads(request(f"{base}/statistics")[1])
    print(f"orthanc: {statistics['CountStudies']} studies, {statistics['CountSeries']} series, {statistics['CountInstances']} instances")
    check(statistics["CountInstances"] == uploaded, "Orthanc holds every uploaded instance")

    echo = subprocess.run(["echoscu", "-aet", "QA", "-aec", "ORTHANC", arguments.orthanc, "4242"], capture_output=True, text=True)
    check(echo.returncode == 0, "C-ECHO")

    find = subprocess.run(["findscu", "-S", "-aet", "QA", "-aec", "ORTHANC", "-k", "QueryRetrieveLevel=STUDY",
                           "-k", "PatientID=QA-PT-001", "-k", "StudyInstanceUID", arguments.orthanc, "4242"],
                          capture_output=True, text=True)
    output = find.stdout + find.stderr
    check(find.returncode == 0 and "StudyInstanceUID" in output, "C-FIND finds the PET-CT study")

    _, body, _ = request(f"{base}/dicom-web/studies?PatientID=QA-PT-001", accept="application/dicom+json")
    studies = json.loads(body)
    check(len(studies) == 1, "QIDO-RS finds exactly one PET-CT study")
    study_uid = studies[0]["0020000D"]["Value"][0]
    _, body, _ = request(f"{base}/dicom-web/studies/{study_uid}/series", accept="application/dicom+json")
    check(len(json.loads(body)) == 2, "QIDO-RS finds its CT and PET series")

    _, body, _ = request(f"{base}/dicom-web/studies?PatientID=QA-CT-001", accept="application/dicom+json")
    ct_uid = json.loads(body)[0]["0020000D"]["Value"][0]
    status, body, content_type = request(f"{base}/dicom-web/studies/{ct_uid}", accept='multipart/related; type="application/dicom"')
    check(status == 200 and content_type.startswith("multipart/related") and len(body) > 1000, "WADO-RS retrieves the CT study")

    try:
        urllib.request.urlopen(f"{base}/statistics", timeout=30)
        check(False, "Requests without credentials are refused")
    except urllib.error.HTTPError as error:
        check(error.code == 401, "Requests without credentials are refused")

    print(f"\n{'FAILED: ' + str(len(failures)) if failures else 'All checks passed'}")
    sys.exit(1 if failures else 0)


if __name__ == "__main__":
    main()
