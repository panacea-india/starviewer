#!/usr/bin/env python3
"""Writes a CycloneDX software bill of materials for one platform's package.

Versions and checksums are read from the files that drive the build, so the list
cannot drift from them: sdk/scripts/unix/sdk_download.sh, the Windows workflow and,
on Linux, the installed system packages. Licences are kept here. licence_source says
how each was established; only "legal review" can settle the others.
"""

import argparse
import json
import re
import subprocess
import sys
from pathlib import Path

REPOSITORY = Path(__file__).resolve().parent.parent

# Patched SDK libraries: name in sdk_download.sh -> licence.
SDK_LIBRARIES = {
    "dcmtk": {"licence": "BSD-3-Clause", "licence_source": "upstream COPYRIGHT file; several notices, GitHub cannot classify it"},
    "vtk": {"licence": "BSD-3-Clause", "licence_source": "upstream Copyright.txt; GitHub cannot classify it"},
    "gdcm": {"licence": "BSD-3-Clause", "licence_source": "upstream Copyright.txt; GitHub cannot classify it"},
    "itk": {"licence": "Apache-2.0", "licence_source": "GitHub licence API"},
    "ecm": {"licence": "BSD-3-Clause", "licence_source": "upstream COPYING-CMAKE-SCRIPTS; GitHub cannot classify it"},
    "threadweaver": {"licence": "LGPL-2.1-only", "licence_source": "GitHub licence API, default branch; confirm for 5.64.0"},
}

VENDORED = [
    {"name": "breakpad", "version": "see starviewer/src/thirdparty/breakpad/revision.txt", "licence": "BSD-3-Clause",
     "licence_source": "from upstream; the vendored copy has no licence file"},
    {"name": "easylogging++", "version": "9.96.7", "licence": "MIT",
     "licence_source": "from upstream; the vendored copy has no licence text"},
    {"name": "qtsingleapplication", "version": "unknown", "licence": "BSD-3-Clause",
     "licence_source": "SPDX header in the vendored source"},
]

QT = {"licence": "LGPL-3.0-only", "licence_source": "Qt's open source terms; also offered under GPL"}

LINUX_PACKAGES = {
    "qtbase5-dev": QT,
    "qtdeclarative5-dev": QT,
    "libqt5xmlpatterns5-dev": QT,
    "libqt5opengl5-dev": QT,
    "libkf5threadweaver-dev": SDK_LIBRARIES["threadweaver"],
    "extra-cmake-modules": SDK_LIBRARIES["ecm"],
    "libssl-dev": {"licence": "Apache-2.0", "licence_source": "OpenSSL 3 licence"},
    "zlib1g-dev": {"licence": "Zlib", "licence_source": "zlib licence"},
}


def sdk_downloads():
    """Returns {library: (version, sha256, url)} from sdk_download.sh."""
    script = (REPOSITORY / "sdk/scripts/unix/sdk_download.sh").read_text(encoding="utf-8")
    found = {}
    for case, body in re.findall(r"^\s+(\w+)\)\n(.*?)^\s+;;", script, re.M | re.S):
        # The last download of a case is the library. An earlier one is its support files.
        matches = re.findall(r"download_and_verify (\w{64}) \\\s+'([^']+)' \\\s+'([^']+)'", body)
        if case in SDK_LIBRARIES and matches:
            sha256, file_name, url = matches[-1]
            version = re.search(r"(\d+\.\d+\.\d+)", file_name).group(1)
            found[case] = (version, sha256, url)
    missing = set(SDK_LIBRARIES) - set(found)
    if missing:
        sys.exit(f"Not found in sdk_download.sh: {sorted(missing)}")
    return found


def component(name, version, licence, licence_source, sha256=None, url=None, patched=False, scope="required"):
    entry = {
        "type": "library",
        "name": name,
        "version": version,
        "scope": scope,
        "licenses": [{"license": {"id": licence}}],
        "properties": [
            {"name": "starviewer:licence_source", "value": licence_source},
            {"name": "starviewer:patched", "value": "true" if patched else "false"},
        ],
    }
    if sha256:
        entry["hashes"] = [{"alg": "SHA-256", "content": sha256}]
    if url:
        entry["externalReferences"] = [{"type": "distribution", "url": url}]
    return entry


def has_patches(name):
    directory = REPOSITORY / "sdk/patches" / name
    return directory.is_dir() and any(directory.iterdir())


def windows_qt_version():
    workflow = (REPOSITORY / ".github/workflows/windows.yml").read_text(encoding="utf-8")
    return re.search(r"install-qt-action@\S+\s+with:\s+version: (\S+)", workflow).group(1)


def dpkg_version(package, allow_missing):
    result = subprocess.run(["dpkg-query", "-W", "-f=${Version}", package], capture_output=True, text=True)
    if result.returncode != 0 or not result.stdout:
        if allow_missing:
            return "not installed"
        sys.exit(f"{package} is not installed")
    return result.stdout


def build(platform, commit, allow_missing):
    downloads = sdk_downloads()
    libraries = ("dcmtk", "vtk", "gdcm", "itk")
    if platform == "windows":
        libraries += ("ecm", "threadweaver")
    components = []
    for name in libraries:
        version, sha256, url = downloads[name]
        # ECM is only needed to build ThreadWeaver and does not reach the package.
        scope = "excluded" if name == "ecm" else "required"
        components.append(component(name, version, SDK_LIBRARIES[name]["licence"], SDK_LIBRARIES[name]["licence_source"],
                                    sha256, url, has_patches(name), scope))
    for item in VENDORED:
        components.append(component(item["name"], item["version"], item["licence"], item["licence_source"]))
    if platform == "windows":
        components.append(component("qt", windows_qt_version(), QT["licence"], QT["licence_source"]))
    else:
        for package, licence in LINUX_PACKAGES.items():
            scope = "excluded" if package == "extra-cmake-modules" else "required"
            components.append(component(package, dpkg_version(package, allow_missing), licence["licence"],
                                        licence["licence_source"], scope=scope))
    return {
        "bomFormat": "CycloneDX",
        "specVersion": "1.5",
        "version": 1,
        "metadata": {
            "component": {"type": "application", "name": "starviewer", "version": commit,
                          "licenses": [{"license": {"id": "GPL-3.0-or-later"}}]},
            "properties": [{"name": "starviewer:platform", "value": platform}],
        },
        "components": components,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--platform", choices=["linux", "windows"], required=True)
    parser.add_argument("--commit", default="unknown")
    parser.add_argument("--out", required=True)
    parser.add_argument("--allow-missing", action="store_true", help="for trying this out away from a build machine")
    arguments = parser.parse_args()
    bom = build(arguments.platform, arguments.commit, arguments.allow_missing)
    Path(arguments.out).write_text(json.dumps(bom, indent=2) + "\n")
    print(f"{len(bom['components'])} components in {arguments.out}")


if __name__ == "__main__":
    main()
