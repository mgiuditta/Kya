# Which non-portable code blocks an arm64 compile

Research for mgiuditta/Kya#20 (map #8, "Kya on macOS"). Checked 2026-09-29 against `docs/research` @ `028d4d4`, with submodules read at their pinned commits (TextureUpload `8dce176`, edBank `d2b70d3`, edFile `b8e68f1`, KyaMesh `aa0bcce`, KyaTexture `263737b`).

**Question:** what in `src/` and `port/` (vendored third-party excluded) stops Kya compiling or running correctly on macOS arm64 (Apple clang, LP64)? Audio (XAudio2), input (WinRT gamepad) and the renderer's Win32 window hook are covered by other tickets (#15, #18, #17) and only get a pointer here.

## How this was checked

Grepping alone can't answer "what stops the compile", because the Windows build uses **clang-cl**, and its MSVC compatibility modes quietly accept a lot of non-standard code. So I compiled the real sources on this Mac (Apple clang 21.0.0, `arm64-apple-darwin25.6.0`):

- **All 295 TUs of the `Kya` target** (the `main_src` list in `CMakeLists.txt`), plus 103 `port/` TUs (audio, input and tests excluded) and the 9 TextureUpload TUs, using `clang++ -fsyntax-only -w`, the same defines as the PC build (`PLATFORM_WIN`, `KYA_USE_PS2_TRIG`, `SKIP_MOVIES`, `DEBUG_FEATURES`, `NOMINMAX`, plus `GLFW_INCLUDE_VULKAN` for port), and C++17 for `Kya` / C++20 for `port` as in CMake. Dependencies came from their pinned submodule commits (spdlog, tracy, recycle, thread-pool, readerwriterqueue, glm, imgui, glfw, json, magic_enum, SPIRV-Reflect) plus Vulkan-Headers `main`.
- **Iteratively on a scratch copy.** Each pass patched the blockers found by the previous one, to expose what they were hiding. The repo itself was not modified.
- **Struct layouts** were compared with `clang -Xclang -fdump-record-layouts-complete` for `--target=x86_64-pc-windows-msvc` against `--target=arm64-apple-darwin`.
- **Clang's MSVC leniencies** were confirmed in LLVM source (`main` @ `f69661de1c6f`): pointer-to-smaller-int is only a warning under `MicrosoftExt` ([SemaCast.cpp ~L2516-2530](https://github.com/llvm/llvm-project/blob/main/clang/lib/Sema/SemaCast.cpp)). Jumps past initialization are only a warning under `MSVCCompat` ([JumpDiagnostics.cpp ~L1041](https://github.com/llvm/llvm-project/blob/main/clang/lib/Sema/JumpDiagnostics.cpp)). `-fdelayed-template-parsing` is on by default for windows-msvc below C++20 ([Driver/ToolChains/Clang.cpp ~L7850-7860](https://github.com/llvm/llvm-project/blob/main/clang/lib/Driver/ToolChains/Clang.cpp)), and C++17 is what `Kya` uses.

What the probe does **not** cover: linking, codegen, or running. Link-time items below come from reading CMake. Two errors unrelated to portability showed up and were ignored: `KyaMesh/src/Mesh.cpp:446,464,470` references members that the current headers don't have, and `DebugMenu/src/DebugSaveLoad.cpp:19` includes `EdenLib/edFile/sources/ps2/WinSaveFile.h`, which is absent at the pinned edFile commit. That looks like stale submodule pins and is worth checking separately.

Facts about the host, measured on this Mac: `sizeof(long)=8`, `sizeof(wchar_t)=4`, `sizeof(long double)=8`, `char` is **signed** (as on Windows; Apple's arm64 ABI differs from Linux here), and a `malloc` result was `0x10583ddf0`. That last one is above 4 GB, because 64-bit macOS reserves the first 4 GB as `__PAGEZERO`. `-msse4.1` fails with `unsupported option '-msse4.1' for target 'arm64-apple-darwin'`, and `<immintrin.h>` fails with `#error "This header is only meant to be used on x86 and x64 architecture"`.

## Summary

| Area | Result |
|---|---|
| Stock tree | **0 / 295** `Kya` TUs compile: every TU includes `Types.h`, which uses `__pragma` and `sprintf_s` |
| After 4 header edits + an MSVC-CRT compat header | 177 / 295 compile. The remaining failures are clang-cl leniencies (pointer truncation, two-phase lookup, goto, and so on) |
| Adding `-fms-extensions` to the `Kya` target on Apple clang | **286 / 295** compile. The other 9 TUs have **31 error sites** that need small source edits (plus 5 template-lookup sites) |
| `-fms-compatibility` on macOS | Not usable: it defines `_MSC_VER`, and Apple's `TargetConditionals.h` then fails every TU ("unknown compiler") |
| SSE / x86 SIMD | Only in the **TextureUpload** submodule (PCSX2 GSVector). `VU0micro.cpp` is already scalar on Windows too. **sse2neon works:** force-including it makes every GSVector4/4i user compile, except one missing macro |
| LP64 `long` | No `long` fields in any struct and no `sizeof(long)`. It compiles, but ~34 sites **behave differently from Windows** (and match the PS2 better) |
| Layout | All 98 `static_assert`s (81 `sizeof`) in src/port hold on arm64, including `edParticles.h:64`. GIFReg/GIF-tag bitfields lay out identically under the MSVC and Apple ABIs. The one real layout break is `edCTextFont.h:41` (`_WIN64`) |
| Windows APIs outside audio/input/window | 6 small sites (GetLocalTime, SetThreadDescription, DbgHelp callstacks, a stray `<Windows.h>`, optional WinHTTP font loader), plus the TextureUpload wrapped-memory allocator |
| Runtime (compiles, then misbehaves) | Backslash asset paths reach `fopen()`. Pointer-truncating round trips crash deterministically (4 GB `__PAGEZERO`). FMA contraction breaks float parity |

Size key: **XS** < 10 lines, **S** 10-50, **M** 50-300, **L** > 300.

## 1. Blockers that break every `Kya` TU (src/, decompiled)

| # | Site | Problem | Fix (Windows-identical) | Size |
|---|---|---|---|---|
| 1 | `src/b-witch/Types.h:10` | `PACK()` uses MSVC `__pragma(pack(push,1))`, which Apple clang rejects ("use of undeclared identifier 'push'"). There are 62 `PACK(` uses in 28 files, but only the macro needs to change | Use the standard `_Pragma("pack(push, 1)") … _Pragma("pack(pop)")`, which clang-cl also accepts. Or keep `__pragma` under `#ifdef _MSC_VER` | XS |
| 2 | `src/b-witch/Types.h:330, 646` | `sprintf_s` inside `#ifdef PLATFORM_WIN` `ToString()` helpers | Compat header (§3) or `snprintf` | XS |
| 3 | `src/b-witch/Types.h:675` | `NAME_NEXT_OBJECT(format, ...)` expands to `…(format, __VA_ARGS__)`. MSVC's preprocessor drops the trailing comma when there are no args; clang doesn't. That gives "expected expression" at **15 call sites in 11 TUs** (BootData.cpp:99,127; CinematicManager.cpp:1709,1716,1919; edText.cpp:136; FrontEndBank.cpp:86,93; FrontEndDisp.cpp:192; MapManager.cpp:245; Pause.cpp:484; SectorManager.cpp:589,610,615; edBank `edBankFile.cpp:524`) | `, ##__VA_ARGS__`. Clang supports it in both modes, and C++20's `__VA_OPT__` isn't available because `Kya` is C++17 | XS |
| 4 | `src/Rendering/edCTextFont.h:41` | `#ifdef _WIN64` picks `int pSubData` (a `STORE_POINTER` handle). On macOS `_WIN64` is undefined, so it falls to `FontPacked_2C* pSubData`: 8 bytes, which **shifts every later field of this packed, file-mapped struct by 4**. `edText.cpp:32` then assigns an `int` handle to a pointer (compile error) | `#ifdef PLATFORM_WIN`, matching the `edText.cpp:31` and `Types.h:755` guards | XS |

(`PLATFORM_WIN` means "PC, not PS2": `CMakeLists.txt:837` sets it for every non-PS2 build, so it is already correct on macOS.)

## 2. clang-cl leniencies in src/ (compile only because Windows uses clang-cl)

| # | Sites | Problem | Fix | Size |
|---|---|---|---|---|
| 5 | **189 diagnostics on ~157 distinct lines in 40 files.** Largest: `ed3D.cpp` 27, `edDlist.cpp` 24, `edPacket.cpp` 23, `ActorHero_Private.cpp` 16, `edCTextFormat.cpp` 13, `CinScene.cpp` 12 | `(int)ptr` / `(uint)ptr` / `reinterpret_cast<int>(ptr)`: "cast from pointer to smaller type loses information". It's an error in standard clang, but only a warning when `MicrosoftExt` is on (clang-cl default) | **Recommended:** add `-fms-extensions` to the `Kya` target for non-MSVC clang. That's zero source edits and the same truncating semantics as Windows (verified: it clears all 189, and Apple's SDK headers still parse). The alternative is to rewrite each as `(int)(intptr_t)p`, also identical on Windows | XS (flag) / M (rewrite) |
| 6 | `src/b-witch/Fx.h:438, 440, 445, 446`; `src/b-witch/ActorWolfen.h:1419` | Template bodies call `edF32Matrix4SetIdentityHard`, `…MulF32Matrix4Hard`, `edF32Vector4SubHard`, `…DotProductHard`, `edF32Matrix4GetInverseOrthoHard` before they're declared. That's only legal under clang-cl's default `-fdelayed-template-parsing` (C++17). Hits 63 TUs | Include or forward-declare the math header in `Fx.h` / `ActorWolfen.h`. `-fdelayed-template-parsing` also works on Apple clang, but it isn't needed | XS |
| 7 | `ActorAton.cpp:1904, 1912`; `ActorFighter.cpp:5734`; `ActorHero_Private.cpp:4173`; `ActorNativShop.cpp:1204`; `FrontEndEnemy.cpp:467`; `edParticles.cpp:838, 877, 925, 992, 1208, 1221` (12) | `goto` jumps past an initialized local. That's a warning only under `MSVCCompat`, which can't be enabled on macOS (see Summary) | Brace-scope the initialized local or split declaration and initialization. Local-variable only, so no layout impact | S |
| 8 | `edParticles.cpp:145-170` (14 lines, `STORE_POINTER(edPartGenBox…)` of a function); `ActorHero_Private.cpp:12404` | Function pointer converted implicitly to `void*` (an MS extension) | Explicit `(void*)` cast | XS |
| 9 | `ActorAton.cpp:4599`; `CinematicManager.cpp:6985, 7075` | `return;` in a non-void function | Return a value (decompiler artefact) | XS |
| 10 | `ActorNativ.cpp:2300` | Pointer compared with `false` | Compare with `nullptr` | XS |
| 11 | `src/b-witch/edMem.cpp:6` (`<malloc.h>`); `src/Rendering/Font.cpp:6-7` (`<corecrt_malloc.h>`, `<corecrt_math.h>`) | Windows-only CRT headers (macOS only has `<malloc/malloc.h>`) | `<stdlib.h>` / `<math.h>`. `kya.cpp:18`'s `<malloc.h>` is inside the PS2 block | XS |

With #1-#11 applied (and the §3 compat header), all **295/295** `Kya` TUs pass on Apple clang. #5 is the only one where I verified the flag route rather than editing each site.

**Runtime caveat for #5.** About 38 of the casts smuggle ints through `void*` message params (`(int)pMsgParam`) and about 22 test alignment (`(uint)p & 0xf`); both are harmless. About 30 **rebuild a pointer from the truncated int**, for example `edPacket.cpp` `*(byte*)((int)pPacket->field_0xc + 3)` and `ed3D.cpp` packet writers. On macOS the heap always sits above 4 GB, so any such site that runs will fault every time. On Windows it only works if the address happens to be below 4 GB. These need finding (grep the probe's cast list, or a trap on first execution) before the game can run reliably. That's M if many turn out to be live.

## 3. MSVC CRT names and extensions (src/ + port/)

A single header, used only when `!defined(_MSC_VER)` so the Windows build doesn't change, covers all of these. Wire it in through `Types.h`, `port.h` and `renderer.h`, or force-include it from CMake. **S** (~40 lines). For comparison, `Pcsx2Defs.h` already does this for `__forceinline` and `__noinline`.

| Name | Sites |
|---|---|
| `_aligned_malloc` / `_aligned_free` | `src/EdenLib/edSys/sources/EdSystem.cpp:50`; `port/Windows/Renderer/include/renderer.h:32-33, 39-40`; `Vulkan/src/Objects/VulkanBuffer.h:81, 86`; `Vulkan/src/VulkanRenderer.cpp:1264, 1300` → `posix_memalign` + `free` |
| `_aligned_realloc` | `VulkanRenderer.cpp:1278` (Vulkan `pfnReallocation` callback) → allocate, copy `malloc_size(old)`, free (no POSIX equivalent) |
| `__forceinline` | `port/Windows/Renderer/include/GIFReg.h:207-208`; `Texture/TextureCache.cpp:205, 282, 423, 507, 720, 922, 928`; `pcsx2/Selectors.h:22`. Today these work on non-MSVC only when `Pcsx2Defs.h` happens to be included first. `win_main.cpp` fails on `GIFReg.h` |
| `__assume` | `renderer.h:472, 555` → `__builtin_unreachable()` |
| `sprintf_s` | `renderer.h:598, 605`; DebugMenu `DebugMesh.cpp:236, 279, 353, 430, 476`, `DebugTexture.cpp:444, 679`, `DebugFrameBuffer.cpp:95`, `DebugMaterialPreviewer.cpp:152`, `Actor/DebugActorBehaviour.cpp:122` (+ Types.h, #2) |
| `strcpy_s` | `port/Archive/archive.cpp:33`; `DebugMenu/src/DebugCamera.cpp:82` |
| `unsigned __int64` / `DWORD64` | `DebugMenu/src/DebugCallstackPreviewer.h:6` (used in `DebugMaterialPreviewer.h:21`, etc.) → `uint64_t` |
| `#pragma warning` | `port/pcsx2/VU/src/Pcsx2Defs.h:319-320`, already under `_MSC_VER`. `#pragma clang optimize` (input.cpp, edParticles) is fine |

Not found anywhere in src/ or port/ (outside vendored code): `__declspec` (except under `_MSC_VER` in VU `Pcsx2Defs.h`), `__debugbreak`, `fopen_s`, `_stricmp`, `__cdecl`/`__stdcall`, `__super`, `__try`, `_Interlocked*`, `_BitScan*`, `__cpuid`, inline asm on x86. `_access` in `edBankBuffer.h:70` is a method name, and the `Sleep`/`QueryPerformanceCounter` hits in `VideoD.cpp` are in comments.

### Other port-side two-phase / conformance errors (C++20, found by the probe)

| Site | Problem | Fix | Size |
|---|---|---|---|
| `Vulkan/src/Objects/VulkanBuffer.h:66, 126, 128, 186, 188, 247, 249` | `GetDevice()` / `GetPhysicalDevice()` used in templates before they're declared | Include `VulkanRenderer.h` or forward-declare | XS |
| `Vulkan/src/Objects/UniformBuffer.h:151, 155, 159, 213` | Same for `GetCurrentFrame()` | Same | XS |
| `DebugMenu/src/DebugSetting.h:39` | `(*settings)[name].get<SettingType>()` in a template needs `.template get<…>()` | Add `template` | XS |
| `DebugMenu/src/Actor/DebugActorBehaviour.cpp:773, 811` | `static` definitions of functions previously declared non-static | Drop `static` or fix the declaration | XS |
| `src/b-witch/Fx.h:544` (reached from DebugMenu TUs, which don't get `Kya`'s flags) | `reinterpret_cast<int>(pFx) - reinterpret_cast<int>(pCVar1)` | `(pFx - pCVar1)` or `intptr_t` casts, or `-fms-extensions` on DebugMenu too | XS |

## 4. SSE / x86 SIMD

| # | Site | Finding | Fix | Size |
|---|---|---|---|---|
| 12 | `port/pcsx2/VU/src/VU0micro.cpp:45-49` | `_mm_store_si128` under `#ifdef _M_X86`. Nothing defines `_M_X86`: it's PCSX2's own macro, MSVC defines `_M_X64`/`_M_AMD64`, and Kya's CMake doesn't set it. So the **scalar `#else` already runs on Windows**. All 15 VU TUs compile cleanly on arm64 | None | 0 |
| 13 | TextureUpload `src/common/VectorIntrin.h:8-44` | Chooses the intrinsics header by `_M_X86` / `_M_ARM64`, and Apple clang defines neither (it uses `__aarch64__`). On Windows the intrinsics arrive only through `<intrin.h>` under `_MSC_VER`, and `_M_SSE` is 0. On macOS no SIMD header is included, so `__m128i`, `_mm_*` and `_MM_SHUFFLE` are unknown in **20 port TUs** (everything that includes `GSVector.h` through `VulkanPS2.cpp`, `TextureCache.cpp` or `Objects/UniformBuffer.h`) and in all TextureUpload TUs | **sse2neon** (header-only, MIT, [DLTcollab/sse2neon](https://github.com/DLTcollab/sse2neon) v1.9.1): add `#elif defined(__aarch64__)` → `#include "sse2neon.h"`. Verified: with it force-included, GSVector4/GSVector4i code in all 20 port TUs and 9 TextureUpload TUs compiles, except for one x86 helper macro (#14) | S |
| 14 | TextureUpload `src/GSVector4.h:521` | `_MM_MK_INSERTPS_NDX` isn't provided by sse2neon | `#define _MM_MK_INSERTPS_NDX(s,d,m) (((s)<<6)|((d)<<4)|(m))` | XS |
| 15 | `GSVector8.h` / `GSVector8i.h` (AVX/AVX2) and `TextureCache.cpp:50` `#if _M_SSE >= 0x501` | Compiled out because `_M_SSE` = 0. It's **already dead on Windows**, so the SSE4 `#else` path is what ships | None | 0 |
| 16 | TextureUpload `src/MultiISA.cpp:5-87` | `Xbyak::util::Cpu` (x86 CPUID) picks SSE4/AVX/AVX2. xbyak is x86-only | Under `__aarch64__`, return a fixed `ProcessorFeatures{}`, skip the xbyak include, and don't link `xbyak` (`CMakeLists.txt` `add_subdirectory("ext/xbyak")`/`target_link_libraries(... xbyak)`) | S |
| 17 | TextureUpload `CMakeLists.txt` (Clang branch) | `-msse4.1` is a hard driver error on arm64 (verified), and `target_link_libraries(TextureUpload rt)` for `NOT WIN32` fails because macOS has no `librt` | Guard on `CMAKE_SYSTEM_PROCESSOR` x86 and skip `rt` on `APPLE` | XS |

Alternative to sse2neon: PCSX2 upstream now has native NEON versions, `pcsx2/GS/GSVector4i_arm64.h` (~75 KB) and `GSVector4_arm64.h` (~20 KB), added in commit `0a4c037898` "GS: ARM64 compatibility" (2024-03-21). It's selected by `ARCH_ARM64` in upstream `GSVector.h:106-108` @ `646df006`. Kya's fork is a 2023 snapshot, so backporting means reconciling API drift: **M-L**. Start with sse2neon (a mechanical 1:1 translation, fine for an upload/swizzle path), and backport only if profiling asks for it. GLM (`GLM_FORCE_INTRINSICS`, `GLM_FORCE_DEFAULT_ALIGNED_GENTYPES`) and xxHash (`XXH_INLINE_ALL`) detect NEON on their own, and both compiled cleanly.

## 5. LP64 vs LLP64 and layout

| # | Site | Finding | Action | Size |
|---|---|---|---|---|
| 18 | `src/Rendering/edCTextFont.h:41` | The only `_WIN64` test in src/port. Layout break, see #4 | #4 | XS |
| 19 | `src/EdenLib/Include/edParticles/edParticles.h:64` | `static_assert(sizeof(PackedType<void*>) == 4)`. `PackedType` stores an `int32_t` handle, so it **holds on arm64** (the probe compiled every includer). Same for all 98 `static_assert`s in src/port | None | 0 |
| 20 | `long` in general: 398 lines in 114 files (`ed3D.cpp` 43, `ActorWolfen.cpp` 25, `ActorHero_Private.cpp` 25, `edParticles.cpp` 19, …) | **No `long` struct fields** in any header, no `sizeof(long)`, and no serialized `long`. Uses are Ghidra locals, casts, and function parameters (`long mode`, `long param_N`), which compile identically | None to compile | 0 |
| 21 | 33 sites shift a `(long)` by ≥ 32: `ed3D.cpp` 15 (3764-4674, 12197, packet `cmdA/cmdB` words), `ActorWolfen.cpp:11462, 11568, 11573, 11598, 11613` (`(long)((ulong)x << 0x3b) < 0` bit tests), `edParticles.cpp:947, 1569, 1580, 1581` (`<< 0x29 >> 0x29` mantissa masks), `kya.cpp:1196, 1198, 1200`, `IniFile.cpp:92, 97` and `light.cpp:621` (`<< 0x38 >> 0x38` sign extension), `CinematicManager.cpp:4905`, `PathFinder.cpp:764`, `ActorAutonomous.cpp:1675`; plus `edVideo/VideoB.cpp:146` (`int7`/`uint7`, which `Types.h:45-46` defines as `long`) | Ghidra models the EE's `long` as 64-bit, and these idioms only make sense at 64 bits. On Windows (`long` = 32 bits) they are UB or truncated: the ActorWolfen tests are **always false**, and the masks and sign extensions are wrong. On macOS they compute what the PS2 did. **They compile, but macOS will not behave like Windows here.** The Windows build is the buggy one | Don't block the port. Open a follow-up that changes these ~34 expressions to `long long`/`ulong`. That fixes Windows and makes both platforms agree; it's a deliberate Windows behaviour change, so it needs its own ticket | S |
| 22 | `Types.h:40, 45, 46` (`undefined5`, `int7`, `uint7` = `long`) | 4 bytes on Windows, 8 on macOS. Only used at `VideoB.cpp:146`, and in no struct | Covered by #21 | 0 |
| 23 | Bitfields / ABI | MSVC and Itanium lay out bitfields differently. Checked the risky ones: `port/include/port.h:162-173` / `src/port/vu1_emu.cpp:67-77` `HW_Gif_Tag` (mixed `u16`/`u32`), `src/edVideo/VideoB.h:17-26` `tGS_DISPLAY2`, and all 36 records in `GIFReg.h`. All have **identical offsets and sizes** under `x86_64-pc-windows-msvc` and `arm64-apple-darwin` | None | 0 |
| 24 | Pointer width, `wchar_t`, `long double`, `char` | Pointers are 8 bytes on both, and serialized pointer fields are 32-bit `STORE_POINTER` handles on both. `wchar_t` (2 vs 4) is only used in the Windows-only GoogleFontLoader. `long double` is 8 on both. `char` is signed on both | None | 0 |

## 6. Windows headers and APIs outside audio / input / window

| # | Site | API | Fix | Size |
|---|---|---|---|---|
| 25 | `port/Windows/windows_impl.cpp:2-9` (always built, in the `Port` lib) | `GetLocalTime` for `sceScfGetLocalTimefromRTC` | `std::time` + `localtime_r` | XS |
| 26 | `Vulkan/src/Native/NativeRendererRecording.cpp:11, 483` | `<windows.h>`, `SetThreadDescription(thread.native_handle(), L"RenderThread")` | `pthread_setname_np("RenderThread")`. On macOS it only names the *calling* thread, so move it into `Run()` | XS |
| 27 | `DebugMenu/include/Callstack.h` (whole file `_WIN32`-guarded: DbgHelp `SymFromAddr`, `CaptureStackBackTrace`); `DebugCallstackPreviewer.h:6`; `DebugMenu/CMakeLists.txt:131` links `Dbghelp.lib` unconditionally | Callstack capture for draw/material previewers | `<execinfo.h>` `backtrace()` + `dladdr`, or stub it out, and make the `Dbghelp.lib` link `WIN32`-only | S |
| 28 | `DebugMenu/src/DebugCamera.cpp:9-10` | Includes `<Windows.h>` but makes no Win32 calls | Remove the include | XS |
| 29 | `DebugMenu/src/GoogleFontLoader.cpp` (only with `ENABLE_GOOGLE_FONT_LOADER`, `DebugMenu/CMakeLists.txt:133-137`) | WinHTTP, `MultiByteToWideChar` | Leave it off on macOS. Porting it to libcurl/NSURLSession is optional | 0 (M optional) |
| 30 | TextureUpload `src/TextureUpload.cpp:4, 7, 82-144` | `GSAllocateWrappedMemory` / `GSFreeWrappedMemory`: `CreateFileMapping`, `VirtualAlloc2`, `MapViewOfFile3`, `#pragma comment(lib, "mincore")`. `GSLocalMemory.cpp:52` needs it for the 4× mirrored 4 MB VRAM | Port PCSX2 upstream's POSIX version: `shm_open` + `mmap(MAP_FIXED)` repeat mapping, `pcsx2/GS/GS.cpp:1038-1083` @ `646df006`. 4 MB is a multiple of Apple Silicon's 16 KB page | S |
| 31 | TextureUpload `GSAlignedClass.h:19, 24, 40, 45`; `UploadBuffer.cpp:4, 11`; `GSLocalMemory.cpp:241, 243, 275, 328, 637, 663`; `GSClut.cpp:17, 98` | `_aligned_malloc` / `_aligned_free` (13 sites) | Shim in TextureUpload `Pcsx2Defs.h` for non-MSVC | XS |
| — | Already guarded or out of scope | `Vulkan/src/DrawTrace.cpp:7, 70` (`_WIN32`); `Texture/TextureUpscale.cpp:207` `OutputDebugStringA` (ONNX-only); `DebugMenu/src/DebugRendererVulkan.cpp:28-90, 204` WndProc hook (window area, `_WIN32`-guarded); `port/Audio/*` XAudio2 (#15); `port/Windows/Input/gamepad.cpp` (#18); `port/Test/src/windows_save_tests.cpp:179-180` (`GetTempFileNameW`, test-only); the root `CMakeLists.txt` `sync_files` robocopy step (#19) | — | — |

## 7. Compiles, but won't run correctly

| # | Site | Problem | Fix | Size |
|---|---|---|---|---|
| 32 | edFile `sources/ps2/_edFileFilerCDVD.cpp:173-180` (PC branch `fopen(pcFileFull, "rb")`), fed PS2 paths such as `cdrom0:\CDEURO\LEVEL\…`. Also backslash literals at `kya.cpp:1442, 1445` (`"CDEURO\\FRONTEND\\SPLASH_N.RAW"`), `CinematicManager.cpp:1432, 1442, 1525, 6202`, `Audio.cpp:3161`, `SaveManagement.cpp` (10 lines, save paths through the MCard filer), `CompatibilityHandlingPS2.cpp:16` | Windows treats `\` as a separator, but on macOS it's a filename character, so **every asset open fails** | Normalize `\` → `/` once in the PC branches of the CDVD and MCard filers and in `LoadFileFromDisk`, not at each literal. The default APFS volume is case-insensitive, so case mismatches stay hidden unless someone uses a case-sensitive volume | S |
| 33 | ~30 pointer-rebuilding casts (#5) | Guaranteed fault on macOS if reached (heap above 4 GB) | Audit and replace with `STORE_POINTER`/`intptr_t` as they're hit | S-M |
| 34 | Floating point | On arm64, Apple clang fuses `a*b+c` into FMA by default (`-ffp-contract=on`). The x86-64 Windows build has no FMA target feature, so it never fuses. Results differ in the last bits (physics, camera, anything compared for equality) | Add `-ffp-contract=off` for Apple arm64 to match Windows | XS |
| 35 | Threads (render thread, `port/pcsx2/VU/src/MTVU.cpp`, `port/Job`, audio) | arm64 memory ordering is weaker than x86 TSO, so data races that are benign on x86 can surface. No plain `volatile` cross-thread flags were found (the `volatile` hits are PS2 register macros and `mfc0` asm), and `bShouldStop` is used under a condition variable | Run a ThreadSanitizer pass once it links | unknown |

## Totals

| Bucket | Sites | Size |
|---|---|---|
| src/ first-order blockers (#1-4) | 4 edits in 2 headers | XS |
| src/ clang-cl leniencies (#6-11) | 36 source sites (5 template, 12 goto, 15 fn-ptr, 3 `return;`, 1 compare) + 3 includes | S |
| src/ pointer truncation (#5) | 189 diagnostics / ~157 lines / 40 files | XS with `-fms-extensions` (M to rewrite) |
| MSVC CRT/extension compat header (§3) | ~40 call sites across src/port | S |
| port/ conformance (§3 table) | 15 sites | XS-S |
| SIMD / TextureUpload (#13-17) | 5 edits (sse2neon hook, 1 macro, MultiISA, CMake flags/libs) | S (M-L for native NEON) |
| Windows APIs (#25-31) | 7 sites | S |
| Runtime (#32-34) | path normalization, pointer audit, FP contraction | S-M |
| LP64 `long` divergence (#21) | ~34 expressions | S (separate ticket; changes Windows) |

About **150-250 changed lines**, excluding the optional GoogleFontLoader port and a native-NEON GSVector backport. None of it needs layout changes to decompiled structs, and everything except #21 can be done without changing the Windows build (guard on `!_MSC_VER` / `__APPLE__` / `__aarch64__`, or use constructs that clang-cl accepts identically). **Decision needed from #19 (CMake):** whether the macOS `Kya` target gets `-fms-extensions`, which I recommend, versus rewriting the 189 casts.
