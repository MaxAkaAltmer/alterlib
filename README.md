# alterlib

**English** · [Русский](README.ru.md)

Alterlib is a free collection of C++ code released under the MIT License.

It is not a monolithic framework, but a set of loosely coupled utilities and subsystems that can be picked, dropped into an existing project, and compiled as ordinary C++ sources and headers. Everything lives under the `alt` namespace; the GUI code additionally uses `alt::ui`, and compute code uses `alt::compute`.

- **Author:** Maxim L. Grishin <altmer@arts-union.ru>
- **Copyright:** 2006–2026
- **License:** [MIT](LICENSE) — third-party code under `external/` keeps its own license

## Overview

Alterlib aims to be the layer you would otherwise rewrite in every project: fixed-width types, Unicode strings, a reflection-style `object` model, containers and data structures, files/threads/time/processes, images and audio, a small GUI stack (GLFW + Dear ImGui, with OpenGL and Vulkan backends), hashing, a home-grown archive/compression pipeline, `object`-to-XML/Protobuf marshaling, and an OpenCL/CUDA compute dispatcher. Subsystems are independent — pull only what you need.

## Core modules (repository root)

Everyday building blocks layered on top of the C++ standard library.

| Module | Purpose |
|---|---|
| `atypes.h`, `atypes_bigendian.h` | Fixed-width integer and float aliases (`uint8`…`uint64`, `real32`, …), state codes, byte-order helpers |
| `astring.*`, `astring_utf.h`, `astring_latin.h` | Unicode string class with UTF-8/16/32 and Latin-1 handling |
| `abyte_array.*` | Growable binary buffer built on `astring` |
| `aobject.*` | Reflectable key/value tree with named children — base model for serialization |
| `avariant.*` | Tagged union: numbers, strings, buffers, arrays, objects |
| `acolor.h` | Color types (`colorRGBA` and friends) |
| `adelegate.h` | Type-erased callable `alt::delegate<RT(AT...)>` and `delegateBase` |
| `afile.*` | Cross-platform file and directory helpers (uses `dirent` on Windows) |
| `atime.*` | Date/time and monotonic tick utilities |
| `athread.*` | Thread, mutex and event wrappers |
| `amath_int.h`, `amath_vec.*` | Integer helpers and vector math |
| `aimage.*` | Image loading/saving and pixel-buffer helpers (uses `stb_image*`) |
| `asound.*` | Audio I/O via miniaudio (opt-in through `ASOUND_ENABLE_MINIAUDIO`) |
| `astreamer.*` | Buffered streaming primitives |
| `anetwork.*` | BSD/Winsock sockets, sessions and delegates |
| `aprocess.*` | Spawn and control external processes (POSIX shm/mmap, Windows ports) |
| `atranslate.*` | Localisation / message catalogs |
| `aeditory.*` | Simple edit history with undo/redo |

### Container & algorithm helpers (`at_*.h`)

| Header | Provides |
|---|---|
| `at_array.h` | `alt::array<T>`, compile-time-sized `alt::fixedArray<T,N>` |
| `at_hash.h` | Hash-based `alt::set<T>`, `alt::map<K,V>` and hash utilities |
| `at_tensor.h` | N-dimensional `alt::tensor<T>` |
| `at_dim.h` | `alt::dimensions<T>` — shape descriptor used with tensors and images |
| `at_bitspace.h` | `bitSpaceBase<T>` — arbitrary-size bit vector / bitmask |
| `at_fraction.h` | Exact rational arithmetic `alt::fraction<T>` |
| `at_graph.h` | Weighted `alt::graph<K,NodeData,LinkData>` |
| `at_ring.h` | `snapshotBuffer<T>` and `dualBuffer<T>` ring buffers |
| `at_priority_cache.h` | `alt::cache<T>` — priority-aware cache |
| `at_profiling.h` | Scoped counters and timing helpers |

## Subsystems

- **`crypto/`** — CRC32 (`crypto/crc32.h`) and `alt::cryptoHash` supporting CRC32, MD5 and SHA1 (MD5/SHA1 built on Crypto++).
- **`compress/`** — Custom archive pipeline: arithmetic coder (`arch_arith`), delta coding (`arch_delta_code`), dictionary coder (`arch_diction`), PPM model (`arch_ppm`), glued together by `arch.*` and `arch_prefix.*`.
- **`gui/`** — Minimal UI toolkit (`alt::ui::window`, `alt::ui::widget`) built on GLFW and Dear ImGui, plus rendering helpers: OpenGL textures/fonts/primitives (`agl_*`) and Vulkan context/textures (`avk_*`).
- **`compute/`** — `alt::compute::computeCluster` abstraction with OpenCL (`opencl_cluster`) and CUDA (`cuda_cluster`) back-ends for dispatching parallel work across devices.
- **`marshaling/`** — Serialize/deserialize `alt::object` trees to XML via tinyxml2 (`aobject_xml`) and to Protobuf via dynamic messages (`aobject_proto`).
- **`scripting/`** — `ALexBase` / `ALexFileNode` — a small lexical/scoping engine used to embed scripts and structured configuration in the `alt::object` model.

## Repository layout

    alterlib/
    ├── crypto/        Hashing primitives (CRC32, alt::cryptoHash)
    ├── compress/      Custom compression / archive codecs
    ├── compute/       OpenCL & CUDA dispatch layer
    ├── gui/           Windows, widgets, GL/VK textures, fonts, primitives
    ├── marshaling/    alt::object <-> XML / Protobuf
    ├── scripting/     Lexer and script host
    ├── external/      Vendored third-party libraries (each keeps its own license)
    └── *.h, *.cpp     Core: types, strings, containers, IO, threads, math, ...

## Third-party dependencies

Libraries alterlib needs are vendored under `external/`. Each keeps its original license — see the corresponding subdirectory for details.

Directly referenced by alterlib sources:

| Library | Referenced by |
|---|---|
| [Dear ImGui](https://github.com/ocornut/imgui) | `gui/awindow.*` (with GLFW/Vulkan/GL backends) |
| [tinyxml2](https://github.com/leethomason/tinyxml2) | `marshaling/aobject_xml.*` |
| [Crypto++](https://www.cryptopp.com/) | `crypto/acrypto.*` (MD5, SHA1) |
| [stb](https://github.com/nothings/stb) | `aimage.*` (image I/O), `gui/agl_font.*` (TrueType rasterizer) |
| [miniaudio](https://github.com/mackron/miniaudio) | `asound.*` (enabled by `ASOUND_ENABLE_MINIAUDIO`) |
| [dirent](https://github.com/tronkko/dirent) | `afile.*` (directory iteration on Windows) |

Also shipped for convenience (used by downstream projects, not required by alterlib itself):

- [Asio](https://think-async.com/Asio/) — asynchronous networking
- [fmt](https://fmt.dev/) — formatting
- [nlohmann/json](https://json.nlohmann.me/) — JSON
- [clipp](https://github.com/muellan/clipp) — command-line parsing
- `glext.h` — OpenGL extension declarations
- [manymouse](https://github.com/jwclulow/manymouse) — multi-mouse input
- `gamepad`, `GetComPortList` — gamepad and serial-port enumeration helpers
- `libnpy` — NumPy `.npy` reader/writer

System dependencies not vendored: GLFW, the Vulkan SDK headers, and Google Protobuf (`google/protobuf/...`) — provide them via your platform when building the relevant subsystems.

## Requirements

- A modern C++ compiler. The core targets C++11/14 features; some modules (for example `aprocess.h`) detect `__cplusplus >= 202002L` / `_MSVC_LANG >= 202002L` and switch to newer implementations when available.
- Alterlib itself has no build script — no top-level `CMakeLists.txt` or solution file. Individual third-party libraries provide their own build projects upstream.

## Using the library

Alterlib is source-included:

1. Add this repository to your project (submodule, subtree, or plain copy).
2. Put the repository root on your include path so headers resolve as `#include "astring.h"`, `#include "gui/awindow.h"`, and so on.
3. Compile the `.cpp` files you actually use together with your own sources — the core headers are self-contained and pull in third-party code only when the corresponding module is built.
4. For subsystems that pull third-party code, add the matching `external/<lib>/` directory to the include path (and, for the libraries that require building — for example Crypto++ — build or link them per their own instructions).

Only the subsystems you compile need to be configured.

## License

Alterlib is distributed under the [MIT License](LICENSE).

Files placed in the `external/` directory may be covered by their own licenses; refer to the corresponding subdirectory for details.
