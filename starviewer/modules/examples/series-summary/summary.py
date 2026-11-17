"""An example module for Starviewer (protocol 1, see doc/extension-boundary.md).

It reads one line of JSON on its standard input (the request to run), writes lines of JSON on its standard output
(hello, progress, log, result or error) and writes its files only in the output folder it was given.
"""
import json
import os
import sys


def send(message):
    sys.stdout.write(json.dumps(message) + "\n")
    sys.stdout.flush()


def is_dicom(path):
    try:
        with open(path, "rb") as f:
            f.seek(128)
            return f.read(4) == b"DICM"
    except OSError:
        return False


def main():
    request = json.loads(sys.stdin.readline())
    input_folder = request["inputFolder"]
    output_folder = request["outputFolder"]

    send({"type": "hello", "protocol": 1, "name": "Folder summary (example)", "version": "1.0"})

    files = []
    for root, _, names in os.walk(input_folder):
        for name in sorted(names):
            files.append(os.path.join(root, name))

    summary = {"files": len(files), "dicom": 0, "other": 0, "bytes": 0, "extensions": {}}
    for index, path in enumerate(files):
        summary["bytes"] += os.path.getsize(path)
        if is_dicom(path):
            summary["dicom"] += 1
        else:
            summary["other"] += 1
        extension = os.path.splitext(path)[1].lower() or "(none)"
        summary["extensions"][extension] = summary["extensions"].get(extension, 0) + 1
        if index % 20 == 0:
            send({"type": "progress", "value": index / max(len(files), 1), "message": "reading the files"})

    send({"type": "log", "message": "%d files, %d of them DICOM" % (summary["files"], summary["dicom"])})

    os.makedirs(output_folder, exist_ok=True)
    with open(os.path.join(output_folder, "summary.json"), "w") as f:
        json.dump(summary, f, indent=2)
    with open(os.path.join(output_folder, "summary.txt"), "w") as f:
        f.write("Files: %d\nDICOM: %d\nOther: %d\nSize: %.1f MB\n" % (summary["files"], summary["dicom"], summary["other"], summary["bytes"] / 1e6))

    send({"type": "progress", "value": 1.0, "message": "done"})
    send({"type": "result", "message": "%d DICOM files of %d" % (summary["dicom"], summary["files"]), "outputs": [
        {"path": "summary.json", "kind": "file", "description": "the numbers"},
        {"path": "summary.txt", "kind": "file", "description": "the numbers as text"}]})


if __name__ == "__main__":
    try:
        main()
    except Exception as error:  # a module says what went wrong instead of ending without a word
        send({"type": "error", "message": str(error)})
        sys.exit(1)
