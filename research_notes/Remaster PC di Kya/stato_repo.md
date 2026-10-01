# Current technical state of the Kya: Dark Lineage decompilation and PC port (local repo `/home/user/Kya`)

Scope: local repo only, at HEAD `2dfa7f0` (2026-09-26). The clone is shallow (59 commits, all by `Icey1717`, 2026-08-05 to 2026-09-26). **Most submodules are not checked out** (`git submodule status` shows a `-` prefix for every submodule). That includes `port/KyaMesh`, `port/KyaTexture`, `src/EdenLib/edFile`, `src/EdenLib/edBank`, `TextureUpload`, imgui, glfw, glm, googletest and tracy. Anything implemented inside them can only be inferred from call sites. Sources below are repo-relative paths with line numbers.

## Q1. Rendering: Vulkan backend, PS2 GS/VU emulation vs native, PCSX2 helpers, shaders, texture cache, framebuffers, resolution, aspect ratio, AA, post-processing

### Takeaway
The PC build renders through a custom Vulkan backend. Its default path is a "Native" renderer: the decompiled `ed3D` code calls directly into native mesh, texture and shadow passes and uses a native projection matrix. A PS2 path (VU1 emulation on a PCSX2-derived VU interpreter, then a GS-style display list) still exists but is off by default and can be switched on from the debug menu. The internal render resolution can be changed only from the debug menu (default 512x512). The output is shown as fixed 4:3 inside an ImGui viewport. There is no MSAA, FXAA or other AA. Post-processing is limited to PS2-equivalent effects (greyscale, alpha fix, fade, heat distortion, shadow blur).

### Cited Findings
- The Vulkan backend lives in `port/Windows/Renderer/Vulkan/src/`. It contains `VulkanRenderer.cpp` (~48 KB, app/swapchain bootstrap), `Native/*` (native renderer: setup, recording, submission, shadow, blending, framebuffer copy, post-processing, debug shapes, preview renderer), `Objects/*` (FrameBuffer, Pipeline, RenderPass, buffers, images, shaders), `Texture/*` (TextureCache, TextureUpscale) and `pcsx2/*` (`VKBuilders`, `Selectors`, `TextureUpload` submodule) — [port/Windows/Renderer/Vulkan/src](port/Windows/Renderer/Vulkan/src)
- PCSX2-derived code: a VU0/VU1 interpreter and MTVU are in `port/pcsx2/VU/src/` (`VU1microInterp.cpp`, `MTVU.cpp`, `VUops.cpp`, etc.). The renderer reuses PCSX2 Vulkan builders and pipeline selectors (`pcsx2/VKBuilders.cpp`, `pcsx2/Selectors.h`) and GS state/regs headers (`include/GSState.h`, `include/GIFReg.h`) — [port/pcsx2/VU/src](port/pcsx2/VU/src); [port/Windows/Renderer/include](port/Windows/Renderer/include)
- The VU1 emulation layer is `src/port/vu1_emu.cpp` (3400 lines). It exposes toggles for hardware draw, interpreter, single-threaded, simplified code and "EnableEmulatedRendering" — [src/port/vu1_emu.h:4-24](src/port/vu1_emu.h). ISPC kernels exist for vertex processing — [src/port/ispc/vtx.ispc](src/port/ispc)
- Emulated (PS2 GS-style) rendering defaults to **off**: `gEnableEmulatedRendering = { "Enable Emulated Rendering", false }` — [port/DebugMenu/src/DebugRendering.cpp:28](port/DebugMenu/src/DebugRendering.cpp)
- The native path is called straight from decompiled code. `ed3D.cpp` calls `Renderer::Kya::GetMeshLibrary().RenderNode(...)`, `GetTextureLibrary().BindMaterialLayer(...)`, `Renderer::Kya::Sprite::RenderNode`, `Renderer::Native::PushMatrixPacket`, shadow passes (`BeginShadowMask`, `BlurShadowMask`, `BeginShadowReceiver`, `EndShadowPass`) and framebuffer capture (`CaptureFrameBuffer`, `BindFrameBufferTexture`). ed3D.cpp has 64 `Renderer::` references — [src/b-witch/ed3D.cpp:2376-2382, 3167, 3636-3637, 4188, 5429, 6404, 7226, 7462, 7594](src/b-witch/ed3D.cpp)
- A native projection is built from the PS2 camera plane extents on Windows only (`#ifdef PLATFORM_WIN`). It is unit-tested in `port/Test/src/tests.cpp:77,96` — [src/b-witch/ed3D.cpp:2030-2055](src/b-witch/ed3D.cpp); [src/port/NativeProjection.h:7-17](src/port/NativeProjection.h)
- The native vertex format is `PS2::DrawBufferData<GSVertexUnprocessedNormal,uint16_t>` held per `SimpleMesh`. `MatrixPacket` (0x1C0 bytes) mirrors the PS2 VU matrix packet layout (obj-to-screen, lights, ambient, etc.) — [port/Windows/Renderer/include/renderer.h:241-313](port/Windows/Renderer/include/renderer.h)
- There are 23 GLSL shaders: native, displaylist (vert/geom/frag), ps2.frag, base, shadow mask/receiver/blur, fade, gray, alphafix, fullscreen, postprocess, meshviewer, debug line. SPIRV-Reflect is used for reflection — [port/Windows/Renderer/Shaders/src](port/Windows/Renderer/Shaders/src); [port/Windows/Renderer/Vulkan/src/VulkanReflect.cpp](port/Windows/Renderer/Vulkan/src/VulkanReflect.cpp)
- Post-processing effects enum: `Greyscale, AlphaFix, Fade` — [port/Windows/Renderer/Vulkan/src/Native/PostProcessing.h:15-20](port/Windows/Renderer/Vulkan/src/Native/PostProcessing.h)
- Heat distortion is a framebuffer copy. The capture is 512x512 by default, with an optional full-resolution capture toggle — [port/Windows/Renderer/Vulkan/src/Native/NativeFrameBufferCopy.cpp:14,41,170-173](port/Windows/Renderer/Vulkan/src/Native/NativeFrameBufferCopy.cpp); git `1bff358 Heat FX full res option`, `c5a6e06 Working heat fx`
- Internal resolution: `kDefaultWidth = 0x200; kDefaultHeight = 0x200` (512x512) — [port/Windows/Renderer/Vulkan/src/Native/NativeRenderer.h:22-25](port/Windows/Renderer/Vulkan/src/Native/NativeRenderer.h). It can be resized at runtime only through debug-menu fields "Render Resolution Width/Height", Apply/Reset and "Auto Apply Resolution", which call `ResizeFrameBuffer(w,h)` — [port/DebugMenu/src/DebugRendering.cpp:30-32,161-221,282-284](port/DebugMenu/src/DebugRendering.cpp)
- The game image is drawn as an ImGui image letterboxed to a hard-coded 4:3 ratio: `kGameAspectRatio = 640.0f / 480.0f` — [port/DebugMenu/src/DebugMenuLayout.cpp:13, 100-125, 134-150](port/DebugMenu/src/DebugMenuLayout.cpp)
- The original game has a widescreen (anamorphic) option. `gSettings.bWidescreen` switches `CCameraManager::aspectRatio` between 1.333333 and 1.777778, and cinematics check for 1.777778 — [src/b-witch/Settings.cpp:72-79](src/b-witch/Settings.cpp); [src/b-witch/Pause.cpp:429-433](src/b-witch/Pause.cpp); [src/b-witch/CinematicManager.cpp:4231,5729](src/b-witch/CinematicManager.cpp)
- AA: the only sample-count settings found are `VK_SAMPLE_COUNT_1_BIT` (post-process render pass and ImGui init). No MSAA, FXAA, SMAA or TAA code was found — [port/Windows/Renderer/Vulkan/src/Native/PostProcessing.cpp:28](port/Windows/Renderer/Vulkan/src/Native/PostProcessing.cpp); [port/DebugMenu/src/DebugRendererVulkan.cpp:199](port/DebugMenu/src/DebugRendererVulkan.cpp)
- Present mode is fixed to FIFO (vsync). Mailbox selection is commented out — [port/Windows/Renderer/Vulkan/src/VulkanRenderer.cpp:1028-1033](port/Windows/Renderer/Vulkan/src/VulkanRenderer.cpp)
- Window: GLFW. There is a `ToggleFullscreen` using `glfwSetWindowMonitor`, a framebuffer-resize callback and `recreateSwapChain` — [port/Windows/Renderer/Vulkan/src/VulkanRenderer.cpp:396-416, 442-452, 571](port/Windows/Renderer/Vulkan/src/VulkanRenderer.cpp)
- AI texture upscaling exists but is optional. ONNX (`4x-UltraSharpV2_fp32_op17.onnx`, loaded from `../../upscaling/models/`) or ncnn/Real-ESRGAN runs as an async job per texture. It is gated behind CMake `ENABLE_UPSCALING` (default OFF) and triggered only from debug-menu "Upscale" / "Upscale All" buttons — [port/Windows/Renderer/Vulkan/src/VulkanRenderer.cpp:285](port/Windows/Renderer/Vulkan/src/VulkanRenderer.cpp); [port/Windows/Renderer/Vulkan/src/Texture/TextureUpscale.cpp:197-312](port/Windows/Renderer/Vulkan/src/Texture/TextureUpscale.cpp); [port/Windows/Renderer/CMakeLists.txt:112-130](port/Windows/Renderer/CMakeLists.txt); [port/DebugMenu/src/DebugTexture.cpp:567,737-757](port/DebugMenu/src/DebugTexture.cpp)
- Renderer architecture debt is self-documented in 10 findings. 01-04 are marked done; 05-10 are open (global state, CMake boundaries, frame resources, draw granularity, redundant binds, RenderDoc analysis) — [docs/renderer-refactor/README.md](docs/renderer-refactor/README.md)
- A sample RenderDoc GPU profile ("AirPostBaseLine") shows Native Render at ~1.59 ms for 821 draws, DebugMenu at 0.14 ms and Alpha Fix at ~0.01 ms — [docs/renderer-refactor/AirPostBaseLine_gpu_timing_summary.md](docs/renderer-refactor/AirPostBaseLine_gpu_timing_summary.md)
- Open visual bugs: map geometry shows through in the flying forest factory; a door is always visible; there are Vulkan validation warnings after the receptacle cutscene — [BugList.md](BugList.md)

### Inferences
- The renderer is already a "native" re-implementation, not a pure GS emulator. That is the right base for a remaster: resolution, AA and widescreen can be added in the Native path without PS2 framebuffer constraints.
- To support widescreen properly, the existing `bWidescreen` / `aspectRatio = 1.777778` game path should be wired into the native projection (`finalHorizontalHalfFOV`). The ImGui viewport's hard-coded 4:3 also has to go. HUD/2D placement (display-list path) would need an anchoring/pillarbox strategy. None of this exists yet.
- The 512x512 default and the debug-only resolution controls mean there is no user-facing graphics options menu.

### Gaps
- Whether the native path covers all draw types (particles, sprites, UI, FMV) at parity with PS2. Recent commits added emitters, trails, sparks, cluster sprites and cinematic lights, which suggests ongoing coverage work, but there is no parity checklist.
- The internals of `KyaMesh`/`KyaTexture`/`TextureUpload` could not be read because those submodules are not checked out.
- Whether the swapchain is HDR-capable or whether gamma/sRGB handling is correct. Textures are created as `VK_FORMAT_B8G8R8A8_SRGB` (`TextureCache.cpp:1170`), but the full color pipeline was not audited.

## Q2. Timing and frame rate

### Takeaway
Game logic advances by a fixed timestep per frame, as on PS2. The PC build hard-caps the loop at 60 Hz with a sleep-plus-spin limiter in `edVideoWaitVsync`, and Vulkan uses FIFO vsync. There is no decoupled simulation/render and no interpolation, so higher frame rates would speed up the game.

### Cited Findings
- `Timer::Update()` sets a constant `frameTime` (0.02 if `g_isNTSC == 1`, else 0.01668335), then `cutsceneDeltaTime = frameTime * timeScale` and `totalTime += frameTime`. Wall-clock `frameDelta` is computed but the step is constant — [src/b-witch/TimeController.cpp:104-121](src/b-witch/TimeController.cpp). `g_isNTSC` is initialized to 1 — [src/EdenLib/edSys/sources/EdSystem.cpp:26](src/EdenLib/edSys/sources/EdSystem.cpp)
- On Windows, `edVideoWaitVsync` is a 60 Hz deadline limiter (`std::ratio<1,60>`). It sleeps until 0.5 ms before the deadline, busy-spins for the rest, and skips missed slots — [src/edVideo/VideoD.cpp:238-258](src/edVideo/VideoD.cpp)
- `edVideoFlip` order: render handlers → `VU1Emu::Wait()` → 2D handlers → `Renderer::Native::OnVideoFlip()` → `edVideoWaitVsync(1)` → `Renderer::Present()` + `FRAME_MARK` — [src/edVideo/VideoD.cpp:262-300](src/edVideo/VideoD.cpp)
- About 479 references to `frameDelta` / `cutsceneDeltaTime` / `lastFrameTime` exist in `src/b-witch` (rg count). Logic consumes these per-frame deltas — [src/b-witch](src/b-witch)
- There is a `Timer::SetTimeScale_001ba6f0` and a pause-timing exposure commit (`ac2b458 Expose pause timing state`) — [src/b-witch/TimeController.cpp](src/b-witch/TimeController.cpp)

### Inferences
- The `g_isNTSC == 1 → 0.02 s (50 Hz)` branch looks inverted relative to the naming (NTSC should be 60 Hz). Either the flag naming is wrong or the PC build simulates 50 Hz steps at a 60 Hz cap, which would make gameplay run about 20% slow. This needs verification before any high-frame-rate work.
- High refresh rate support would need either fixed-step simulation plus render interpolation, or a variable-delta audit of hundreds of call sites. Neither exists.

### Gaps
- No evidence found of any uncapped, 120 Hz or variable-refresh mode. Physics/animation sensitivity to delta changes is unknown.

## Q3. Audio, FMV, input, saves, file I/O

### Takeaway
Audio is implemented on Windows with XAudio2. That covers samples, streams, and PS2 sequenced music re-synthesized from the original banks, and it was confirmed working in-game in September 2026. FMV (PSS/MPEG-2) has a decoder library, but playback is compiled out (`SKIP_MOVIES` is always defined) and the frame blit is commented out. Input goes through Windows.Gaming.Input (WinRT gamepad, first pad only) plus keyboard mapped through ImGui keys. There is no SDL, rebinding or rumble. Saves work on Windows through a `WinSaveFile` filer with heavy validation tests and a backup system. Assets are read from an extracted `assets/CDEURO/...` tree. No ISO reader was found.

### Cited Findings
- Audio backend: `port/Audio/` contains `edMusicData`, `edMusicSynth`, `edMusicService`, `edSoundSampleService`, `edSoundStreamService` and `edSysTransferService`, and links `XAudio2 ole32` on WIN32. An offline `KyaMusicRender` tool renders songs to WAV — [port/Audio/CMakeLists.txt](port/Audio/CMakeLists.txt)
- The music plan says the backend parses PS2 sequence and instrument banks, decodes ADPCM, and renders 48 kHz stereo with envelopes, loops, pitch bend and modulation. It uses a shared XAudio2 device and 10 ms blocks. "The user confirmed working in-game playback on 2026-09-11"; "detailed PS2 fidelity comparison remains follow-up work" — [port/Audio/MusicPlaybackImplementationPlan.md:1-30](port/Audio/MusicPlaybackImplementationPlan.md)
- Stream lookup tries `assets/<hostPath>` and `assets/<hostPath>.VAG`, stripping a `cdrom0:` prefix — [port/Audio/edSoundStreamService.cpp:214-239](port/Audio/edSoundStreamService.cpp)
- Audio git history: `dd97825 Audio introduction` … `b8bc517 Audio impl finished`, `fce8cc5 Music playback`, `2dfa7f0 Fix audio not playing in level transitions` — git log
- FMV: the PSS decoder library (`port/ext/pss`, libmpeg2 submodule) is linked into `Port` — [port/CMakeLists.txt:17,53](port/CMakeLists.txt). In the game, `PlayMovie` returns immediately under `#ifdef SKIP_MOVIES` — [src/b-witch/kya.cpp:1703-1705](src/b-witch/kya.cpp). On Windows the decoded image is not drawn (`//Renderer::RenderImage(image + 0x10, 0x280, 0x200);`) — [src/b-witch/kya.cpp:1727-1731](src/b-witch/kya.cpp). `SKIP_MOVIES` is defined unconditionally for the Kya target — [CMakeLists.txt:881-888](CMakeLists.txt)
- Movies are expected at `CDEURO/movies/<name>.pss` — [src/b-witch/kya.cpp:1709-1710,1785](src/b-witch/kya.cpp)
- Gamepad: `winrt/Windows.Gaming.Input` reads `Gamepad::Gamepads().GetAt(0)` (first pad only) — [port/Windows/Input/gamepad.cpp:1-60](port/Windows/Input/gamepad.cpp). Keyboard uses a fixed `gKeyMap` from game route IDs to `ImGuiKey` — [port/DebugMenu/src/DebugMenuInput.cpp:13-78](port/DebugMenu/src/DebugMenuInput.cpp). The game-side poll copies pressed/released/analog per button — [src/port/input.cpp:1-35](src/port/input.cpp). No vibration hooks were found in `port/Windows/Input`. The game logic has vibration code (e.g. `CVibrationDyn` in `src/b-witch/ActorBox.cpp:66-117`)
- Saves: Windows tests include `src/EdenLib/edFile/sources/ps2/WinSaveFile.h` and cover checkpoint parsing, path resolution, publish-on-successful-close and staged backup loading — [port/Test/src/windows_save_tests.cpp:1-260](port/Test/src/windows_save_tests.cpp). Save directories are resolved as `std::filesystem::absolute(serial)`, i.e. relative to the CWD — [port/DebugMenu/src/DebugSaveLoadPaths.h:8-29](port/DebugMenu/src/DebugSaveLoadPaths.h). The backup browser and direct-load design is documented in [docs/superpowers/specs/2026-09-24-backup-save-direct-load-design.md](docs/superpowers/specs/2026-09-24-backup-save-direct-load-design.md). Git: `e35c68e Auto saving`, `50f7cea Save backup menu`, `cb14bc1 Merge direct backup save loading`
- File I/O: the `edFile`/`edBank` decomp are separate submodules (Icey1717/edFile, edBank) — [.gitmodules](.gitmodules); [README.md](README.md). Tests and tools reference an extracted `assets/CDEURO/LEVEL/...` tree, e.g. `KyaMusicRender.exe assets/CDEURO/LEVEL/LEVEL_1/LEVELIOP.BNK` — [port/Audio/MusicPlaybackImplementationPlan.md:54-60](port/Audio/MusicPlaybackImplementationPlan.md). The README points to the external KyaBank tool for extracting `.BNK` — [README.md](README.md)
- Language: on Windows the system language is hard-wired (`#define WIN_ENGLISH_LANGUAGE`) — [src/b-witch/kya.cpp:1327-1333](src/b-witch/kya.cpp)
- An ASan log shows heap-buffer-overflow in `CAudioManager::AddSoundStreams` during level load. It is dated before commit `470fa23 Various asan fixes`, so its current status is unknown — [MemoryErrors.txt](MemoryErrors.txt)

### Inferences
- FMV is a concrete missing piece. The decoder exists but there is no Vulkan presentation and probably no audio-track output. A remaster would likely swap in a modern decoder (e.g. FFmpeg) or pre-transcoded videos.
- Input needs a cross-platform layer (SDL3 or GLFW gamepad), multi-controller support, rebinding, rumble and button-prompt swapping.
- Save location next to the CWD is unsuitable for a shipped product; it should move to a user-data directory.

### Gaps
- How the asset root and the BNK/IOP files are resolved at runtime lives in the unchecked-out `edFile` submodule.
- Whether PSS audio is decoded at all (the `port/ext/pss` sources mention audio stream headers, `pss.c:47-86`, but no output path was found).

## Q4. KyaMesh and KyaTexture submodules; modding and texture replacement hooks

### Takeaway
KyaMesh and KyaTexture (Icey1717 repos) are not checked out locally. Call sites show they are the runtime libraries that turn PS2 mesh/G2D texture data into native renderer objects (`Renderer::Kya::MeshLibrary`, `TextureLibrary`, `G2D`). They are not offline exporters. No texture dump/replace hook was found; the only texture-enhancement path is the optional AI upscaler in the debug menu.

### Cited Findings
- Submodule URLs: `https://github.com/Icey1717/KyaMesh.git` and `https://github.com/Icey1717/KyaTexture` — [.gitmodules](.gitmodules). Their local directories are empty (`git submodule status` prefix `-`)
- They are built as CMake targets `Texture` and `Mesh` and linked into `Port` — [port/CMakeLists.txt:35-36,53](port/CMakeLists.txt)
- Initialized at startup through `Renderer::Kya::TextureLibrary::Init()` and `MeshLibrary::Init()` — [port/Windows/Host/src/win_main.cpp:26-27](port/Windows/Host/src/win_main.cpp)
- Runtime use: `GetMeshLibrary().RenderNode(pNode, layerIndex)`, `GetTextureLibrary().BindMaterialLayer(...)`, `BindFromDmaMaterial(...)` — [src/b-witch/ed3D.cpp:3636-3637,4188,4430,5671,6404](src/b-witch/ed3D.cpp). The debug texture browser iterates `Renderer::Kya::G2D` files and materials — [port/DebugMenu/src/DebugTexture.cpp:24-180](port/DebugMenu/src/DebugTexture.cpp)
- AGENTS.md says to commit inside these submodules first and then bump the parent pointer, so they are actively co-developed — [AGENTS.md](AGENTS.md)
- A grep for replace/override/dump/png/dds in `TextureCache.cpp` and `DebugTexture.cpp` found no texture replacement or dump path (rg, no matches). The upscaler is the only texture-enhancement hook — [port/Windows/Renderer/Vulkan/src/Texture/TextureUpscale.h:9-23](port/Windows/Renderer/Vulkan/src/Texture/TextureUpscale.h)

### Inferences
- Texture packs (hash-keyed dump/replace, like PCSX2) and mesh replacement would have to be built, most naturally inside KyaTexture/TextureCache, where textures are already keyed and uploaded natively.

### Gaps
- The actual capabilities of KyaMesh/KyaTexture (export formats, caching) cannot be checked without the submodules.

## Q5. Debug menu, test infrastructure, build system, platforms

### Takeaway
There is an extensive Dear ImGui debug suite: actors, hero, camera/free-cam, cinematics, collision, materials, meshes, textures, draw inspector, frame buffer, saves, scenarios, memory, logs, hero replay. The test project (GoogleTest `KyaPortTest`) covers projection, draw trace, audio and Windows saves, with the renderer built in `HEADLESS` mode. The build is CMake + Ninja, but the Windows-only presets (Clang/VS2022), XAudio2, WinRT input and `_aligned_malloc` keep the PC port Windows-only. The `linux-debug` preset is for the PS2 toolchain.

### Cited Findings
- Debug menu modules (port/DebugMenu/src): DebugActor (+ per-actor panels: Wolfen, Ship, Switch, Teleporter, Wind, JamGut, MovingPlatform, NativShop…), DebugCamera, FreeCamera, DebugCinematic, DebugCollision, DebugSceneryCollision, DebugDrawInspector, DebugFrameBuffer, DebugHero, DebugHeroReplay, DebugInput, DebugMaterial(+Previewer), DebugMesh, DebugMeshViewer(+Vulkan), DebugMemory, DebugProjection, DebugRendering, DebugSaveLoad, DebugScenario, DebugScene, DebugShop, DebugTexture, DebugTutorial, DebugMenuWorld, DebugCallstackPreviewer, GoogleFontLoader — [port/DebugMenu/src](port/DebugMenu/src)
- The rendering panel holds timings, resolution, complex blending, GLSL pipeline, force highest LOD, VU1 emulation toggles and a Display List Viewer — [port/DebugMenu/src/DebugRendering.cpp:150-270](port/DebugMenu/src/DebugRendering.cpp)
- Tests: `KyaPortTest` built from `tests.cpp`, `audio_sample_tests.cpp`, `audio_music_tests.cpp` and `windows_save_tests.cpp` (`draw_trace_tests.cpp` is also present). It is built with `HEADLESS` and registered via `add_test` — [port/Test/CMakeLists.txt:10,23,28](port/Test/CMakeLists.txt); [port/Test/src](port/Test/src). One XAudio2 test is disabled (`DISABLED_XAudio2CompletionAndShutdown`) — [port/Test/src/audio_music_tests.cpp:311](port/Test/src/audio_music_tests.cpp)
- Presets: `windows-base` is conditioned on `hostSystemName == Windows`, with x64-debug, x64-debug-asan, x64-release, x64-release-info and x86-debug — [CMakePresets.json](CMakePresets.json). The README says "currently only compiles for Windows" and "may not run successfully once built" — [README.md](README.md)
- The PS2 target links the Sony SDK libs (`sdr dma cdvd graph pkt ... mpeg ipu`) from `/usr/local/sce`. The Windows target defines `PLATFORM_WIN`, `KYA_USE_PS2_TRIG` and optional ISPC / Tracy profiling — [CMakeLists.txt:819-879](CMakeLists.txt)
- `DEBUG_FEATURES` is always defined — [CMakeLists.txt:888](CMakeLists.txt)
- Profiling uses Tracy (`FRAME_MARK`, `ZONE_SCOPED`) — [port/include/profiling.h:7-17](port/include/profiling.h). RenderDoc analysis scripts are in [docs/renderer-refactor/analyze_renderdoc_gpu_timing.py](docs/renderer-refactor)
- 64-bit pointer handling: PS2 32-bit pointer fields are stored as `int` handles through the `PointerConv` registry (`STORE_POINTER`/`LOAD_POINTER`) — [src/port/pointer_conv.h:1-45](src/port/pointer_conv.h)
- Tooling: `tools/ghidra-module`, `tools/local_renamer` (TS/vite naming tool), `tools/special_paste` (AHK + symbol index), plus python helpers (demangler, function-to-file mapping) — [tools](tools); [python](python)

### Inferences
- A remaster would need a release configuration without `DEBUG_FEATURES` and the ImGui shell (the game currently displays inside an ImGui viewport), a user-facing options menu, and platform abstraction (SDL3 or similar) for Linux/Steam Deck.

### Gaps
- No CI configuration was found in the repo (no `.github/workflows` checked). Test pass status is unknown because the build could not run on this Linux container.

## Q6. Decompilation completeness

### Takeaway
There is no formal progress tracker (no matching percentage or function list with status). The code base is large (~328k lines in `src/`) and playable through multiple levels, but it still has 673 `IMPLEMENTATION_GUARD*` markers (`assert(false)` on Windows) and 73 near-empty placeholder `.cpp` files, concentrated in ed3D, fighters/combat, hero, Wolfen, Nativ, particles and pause.

### Cited Findings
- `IMPLEMENTATION_GUARD(x)` is `assert(false)` on Windows and a log on PS2. There are category variants (FX, EMOTION, ACTOR, LIGHT, UI, SHADOW, LOG, HELP, and `_PS2`, which is a no-op) — [src/b-witch/Types.h:738-752](src/b-witch/Types.h)
- Counts (rg over `src/`): 673 `IMPLEMENTATION_GUARD*` occurrences, 29 `assert(false)`, 2 `TODO`. By variant: `_PS2` 31, `_LOG` 28, `_FX` 21, `_PROFILE` 7, `_LIGHT` 6, others 1-3 — [src](src)
- Top files by guard count: ed3D.cpp 59, ActorFighter.cpp 49, ActorHero_Private.cpp 33, ActorWolfen.cpp 29, ActorNativ.cpp 26, edParticles.cpp 22, Pause.cpp 19, edDlist.cpp 17, vu1_emu.cpp 16, ActorEventGenerator.cpp 16, ActorJamGut.cpp 14, CameraViewManager.cpp 12, light.cpp 11, CinematicManager.cpp 11, OBBTree.cpp 11, MapManager.cpp 10 — [src](src)
- 73 of 264 `.c/.cpp` files in `src/b-witch` are 6 lines or fewer, i.e. empty include-guard stubs such as `Movie_PSS.cpp` and `playpss.c` — [src/b-witch/Movie_PSS.cpp](src/b-witch/Movie_PSS.cpp). `python/CreatedMissingFiles.md` lists 229 files generated from the original symbol-to-filename map — [python/CreatedMissingFiles.md](python/CreatedMissingFiles.md)
- `python/FunctionToFilename.txt` (8062 lines) maps original symbols to original source paths (`D:\Projects\b-witch\...`), extracted from the `SLUS_204.40` ELF — [python/FunctionToFilename.txt](python/FunctionToFilename.txt)
- The repo includes `edSys-ghidra.elf` and `edSys.irx` (IOP module) — repo root

### Inferences
- The guards trip `assert(false)` in debug builds when the player reaches unported code paths. Fighter combat, some Wolfen/Nativ behaviours, particles and pause-menu items are the likely crash or no-op areas in a full playthrough. Treat full-game completability as unverified.

### Gaps
- There is no objective measure of completeness: no per-function matching or coverage, and no list of levels verified as completable.

## Q7. Git history highlights (recent focus)

### Takeaway
The last ~2 months (59 commits visible, shallow clone) were spent on audio (end-to-end), saves (autosave, backup browser, validated direct-load), native rendering of FX (emitters, trails, sparks, heat distortion, cluster sprites, shadows, cinematic lights) and Windows/ASan stability fixes.

### Cited Findings
- Audio: `87036f0 infux audio flush`, `dd272c5 CAudioManager Impl`, `bd07c1a Sound instance playing`, `a0e0275 Audio getting into game`, `efb0888 Sample playing`, `fce8cc5 Music playback`, `b8bc517 Audio impl finished`, `2dfa7f0 Fix audio not playing in level transitions` — git log
- Rendering/FX: `37dd254 Add proper shadow pass`, `6810ea8/5fad1a1 Emitter rendering/drawing`, `ef98dfd Add PS2 full screen draw passes`, `9e5f805 Frame buffer drawing part 1`, `c5a6e06 Working heat fx`, `dff6f59 Static mesh refactor and cluster sprites`, `c26ccd2 Trail management`, `ec823c7 Fx Tail management completed`, `11d14e8 Implement CFxSpark::Init`, `e09fbac Add Draw Inspector`, `8110fa8 Add cinematic lights`, `5ac425e hacky fix ... animation for g3d hierarchies` — git log
- Saves: `e35c68e Auto saving`, `50f7cea Save backup menu`, `127ee1b`…`cb14bc1` (staged backup save validation series) — git log
- Stability: `470fa23 Various asan fixes, mostly windows only`, `5c6186a Fix the polymorphic destructor for actors on windows`, `95bdfe5 ... original ps2 behaviour behind ifdef` — git log
- Single author: all 59 commits are by `Icey1717`. Date range is 2026-08-05 to 2026-09-26 — `git log`

### Inferences
- The project is in an "achieve playable parity" phase, not a "remaster features" phase. Remaster features (widescreen, high resolution as a user setting, AA, uncapped FPS, FMV, input rework, packaging) are almost entirely unstarted, apart from the debug-only resolution scaling and the experimental AI upscaler.

### Gaps
- History before 2026-08-05 is not available in this shallow clone.
