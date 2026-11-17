# The extension boundary: modules that run as a separate program

Starviewer can run a **module** that is a separate program, instead of loading its code. This is for functions that need their own libraries (an AI model with PyTorch or
TensorFlow, a script, a tool of a lab), that would make the viewer big or unstable, or that are research code that should not be mixed with the viewer. A module cannot crash the viewer,
cannot see anything but the folders the user chose, and is marked as research unless its author says otherwise.

This is version 1 of the protocol. Nothing in it is for clinical use by itself: **AI functions can raise the regulatory class of a product**, so a module that does segmentation or
detection is shown as *research only* and its output has to be looked at by a person.

## A module

A folder with a `module.json` and a program. Starviewer looks for modules in the subfolders of `modules` next to the program (the ones that come with it) and of `modules` in the user data folder.

```json
{
  "name": "Folder summary (example)",
  "version": "1.0",
  "description": "What it does, in a sentence or two.",
  "protocol": 1,
  "executable": "run.cmd",
  "arguments": [],
  "researchOnly": true,
  "network": "none",
  "timeoutSeconds": 300
}
```

| Field | Rule |
| --- | --- |
| `protocol` | Must be 1 |
| `name`, `version` | Required |
| `executable` | A path **inside the folder of the module**: not absolute, no `..`, and it must exist there (links that go out of the folder are refused) |
| `arguments` | A list of strings given to the program |
| `researchOnly` | `true` unless said otherwise; shown to the user |
| `network` | `"none"`, or a text that says what the module connects to (shown to the user before it runs). The viewer does not stop the network of a module: this is a statement of its author |
| `timeoutSeconds` | From 1 to 86400 (default 600). The module is stopped after this time |

A module that starts other programs should start them in a way that they end with it (on Windows Starviewer stops the whole tree of processes; a script that starts the real program should
use `exec` on Linux).

## The conversation

The module is started with its folder as the working directory and the variable `STARVIEWER_MODULE=1`. Starviewer writes **one line** of JSON to its standard input:

```json
{"type":"run","protocol":1,"inputFolder":"C:\\Users\\me\\study","outputFolder":"C:\\Users\\me\\out","parameters":{}}
```

The module writes **lines of JSON** to its standard output (one object per line, at most 1 MB a line, at most 20,000 lines). Its standard error is kept for the error message if it fails.

| Message | Fields | Meaning |
| --- | --- | --- |
| `hello` | `protocol` (1), `name`, `version` | Must be the first message |
| `progress` | `value` (0 to 1), `message` | Shown in the progress bar; the user can cancel from it |
| `log` | `message` | A line for the log |
| `result` | `message`, `outputs`: a list of `{path, kind, description}` with `kind` `"file"` or `"folder"` | The end, with success. A path is relative to the output folder, or absolute **inside** it |
| `error` | `message` | The end, with a failure |

The module has to end with a `result` or an `error`. If it ends without one, or writes a line that is not a message of the protocol, or is not valid JSON, or does not say `hello` first, the run is
a failure and its outputs are ignored. If the user cancels, the module gets the line `{"type":"cancel"}` on its standard input and 10 seconds to stop; then it is killed.

## What Starviewer checks

- The module only receives the paths of the **input folder** (to read) and the **output folder** (to write). The output folder cannot contain the input folder or be inside it.
- Every output must be **inside the output folder** after resolving `..` and links; one that is not makes the whole run fail.
- A module that takes longer than its timeout, writes too much, or stops answering is killed (with what it started, on Windows).
- The viewer does not send the images anywhere: a module that does is declared in `network`, and it is the user who decides to run it on those images. Images that are shared should be de-identified first
  (Tools > De-identify images).

## Writing a module

Any language works. The example in `modules/examples/series-summary` is a Python script of 70 lines with a one-line launcher. The pieces are `ExternalModules` (the manifest and the messages) and
`ModuleRunner` (the process) in `src/core/externalmodule.*`, and they are tested with real child processes (`test_externalmodule.cpp`), including the failures above.

A module for AI segmentation would be the same: read the DICOM files of `inputFolder`, run the model, write a DICOM SEG or RTSTRUCT or NIfTI in `outputFolder`, say it in `outputs`. Starviewer does not yet
open a SEG or load the outputs of a module by itself: the dialog lists them and opens their folder.

## Not done in version 1

A way to give parameters in the dialog from a description in the manifest (they can be typed as JSON); loading the outputs as a series; modules on Linux were not tried (the code for the
process is portable, the launcher of the example is for Windows); a limit of memory or of the files a module can read (the user is trusting the module with the folders given);
signing of modules; and a module that stays running between runs.
