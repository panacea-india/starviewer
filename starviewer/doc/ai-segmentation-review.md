# Review of the `segmentation-ai` branch (2025) and how to rebuild it behind the extension boundary

**What was looked at.** The diff of `origin/segmentation-ai` against its merge base with `phase1` (28 files, 3,750 lines added, the last commit "fill model parameters automatically from a JSON file"),
the CMake changes, the headers of the segmentation classes and the list of files. The 1,118 lines of `qdlsegmentationextension.cpp` and the models were **not** read in full, and nothing of the branch was
built or run: the SDK of this project has neither TensorFlow nor LibTorch, and the test machine has no GPU. This is a design review, not a validation.

## What the branch is

| Part | Files | What it does |
| --- | --- | --- |
| Segmentation core | `core/deeplearningsegmentation.*` (about 490 lines) | Prepares a volume for a model: takes the slices (a range), reslices them to the anatomical view with the direction cosines, resamples them to the size the model expects (interpolation chosen), normalises (an approach chosen), runs the model slice by slice through a virtual `run()`, and fills a mask with the labels the model gives (activation function chosen) |
| TensorFlow back end | `core/deeplearningtensorflowsegmentation.*` | Runs a TensorFlow saved model through the **C API** (graph, session, tensors) |
| Torch back end | `core/deeplearningtorchsegmentation.*` | Runs a TorchScript model (`torch::jit::script::Module`) through LibTorch |
| Playground extension | `extensions/playground/dlsegmentation/*` (about 2,400 lines with the .ui) | The interface: choose a model, fill the parameters from a JSON file, run, show the mask, save and load it as `.vti` |
| Unrelated changes | `zoomwheeltool.*`, changes in `drawer`, `q3dviewer`, `shortcuts`, `toolregistry` | A zoom with the wheel and small hooks for the mask; they belong to other work |
| Build | `core/CMakeLists.txt`, `Options.cmake` | With `USE_DEEPLEARNING`, **`core` links TensorFlow and LibTorch** (they are added to the include and link paths of `core`, so everything that links `core` gets them) |

## What is good in it and worth keeping

- The **pre- and post-processing** is the hard part and is already thought through: reslicing to an anatomical view, cropping, resampling with a choice of interpolation, normalisation, channels, the range of slices,
  and the labels that come from the model. This is what a module needs, and it can move to the module nearly as it is (it is VTK and the model call).
- The idea of a **JSON file with the parameters of a model** (input size, interpolation, normalisation, channels, activation) is the right one: it is what the `parameters` of the module protocol carry.
- Saving the mask as a VTK image (`.vti`) keeps the geometry.

## What should not be merged as it is

1. **The libraries are linked into `core`.** TensorFlow and LibTorch are hundreds of megabytes, bring their own copies of common libraries (a risk of a clash with ITK, VTK, protobuf, OpenMP, CUDA runtimes) and
   would be loaded by every build and every window of the viewer. An error in a model or in these libraries would end the viewer, with the patient on the screen. This is the reason for the extension boundary.
2. **It runs in the process of the viewer** with no limit of time or memory and no way to cancel a prediction that is running.
3. **There is nothing about validation or provenance**: which model, which version, which data it was trained on, which hash. A mask from a model that nobody can identify cannot be trusted or reproduced.
4. **The mask is not a DICOM object** (SEG or RTSTRUCT), so it cannot be kept with the study, checked or shared; `.vti` is for the program only.
5. **Regulatory class.** AI functions can raise the class of the product. The branch does not say "research" anywhere and does not warn that a mask has to be looked at by a person.
6. Its unrelated changes (`zoomwheeltool`, `drawer`, `q3dviewer`) should be taken as separate, reviewed changes, not with this one. Its base is old: the branch has to be redone on `devel` anyway.

## The plan: a module behind the boundary

The same function as a **module** (see `extension-boundary.md`), with the viewer not linking any AI library:

1. **A module `dl-segmentation`** (a program with its own Python or C++ environment with TensorFlow or PyTorch, installed apart from the viewer). Its manifest says `researchOnly: true` and
   `network: "none"`. It gets `inputFolder` (the DICOM series) and `outputFolder`, and the `parameters`: the model path, the input size, the interpolation, the normalisation, the channels, the
   activation and the range of slices. These are the fields of the JSON file of the branch.
2. The module does the pre- and post-processing of the branch (ported from `DeepLearningSegmentation`) and writes **a DICOM SEG** (or an RTSTRUCT, or a NIfTI to begin with) in `outputFolder`, with the
   name, version and hash of the model in the object (Segmentation Series and Software Versions), and says the files in `outputs`.
3. **In the viewer**, the work that is still needed (and that does not exist yet): showing a DICOM SEG on the images (the RTSTRUCT display of `Q2DViewerRtStructureHandler` is the pattern), and loading the
   outputs of a module as a series. Until then the Modules dialog opens the results folder.
4. **A banner** "Research only: not for diagnosis" when a mask from a module is shown, and the model hash in the annotations.
5. **Validation before any use with a patient:** a test set with known masks, the Dice of the model on it recorded with the version, and a reader (a clinician) who looks at the masks on real studies.
   The synthetic and public data of `quality/testlab` can check that the pipeline preserves geometry (a mask of a known sphere must land on the sphere), but not that a model is right.

## What was done

Only the boundary (`extension-boundary.md`, `ExternalModules` and `ModuleRunner` with tests, the Modules dialog and an example module). The module `dl-segmentation` is **not written**: it needs a model, an
environment with TensorFlow or PyTorch, and a data set to validate it, none of which are on this machine.
