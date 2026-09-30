# Renderer upgrades for the native Vulkan renderer

Research for issue #39 (parent map #36). Static analysis of `macos/arm64-port` at `0aa81c2`. The game was not run. Log sizes come from an existing run in `bin/MAC/logs` of the main checkout.

Effort scale: **S** is under a day and touches one or two files. **M** is a few days and touches several subsystems. **L** is a week or more, or needs changes to decompiled game logic.

## Summary

| Upgrade | Status today | Kind of work | Effort |
|---|---|---|---|
| Baseline: ~19-40 FPS in levels | Logging is always on at trace level and writes GBs per run | Config first, then a small code fix | S |
| Arbitrary internal resolution | Exists in the debug menu (Rendering > Resolution) | Persist and expose it | S |
| Widescreen, FOV and HUD | The game has a 16:9 option (Hor+ FOV). The port always letterboxes to 4:3. The HUD is drawn in PS2 screen space | Engine work | M |
| 60+ FPS | Fixed 0.02 s step per frame, with a 1/60 limiter, so the game runs 1.2x fast today | Fix the speed bug, then make the step a parameter | S (bug), M (fixed high refresh), M-L (variable) |
| MSAA | Missing. Every attachment and pipeline is 1x | Engine work | M |
| FXAA | Missing, but there is a post-process effect framework | New effect and shader | S |
| Higher-resolution shadows | Shadow mask size comes from level data (default 128x128) | Scale factor at one call site | S |
| Longer draw distance | Far culling comes from per-sector fog/clip data | Multiplier, but fog and sector streaming limit it | S to M |
| No LOD pop | "Force Highest LOD" exists in the debug menu | Persist it, or add a LOD bias | S |
| Texture filtering and mipmaps | No anisotropy, one mip level only, PS2 mip chains dropped | Engine work in the texture cache | M |
| AI-upscaled textures | Exists behind CMake options, off by default, and triggered by hand from the debug menu | Build config and a batch or cache step | S to M |

## 1. Baseline: frame rate in levels and log volume

This is the first thing to fix, because it distorts any measurement of the other items.

- `port/include/logging.h:3-4` defines `ENABLE_MY_LOG` whenever `PLATFORM_WIN` is set. The port sets `PLATFORM_WIN` on every OS, macOS included (`CMakeLists.txt:870`). There is no CMake switch to turn logging off.
- `port/Log/log.cpp:11` hard-codes `gLogLevel = spdlog::level::trace`. It applies to every logger (`log.cpp:19`). No env var, ini or define changes it. The level selector in the debug menu has an empty body (`port/DebugMenu/src/DebugMenuLog.cpp:131`).
- `Log::AddLog` (`port/Log/log.h:81-101`):
  - it builds a `std::string` from the category and looks it up in a map before checking `bEnabled`
  - the caller has already evaluated every argument by then, including heap-allocating `ToString()` calls
  - every message is then formatted twice: once into `asyncLog` and once into the category logger
- `asyncLog` is `basic_logger_st` (`log.cpp:15`), which is single-threaded, yet the audio, main and render threads all write to it. That is also a thread-safety bug.
- The only runtime control is a per-category on/off switch, read from `category_config.txt` in the working directory (`log.cpp:44-75`). `bin/MAC/` has no such file, so every category is on.

Sizes from one run: `ed3D.txt` 577 MB, `LightManager.txt` 49 MB, `event.txt` 45 MB, `NativeRenderer.txt` 43 MB, `Actor.txt` 34 MB, `TextureCache.txt` 18 MB.

The hottest call sites run per strip or per draw, every frame:
- `src/b-witch/ed3D.cpp:8136` (`ed3DLinkClusterStripToViewport`)
- `ed3D.cpp:3354` and `ed3D.cpp:4184` (`ed3DFlushStrip*`)
- `port/Windows/Renderer/Vulkan/src/Native/NativeRendererSubmission.cpp:332,354` (`BindTexture`, logged at **Info**)
- `port/Windows/Renderer/Vulkan/src/Texture/TextureCache.cpp:1290`. `LOG_TEXCACHE` is hard-wired to Info at `TextureCache.cpp:27`, and the no-op version is already there, commented out, at line 28.

Fixes, cheapest first:
1. **Config (S, no rebuild).** Add `bin/MAC/category_config.txt` that turns off `ed3D`, `NativeRenderer`, `TextureCache`, `LightManager`, `event`, `Animation`, `Collision`, `Actor` and `CustomShell`. This stops formatting and I/O, but not the argument evaluation. Use it to confirm the diagnosis.
2. **Code (S).** Check the level and enabled flag inside `MY_LOG_CATEGORY` before evaluating arguments (`do { if (Log::Enabled(...)) ... } while (0)`). Add a runtime minimum level, defaulting to Warning. Drop or fix the duplicate `asyncLog` write. Add a CMake option that stops defining `ENABLE_MY_LOG` in release builds.

Two smaller per-frame stalls to check after logging is off:
- `EndSingleTimeCommands` calls `vkQueueWaitIdle` for every one-off upload (`port/Windows/Renderer/Vulkan/src/Objects/VulkanCommands.cpp:44`).
- The swapchain is always FIFO, meaning vsync (`port/Windows/Renderer/Vulkan/src/VulkanRenderer.cpp:1071-1079`, with the MAILBOX branch commented out).

## 2. Arbitrary resolution

**Exists (debug menu). Making it a user setting: S.**

- The native render target defaults to 512x512 (`kDefaultWidth/kDefaultHeight = 0x200`, `port/Windows/Renderer/Vulkan/src/Native/NativeRenderer.h:22-23`). This matches the PS2 surface the game creates (`src/b-witch/ed3D.cpp:718-719`).
- `Rendering > Resolution` in the debug menu has width/height inputs, Apply and Reset, and an "Auto Apply Resolution" setting that persists. See `port/DebugMenu/src/DebugRendering.cpp:30-32,161-221,282-283`.
- These call `Renderer::Native::ResizeFrameBuffer`, which queues a resize. `ApplyPendingResizeInternal` then rebuilds the color and depth targets and the shadow receiver target (`NativeRendererSetup.cpp:499-530`).
- The heat-distortion capture defaults to 512x512. A "full resolution" toggle exists (`DebugRendering.cpp:169-176`, `NativeFrameBufferCopy.cpp:13`).
- The native 3D projection is built from FOV extents, not from pixel sizes (`BuildNativeProjection`, `ed3D.cpp:2052`). The 2D display list converts PS2 pixel coordinates to NDC against the current viewport (`NativeDisplayList.cpp:573-595`). Both scale with the render target.
- What is left:
  - a normal settings entry or command-line flag, instead of the debug menu
  - defaulting the target to the window size, or a multiple of it
  - checking that framebuffer-copy effects stay correct at non-square sizes: `frameBufferScaleX/Y` is normalized by 512 in `NativeRendererSubmission.cpp:416-417`

## 3. Widescreen, FOV and HUD

**Engine work: M.**

- **The game already supports 16:9.** `CSettings` switches `CCameraManager::aspectRatio` between `1.333333f` and `1.777778f` using `bWidescreen` (`src/b-witch/Settings.cpp:72-80`). The value is saved at byte 0x27 of the settings blob (`Settings.cpp:36`).
- The camera applies it as Hor+: `edFCameraSetSizeRatioFov` sets `finalHorizontalHalfFOV = base * aspectRatio` (`src/b-witch/CameraViewManager.cpp:1507-1511`, called at `:1207` and `:623`).
- Cutscenes with letterbox bands narrow the FOV by 0.75 in 16:9 (`src/b-witch/CinematicManager.cpp:4229-4234`).
- A few UI paths already read `aspectRatio`: `ActorNativShop.cpp:1371`, `MapManager.cpp:1471,2726` and `CameraShadow.cpp:578`.
- **The port always shows 4:3.** Menus visible or hidden, the game image is an ImGui image fitted to `kGameAspectRatio = 640/480` (`port/DebugMenu/src/DebugMenuLayout.cpp:13,97-124,134-146`). Turning on the in-game widescreen option today gives a Hor+ image squeezed into a 4:3 box, which is the anamorphic PS2 output.
- **The HUD is drawn in PS2 screen space.** 2D `edDList` primitives use pixel coordinates in the 512-wide surface, mapped to NDC by `x / gViewport.width` (`NativeDisplayList.cpp:577-578`). At a 16:9 output with the 3D view correct, HUD elements stretch horizontally. That matches the PS2 in 16:9 mode, where the TV unsqueezed everything.
- Work needed:
  1. Present at the window aspect: drive `kGameAspectRatio` from `aspectRatio` or the swapchain, and size the render target to match (S).
  2. Set `bWidescreen` from a port setting at startup (S).
  3. Correct the HUD, for example by scaling 2D display-list X by `(4/3) / aspect` around a per-element anchor, or by pillarboxing a 4:3 HUD layer (M). This needs to know which draws are HUD, which are full-screen effects (fades, heat haze, the frame-buffer copies used by FX) and which are 3D-projected sprites.
  4. Check the frustum math in `CCameraManager::ComputeFrustrumPlanes`, which divides by the 4:3 constant `1.333333f` (`CameraViewManager.cpp:1106-1108`), and the shadow cameras.

## 4. 60+ FPS and the timing model

**Game logic is tied to the frame count. Fixed 60/120/144 Hz: M. Uncapped or variable refresh: M to L. There is a speed bug today (S to fix).**

**How the step works**
- `Timer::Update` (`src/b-witch/TimeController.cpp:102-121`) sets the step to a constant, not to a measured time: `frameTime = 0.02f` when `g_isNTSC == 1`, otherwise `0.01668335f`. From that it derives `cutsceneDeltaTime = frameTime * timeScale`.
- The name is inverted: `1` selects the 50 Hz step. `g_isNTSC` is initialised to 1 (`src/EdenLib/edSys/sources/EdSystem.cpp:26`) and nothing assigns it again, so the step is always 0.02 s.
- `BWITCH.INI` parsing sets `gVideoConfig.isNTSC` (`src/b-witch/kya.cpp:958-990`), but that value never reaches `g_isNTSC`.
- The real wall-clock delta, `frameDelta`, is computed at `TimeController.cpp:118` and never read.

**How the PC loop is paced**
- Every loop runs one fixed step per rendered frame: input, `Timer::Update`, Manage, Draw, `edVideoFlip`. There is no accumulator and no interpolation (`src/b-witch/kya.cpp:2106-2197`).
- `edVideoFlip` calls `edVideoWaitVsync`. On PC that is a software limiter at `std::ratio<1, 60>` (`src/edVideo/VideoD.cpp:241-257`), on top of the FIFO swapchain.

**Consequences**
- **Speed bug:** at a steady 60 FPS the simulation advances 60 × 0.02 = 1.2 s per real second, so it runs 20% fast. Fix it by making the limiter `ratio<1,50>`, or by switching both `g_isNTSC` and `gVideoConfig.isNTSC` to the 60 Hz path (S).
- **The current ~19-40 FPS runs the game in slow motion**, not merely choppily. This makes the logging fix in section 1 more urgent.
- **Uncapping the limiter today** would speed the game up in proportion to FPS: 2.4x at 120 Hz.

**What already works:** most systems scale by `cutsceneDeltaTime` (about 450 call sites):
- movement (`ActorMovable.cpp:403-424`)
- animation (`Actor.cpp:917-923`, `Animation.cpp:1775`)
- particles (`edParticles.cpp:1801-1873`)
- cinematics
- some code normalizes to 50 Hz explicitly, e.g. `powf(x, dt * 50.0f)` (`Actor.cpp:4819`) and `0.02f / dt` (`ActorShoot.cpp:250`); that code survives a different step

**What breaks at a different step**
- Fades count frames using `speed * 50` or `* 60` (`LargeObject.cpp:574-590`) and advance once per flip (`VideoD.cpp:132-150`).
- Hero stick analysers size their ring buffers from 0.02 or 0.0167 (`ActorHero_Private.cpp:15105-15113`).
- Texture scroll uses a 0.8333 factor for 60 Hz (`ActorFighter.cpp:9389`).
- ed3D texture animation takes whole milliseconds (`ed3DSetDeltaTime((uint)(dt*1000))`, `LargeObject.cpp:1210-1227`). At 144 Hz, 6.94 ms truncates to 6 ms, about 14% slow.
- Sound fades use a hard-coded 0.02 (`edSoundInit.cpp:76`).
- Fixed-alpha LERPs and damping run once per frame, for example the camera (`CameraGame.cpp:1708`), lights (`LightManager.cpp:463-474`) and `* 0.975f` (`CameraMouseQuake.cpp:285`). These are samples; a full audit is still needed.
- `_edSystemTimerGet` returns seconds since boot as a `float` (`TimeController.cpp:86-91`), so it loses precision as uptime grows. Cinematic audio sync reads it (`CinematicManager.cpp:6091-6138`).

**Options**
1. Fix the 1.2x speed bug by matching the limiter to the step (S).
2. Fixed high refresh: set `frameTime = 1/N` and `kFrameDuration = 1/N`, then make each fixed-step item above use the step (M).
3. Variable refresh: use `frameDelta` with a clamp, switch the clock to `double`, and audit the per-frame counters (M to L).
4. Keep a 50 Hz simulation and interpolate rendering between steps. This preserves gameplay exactly but needs snapshots of actor, camera and bone state (L).

The debug menu shows FPS and time scale (`port/DebugMenu/src/DebugMenuDebugPanel.cpp:39-54`) but has no frame-rate setting.

## 5. MSAA and FXAA

**MSAA: M. FXAA: S.**

- Every render pass, image and pipeline hard-codes `VK_SAMPLE_COUNT_1_BIT`:
  - `port/Windows/Renderer/Vulkan/src/Objects/FrameBuffer.cpp:30,44,222`
  - `Objects/Pipeline.cpp:194,208`
  - `Objects/VulkanRenderPass.cpp:25,47`
  - `Objects/VulkanImage.cpp:217`
  - `Native/NativeRendererSetup.cpp:201,334`
  - `Native/NativeDisplayList.cpp:171,185,294`
  - `Native/NativeShadow.cpp:163`
  - `Native/PostProcessing.cpp:28,115`
  - `Native/NativeDebugShapes.cpp:417,531`
- The PCSX2 builder already has `SetMultisamples` (`pcsx2/VKBuilders.cpp:571`), but nothing calls it with anything but 1x.
- MSAA needs:
  - multisampled color and depth targets and a resolve attachment on the main native pass
  - a sample count on every pipeline that renders into it
  - resolves before every mid-frame read of the target: the heat-distortion and frame-buffer copies (`NativeFrameBufferCopy.cpp:143` uses `vkCmdBlitImage`, which cannot read MSAA images) and the depth copy in `NativeDebugShapes.cpp:768-794`
- On Apple GPUs (MoltenVK) MSAA is cheap on tile memory if the pass does not store the multisampled attachments.
- FXAA fits the existing post-process framework. `PostProcessing::Effect` (`Native/PostProcessing.h:16-21`) already has Greyscale, AlphaFix and Fade, each a full-screen fragment shader created with `CreatePipeline(Effect, "shaders/*.frag.spv")` (`PostProcessing.cpp:254-255`). FXAA is one more enum value, a `fxaa.frag.glsl` and a toggle. Run it before the 2D HUD, or accept that it softens text.
- Supersampling already works through the resolution setting. For example, render at 2x the window size and let the ImGui composite downsample it with its linear sampler (`port/DebugMenu/src/DebugRendererVulkan.cpp:296-297`).

## 6. Higher-resolution shadows

**Scale factor: S.**

- These are not depth shadow maps. They are projected blob masks: the caster pass renders a coverage mask, blurs it (`Shaders/src/shadow_blur.frag.glsl`), and projects it onto receivers (`Native/NativeShadow.cpp`).
- The mask size comes from level data. `ed3D.cpp:2376-2382` copies `pShadowConfig.texWidth/texHeight`, `nbBlurSamples` and `blurRadius` into `ShadowPassSettings`. The defaults are 128x128 with no blur (`port/Windows/Renderer/include/renderer.h:295-302`).
- `Shadow::NormalizeSettings` (`NativeShadow.cpp:18-24`) clamps only to at least 1 texel and at most 32 blur samples. Targets are cached per size (`GetOrCreateTarget`, `NativeShadow.cpp:241-259`).
- Higher resolution means multiplying `width/height` by a quality factor in `NormalizeSettings` or at the `ed3D.cpp` call site. Scale `blurRadius` by the same factor so the soft edge stays the same width in world space.
- The PS2-side projection (`gCamPos` at `ed3D.cpp:2361-2364`) uses the PS2 surface size. Check that the native path uses only the normalized shadow projection (`PushShadowProjectionMatrix`, `ed3D.cpp:6085`) and never texel counts.

## 7. Draw distance and LOD pop

**No LOD pop: exists (debug menu). Longer draw distance: S to M.**

- **LOD.**
  - `ed3DChooseGoodLOD` (`src/b-witch/ed3D.cpp:9080-9140`) picks the first LOD whose `sizeBias^2` or `pLodBiases[i]` exceeds `gCurLOD_RenderFOVCoef * distance^2`.
  - If none qualifies, `pLod` is NULL and the object is not drawn. That is a second, distance-based cull on top of the far plane.
  - "Force Highest LOD" in the debug menu (`port/DebugMenu/src/DebugRendering.cpp:229`, `ed3D.cpp:118-121,9093-9096`) sets `lodCount = 1`. It forces LOD 0 and skips the LOD distance cull too.
  - A softer option is a LOD bias multiplier on `cameraSize` at `ed3D.cpp:9119` (S). Cluster geometry uses a second LOD path at `ed3D.cpp:8537-8560`, which would need the same bias.
  - The toggle is a plain `bool` today, not a persisted `Debug::Setting`, so it resets every launch.
- **Far plane.**
  - The scene culling distance is `sceneConfig.clipValue_0x4` (default 250, `ed3D.cpp:1854-1855`). It becomes the frustum far plane `-clipValue_0x18` (`ed3D.cpp:2198,2230,2288`).
  - `CScene::HandleFogAndClippingSettings` (`src/b-witch/LargeObject.cpp:1234-1248`) writes it every frame from sector data (`clipValue_0x0`) and interpolates it together with fog color and density.
  - A multiplier at `LargeObject.cpp:1248` extends culling (S), but:
    - fog is tuned to the same distance and would need its range scaled (`ed3DGetFxFogProp`, same function)
    - sectors are streamed by the game, so geometry outside loaded sectors does not exist
    - actors have their own per-sector management (`CActorManager::Level_Manage`, `src/b-witch/ActorManager.cpp:317-343`)
  - Getting past sector streaming is game-logic work (L).

## 8. Texture filtering, mipmaps and upscaled textures

**Anisotropy and mip generation: M. Upscaling pipeline: exists (optional build).**

- The game-texture sampler cache is `PS2::GetSampler` (`port/Windows/Renderer/Vulkan/src/Texture/TextureCache.cpp:1009-1061`). It creates samplers with:
  - `anisotropyEnable = VK_FALSE`. The anisotropy lines are commented out at `:1038-1039`.
  - `mipmapMode = NEAREST`
  - min and mag filters taken straight from the GS `ltf` bit (`:1027-1028`)
- Every `VulkanImage` has `mipLevels = 1` (`Objects/VulkanImage.cpp:211`). `ImageData::maxMipLevel` is carried (`include/renderer.h:185`) and logged (`TextureCache.cpp:34`) but never uploaded. PS2 mip chains are dropped, so distant textures shimmer at any resolution above native.
- Work needed:
  - enable `samplerAnisotropy` on the device and set `maxAnisotropy` from a setting (S)
  - allocate full mip chains and generate them with `vkCmdBlitImage` after upload, or upload the PS2 levels, then use `LINEAR` mip mode (M). The upload path (`TextureCache.cpp`) and the texture swap path (`Texture/TextureUpdate.cpp`) both need it.
  - optionally, a setting to force bilinear on textures the game marks as point-sampled
  - palette (CLUT) textures stay `NEAREST` on purpose (`TextureCache.cpp:1018-1020`)
- The AI texture upscaler exists:
  - `Texture/TextureUpscale.{h,cpp}` has a background job that runs ncnn/Real-ESRGAN or ONNX Runtime and swaps the result in with `RequestTextureUpdate` (`Texture/TextureUpdate.cpp:23`)
  - it is gated by `ENABLE_UPSCALING` and `ENABLE_UPSCALING_ONNX`, both OFF by default (`port/Windows/Renderer/CMakeLists.txt:112,140`)
  - the model path is hard-coded to `../../upscaling/models/4x-UltraSharpV2_fp32_op17.onnx` (`VulkanRenderer.cpp:278`)
  - it is triggered only by the debug menu buttons "Upscale" and "Upscale All" (`port/DebugMenu/src/DebugTexture.cpp:567,737`)
  - `TextureUpdate.cpp:71,78` calls `vkDeviceWaitIdle` per swap
- Shipping the upscaler needs:
  - a macOS build of ONNX Runtime (CoreML EP)
  - an on-disk cache keyed by texture hash, so it does not run every session
  - mipmaps, because 4x textures without mips alias badly

## Suggested order

1. Turn off logging and confirm the frame rate (S). Every other measurement depends on it.
2. Make resolution, Force Highest LOD, the shadow scale and anisotropy into persisted settings (S each).
3. Add FXAA as a post-process effect (S).
4. Mipmap generation (M), then widescreen with HUD correction (M), then MSAA (M).
5. Fix the 1.2x speed bug now (S). Schedule fixed high refresh (M) after the fixed-step audit in section 4.
