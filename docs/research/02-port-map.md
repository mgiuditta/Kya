# 02 — Port Map: How PS2 Hardware Is Replaced on PC

## TL;DR

1. **GS/GIF/DMA are not emulated.** On PC the `SCE_GS_SET_*` / `SCE_GIF_SET_TAG` macros turn into calls to `Renderer::Set*` that only record GS state (`port/include/port.h:95-106`, `port/include/port.h:394-406`). DMA sends compile to no-ops, and the old GS-emulating draw path is stubbed out (`port/Windows/Renderer/Vulkan/src/VulkanPS2.cpp:457-470`, commit `ebae900`). What actually draws the game is a **"Native" Vulkan renderer**: `ed3D` hands it mesh nodes and matrices directly (`src/b-witch/ed3D.cpp:3007`, `src/b-witch/ed3D.cpp:3636`).
2. **VU1 microcode is reimplemented in C++** (`Vu1Core`, `src/port/vu1_emu.cpp:233-2540`). A PCSX2 VU interpreter is kept as an optional fallback (`src/port/vu1_emu.cpp:3049-3056`). The resulting vertices go into buffers whose `Draw` does nothing, so this path now looks vestigial. **VU0** is plain C++ math in the decompiled code.
3. **IOP/SPU2/.irx are replaced at the RPC boundary.** `port/Audio` offers a PC "transfer service", a PS-ADPCM/VAG decoder, a software sequencer/synth for music, and **XAudio2** output (`port/Audio/edSoundStreamService.cpp:17-21`, `port/Audio/TransferServicePlan.md:5-10`).
4. **Pointers:** 32-bit fields in serialized PS2 data store **handles, not pointers**. `STORE_POINTER` puts the host pointer in a generational slot table and returns a 32-bit key (20-bit slot, 11-bit generation, 1 transient bit), and `LOAD_POINTER` resolves the key back (`src/port/pointer_conv.h:37-46`, `src/port/pointer_conv.cpp:12-17`).
5. **Portability:** window, surface and ImGui already go through **GLFW**. Everything that is Win32-only is concentrated in six places: XAudio2 audio, WinRT gamepad, a few DebugMenu files (WndProc hook, DbgHelp, clipboard, WinHTTP), MSVC-isms (`_aligned_malloc`, `sprintf_s`, `__forceinline`, `__pragma`), and a Windows-only build (`.exe` shader tools, robocopy, presets). The biggest **macOS** blocker is the renderer's hard requirement on `geometryShader`, which MoltenVK does not support (`port/Windows/Renderer/Vulkan/src/VulkanRenderer.cpp:131`).

> Checkout caveat: most submodules are **not initialized** in this working tree. That includes `port/KyaMesh`, `port/KyaTexture`, `src/EdenLib/edFile`, `src/EdenLib/edBank`, glfw, imgui and spdlog (`git submodule status` shows `-`), and the checked-out `TextureUpload` gitlink is empty. Anything said below about those modules comes from their call sites and CMake wiring, not from reading their source. Those claims are marked *(inferred)*.

---

## 1. Build wiring (CMake)

| Item | Where |
|---|---|
| Every non-PS2 configure is treated as a "Win Build". `port/` is only added in that branch | `CMakeLists.txt:67-68`, `CMakeLists.txt:77`, `CMakeLists.txt:131` |
| Compiler forced to `clang` for C and C++ | `CMakeLists.txt:34-35` |
| PC-only shims added to `Kya`: `vu1_emu`, `hardware_draw`, `debug_draw`, `random`, `input` | `CMakeLists.txt:740-752` |
| `pointer_conv.cpp` is built on every platform (on PS2 it is a macro passthrough) | `CMakeLists.txt:558-559` |
| On PC `Kya` is a **static library**; on PS2 it is an executable | `CMakeLists.txt:771` (PS2 branch just above) |
| `PLATFORM_WIN` is PUBLIC on `Kya`; `KYA_USE_PS2_TRIG` is private | `CMakeLists.txt:837`, `CMakeLists.txt:840` |
| `Kya` links `Port Log VU Job steinwurf::recycle` | `CMakeLists.txt:862` |
| `SKIP_MOVIES DEBUG_FEATURES` are always defined | `CMakeLists.txt:888` |
| Asset sync uses `DoRobo.bat` (robocopy) | `CMakeLists.txt:805-810`, `DoRobo.bat:2` |
| Host exe and tests are added | `CMakeLists.txt:895-896` |
| **`Kya_Win`** exe: `win_main.cpp`, output name `Kya_$<CONFIG>` (hence "Kya_Debug") | `port/Windows/Host/CMakeLists.txt:3-4`, `port/Windows/Host/CMakeLists.txt:15` |
| **`KyaPortTest`**: gtest, 5 test sources, registered with CTest | `port/Test/CMakeLists.txt:10-11`, `port/Test/CMakeLists.txt:26-28` |
| `KyaPortTest` sets `HEADLESS` on the shared `Renderer` target. No source uses `#ifdef HEADLESS`; headless mode is a runtime switch, `Renderer::SetHeadless(true)` | `port/Test/CMakeLists.txt:23`, `port/Test/src/tests.cpp:546`, `port/Windows/Renderer/Vulkan/src/VulkanRenderer.cpp:1341` |
| `Port` library: `windows_impl.cpp`, `gamepad.cpp`, `object_naming`, `input_functions`. It links `Renderer pss Tracy Texture Mesh Sprite Audio` | `port/CMakeLists.txt:40-53` |
| Sub-libraries: Pool, Log, Job, Hash, Archive, Audio, pss, tracy, readerwriterqueue, VU, Renderer, KyaTexture, KyaMesh, Sprite, DebugMenu | `port/CMakeLists.txt:10-38`, `port/CMakeLists.txt:55` |
| `Renderer` links glfw, Vulkan, SPIRV-Reflect, imgui, Log, Hash, Archive, readerwriterqueue, Tracy, glm, TextureUpload | `port/Windows/Renderer/CMakeLists.txt:186-187` |
| Shaders: GLSL is compiled with `$ENV{VULKAN_SDK}/Bin/glslangValidator.exe` and HLSL with `dxc.exe` | `port/Windows/Renderer/Shaders/CMakeLists.txt:5-11`, `port/Windows/Renderer/Shaders/CMakeLists.txt:44-70` |
| Build has a cycle: `Sprite` links `Kya`, and `Kya` → `Port` → `Sprite` | `port/Sprite/CMakeLists.txt:11`, `port/CMakeLists.txt:53` |
| Presets: `windows-base` only runs when `hostSystemName == Windows`; `linux-debug` is the PS2 toolchain | `CMakePresets.json:5-18` |

Entry point (`port/Windows/Host/src/win_main.cpp:20-34`) runs these steps in order: `Renderer::Setup` → `DebugMenu::SetupRenderer` → `Renderer::Native::Setup` → `DebugMenu::Init` → `TextureLibrary::Init` / `MeshLibrary::Init` → keyboard/mouse and gamepad hookup → `main_internal` (the decompiled game main) → `Audio::Shutdown`.

The game code itself contains **336 `PLATFORM_WIN` sites across 83 files**; `src/b-witch/ed3D.cpp` alone has 89.

---

## 2. GS + GIF + DMA → Vulkan

### 2.1 GS register writes become C++ calls (state capture only)
- `port/include/port.h` replaces the Sony `libgraph`/`libdma` headers on PC. It defines a DMA tag struct and emulates `sceDmaAddEnd/Call/Ref` by writing plain tag words (`port/include/port.h:26-66`).
- Under `PLATFORM_WIN`, every `SCE_GS_SET_*` macro still returns the packed 64-bit value **and** calls into the renderer as a side effect:
  - `SCE_GIF_SET_TAG` calls `SetGIFTAGWin`, which calls `Renderer::SetPrim` when `PRE` is set (`port/include/port.h:94-106`)
  - `TEX0` calls `Renderer::SetTEX` (`port/include/port.h:336-340`)
  - `ST`, `XYZ` (which kicks a vertex), `ALPHA`, `FRAME`, `TEST`, `RGBAQ`, `ZBUF`, `XYOFFSET`, `SCISSOR`, `PRIM` and `COLCLAMP` all have matching wrappers (`port/include/port.h:366-598`)
- The DMAC register addresses (`D0_CHCR`…) and the `SCE_VIF1_SET_*` encoders are kept as plain constants and macros (`port/include/port.h:624-739`).
- The `Renderer::Set*` functions write into a `PS2::GSState` object (`port/Windows/Renderer/Vulkan/src/VulkanPS2.cpp:124-215`, `port/Windows/Renderer/include/GSState.h`). Register bitfield types live in `port/Windows/Renderer/include/GIFReg.h`.
- **The draw is stubbed.** `Renderer::Draw()` and `Draw(drawBuffer, …)` have empty bodies (`port/Windows/Renderer/Vulkan/src/VulkanPS2.cpp:457-470`). Commit `ebae900` ("Remove legacy Vulkan PS2 draw path") deleted 263 lines and `VulkanHardwarePS2.*`. What survives is the plumbing: GS state, vertex kicking and strip handling (`KickVertex`/`UpdateXyTail`/`TraceUpdateSkip`, `port/Windows/Renderer/include/renderer.h:399-557`), a PCSX2-style sampler selector (`VulkanPS2.cpp:416-440`), and uniform buffers.
- DMA sends are no-ops on PC because their bodies sit inside `#ifdef PLATFORM_PS2` (`src/Rendering/CustomShell.cpp:357`, `:382`, `:409`, `:436`).
- The scratchpad (0x70000000) becomes an ordinary global struct (`src/b-witch/ed3DScratchPadGlobalVar.cpp:52-59`), and `SCRATCHPAD_ADDRESS` rebases offsets onto it (`src/b-witch/ed3D.h:62-69`).

### 2.2 The "Native" renderer (the path that actually renders)
- It is the primary path, not an experiment. `docs/renderer-refactor/*` reviews it as the production renderer (`docs/renderer-refactor/README.md:1-18`, `docs/renderer-refactor/08-native-draw-granularity.md:1-20`), and a real capture shows 821 draws in the main native pass.
- Source: `port/Windows/Renderer/Vulkan/src/Native/*` (`port/Windows/Renderer/CMakeLists.txt:47-67`).
- Hooks from the game:
  - 3D meshes: `Renderer::PushGlobalMatrices` (`src/b-witch/ed3D.cpp:3007`), `PushAnimMatrix` (`:3458`), `Kya::GetMeshLibrary().RenderNode` and `GetTextureLibrary().BindMaterialLayer` (`:3636-3637`), `Native::PushMatrixPacket` (`:3167`, `:3192`)
  - Shadows (`:2382`, `:5672`, `:7462-7594`), framebuffer capture (`:7162-7226`), clear modes (`src/edVideo/Viewport.cpp:241-264`), fade and flip (`src/edVideo/VideoD.cpp:125`, `:280`)
  - Projection matrix is built natively from camera extents (`src/port/NativeProjection.h:5-16`)
- Meshes and textures come from the **KyaMesh** / **KyaTexture** submodules (`Mesh.h` / `Texture.h`, `src/b-witch/ed3D.cpp:46-48`). *(inferred)* They decode G3D/G2D bank data straight into `SimpleMesh` / `SimpleTexture` (`port/Windows/Renderer/include/renderer.h:205-266`) and skip the VIF/VU/GIF packet stream entirely. Display-list strips are cached through `GetMeshLibraryMutable().CacheDlistStrip` (`src/b-witch/edDlist.cpp:3210`).
- 2D/HUD goes through the `DISPLAY_LIST_*` macros (`port/Windows/Renderer/include/displaylist.h:24-30`, used at `src/b-witch/edDlist.cpp:1436`) into `NativeDisplayList.cpp:507-598`. That pipeline uses a **geometry shader** (`NativeDisplayList.cpp:419`) to expand GS SPRITE line pairs into quads (`Shaders/src/displaylist.geom.glsl:3-4`).
- Sprite nodes: `port/Sprite` decodes the sprite GIF batches into renderer vertices (`port/Sprite/src/Sprite.cpp:183-272`); it is called from `src/b-witch/ed3D.cpp:5429`.
- GS blending (`ALPHA` A/B/C/D + FIX) maps to Vulkan blend state, using dynamic state where possible (`port/Windows/Renderer/Vulkan/src/Native/Blending.h:22-30`).
- Command recording runs on its own `RenderThread` (`NativeRendererRecording.cpp:480`) fed through `readerwriterqueue` (`NativeRendererRecording.cpp:10`).
- Post-processing (alphafix, fade) is in `Native/PostProcessing.cpp:254-255`.

### 2.3 Texture cache, framebuffers, pipelines, shaders
- **Texture cache:** `Vulkan/src/Texture/TextureCache.cpp` uses PCSX2-derived block and column reads plus CLUT expansion (`TextureCache.cpp:205`, `:282`, `:666-742`, `:922`) built on `GSVector4i` SSE vectors from `GSVector.h` (`TextureCache.cpp:7`, `:76-85`). *(inferred)* `GSVector.h` and `UploadBuffer.h` come from the `TextureUpload` submodule (`TextureCache.h:10`, `port/Windows/Renderer/CMakeLists.txt:14`). The cache produces `PS2::GSSimpleTexture` (`TextureCache.h:36-60`, `TextureCache.cpp:1072-1086`). An optional AI upscale (ncnn/ONNX) is available (`port/Windows/Renderer/CMakeLists.txt:112-174`, `Texture/TextureUpscale.cpp`).
- **Framebuffers** are keyed by the GS `FBP` (`Vulkan/src/Objects/FrameBuffer.h:53-57`).
- **Pipelines:** PCSX2 `Selectors.h` / `VKBuilders.*` (`port/Windows/Renderer/Vulkan/src/pcsx2/`). SPIR-V reflection drives the descriptor layouts (`VulkanReflect.cpp`). Native pipelines are keyed by `PipelineKey` (`Native/NativeRenderer.h:30-69`), and MD5 (`port/Hash/hash.h`) serves as the pipeline/shader cache key (`Objects/Pipeline.cpp:2`).
- **Shaders:** GLSL in `Shaders/src/*.glsl` plus a PS2 HLSL uber-shader (`Shaders/src/ps2/ps2.hlsl`). All are compiled to `bin/WIN/shaders/*.spv` at build time. Files used at runtime: `native.*`, `shadow_*`, `displaylist.*` (with `.geom`), `postprocess`/`alphafix`/`fade`, and debug lines (`Native/NativeRenderer.cpp:35-49`, `Native/PostProcessing.cpp:78`). A `ShaderCompiler` tool packs PS2 shader variants through `Archive` (`Shaders/compiler/src/ShaderCompiler.cpp:92`, `:104-113`, `:263`).
- **Required device features:** `geometryShader`, `dualSrcBlend`, `synchronization2`, `colorWriteEnable`, and EDS3 colorBlendEnable/Equation/WriteMask (`VulkanRenderer.cpp:129-140`, checked at `:184`, enabled at `:727`).

---

## 3. VU0 / VU1

- **VU1: reimplemented in C++ and hand-translated per microprogram.**
  - `VU1Emu::ProcessVifList` walks the VIF list (STCYCL/STMASK/STROW, UNPACK into a 16 KB fake VU mem, MSCAL). Code refs: `src/port/vu1_emu.cpp:32-34`, `:2969-3100`.
  - On `MSCAL` it runs `Vu1Core::RunCode(addr)`, which dispatches to routines such as `_$DrawingStart_XYZ32`, `_$Clipping`, `_$GouraudMapping_No_Fog_16_2`, `_$XYZW_16_ConvBones_Rigid`, `_$ParallelLightning_addcolor` and `_$Flare_Start` (`src/port/vu1_emu.cpp:233-2540`, `:2719-2735`).
  - The routines emulate `XGKICK` by calling `Renderer::KickVertex` (`src/port/vu1_emu.cpp:871-906`, `:2836-2843`).
  - Optional ISPC kernels exist for these routines (`src/port/ispc/vtx.ispc`, `CMakeLists.txt:756-762`); the ISPC call is commented out (`vu1_emu.cpp:1391`).
- **Alternative VU1 modes** (runtime flags at `src/port/vu1_emu.cpp:38-44`):
  - `bEnableInterpreter` runs the original microcode uploaded by `VU1Emu::SendVu1Code` (`src/b-witch/ed3D.cpp:1723`) on the **PCSX2 VU interpreter**, `port/pcsx2/VU` (`port/pcsx2/VU/include/PCSX2_VU.h:5-13`, `src/port/vu1_emu.cpp:3049-3056`).
  - `bEnableHardwareDraw` skips VU processing and reads the GIF packet straight out of VU memory (`src/port/hardware_draw.cpp:9-38`).
  - Multithreaded mode uses `BS::thread_pool` (`src/port/vu1_emu.cpp:57`) with `recycle::shared_pool` pools (`src/port/vu1_emu.cpp:2906-2907`).
- **Status:** `ed3D` still feeds VU1Emu on PC (`src/b-witch/ed3D.cpp:4222-4226`, `:6813`), but the vertices it produces end up in `Renderer::Draw()`, which is empty (see §2.1). *(inferred)* This path now only costs CPU and serves debugging/tracing. `bEnableEmulatedRendering=false` short-circuits it (`src/port/vu1_emu.cpp:2725`).
- **VU0:** the macro-mode math from the decompiled code is plain C++ (`src/b-witch/MathOps.h:6-11`), with `libvu0.h` included on PS2 only. PS2-exact trig lives in `Ps2Trig.h`, enabled by `KYA_USE_PS2_TRIG` (`CMakeLists.txt:840`). `sceDevVu0Reset` and similar calls are PS2-only.
- `src/PSX2/EdenLib/edPacket/edpacket.cpp` is a 4-line placeholder and is not built.

---

## 4. IOP / Sound (SPU2, `edSys.irx`) → `port/Audio`

- **Contract:** reproduce the EE↔IOP transfer semantics (ordered indices, owned data, main-thread callbacks) *without* emulating SIF RPC, DMA or IOP threads (`port/Audio/TransferServicePlan.md:5-15`).
- **Transfer service:** `Audio::Submit/PumpThrough/PumpAll/LookupLoadedData/ReleaseLoadedData` (`port/Audio/edSysTransferService.h:26-36`). It is wired into the decompiled `_edSysDataTransfer` in place of `sceSifSendCmd` (`src/EdenLib/edSys/sources/ps2/_edSysDataTransfer.cpp:14-30`, `:119-121`). Values returned to `ed_sound_sample` are opaque handles, not pointers (`TransferServicePlan.md:12-13`).
- **SFX and streams:** `edSoundSampleService` / `edSoundStreamService`, called from `src/EdenLib/edSound/sources/ps2/_edSoundPlay.cpp:114-130`, `:307-330`, `:416-443`. Includes a PS-ADPCM/VAG decoder (`port/Audio/edSoundStreamService.cpp:140-146`, `:282-340`) and a host-path lookup (`:225-243`).
- **Music:** sequence and bank parsing (`edMusicData.cpp:156-165`) plus a 48 kHz software synth (`port/Audio/MusicPlaybackImplementationPlan.md:15-20`), installed from `src/EdenLib/edMusic/sources/ps2/_edMusicPlay.cpp:24-25`.
- **Output backend:** **XAudio2 + COM**, guarded by `_WIN32` (`port/Audio/edSoundDevice.h:3-13`, `edSoundStreamService.cpp:17-21`, `:69-80`; `edMusicService.cpp:11-30`, `:99-103`). Linked with `if(WIN32) XAudio2 ole32` (`port/Audio/CMakeLists.txt:22-24`).
- Music output already goes through a pluggable `MusicOutputFactory` (`port/Audio/edMusicService.h:30`), so another backend can be added without touching the synth.
- An offline `KyaMusicRender` tool is included (`port/Audio/CMakeLists.txt:26-27`).
- **Movies (IPU/PSS):** `port/ext/pss` wraps libmpeg2 with `fmemopen_windows` (`port/ext/pss/pss/CMakeLists.txt:11-13`). Movies are skipped because `SKIP_MOVIES` is always on (`CMakeLists.txt:888`, `src/b-witch/kya.cpp:1703`).

---

## 5. Input, file I/O, memory card, timers, threads

| Subsystem | PC replacement | Refs |
|---|---|---|
| Pads (`libpad2`) | `edDevInit` installs `Input::_edDevKeyboard/_edDevMouse` in place of DualShock2 handlers. These poll `Input::gInputFunctions`, a table of `std::function`s | `src/EdenLib/edDev/Sources/edDevInit.cpp:149`, `:238`; `src/port/input.cpp:12-33`; `port/include/input_functions.h:36-54` |
| Keyboard/mouse | ImGui key queries mapped to PS2 routes | `port/DebugMenu/src/DebugMenuInput.cpp:13-46` |
| Gamepad | **WinRT `Windows.Gaming.Input`** | `port/Windows/Input/gamepad.cpp:3-10`, `:238-241` |
| Input replay | Swaps the function table for recorded input | `port/DebugMenu/src/DebugHeroReplay.cpp:103-133` |
| CD/DVD file I/O | `FormatForPC` (declared at `_edFileFilerCDVD.h:28-30`, implemented in the uninitialized `edFile` submodule) converts `cdrom0:\...` paths to host paths, used at `src/b-witch/kya.cpp:1640-1645`. Raw loads use `fopen` | `src/b-witch/kya.cpp:931-945` |
| BNK/banks | `edCFiler_BNK`, `edBank*` (the `edBank` submodule) are decompiled code working over the host filer *(inferred)* | `CMakeLists.txt:383-384`, `:457-471` |
| `port/Archive` | **Not the game archive loader.** A trivial pack/extract format used only for PS2 shader packs | `port/Archive/archive.h:6-16`, `Shaders/compiler/src/ShaderCompiler.cpp:263-265` |
| Memory card | `_edFileFilerMCard` gains a PC-only `cmdbreak` and routes through `WinSaveFile.h` (in the `edFile` submodule) *(inferred)*. Covered by 52 save tests | `src/EdenLib/Include/edFile/ps2/_edFileFilerMCard.h:45-47`; `port/Test/src/windows_save_tests.cpp:3-4` |
| Save paths | Game code builds backslash paths such as `"\\slot_"` and `"\\settings.dat"` | `src/b-witch/SaveManagement.cpp:624`, `:678` |
| RTC | `sceScfGetLocalTimefromRTC` → Win32 `GetLocalTime` | `port/Windows/windows_impl.cpp:4-9` |
| VSync/frame pacing | `std::chrono` 60 Hz limiter (sleep, then spin) | `src/edVideo/VideoD.cpp:242-258` |
| Game timer | `std::chrono::high_resolution_clock` | `src/b-witch/TimeController.cpp:88-91` |
| Threads | VU1 jobs: `BS::thread_pool`; native render thread: `std::thread`; level tasks: `std::async(deferred)`. `port/Job/job.h`'s own `Job::JobPool` is unused (the `Job` target only exports the BS headers) | `src/port/vu1_emu.cpp:57`; `NativeRendererRecording.cpp:480`; `src/b-witch/LevelScheduler.cpp:3142`; `port/Job/CMakeLists.txt:3-11` |
| Main memory | The heap header is `_aligned_malloc`'d instead of sitting at fixed address `0x004a5780` | `src/EdenLib/edSys/sources/EdSystem.cpp:50`, `src/b-witch/edMem.cpp:1294-1298` |
| RNG | PS2 LCG reproduced | `src/port/random.cpp:5-21` |

---

## 6. Pointer conversion (`STORE_POINTER` / `LOAD_POINTER`)

- **Problem:** serialized PS2 structures contain 32-bit pointer fields, which cannot hold a 64-bit host pointer. Those fields are typed `strd_ptr(T)`, which expands to `int` (`src/port/pointer_conv.h:21`).
- **On PC:**
  - `STORE_POINTER(p)` becomes `PointerConv::AddPointer(p)` and returns a 32-bit **handle** (`src/port/pointer_conv.h:37`).
  - `LOAD_POINTER(k)` becomes `ResolvePointerChecked(k)`, which asserts the handle is live (`src/port/pointer_conv.h:40`, `src/port/pointer_conv.cpp:189-199`).
  - `LOAD_POINTER_CAST(T, k)` does the same with a cast (`src/port/pointer_conv.h:46`).
- **On PS2:** these macros are identity casts (`src/port/pointer_conv.h:47-55`).
- **Handle encoding:** bits 0-19 slot, bits 20-30 generation (11 bits), bit 31 transient flag (`src/port/pointer_conv.cpp:12-17`, `:51-59`). Handle `0` means null (`:89-91`, `:191-194`).
- **Registries:** two of them, persistent and transient. Each has a slot vector, a `pointer→handle` map (so storing the same pointer again returns the same handle and bumps its refcount), and a free list (`src/port/pointer_conv.cpp:19-34`, `:87-126`).
- **Release:** decrements the refcount. At zero it frees the slot and bumps the generation, so stale handles fail to resolve instead of aliasing (`src/port/pointer_conv.cpp:128-157`).
- **Lifetime:** the transient registry is wiped on level teardown (`src/b-witch/LargeObject.cpp:861-863`).
- **Usage:** 887 uses across 58 files in `src/`.
- **Dead code:** `PointerWrapper<T>` refers to an undefined `STORE_POINTER_INT` (`src/port/pointer_conv.h:9-19`). It compiles only because nothing instantiates it.
- The same scheme is used for VIF references, e.g. `LOAD_POINTER(pVifPkt->asU32[1])` (`src/port/vu1_emu.cpp:2998`, `:3110`).

---

## 7. Other `port/` directories

| Dir | Purpose | Refs |
|---|---|---|
| `DebugMenu` | Dear ImGui overlay (GLFW + Vulkan backends): actors, camera, collision, draw inspector, textures/meshes, save/load, replay, callstack previewer | `port/DebugMenu/CMakeLists.txt:11-131`, `src/DebugRendererVulkan.cpp:202` |
| `Hash` | Header-only MD5, used for shader/pipeline caching | `port/Hash/hash.h:5-30`, `Objects/Pipeline.cpp:2` |
| `Log` | spdlog wrapper with categories and levels | `port/Log/log.h:1-40`, `port/Log/CMakeLists.txt:3-13` |
| `Pool` | `steinwurf/recycle` object pools (VU1 job pools) | `port/Pool/CMakeLists.txt:1`, `src/port/vu1_emu.cpp:12` |
| `Sprite` | Sprite-node → native vertex conversion | §2.2 |
| `Test` | gtest `KyaPortTest`: 20 core, 32 audio, 12 draw-trace, 52 save tests; test data (`font.bin`, `splash.bin`) | `port/Test/CMakeLists.txt:10-28`, `port/Test/data/` |
| `KyaMesh` / `KyaTexture` | Submodules (not checked out) that provide `Mesh` / `Texture` targets: `Renderer::Kya::MeshLibrary` and `TextureLibrary` | `port/CMakeLists.txt:36-37`, `port/Windows/Host/src/win_main.cpp:28-29` |
| `ext` | `pss` (MPEG), `readerwriterqueue`, `tracy` | `port/CMakeLists.txt:17-31` |
| `include` | `port.h` (GS/DMA/VIF shim), `input_functions.h`, `gamepad.h`, `logging.h`, `profiling.h` (Tracy macros) | `port/include/` |
| `pcsx2/VU` | PCSX2 VU0/VU1 interpreter + disassembler (`VU` target) | `port/pcsx2/VU/CMakeLists.txt:1-33` |
| `src` (under `port/`) | `gInputFunctions` storage and `ObjectNaming` | `port/src/input_functions.cpp:3-6`, `port/include/port.h:742-749` |

---

## 8. Platform-specific surface (for an SDL2 / macOS / Linux port)

### 8.1 Already portable
- Windowing, surface creation and events go through **GLFW** (`VulkanRenderer.cpp:433-447`, `:654`, `:926-930`, `:1133-1137`), and ImGui uses the GLFW backend.
- The renderer is plain Vulkan. It is headless-capable at runtime.
- Frame pacing, timers and threads use `std::chrono` / `std::thread`.
- The pointer registry, VU1Emu, the PCSX2 VU interpreter (`Pcsx2Defs.h` already has non-MSVC branches, `port/pcsx2/VU/src/Pcsx2Defs.h:97-160`), the audio decode/synth/transfer layer, Log, Hash, Archive and Sprite are standard C++.
- Keyboard input goes through ImGui (`DebugMenuInput.cpp`).
- Some code already has `_WIN32` fallbacks, e.g. `VKBuilders.h:51-55` (`_vscprintf` vs `vsnprintf`) and `DrawTrace.cpp:70-74`.

### 8.2 Win32 / MSVC dependencies (concrete list)

| Area | File:line | What | Effort |
|---|---|---|---|
| **Audio output** | `port/Audio/edSoundStreamService.cpp:17-21`, `:43-80` and 10 more `_WIN32` blocks; `edSoundSampleService.cpp:11`, `:44`, `:85`; `edMusicService.cpp:11-30`, `:99`; `edSysTransferService.cpp:120`; `edSoundDevice.h:3-13`; `port/Audio/CMakeLists.txt:22-24` | XAudio2 + COM (`CoInitializeEx`) | **M**: write an SDL2 audio (or miniaudio) device with one mixer. Music already has an output factory; samples and streams need a mixer to replace their XAudio2 source voices |
| **Gamepad** | `port/Windows/Input/gamepad.cpp:1-241` | WinRT `Windows.Gaming.Input` | **S**: `SDL_GameController` implementing the same `gInputFunctions.controller*` |
| **RTC** | `port/Windows/windows_impl.cpp:1-9` | `GetLocalTime` | **XS**: `std::chrono` / `localtime_r` |
| **Render thread naming** | `Native/NativeRendererRecording.cpp:11`, `:483` | `<windows.h>`, `SetThreadDescription` | **XS**: `#ifdef` or `pthread_setname_np` |
| **Draw trace stacks** | `Vulkan/src/DrawTrace.cpp:7-8`, `:70-74` | `CaptureStackBackTrace` (guarded) | **XS**: already guarded; `backtrace()` optional |
| **Aligned alloc** | `include/renderer.h:32-40`; `Objects/VulkanBuffer.h:81-86`; `VulkanRenderer.cpp:1264`, `:1300`; `src/EdenLib/edSys/sources/EdSystem.cpp:50` | `_aligned_malloc/_aligned_free` | **XS**: `std::aligned_alloc` / `posix_memalign` behind a helper |
| **Safe CRT** | `include/renderer.h:598`, `:605`; `src/b-witch/Types.h:330`, `:646`; `port/Archive/archive.cpp:33`; DebugMenu (`DebugMesh.cpp:236-476`, `DebugTexture.cpp:444`, `:679`, `DebugFrameBuffer.cpp:95`, `DebugMaterialPreviewer.cpp:152`, `Actor/DebugActorBehaviour.cpp:122`, `DebugCamera.cpp:82`) | `sprintf_s`, `strcpy_s` | **XS**: `snprintf` or a compat header |
| **Compiler extensions** | `src/b-witch/Types.h:9-10` (`__pragma(pack)`, used by every `PACK(...)`); `include/GIFReg.h:207-208`; `Texture/TextureCache.cpp:205`, `:282`, `:423`, `:507`, `:720`, `:922-928`; `pcsx2/Selectors.h:22` (`__forceinline`); `include/renderer.h:472`, `:555` (`__assume`); `DebugCallstackPreviewer.h:6` (`__int64`) | MSVC keywords (clang-cl accepts them; clang on macOS/Linux needs `-fms-extensions` or shims) | **S**: one `compat.h` (`#define __forceinline inline __attribute__((always_inline))`, `PACK` → `_Pragma("pack(push,1)")`, etc.) |
| **x86 SIMD** | `TextureCache.cpp:7`, `:76-85` (`GSVector4i` from TextureUpload); `port/pcsx2/VU/src/VU0micro.cpp:46` (`_mm_store_si128`) | SSE intrinsics | **S–M on Apple Silicon/ARM**: sse2neon or PCSX2's NEON GSVector. Nothing to do on x86_64 Linux |
| **DebugMenu WndProc hook** | `port/DebugMenu/src/DebugRendererVulkan.cpp:28-70`, `:204-210` | `SetWindowLongPtrW`, `WM_NCHITTEST`, `WM_INPUT` raw mouse | **XS**: guarded; GLFW raw mouse motion is the portable equivalent |
| **Callstack** | `port/DebugMenu/include/Callstack.h:3-40`; `port/DebugMenu/CMakeLists.txt:131` (`Dbghelp.lib` linked unconditionally) | DbgHelp symbolization | **S**: `backtrace` + `dladdr`, or stub; make the link conditional |
| **Clipboard** | `port/DebugMenu/src/DebugCamera.cpp:9-10`, `:70-90` | `OpenClipboard`/`GlobalAlloc` (unguarded `<Windows.h>`) | **XS**: `ImGui::SetClipboardText` / `glfwSetClipboardString` |
| **Google font loader** (optional) | `port/DebugMenu/src/GoogleFontLoader.cpp:3-4`, `:92-114`; CMake `:133-136` | WinHTTP | **XS**: keep off, or use libcurl |
| **Save tests** | `port/Test/src/windows_save_tests.cpp:225`, `:297`, `:321` | `CreateFileW` share-lock tests | **S**: file-lock semantics differ on POSIX; port the tests to `flock` or skip them |
| **Paths / case** | `src/b-witch/kya.cpp:1442-1445`, `SaveManagement.cpp:624-1687`, `CinematicManager.cpp:1432-1525` (backslashes, upper-case CD names); `FormatForPC` (edFile) | Windows separators, case-insensitive FS | **M**: normalise in `FormatForPC` and the save paths, plus case-insensitive lookup (or lower-cased assets) on Linux. macOS is case-insensitive by default |
| **Type widths** | `src/b-witch/Types.h:38-45` (`undefined5`, `int7`, `uint7` as `long`); ~349 bare `long` uses in `src/` | LLP64 (Win: `long`=32) vs LP64 (Linux/macOS: `long`=64) | **M, high risk**: audit layout-sensitive structs (73 `static_assert(sizeof…)` help); prefer fixed-width typedefs |
| **Build system** | `CMakeLists.txt:77` (any non-PS2 host = "Win Build"), `:805-810` + `DoRobo.bat` (robocopy); `Shaders/CMakeLists.txt:5-11` and `Renderer/CMakeLists.txt:197-213` (`glslangValidator.exe`, `dxc.exe`, `Bin32`, backslash escaping); `ShaderCompiler.cpp:92`, `:263` (`"\\"` paths, `std::system` dxc); `CMakePresets.json:5-18` (Windows-only presets); `Host/CMakeLists.txt:3` target name `Kya_Win` | Windows toolchain assumptions | **S–M**: `find_program(glslangValidator/dxc)`, `cmake -E copy_directory`, POSIX presets, rename the host target |
| **Movies** | `port/ext/pss/pss/CMakeLists.txt:11-13` (`fmemopen_windows`) | Windows `fmemopen` polyfill | **XS**: native `fmemopen` exists on POSIX (and movies are skipped anyway) |

### 8.3 Renderer blockers for macOS (MoltenVK)
- **Geometry shaders are required**: `RequiredDeviceFeatures.geometryShader = true` (`VulkanRenderer.cpp:131`), enabled at `:727`, and used by the display-list pipeline (`Native/NativeDisplayList.cpp:236`, `:360`, `:419`) and `Objects/Pipeline.cpp:28`. **MoltenVK does not support geometry shaders.** Fix: expand SPRITE quads on the CPU or in a vertex shader (vertex pulling, 4 or 6 verts per sprite). Effort **M**.
- Other features to check against MoltenVK: `dualSrcBlend`, `synchronization2`, `colorWriteEnable` and EDS3 colorBlend* (`VulkanRenderer.cpp:129-140`). Make the optional ones fall back to baked pipeline state. Effort **S–M**.
- There is no `VK_KHR_portability_enumeration` / `portability_subset` handling (`rg -i portability` finds nothing in `port/Windows/Renderer/Vulkan`), and MoltenVK requires it. Effort **XS**.
- Linux (native Vulkan drivers) has no feature blocker; only the items in §8.2 apply.

### 8.4 SDL2 note
Moving from GLFW to SDL2 is optional, since GLFW already works on macOS and Linux. It would touch:
- window and surface creation and fullscreen (`VulkanRenderer.cpp:403-447`, `:566-576`, `:654`, `:926-930`, `:1042`, `:1133-1137`)
- the ImGui backend (`DebugRendererVulkan.cpp:202`)
- `FreeCamera` key polling (`port/DebugMenu/src/FreeCamera.cpp:125-147`)
- `DebugCamera.cpp:4`

The case for SDL2 is that one dependency would then cover audio and gamepad as well. Effort **S–M**.

### 8.5 Rough overall effort
- **Linux x86_64:** compat header + build tooling + audio backend + gamepad + path/case handling + LP64 audit, roughly **M–L**. The LP64 audit carries the most risk.
- **macOS arm64:** everything in Linux, plus the geometry-shader removal, MoltenVK feature fallbacks and SSE→NEON for the texture cache / TextureUpload. Roughly **L**.
