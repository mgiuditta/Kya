# What the macOS arm64 build needs from CMake and tooling

Research for [mgiuditta/Kya#19](https://github.com/mgiuditta/Kya/issues/19), part of map #8. Already decided: native arm64 port on MoltenVK through the LunarG Vulkan SDK (#14), no Windows behaviour change, upstream-friendly, and OS-specific CMake/sources live in `port/` in per-OS files (#17).

All `file:line` references are to branch `docs/research` (commit `028d4d4`) with the submodules at their pinned commits (`git submodule update --init --depth 1`).

## Answer

A macOS arm64 build needs **about a dozen small, guarded CMake edits plus one real third-party port: TextureUpload's SSE vector code**. Nothing requires changing the Windows presets or Windows output.

1. **Presets:** add a hidden `macos-base` preset (Ninja, `condition` hostSystemName == `Darwin`) plus `macos-debug`, `macos-debug-asan` and `macos-release`. Append them and leave the Windows entries untouched.
2. **Compiler:** use AppleClang (Xcode). The root `CMakeLists.txt:34-35` forces `CMAKE_CXX_COMPILER` to `clang` (the C driver). That works on Windows only because MSVC-ABI objects pull in their runtime libs through `/DEFAULTLIB`. On macOS it fails to link libc++, so it has to go behind `if(CMAKE_HOST_WIN32)`.
3. **Shaders:** take `glslangValidator` from `find_package(Vulkan)` (`Vulkan_GLSLANG_VALIDATOR_EXECUTABLE`) instead of `$ENV{VULKAN_SDK}/Bin/glslangValidator.exe`. dxc is **not needed at build time**, because the HLSL glob matches nothing. It is needed at **runtime** by the DebugMenu mesh viewer, and the LunarG macOS SDK ships it. Homebrew has no dxc formula.
4. **Assets:** replace `DoRobo.bat`/robocopy with `cmake -E copy_directory_if_different` on non-Windows hosts.
5. **Vulkan:** `find_package(Vulkan)` already handles macOS through `VULKAN_SDK`. Link the loader, not MoltenVK. Portability enumeration is a code change, already covered by #10.
6. **ASan:** AppleClang ships `libclang_rt.asan_osx_dynamic.dylib`, so only the flags are needed. The Windows DLL lookup and copy steps must be guarded.
7. **Dependencies:** every submodule builds on macOS arm64 **except**:
   - **TextureUpload:** needs `-msse4.1` removed, `rt` not linked, NEON vector headers, and `_aligned_malloc` replaced.
   - **fmemopen_windows:** Windows-only; macOS has a native `fmemopen`.
   - **ISPC:** off by default, but the flags are hard-coded for x86-64.
   - **DebugMenu:** links `Dbghelp.lib` unconditionally.

## 1. Current state (what is Windows-specific)

| # | Where | What | Effect on macOS |
|---|---|---|---|
| 1 | `CMakePresets.json:4-24` | All PC presets inherit `windows-base`, whose `condition` is `hostSystemName == Windows`. `linux-debug` (`:79-104`) is the PS2 ee-gcc cross build | No preset is visible on macOS |
| 2 | `CMakeLists.txt:34-35` | `set(CMAKE_C_COMPILER "clang")`, `set(CMAKE_CXX_COMPILER "clang")` before `project()` | A normal variable shadows any `-D`/preset compiler. `clang` as the C++ link driver does not add `-lc++` on Darwin, so the link fails with undefined `std::` symbols (see §3) |
| 3 | `CMakeLists.txt:76-77`, `:133` | Every non-PS2 host is the "Win Build". `PLATFORM_BINARY_FOLDER` is `WIN` | The output would go to `bin/WIN`. Also, `:133` runs *after* `add_subdirectory("port")` (`:131`), so `port/` can't use the variable today |
| 4 | `CMakeLists.txt:79-101` | ASan block looks for `clang_rt.asan_dynamic-x86_64.dll` and errors if it is missing | Configure fails on macOS with ASan ON |
| 5 | `CMakeLists.txt:110-127` | Clang Debug flags include `-gcodeview` and `-Xlinker /DEBUG` | ld64 would treat `/DEBUG` as an input file. CodeView is Windows-only |
| 6 | `CMakeLists.txt:805-810`, `DoRobo.bat:1-3` | `sync_files` runs `DoRobo` (a `.bat` wrapping `robocopy /XO /s`), and `Kya` depends on it | `DoRobo` doesn't exist as a command on macOS, so every build fails |
| 7 | `CMakeLists.txt:878` | `.natvis` files are added as sources | Harmless: Ninja/Makefiles ignore the extension |
| 8 | `port/Windows/Renderer/Shaders/CMakeLists.txt:5-12` | `glslangValidator.exe` from `$ENV{VULKAN_SDK}/Bin` (or `Bin32` if the host CPU isn't `AMD64`), `dxc.exe`, output to `bin/WIN/shaders` | On arm64 the `else()` branch picks `Bin32/glslangValidator.exe`, so every `.spv` custom command fails |
| 9 | `port/Windows/Renderer/Shaders/CMakeLists.txt:27-31` | The HLSL glob wants `*.frag.hlsl`/`*.vert.hlsl`/`*.geom.hlsl`, but the only HLSL files are `src/meshviewer.hlsl` and `src/ps2/ps2.hlsl` | **The glob matches nothing, so dxc never runs at build time** (`:49-71` are dead) |
| 10 | `port/Windows/Renderer/CMakeLists.txt:197-215`, `Shaders/compiler/CMakeLists.txt:12-25` | `DXC_COMPILER`, `SHADER_SRC_DIR` and `SHADER_OUTPUT_FOLDER` are baked in as defines. `NATIVE_PATH` + backslash doubling | Fine on macOS (NATIVE_PATH gives `/`, and the REPLACE does nothing), but it still points at `Bin/dxc.exe` |
| 11 | `port/Windows/Renderer/Vulkan/src/Objects/VulkanShader.cpp:54-67`, `:122-142` | Runtime `std::system(DXC_COMPILER ...)`. Called by `DebugMeshViewerVulkan.cpp:59-62` from `Setup()` (`:248`), which `DebugRendererVulkan.cpp:267` calls. The PS2 path (`Pipeline.cpp:24`) has no callers of `PS2::GetPipeline` (dead, as found in #11) | The debug overlay startup runs dxc. Without it, the `assert(result == 0)` fires, then `ReadFile` throws |
| 12 | `port/Windows/Renderer/CMakeLists.txt:3` | `find_package(Vulkan REQUIRED FATAL_ERROR)` | `FATAL_ERROR` isn't a keyword. CMake ≥3.24 FindVulkan strips it with an AUTHOR_WARNING (FindVulkan.cmake "Ignoring unknown component 'FATAL_ERROR'"). Harmless |
| 13 | `port/Windows/Renderer/CMakeLists.txt:180` | `PLATFORM_WIN` define on Renderer | Keep it: #17 decided `PLATFORM_WIN` means "PC port" |
| 14 | `port/CMakeLists.txt:40-47` | `Windows/windows_impl.cpp` and `Windows/Input/gamepad.cpp` (WinRT) are in `Port` unconditionally | Needs the per-OS source list decided in #17 |
| 15 | `port/Audio/CMakeLists.txt:22-24` | XAudio2 link already under `if(WIN32)` | The CMake side is fine. The backend source is covered by #17/#9 (miniaudio) |
| 16 | `port/DebugMenu/CMakeLists.txt:131` | `Dbghelp.lib` in `target_link_libraries` unconditionally (`winhttp.lib` at `:135` is behind an option that defaults OFF) | macOS link error: `Dbghelp.lib` not found |
| 17 | `port/Windows/Host/CMakeLists.txt:6-11`, `port/Test/CMakeLists.txt:16-21` | POST_BUILD copy of the ASan DLL when ASan is ON | Needs `AND WIN32` |
| 18 | `src/port/ispc/ispc.cmake:15`, `CMakeLists.txt:60-65` | ISPC is enabled only if `$ENV{ISPC}` is set. Args hard-code `--arch=x86-64 --target=sse2` | Off by default, so it doesn't block. If enabled on arm64, use `--arch=aarch64 --target=neon-i32x4` |
| 19 | `port/Windows/Renderer/Vulkan/src/pcsx2/TextureUpload/CMakeLists.txt:43-48` | `-msse4.1` PUBLIC on any Clang/GCC. `rt` linked when `NOT WIN32` | arm64 clang rejects `-msse4.1` (and it is PUBLIC, so it propagates to Renderer and everything that links it). macOS has no `librt` |
| 20 | `port/ext/pss/pss/ext/libfmemopen/CMakeLists.txt:3-9` | Builds `fmemopen_windows/libfmemopen.c` (`windows.h`, `GetTempFileNameA`, `_sopen_s`) | Doesn't compile on macOS. macOS already has POSIX `fmemopen` |

The earlier map (`docs/research/02-port-map.md`, "Build system" row) listed items 2, 3, 6 and 8-10. This table adds the dxc build/runtime split, the compiler link problem, the Debug linker flag, `Dbghelp.lib`, and the TextureUpload/fmemopen dependencies.

## 2. Presets

Preset schema version 3 (the file's current version) supports `condition`, `inherits` and `hidden`. Append these after `linux-debug`. The Windows entries stay byte-identical, and the Darwin `condition` hides them from Visual Studio.

```json
{
  "name": "macos-base",
  "hidden": true,
  "generator": "Ninja",
  "binaryDir": "${sourceDir}/out/build/${presetName}",
  "installDir": "${sourceDir}/out/install/${presetName}",
  "cacheVariables": {
    "PS2": false,
    "CMAKE_OSX_ARCHITECTURES": "arm64",
    "CMAKE_C_COMPILER": "clang",
    "CMAKE_CXX_COMPILER": "clang++"
  },
  "condition": { "type": "equals", "lhs": "${hostSystemName}", "rhs": "Darwin" }
},
{ "name": "macos-debug", "displayName": "macOS arm64 Debug", "inherits": "macos-base",
  "cacheVariables": { "CMAKE_BUILD_TYPE": "Debug" } },
{ "name": "macos-debug-asan", "displayName": "macOS arm64 Debug (AddressSanitizer)", "inherits": "macos-debug",
  "cacheVariables": { "ENABLE_ADDRESS_SANITIZER": "ON" },
  "environment": { "ASAN_OPTIONS": "halt_on_error=0" } },
{ "name": "macos-release", "displayName": "macOS arm64 RelWithDebInfo", "inherits": "macos-base",
  "cacheVariables": { "CMAKE_BUILD_TYPE": "RelWithDebInfo" } }
```

Notes:

- `VULKAN_SDK` is **not** put in the preset, because the SDK path contains its version. The developer runs `source ~/VulkanSDK/<ver>/setup-env.sh` first (LunarG macOS Getting Started). That script also sets the loader's ICD/layer search paths, which the run needs.
- Ninja keeps parity with the Windows presets. It needs `brew install ninja` (Ninja isn't part of Xcode). `"Unix Makefiles"` would work with no extra tool.
- `CMAKE_OSX_DEPLOYMENT_TARGET` isn't needed for a developer-only build (distribution is out of scope in #8).

## 3. Compiler: AppleClang, not Homebrew LLVM

- **Windows today** uses the Clang tools shipped with VS 2022. `CMakeLists.txt:34-35` picks `clang` (the GNU-style driver), **not `clang-cl`**. That is why MSVC-only flags are passed through `-Xlinker` (`:123-126`) and `-gcodeview` (`:121`). The preset only sets `intelliSenseMode: windows-clang-x64` (`CMakePresets.json:18-22`).
- **macOS:** use **AppleClang** from Xcode. On this Mac: `Apple clang version 21.0.0 (clang-2100.3.34.2)`, target `arm64-apple-darwin25.6.0`. It supports C++20 (`port/CMakeLists.txt:3`, TextureUpload `CMakeLists.txt:3`) and the `-Wimplicit-float-conversion`/`-Werror=` set (`CMakeLists.txt:846-852`), and it ships the ASan runtime (§6). Homebrew `llvm` (23.1.2 bottle for arm64_tahoe) would add a second libc++/SDK pairing to maintain for no benefit, and upstream contributors on macOS will have Xcode anyway.
- **Required CMake edit:** wrap `CMakeLists.txt:34-35` in `if(CMAKE_HOST_WIN32) ... endif()`. `WIN32`/`APPLE` aren't set before `project()`, but the `CMAKE_HOST_*` variables are. This leaves Windows identical and lets the preset (or CMake's default `c++`) pick `clang++`.
  - Why it breaks otherwise: with CXX = `clang`, CMake links C++ executables through the `clang` driver, which only adds the C++ standard library when invoked as `clang++`. On Windows/MSVC ABI the objects carry `/DEFAULTLIB` directives, so this never showed up.
- **Also guard** the Debug flags `-gcodeview` and `-Xlinker /DEBUG` (`CMakeLists.txt:117-126`) with `WIN32`. `-fno-limit-debug-info`, `-fno-omit-frame-pointer` and `-fno-inline` are fine on AppleClang.
- `ld64` has none of GNU ld's archive-order problem, so the cyclic static-lib links (`Kya` ↔ `Port`/`DebugMenu`/`Mesh`/`Texture`/`Sprite`, e.g. `port/KyaMesh/CMakeLists.txt:24`, `port/Sprite/CMakeLists.txt:11`) should link as they do with lld-link. This is a low risk; confirm it at the first link.

## 4. Shader tooling

**Build time (GLSL only).** The only build-time compiler that runs is glslangValidator (`Shaders/CMakeLists.txt:37-47`). Recommended edit:

```cmake
if (APPLE)   # or: if (NOT CMAKE_HOST_WIN32)
	set(GLSL_VALIDATOR "${Vulkan_GLSLANG_VALIDATOR_EXECUTABLE}")
	set(DXC_COMPILER   "${Vulkan_dxc_EXECUTABLE}")
elseif (${CMAKE_HOST_SYSTEM_PROCESSOR} STREQUAL "AMD64")
	... existing Windows lines unchanged ...
```

- `find_package(Vulkan)` runs in `Renderer/CMakeLists.txt:3` *before* `add_subdirectory("Shaders")` (`:5`), so the variables are visible. FindVulkan always searches for `glslangValidator` and `glslc` for backward compatibility (FindVulkan.cmake: "if(NOT glslangValidator IN_LIST Vulkan_FIND_COMPONENTS) list(APPEND ...)"). `Vulkan_GLSLANG_VALIDATOR_EXECUTABLE` exists since CMake 3.21. To get `Vulkan_dxc_EXECUTABLE`, request `COMPONENTS dxc` (CMake 3.25+) or call `find_program(dxc HINTS $ENV{VULKAN_SDK}/bin)`, both only on Apple.
- `-gVS` (`:44`) is a glslangValidator flag, so it works unchanged.
- `displaylist.geom.glsl` still compiles to SPIR-V on macOS, because compiling doesn't need device support. Whether it's *used* is #11's runtime check.
- Output folder: replace the hard-coded `bin/WIN` in `Shaders/CMakeLists.txt:12` and `Renderer/CMakeLists.txt:198` with `bin/${PLATFORM_BINARY_FOLDER}`. Set `PLATFORM_BINARY_FOLDER` to `MAC` on Apple (and keep `WIN` otherwise) **before** `add_subdirectory("port")` (move `CMakeLists.txt:133` up). On Windows the result is the same `bin/WIN`.

**dxc.** It isn't needed to build, because the HLSL glob is empty (table row 9). It *is* invoked at runtime by the DebugMenu mesh viewer HLSL pipeline (row 11). Options:

- (a) Point `DXC_COMPILER` at the SDK's `dxc`. The LunarG macOS SDK lists "DXC" among its SPIR-V tools, in `macOS/bin`. **Recommended**, because the Vulkan SDK is required anyway.
- (b) Skip `CreatePipelineHlsl()` when dxc is absent (the GLSL mesh-viewer pipeline already exists, `DebugMeshViewerVulkan.cpp:45-57`). Use (b) only if the Homebrew toolchain must be supported, since Homebrew has **no** dxc formula (`brew search dxc` / `directx` returns only `directx-headers`).
- `ShaderCompiler` (`Shaders/compiler`) is built as a normal executable but never run by the build (no custom command references it). It compiles portably. Its `"\\ps2"` join (`ShaderCompiler.cpp:92`) only matters if someone runs it, and it belongs to the dead PS2 path.

## 5. Replacing robocopy `sync_files`

`DoRobo.bat` = `robocopy <assets/> <bin/WIN/> /XO /s`, always `EXIT /B 0`. That is a recursive copy that skips files where the destination is newer, and it never fails the build even if `assets/` (gitignored) is missing.

Recommended (non-Windows branch only; Windows keeps `DoRobo`):

```cmake
if (CMAKE_HOST_WIN32)
	add_custom_target(sync_files ... existing DoRobo lines ...)
else()
	add_custom_target(sync_files
		COMMAND ${CMAKE_COMMAND} -E make_directory "${KYA_EXECUTABLE_DIRECTORY}"
		COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
			"${CMAKE_CURRENT_SOURCE_DIR}/assets/" "${KYA_EXECUTABLE_DIRECTORY}/")
endif()
```

- `copy_directory_if_different` needs CMake ≥ 3.26. The root minimum is 3.19.4 (`CMakeLists.txt:1`), so either raise the minimum only inside the Apple branch (`if(CMAKE_VERSION VERSION_LESS 3.26) message(FATAL_ERROR ...)`) or fall back to `copy_directory`. Homebrew CMake is 4.4.3, so that's fine in practice.
- Semantics: "copy if content differs" versus robocopy's "skip if destination newer". The two agree for read-only disc data.
- To keep robocopy's "never fail" behaviour when `assets/` is absent (e.g. CI or tests), add the target only `if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/assets")`, or print a message instead.
- Alternative for multi-GB disc data: `cmake -E create_symlink assets/CDEURO bin/MAC/CDEURO`. This avoids a copy and works because macOS APFS is case-insensitive by default (see `02-port-map.md` "Paths / case"). It's a nice-to-have, not needed.
- `file(COPY ...)` would run at configure time only, and so miss asset changes between builds. The custom target is better.

## 6. Vulkan SDK / MoltenVK discovery and ASan

**Vulkan.**

- Install the LunarG macOS SDK (latest page: 1.4.363.0). Its libraries are universal (Intel and Apple Silicon). `VULKAN_SDK` points at `<sdk>/macOS`, which contains `bin`, `include`, `lib` and `share/vulkan` (ICD/layer manifests).
- `find_package(Vulkan)` reads `$ENV{VULKAN_SDK}/include`, `/bin` and `/lib` on all platforms, and on Apple also adds the `../MoltenVK` hints (FindVulkan.cmake, lines ~385-450 of current master). The existing `${Vulkan_LIBRARIES}`/`${Vulkan_INCLUDE_DIRS}` usage (`Renderer/CMakeLists.txt:183`, `:186`, `:189`; `ext/imgui/CMakeLists.txt:21-23`) works unchanged.
- **Link the loader, not MoltenVK.** LunarG: "you link only to the Vulkan Loader, and not the MoltenVK library directly". So don't request the `MoltenVK` component. This also keeps KosmicKrisp selectable through vkconfig.
- The runtime ICD lookup needs `setup-env.sh` (or the SDK's "System Global Installation" into `/usr/local`). CMake's default build RPATH covers the loader dylib's directory.
- Portability enumeration (`VK_KHR_portability_enumeration` + `VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR` + `VK_KHR_get_physical_device_properties2`, then enabling `VK_KHR_portability_subset`) is a **code** change, already specified in #10. LunarG notes KosmicKrisp "does not require or support this extension".
- GLFW 3.5.0 (`ext/glfw/CMakeLists.txt:3`) loads `libvulkan` itself. Since the app links the loader, calling `glfwInitVulkanLoader(vkGetInstanceProcAddr)` before `glfwInit` avoids a second loader search. This is optional code, not CMake.
- Alternative toolchain: Homebrew `vulkan-loader`/`vulkan-headers`/`molten-vk` (1.4.2)/`glslang`/`vulkan-validationlayers` all have arm64_tahoe bottles. But there's no dxc (see §4), and ICD discovery depends on the Homebrew prefix. **Recommend documenting the LunarG SDK as the supported path** (it matches #14).

**ASan.**

- AppleClang ships `libclang_rt.asan_osx_dynamic.dylib` (found in `clang --print-runtime-dir` = `.../XcodeDefault.xctoolchain/usr/lib/clang/21/lib/darwin` on this Mac). The driver links it and adds its rpath, so **no DLL lookup or copy is needed**.
- CMake edits:
  - `CMakeLists.txt:79-96`: do the `--print-runtime-dir` / `.dll` lookup only `if(WIN32)`. Keep `:99-100` (`-fsanitize=address -fsanitize-recover=address -fno-omit-frame-pointer`, link `-fsanitize=address`) for both.
  - `port/Windows/Host/CMakeLists.txt:6` and `port/Test/CMakeLists.txt:16`: `if (ENABLE_ADDRESS_SANITIZER AND WIN32)`.
  - Update the option text at `CMakeLists.txt:21` and the error text at `:81`.
- `ASAN_OPTIONS=halt_on_error=0` needs `-fsanitize-recover=address`, which is already passed.

## 7. Submodules and third-party dependencies on macOS arm64

Pinned commits come from `git submodule status`. Verdicts come from reading each dependency's CMake and grepping for x86/Win32-only code.

| Dependency | Path / pin | macOS arm64 verdict |
|---|---|---|
| Tracy 0.12.2 | `port/ext/tracy` `c556831` (`public/common/TracyVersion.hpp:8-10`) | ✅ Builds. The only Win32-specific CMake is `if(WIN32 AND ...)` (`CMakeLists.txt:75`). The `TRACY_CALLSTACK` setup (`port/CMakeLists.txt:20-29`) is unchanged |
| Dear ImGui 1.92.5 WIP | `port/Windows/Renderer/ext/imgui/imgui` `1d942eb` | ✅ Only the GLFW+Vulkan backends are compiled (`ext/imgui/CMakeLists.txt:5-16`) |
| GLFW 3.5.0 | `port/Windows/Renderer/ext/glfw` `e7ea71b` | ✅ Cocoa backend. It adds its own frameworks. `GLFW_BUILD_*` are already OFF (`port/CMakeLists.txt:6-8`) |
| GLM 1.1.x | `port/Windows/Renderer/ext/glm` `d34c19e` | ✅ With `GLM_FORCE_INTRINSICS` (`Renderer/CMakeLists.txt:93`), GLM picks `GLM_ARCH_ARMV8` (NEON) on arm64 (`glm/simd/platform.h`, `__ARM_ARCH >= 8` branch) |
| SPIRV-Reflect | `ext/SPIRV-Reflect` `ef913b3` | ✅ Portable C. Options are already set (`CMakeLists.txt:4-9`) |
| **TextureUpload** (Icey1717, PCSX2-derived, LGPL-3.0+) | `port/Windows/Renderer/Vulkan/src/pcsx2/TextureUpload` `8dce176` | ❌ **Needs porting** (details below). It's on the live texture path: `TextureCache.cpp:7,14`, `VulkanPS2.cpp:7`, `UniformBuffer.h:2`, `KyaTexture/src/Texture.h:13` |
| └ xbyak 7.22 | `TextureUpload/ext/xbyak` | ✅ Compiles: CPU detection is under `XBYAK_INTEL_CPU_SPECIFIC` (`xbyak_util.h:30-31`). But `MultiISA.cpp:34-41` then returns `VectorISA::None`, so it needs an arm64 branch |
| └ xxHash | `TextureUpload/ext/xxHash` | ✅ Portable. `xxh_x86dispatch.c` isn't in the source list |
| KyaTexture | `port/KyaTexture` `263737b` | ✅ No platform code. Plain CMake (`CMakeLists.txt:14-27`) |
| KyaMesh | `port/KyaMesh` `aa0bcce` | ✅ Same as KyaTexture |
| edBank / edFile | `src/EdenLib/*` | ✅ No Win32 code (edFile's path handling is a source-level item, `02-port-map.md`) |
| spdlog | `port/Log/spdlog` `486b555` | ✅ Its Windows sinks are `#ifdef`'d |
| googletest | `port/Test/ext/googletest` `65cfeca` | ✅ |
| steinwurf/recycle, bshoshany/thread-pool, readerwriterqueue, nlohmann/json, magic_enum, imGuIZMO.quat | various | ✅ Header-only/portable. `readerwriterqueue/atomicops.h:125-127` uses `_mm_mfence` only under `AE_ARCH_X64/X86` |
| libmpeg2 (in-tree CMake) | `port/ext/pss/pss/ext/libmpeg2` `5e8464c` | ✅ `include/config.h` is empty, so no `ARCH_*` is defined and only the portable C paths are active (`cpu_accel.c:32`, `motion_comp_arm.c:25`, `idct_mmx.c:26` are all `#if ARCH_*`). The x86/ARM asm files compile to nothing. **Don't** define `ARCH_ARM` (that's 32-bit ARM asm) |
| **fmemopen_windows** | `port/ext/pss/pss/ext/libfmemopen/fmemopen_windows` `b57cad1` | ❌ Windows-only (`libfmemopen.c` uses `windows.h`, `_sopen_s`). macOS has POSIX `fmemopen`, so on Apple skip `add_subdirectory("ext/libfmemopen")` and the link (`pss/CMakeLists.txt:11-13`), and guard `#include <libfmemopen.h>` in `pss/src/decode.c:5` (in-tree, not a submodule) |
| ISPC (optional tool) | `src/port/ispc/ispc.cmake` | ⚠️ Off unless `$ENV{ISPC}` is set. On arm64 use `--arch=aarch64 --target=neon-i32x4` (Homebrew `ispc` 1.31 has an arm64 bottle). Not needed for a first build |
| PCSX2 VU (in-tree) | `port/pcsx2/VU` | ✅ The only intrinsic, `VU0micro.cpp:45-49`, is under `#ifdef _M_X86` |
| ncnn / ONNX Runtime | `Renderer/CMakeLists.txt:112-174` | ⚠️ Both OFF by default. The ONNX branch downloads `onnxruntime-win-x64` and links a `.lib`, so it must stay off (or get its own osx-arm64 URL) on macOS |

**TextureUpload, in detail.** This is the only third-party code that needs real work. It lives in Icey1717's repo, so per `AGENTS.md` changes are committed there first and the parent gitlink is bumped after.

1. `CMakeLists.txt:43-47`: add `-msse4.1` only on x86 (`if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86|AMD64")`). Link `rt` only on Linux (`if(UNIX AND NOT APPLE)`), because macOS has no `librt`.
2. Vector ISA: `common/VectorIntrin.h:12-45` already has an `_M_ARM64` → `<arm_neon.h>` branch. But nothing defines `_M_X86`/`_M_ARM64` (PCSX2 defines them in its own CMake). On Windows it works only because `_MSC_VER` pulls `<intrin.h>` (`:8-10`) and `_M_SSE` stays undefined, so the AVX paths are off (`GSVector8i.h:32`, `GSVector8.h:40`, `GSBlock.h:33`). On macOS `GSVector4i.h`/`GSVector4.h` (179 and 84 `_mm_*` uses) have no intrinsics at all. Two options:
   - **sse2neon** (MIT, single header): include it from `VectorIntrin.h` under `__aarch64__`. This is the smallest diff and gets to a first compile.
   - Back-port **PCSX2's `GSVector4_arm64.h` / `GSVector4i_arm64.h`** (they exist upstream in `pcsx2/GS/`, same LGPL-3.0+ licence). That's native NEON and a bigger change. **Recommend sse2neon first**, and the PCSX2 headers only if profiling asks for them.
3. `_aligned_malloc`/`_aligned_free` (`GSAlignedClass.h:19,40`, `UploadBuffer.cpp:4`): map them to `std::aligned_alloc`/`free` (or `posix_memalign`) under `#ifndef _WIN32`.
4. `MultiISA.cpp`: add an `#if defined(__aarch64__)` branch that returns a NEON/"None-but-OK" ISA without Xbyak CPUID.

## 8. Tooling checklist for a macOS developer

Facts gathered on this machine (macOS 26.6.2, arm64) without installing anything:

- Present: Xcode AppleClang 21 (`/usr/bin/clang`, `clang++`), ASan runtime, Homebrew.
- **Absent:** `cmake`, `ninja`, `glslangValidator`, `glslc`, `dxc`, `ispc`, and any Vulkan SDK (`VULKAN_SDK` unset). So no configure could be tried.
- Required: Xcode (or the Command Line Tools), CMake ≥ 3.26 (Homebrew 4.4.3), Ninja (Homebrew 1.13.2), and the **LunarG Vulkan SDK for macOS** (provides the loader, MoltenVK, glslangValidator, dxc and validation layers; `source setup-env.sh`).
- Optional: `ispc`.
- CMake 4.x: every `cmake_minimum_required` reached from the build is ≥ 3.5 (tracy 3.10, spdlog 3.10, SPIRV-Reflect 3.16, glfw 3.16, googletest 3.5, xbyak 3.5, glm 3.6, json 3.1...3.14 (range max ≥ 3.5), imgui wrapper 3.5). ncnn is already patched with `CMAKE_POLICY_VERSION_MINIMUM` (`Renderer/CMakeLists.txt:130-133`). `port/ext/pss/CMakeLists.txt:1` says 3.0 but is never added (only `pss/pss` is, `port/CMakeLists.txt:17`).

## 9. Summary of edits (all guarded, Windows output unchanged)

| File | Edit |
|---|---|
| `CMakePresets.json` | Append `macos-base` (hidden, Darwin condition), `macos-debug`, `macos-debug-asan`, `macos-release` |
| `CMakeLists.txt:34-35` | `if(CMAKE_HOST_WIN32)` around the compiler override |
| `CMakeLists.txt:79-101` | ASan DLL lookup only on WIN32. Flags stay for both |
| `CMakeLists.txt:117-126` | `-gcodeview` / `-Xlinker /DEBUG` only on WIN32 |
| `CMakeLists.txt:131-133` | Set `PLATFORM_BINARY_FOLDER` (`WIN` / `MAC`) before `add_subdirectory("port")` |
| `CMakeLists.txt:805-810` | `sync_files`: keep DoRobo on Windows, `cmake -E copy_directory_if_different` elsewhere |
| `port/Windows/Renderer/Shaders/CMakeLists.txt:5-12` | Apple branch: glslangValidator/dxc from FindVulkan. Output `bin/${PLATFORM_BINARY_FOLDER}/shaders` |
| `port/Windows/Renderer/CMakeLists.txt:197-198` | Same dxc/output-folder source |
| `port/CMakeLists.txt:40-47` | Per-OS source list (#17) |
| `port/DebugMenu/CMakeLists.txt:131` | `Dbghelp.lib` only on WIN32 |
| `port/Windows/Host/CMakeLists.txt:6`, `port/Test/CMakeLists.txt:16` | `AND WIN32` on the ASan DLL copy |
| `port/ext/pss/pss/CMakeLists.txt:11-13` + `src/decode.c:5` | Skip libfmemopen on Apple |
| `src/port/ispc/ispc.cmake:15` | arm64 args (optional) |
| TextureUpload (submodule) | `-msse4.1`/`rt` guards, sse2neon, aligned alloc, MultiISA arm64 |

Out of scope here (source-level, covered by other tickets): Win32 includes in DebugMenu/renderer sources (`DebugCamera.cpp:10`, `NativeRendererRecording.cpp:11` include `windows.h` unguarded; `Callstack.h:9` `DbgHelp.h`), WinRT gamepad, the audio backend, the geometry shader, EDS3 and portability enumeration code.

## Sources

- Repo CMake and sources at the `file:line` references above (branch `docs/research`, submodules at pinned commits).
- LunarG, *Getting Started with the macOS Vulkan SDK* (latest, 1.4.363.0): https://vulkan.lunarg.com/doc/view/latest/mac/getting_started.html (layout `macOS/bin|include|lib|share/vulkan`, universal libraries, DXC in the SPIR-V toolchain, "link only to the Vulkan Loader", portability enumeration opt-in, KosmicKrisp note, vkconfig driver override).
- CMake FindVulkan documentation: https://cmake.org/cmake/help/latest/module/FindVulkan.html. Source: https://github.com/Kitware/CMake/blob/master/Modules/FindVulkan.cmake (`FATAL_ERROR` compatibility shim, implicit `glslangValidator`/`glslc` components, `VULKAN_SDK` hints, Apple MoltenVK hints, `dxc` component since 3.25).
- CMake `-E copy_directory_if_different` (added in 3.26): https://cmake.org/cmake/help/latest/manual/cmake.1.html#run-a-command-line-tool
- PCSX2 arm64 GSVector headers: https://github.com/PCSX2/pcsx2/tree/master/pcsx2/GS (`GSVector4_arm64.h`, `GSVector4i_arm64.h`).
- sse2neon: https://github.com/DLTcollab/sse2neon
- Homebrew formula metadata (`brew info --json=v2`) for cmake 4.4.3, ninja 1.13.2, molten-vk 1.4.2, vulkan-loader/headers/validationlayers 1.4.357.0, glslang 16.6.0, llvm 23.1.2, ispc 1.31.0, glfw 3.5.1. No dxc formula (`brew search dxc`, `brew search directx`).
- Local probes on this Mac: `clang --version`, `clang --print-runtime-dir`, `which cmake ninja glslangValidator glslc dxc ispc`, `sw_vers`, `uname -m`.
