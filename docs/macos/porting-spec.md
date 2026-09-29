# Porting spec: Kya on macOS arm64 (developer use)

Status: approved (wayfinder map "Wayfinder: Kya on macOS", mgiuditta/Kya#8), 2026-09-29.
Every decision below links to the research that backs it, in `docs/research/macos/`.

## Goal and constraints

- Run the PC port natively on **Apple Silicon (arm64)** through **MoltenVK** (the LunarG Vulkan SDK), for development.
- **The Windows build stays identical.** Every change has to meet one of three conditions:
  - it is a no-op on Windows (portable code that is semantically equivalent),
  - it sits behind a runtime feature check that is true on Windows,
  - it is compiled only on Apple (`if(APPLE)`, `__APPLE__`, or a separate file).
- **Aimed at upstream** (Icey1717/Kya): small PRs, with the Windows-neutral preparation first.
- Out of scope: Intel Macs, signing/notarization/distribution, any change to Windows behaviour, and compatibility layers. Wine, CrossOver and GPTK all go through MoltenVK and hit the same gaps; see [compatibility-layers](../research/macos/compatibility-layers.md).

## Architecture

### Platform (see the "Define the platform abstraction layer in port/" ticket)

- In `src/`, `PLATFORM_WIN` keeps meaning "PC port" (it is set for every build that isn't PS2), and macOS inherits it. We don't rename it.
- OS macros appear **only in `port/`**. The seam is **one file per OS with the same signatures**, chosen by CMake. No virtual interfaces.
  - `port/macOS/macos_impl.cpp`: RTC (`sceScfGetLocalTimefromRTC`).
  - `port/macOS/Input/gamepad_glfw.cpp`: `KyaGamepad::AddGamepadSupport()`.
  - `port/macOS/Host/` → `Kya_Mac` target, the counterpart of `Kya_Win`.
- The only guards allowed in `src/` are portable constructs (`PLATFORM_WIN`, `sizeof`, `static_assert`), never `_WIN64`/`__APPLE__`.

### Renderer ([moltenvk-support](../research/macos/moltenvk-support.md), [geometry-shaders](../research/macos/geometry-shaders.md), [blend-pipeline-variants](../research/macos/blend-pipeline-variants.md))

- **Portability:** use `VK_KHR_portability_enumeration` + `ENUMERATE_PORTABILITY_BIT` when the extension is present, and enable `VK_KHR_portability_subset`.
- **Geometry shaders:** `geometryShader` becomes optional, decided by a runtime check. When it's missing, `NativeDisplayList.cpp` expands sprites into quads on the CPU (about 50 lines). Windows keeps using `displaylist.geom`.
- **Blend:**
  - Delete `SetBlendingDynamicState`, which is dead code from the debug viewer, and stop requiring the two EDS3 blend features.
  - Add `bDynamicColorWrite = colorWriteEnable && EDS3ColorWriteMask` with optional extensions. When it's false, color write enable/mask go into the pipeline key: 3 write states, at most about 95 native variants, created lazily and shared across the main stages. When it's true (Windows), the dynamic path stays unchanged and the key's write bits are pinned to RGBA.
  - Add a `VkPipelineCache`. On MoltenVK it avoids repeating the SPIR-V→MSL conversion.

### Audio ([audio-backend](../research/macos/audio-backend.md))

- **Backend:** miniaudio (`ma_engine`/`ma_sound`), vendored as a single header (Unlicense/MIT-0), in its own file in `port/Audio`.
- **Seams:** samples and music plug into the existing `SampleVoice`/`MusicOutput` seams. Streams get a new `StreamVoice` seam, which must not change Windows behaviour.
- Windows stays on XAudio2.
- **Build:** `MA_NO_RUNTIME_LINKING`, linked against CoreAudio/AudioToolbox/CoreFoundation.
- **Pitch:** keep XAudio2's 4.0 pitch cap.
- **Shutdown:** widen the `_WIN32` guard in `edSysTransferService.cpp:119-121` so shutdown runs on every PC build.
- **Phase inversion** (L/R gains with opposite signs) would need a custom miniaudio node. Build it only if the game actually uses it.

### Input ([gamepad-input](../research/macos/gamepad-input.md))

- Use the GLFW gamepad API, which is already a dependency (`glfwGetGamepadState`). No new dependencies.
- Two mapping details:
  - Triggers need `(v+1)/2`.
  - The Y-axis sign has to be checked on a real controller.
- Copy the WinRT clamping behaviour on the half-axes.
- Rumble isn't used today. If it's ever needed, do it later with GameController haptics in a `.mm` file.
- The keyboard already goes through ImGui/GLFW.

### Build ([build-tooling](../research/macos/build-tooling.md))

- **Presets:**
  - a hidden `macos-base` preset: Ninja, `hostSystemName == Darwin`, arm64, AppleClang;
  - `macos-debug`, `macos-debug-asan` and `macos-release` on top of it;
  - Windows presets untouched.
- **Guards in the root CMakeLists:**
  - the forced `clang` compiler goes behind `CMAKE_HOST_WIN32`;
  - `-gcodeview` and `/DEBUG` behind `WIN32`;
  - the ASan DLL lookup and copies behind `WIN32`.
- **Extra flags for the Apple `Kya` target:**
  - `-fms-extensions` keeps the 189 pointer→int casts with the same meaning as on Windows. **Decision:** we use the flag instead of rewriting the casts.
  - `-ffp-contract=off` keeps float parity with x86-64.
- **Shaders:** on Apple, glslangValidator comes from `find_package(Vulkan)`. `PLATFORM_BINARY_FOLDER` (`WIN`/`MAC`) replaces the hard-coded `bin/WIN`. dxc is only needed at runtime, by the DebugMenu mesh viewer; it ships with the LunarG SDK.
- **`sync_files`:** use `cmake -E copy_directory_if_different` outside Windows, and tolerate a missing `assets/`.
- **Dependencies:**
  - `Dbghelp.lib` only on WIN32.
  - `fmemopen_windows` isn't built on Apple, which has native `fmemopen`.
  - ISPC and ONNX upscaling stay off on macOS.

### Code portability ([arm64-portability](../research/macos/arm64-portability.md))

- **Blockers shared by every TU:**
  - `Types.h`: `__pragma` → `_Pragma`, and `, ##__VA_ARGS__` in `NAME_NEXT_OBJECT`.
  - `edCTextFont.h:41`: `_WIN64` → `PLATFORM_WIN`. This is the only real layout break.
- **MSVC CRT compat header** (`!_MSC_VER` only):
  - `_aligned_*`, `sprintf_s`/`strcpy_s`, `__forceinline`, `__assume`, `DWORD64`;
  - `<malloc.h>`/`corecrt_*` includes replaced.
- **About 36 small fixes clang-cl lets through:** goto past initialization, function pointer ↔ `void*`, two-phase lookup, `return;`. They are neutral on Windows.
- **TextureUpload** (submodule, edited first in its own repo):
  - sse2neon for GSVector4/4i;
  - replace the xbyak detection in `MultiISA.cpp`;
  - `VirtualAlloc2` → `shm_open`/`mmap` (as in upstream PCSX2 `GS.cpp`);
  - drop `-msse4.1` and `rt` on Apple.

  PCSX2's native NEON backport stays an optional later improvement.
- **Runtime:**
  - **Asset paths:** normalize backslashes in one place in the edFile PC filers.
  - **Truncating casts:** about 30 casts rebuild pointers from a truncated `int`. On macOS they always crash, because the heap sits above 4 GB. **Decision:** audit them and replace them with `STORE_POINTER`/`intptr_t` as they come up, starting from the `-fms-extensions` diagnostics list.
  - **Threads:** do a ThreadSanitizer pass once the build links.

## Milestones and PR sequence

Each PR builds and runs on Windows with identical behaviour. The ones marked **(prep)** don't touch any macOS files.

| # | PR | Contents | Milestone reached |
|---|---|---|---|
| 0 | (local) | Toolchain on the Mac (cmake, ninja, Vulkan SDK) and disc extraction into `assets/`, per the "Extract the disc into assets/ and set up the Mac toolchain" ticket | Environment ready |
| 1 | **(prep)** Portable src headers | `Types.h`, `edCTextFont.h`, compat header, the 36 clang leniencies, includes | `Kya` compiles with Apple clang (−TextureUpload) |
| 2 | **(prep)** TextureUpload arm64 | sse2neon, MultiISA, mmap, flags (in the submodule repo, then a gitlink bump) | The whole tree compiles |
| 3 | macOS CMake | presets, guards, `-fms-extensions`, `-ffp-contract=off`, shaders, `sync_files`, deps | **First link** of `Kya_Mac` |
| 4 | macOS platform files | `macos_impl.cpp`, `Host/Kya_Mac`, GLFW gamepad, path normalization (edFile) | Window opens, input works |
| 5 | **(prep)** Portable renderer | portability flags, optional GS + CPU quad fallback, `bDynamicColorWrite` + write variants, pipeline cache, dead blend code removed | **First frame** on MoltenVK |
| 6 | Truncated pointers | audit and fix the casts hit during boot and the first levels | **Boot to gameplay** |
| 7 | miniaudio audio | `StreamVoice` seam, miniaudio backend, shutdown guard | Audio |
| 8 | Parity | TSan, float parity check, GLFW axis verification | Parity with Windows |

PRs 1, 2 and 5 are neutral on Windows and useful on their own (portability), so they are good first candidates for upstream. PRs 3, 4, 6 and 7 bring in macOS support.

## Open risks

- There may be more than 30 truncated pointer casts, or they may be hard to reach. A trap on first execution helps find them.
- Phase inversion in audio (a custom node) is only needed if the game uses it.
- Submodule pointers may be stale: KyaMesh uses struct members the headers don't have, and `DebugSaveLoad.cpp` includes the missing `WinSaveFile.h`. Check this before PR 1.
- The maintainer hasn't been consulted yet. The draft of the upstream issue exists, and their answer could change the platform defaults.
