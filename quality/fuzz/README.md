# Fuzzing the DICOM readers

Coverage-guided fuzzers for the third-party code that parses and decodes DICOM files for
Starviewer. A malicious file is the main way untrusted data reaches the workstation (hazard
[H-11](../hazard-log.md), anomaly A-05).

| Program | Target |
| --- | --- |
| `fuzz_dcmtk` | DCMTK: parse the file, format every element, and decode the first frame. The JPEG, JPEG-LS and RLE decoders are registered, as in the application |
| `fuzz_gdcm` | GDCM: read the file as an image and decode all its pixel data. ITK's GDCM reader, which Starviewer uses, goes through the same `gdcm::ImageReader` |

Both use libFuzzer and AddressSanitizer from Visual Studio 2022, with the DCMTK and GDCM
libraries of the SDK that Starviewer is built with. They need nothing else.

What they do not cover: Starviewer's own code that reads the tags (`DICOMTagReader` and the
filler steps), because it needs Qt and the whole core library. DICOM network input (C-FIND,
C-STORE and DICOMweb responses) is a different path again.

## Build and run

From an x64 Visual Studio developer prompt:

```bat
build.cmd C:\fuzz                                        :: SDK defaults to C:\sdk\sdk-install, or set SDK=
run.cmd dcmtk C:\fuzz C:\fuzz\seeds 600
run.cmd gdcm  C:\fuzz C:\fuzz\seeds 600
```

The seeds are small files of the synthetic corpus of the [test lab](../testlab/README.md),
which covers every transfer syntax. Copy those under about 300 KB into the seed folder. Inputs
that reach new code are kept in `corpus_<name>` and a second run carries on from them. An input
that crashes a fuzzer is written to `crashes_<name>`, and running the program on that one file
reproduces it: `fuzz_dcmtk.exe crashes_dcmtk\crash-...`.

## Choices to know about

- **Large images are skipped** after the file has been parsed (more than 8,192 pixels a side, 512
  frames, or 64 million pixels). Without that, the first findings are allocations of many
  gigabytes, and the run stops there. Whether the application should refuse such files is a
  separate question.
- **Huge allocations return null** (`allocator_may_return_null=1`, `max_allocation_size_mb=512`),
  as `new` does when the system is short of memory. DCMTK allocates the length an element
  claims, up to 4 GB, before it reads the value. It handles the failure, but a file of a few
  kilobytes still makes the application ask for it.
- **Exceptions are ignored**, as Starviewer's GDCM reader catches them all.
- **Only this file is instrumented.** The libraries are not built with AddressSanitizer, so heap
  overflows inside them are found only when they reach a block that the runtime tracks. Building
  DCMTK and GDCM with `/fsanitize=address` would find more, and is the next step.

## Results

See [Fuzzing runs](../test-protocol.md#fuzzing-runs) in the test protocol.
