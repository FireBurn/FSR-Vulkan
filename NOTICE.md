# Third-party notices and distribution boundary

`LICENSE.txt` applies to the original portability layer, public headers,
examples, tests, and documentation in this project.

The `upstream/ffx-1.1.4/` and `upstream/ffx-2.3.0/` trees contain the pinned
FidelityFX SDK source subsets used to build FSR3 upscaling, Optical Flow, and
Frame Interpolation. Retain the copyright and MIT notices embedded in those
files. The complete SDK 1.1.4 MIT license is preserved at
`upstream/ffx-1.1.4/sdk/LICENSE.txt`; `UPSTREAM.md` records the exact SDK
revisions, source hashes, and the per-file license rule used for the SDK 2.3
subset.

This project does not vendor proprietary model binary weights, signed AMD DLLs,
or NVIDIA/Intel runtime libraries into the redistributable source tree.
Extraction tools (`tools/extract_fsr412_vk.py`, `tools/ffx_dxil/pack_dlss_model.py`,
`tools/ffx_dxil/pack_xess_model.py`) operate offline and produce local uncommitted
model/shader packages. All compute shaders (`shaders/dlss/`, `shaders/xess/`,
`shaders/fsr3/`) and contracts provided in this repository are clean-room,
open-source implementations under the project's MIT license.
