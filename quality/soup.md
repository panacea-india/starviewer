# Third-party software (SOUP)

IEC 62304 calls third-party software "software of unknown provenance" (SOUP).

Each CI run produces a bill of materials in CycloneDX format, and uploads it with the
packages as `starviewer-linux.cdx.json` or `starviewer-windows.cdx.json`.
[generate_sbom.py](generate_sbom.py) reads versions and checksums from the files that drive
the build, so the list follows them. Licences are kept in that script.

```sh
python3 quality/generate_sbom.py --platform linux --out sbom.json
```

## Direct dependencies

| Software | Version | Licence | Purpose | Patched |
| --- | --- | --- | --- | --- |
| DCMTK | 3.6.7 | BSD-3-Clause | DICOM parsing, networking, JPEG and RLE decoding | Yes |
| VTK | 9.2.5 | BSD-3-Clause | Rendering | Yes |
| GDCM | 3.0.21 | BSD-3-Clause | JPEG 2000 and other decoding | Yes |
| ITK | 5.3.0 | Apache-2.0 | Image processing | Yes |
| Qt | 5.15.2 on Windows, Ubuntu 24.04's 5.15 on Linux | LGPL-3.0 | Interface, networking, database | No |
| ThreadWeaver | 5.64.0 on Windows, Ubuntu 24.04's on Linux | LGPL-2.1 | Background jobs | No |
| OpenSSL | Ubuntu 24.04's on Linux, **absent on Windows** (A-03) | Apache-2.0 | TLS | No |
| zlib | Ubuntu 24.04's on Linux; DCMTK's support package on Windows | Zlib | Compression | No |
| libiconv | DCMTK's support package, on Windows | LGPL-2.1 | Character sets | No |
| Breakpad | See `src/thirdparty/breakpad/revision.txt` | BSD-3-Clause | Crash reports | Vendored |
| Easylogging++ | 9.96.7 | MIT | Logging | Vendored |
| QtSingleApplication | Not recorded | BSD-3-Clause | Single instance | Vendored |

CI also uses `appimagetool` 1.9.1 to pack the AppImage, and NSIS for the installer.

## Licence confidence

Only ITK's and ThreadWeaver's licences were confirmed through GitHub's licence data, and
QtSingleApplication's from its source. GitHub cannot classify the DCMTK, VTK and GDCM licences
because their files combine several notices. The values above are their commonly stated
licences and need the legal review that open core requires anyway. The vendored Breakpad and
Easylogging++ have no licence text in this repository (A-08).

## Still to do

The following are needed before this list can support a release:

- **Nested libraries:** these libraries bundle others, for example OpenJPEG and CharLS in GDCM.
  They are not listed yet.
- **Vulnerability monitoring:** nobody is watching these components for advisories yet. Several
  of them are also old: Qt 5.15.2, ThreadWeaver 5.64.0 and DCMTK 3.6.7 (A-05).
- **Known anomalies of each library:** IEC 62304 expects these to be reviewed for each SOUP item.
- **Requirements on each library:** what the application relies on it to do.
