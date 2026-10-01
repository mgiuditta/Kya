# Rendering / engine enhancement techniques for a PS2-origin PC remaster (Vulkan)

Context for the report writer: the Kya port already has a Vulkan renderer with PCSX2-derived helpers (`port/Windows/Renderer/Vulkan/src/pcsx2/VKBuilders.*`), a `PostProcessing.cpp`, a `TextureCache.cpp`, a `Pipeline.cpp`, and the decompiled camera code holds a hardcoded `aspectRatio = 1.333333f` plus `baseHorizontalHalfFOV`, `finalHorizontalHalfFOV`, and `computedVerticalHalfFOV` fields (`src/b-witch/CameraViewManager.cpp:54`, `CameraViewManager.h:36-38`). I checked these local facts directly in the repo. The rest of this document comes from web sources.

Research limits: the official PCSX2 docs pages for graphics settings returned 404 at the URLs I tried, so the PCSX2 hack descriptions below come from the PCSX2 blog, the PCSX2 wiki/guides, and a PCSX2 commit mirror. I found no detailed primary source on how Ship of Harkinian handles particles and animation under interpolation, on Digital Foundry remaster analyses, or on SSAO/shadow upgrades for PS2 titles specifically.

## 1. Internal resolution upscaling and PS2 GS quirks

### Takeaway
PS2 games were authored for native-resolution GS rasterization. Rendering at a higher internal resolution causes predictable classes of artifacts: lines or grids in sprites and 2D, "ghost" or offset bloom, and broken full-screen effects. PCSX2 handles these with per-game hacks: Round Sprite, Align/Merge Sprite, Half Pixel Offset modes including "Align to Native", and Texture Offsets. A native port can fix the causes at the source instead, because it knows which draws are post-process or 2D passes.

### Cited Findings
- PCSX2 states plainly that "PS2 games were never meant to be upscaled". Upscaling glitches are described as "unavoidable" and need workarounds rather than accurate emulation. — [PCSX2 blog 2015](https://pcsx2.net/blog/2015/major-gsdx-progress-and-monthly-progress-reports)
- The Align Sprite hack was made for black vertical lines at upscaled resolutions in Namco titles (Tekken, Soul Calibur, Ace Combat). The cause is how the engine positions sprites. — [PCSX2 blog 2015](https://pcsx2.net/blog/2015/major-gsdx-progress-and-monthly-progress-reports)
- The Round Sprite hack fixes sprite glitches, such as boxes around character portraits in Ar tonelico, that appear "at any resolution aside from native". — [PCSX2 blog 2015](https://pcsx2.net/blog/2015/major-gsdx-progress-and-monthly-progress-reports)
- Upscaling makes bloom and blur filters misalign and leave a duplicate "ghost" image. Half-Pixel Offset shifts the coordinate system slightly to realign them. — [PCSX2 GameDB/guide summaries via search](https://heldgames.com/guides/ps2-games-transformed-by-upscaling); see also [PCSX2 forum thread on ghosting](https://forums.pcsx2.net/archive/index.php/thread-53583.html)
- Recommended troubleshooting order for grid-like patterns: Round Sprite (Half/Full), then Align Sprite or Merge Sprite, then the Half Pixel Offset modes, then Texture Offsets as a last resort. Once a HPO mode fixes the ghosting, don't push to a more aggressive mode. — [PCSX2 configuration guide (mirrored)](https://www.scribd.com/document/470754687/Configuration-Guide)
- PCSX2's halfPixelOffset setting gained a fifth mode (value 4), "Align to Native". — [PCSX2 commit mirror](https://git.quad4.io/Mirrors/pcsx2/commit/69c5172b45e4df8ffd05049226bc07e14d13ddf9)

### Inferences
- In a native port, the artifacts come from three mechanisms. These are well-known GS behaviours, but no fetched source verified them:
  1. Texel/pixel center mismatches when a native-resolution post-process samples a render target that is now N times larger. This causes ghosting and misaligned bloom.
  2. Sprites whose UVs are snapped to texel edges at 1x but land between texels at Nx. This causes lines and seams in HUD, fonts, and tiled 2D.
  3. Effects that read depth as color or shuffle channels: "depth/channel shuffle" tricks that copy depth into a color target via 16-bit format reinterpretation. These break when the copy is done at scaled resolution with filtering.
- The native-port fix: keep 2D/HUD and full-screen passes in a resolution-aware path. Use point sampling with explicit texel-center UV math, compute post-process UVs from target size rather than PS2 pixel coords, and run blur/bloom kernels either at native resolution (then upsample) or with kernel radii scaled by the resolution factor. Otherwise bloom becomes thinner and weaker at 4K. That is the native equivalent of PCSX2's "Align to Native"/"Native scaling" idea.
- Kya's renderer already has PCSX2-derived code. Review any GS-style post passes in `PostProcessing.cpp` against the list above.

### Gaps
- I could not fetch PCSX2's current official settings docs (404 at `pcsx2.net/docs/configuration/settings/graphics` and `/docs/troubleshooting/hw-fixes/`). So I have no verbatim definitions of Merge Sprite, Bilinear Upscale, Native Scaling, Texture Offsets, or the individual HPO modes ("Normal (Vertex)", "Special (Texture)", etc.).
- I found no primary write-up on channel-shuffle effects at high resolution.

## 2. Widescreen: Hor+ projection, culling, HUD anchoring, FMV pillarboxing

### Takeaway
On PS2, widescreen is usually done with game-code patches (pnach) that change the projection/FOV and often the culling. A native port can do proper Hor+: keep the vertical FOV and derive the horizontal FOV from the real aspect ratio. Kya's camera code already separates horizontal and vertical half-FOV and hardcodes 4:3, which is the obvious hook. HUD anchoring and FMV pillarboxing have to be solved in the port.

### Cited Findings
- PCSX2 patches are text files of commands applied to a running game, keyed by serial and executable CRC. Widescreen patches use a `[Widescreen 16:9]` header for auto-detection and set `gsaspectratio` (Stretch, Auto 4:3/3:2, 4:3, 16:9). — [PCSX2 docs: Writing patches](https://pcsx2.net/docs/advanced/writing-patches)
- Community widescreen patch archives exist per game. — [PCSX2 forum widescreen archive](https://forums.pcsx2.net/post-271622.html)
- RT64 supports "arbitrary aspect ratios, including Ultrawide", but notes "limited game support". This shows widescreen needs per-game work even in a sophisticated renderer. — [RT64 GitHub](https://github.com/rt64/rt64)

### Inferences
- Hor+ implementation: keep vertical FOV constant. Set `tan(hfov/2) = tan(vfov/2) * aspect`. Feed the same aspect into frustum or visibility culling, or objects will pop at screen edges. Kya's `computedVerticalHalfFOV` / `baseHorizontalHalfFOV` fields (`src/b-witch/CameraViewManager.h`) and the hardcoded `aspectRatio = 1.333333f` (`CameraViewManager.cpp:54`) are the place to start. Make sure any per-object or sector culling (portal/sector visibility, LOD distances) uses the widened frustum.
- HUD: the PS2 HUD is authored in a 640x448/512-ish 4:3 space. Remap it with a virtual 4:3 canvas centered on screen, and anchor elements to their nearest edge (left, right, or center), rather than stretching. Full-screen 2D overlays (fades, letterbox bars, screen flashes) should stretch to the full width. Menus and FMVs should be pillarboxed.
- FMVs: decode at the source aspect, draw into a letterboxed or pillarboxed viewport, and don't scale non-uniformly.
- Pitfall: some 2D effects (screen-space distortion, sky planes, lens flares) assume a 4:3 viewport and need case-by-case fixes.

### Gaps
- I found no technical write-up of how specific PS2 widescreen pnach patches handle culling and HUD internally.
- I found no Digital Foundry analysis on widescreen in PS2 remasters.

## 3. High frame rate: fixed timestep + interpolation and decoupled simulation

### Takeaway
There are three proven approaches:
1. Gaffer-style fixed simulation tick with render-state interpolation by `alpha = accumulator/dt`.
2. Render-command or matrix interpolation layered on an unmodified 20/30 Hz game. Ship of Harkinian records matrices; RT64 matches draw calls.
3. Making game logic delta-time aware and fixing each bug that appears. This is the OpenGOAL route, and it needs many per-feature fixes.

For a decompiled game like Kya, (1)+(2) keeps original gameplay timing. (3) risks physics and gameplay changes.

### Cited Findings
- Fix Your Timestep: "The renderer produces time and the simulation consumes it in discrete dt sized steps." The remainder stays in an accumulator. Interpolate previous and current state with `alpha = accumulator / dt`. Clamp frame time (`if (frameTime > 0.25) frameTime = 0.25`) to avoid the "spiral of death", where a slow simulation falls further behind. — [Gaffer On Games](https://gafferongames.com/post/fix_your_timestep/)
- Ship of Harkinian (Shipwright) records a tree of matrix operations per frame (MatrixPush/Pop/Translate/Rotate…) delimited by OPEN_DISPS/CLOSE_DISPS and manual `FrameInterpolation_OpenChild/CloseChild` calls. It first tried interpolating parameters (angles, scales), which "proved problematic for certain animations". It now interpolates final matrices. For large (~180°) rotations it decomposes into model space and interpolates the rotation separately, to avoid a "paper effect". Interpolation runs between two recorded frames with step 0..1, and `FrameInterpolation_DontInterpolateCamera()` handles camera cuts. — [Shipwright frame_interpolation.cpp](https://raw.githubusercontent.com/HarbourMasters/Shipwright/develop/soh/soh/frame_interpolation.cpp)
- RT64 tries "automatic matching of draw calls between frames based on their rendering parameters, the textures in use, their position, orientation, their tracked velocities". Transformations "are decomposed into their constituent components… interpolated separately and recomposed". It calls this "highly experimental… prone to errors". — [RT64 GitHub](https://github.com/rt64/rt64)
- Because RT64 defers all transformations, it can patch object and camera transforms and produce new frames quickly, which removes CPU bottlenecks at high framerates. — [RT64 coverage via search](https://www.GitHub.com/rt64/rt64)
- OpenGOAL (Jak 1–3) runs the game logic at higher framerates. Jak 1/2 support above 60 fps, with 90 fps experimental, and Jak 3 is limited to 60. This needed many per-feature fixes: projectile and attack speeds, camera turning speeds, flyingsaw movement. It also scales the input buffer from 3 frames at 60 fps up to 15 frames at the highest setting. — [OpenGOAL Q4 2025 progress report](https://opengoal.dev/blog/progress-report-q4-2025); [OpenGOAL Q2 2026 progress report](https://opengoal.dev/blog/progress-report-q2-2026)
- PCSX2 60 fps patches are per-game code patches keyed by game ID. — [PCSX2 docs: Writing patches](https://pcsx2.net/docs/advanced/writing-patches)

### Inferences
- For Kya, the recommended plan:
  - Keep the original logic tick (likely 50/60 Hz field-based or 30 Hz; verify in the decompiled main loop).
  - Snapshot render-relevant state each tick: camera position, look-at, and FOV; object world matrices; skeletal bone matrices after animation; particle positions.
  - Render N frames per tick by lerping or slerping between the last two snapshots. This is a native-code version of the SoH approach. The port knows object identity, so it avoids RT64's heuristic matching.
- Pitfalls:
  - Camera cuts and teleports need an explicit "don't interpolate" signal, like SoH's `DontInterpolateCamera`.
  - Spawned or destroyed objects have no previous state. Render them at the current state.
  - Particles: either interpolate per-particle positions with stable IDs, or simulate them at render rate (they are usually cosmetic).
  - UV-scrolling, texture animation, and screen-space effects tied to frame counts must be driven by interpolated time.
  - Interpolation adds about one tick of latency.
  - Interpolate matrices by decomposition (translate/quat/scale), not component-wise lerp, to avoid shear or "paper" collapse.
- Things the game code ties to frame counts, such as vblank counters, VU timing, and per-frame decrements, are the main source of bugs if you instead try the OpenGOAL-style variable dt.

### Gaps
- I didn't fetch SoH's documentation on which subsystems (particles, skeletal animation, 2D) were problematic. The source comment only covers matrices.
- I found no primary docs on how RT64's recompiled ports (e.g., Zelda64Recomp) add interpolation hints.

## 4. Anti-aliasing, texture filtering, dithering, color clamping, gamma

### Takeaway
The cited sources cover upscaled internal resolution (SSAA) in RT64. They don't detail the AA modes themselves. For PS2-style content, supersampling or MSAA on geometry plus careful handling of alpha-tested foliage is the safe route. TAA needs motion vectors and is risky with the 2D/post-pass tricks PS2 games use.

### Cited Findings
- RT64 offers rendering "with a higher resolution and downsample to a resolution closer to the original game", which is effectively SSAA and a "faithful" option. — [RT64 GitHub](https://github.com/rt64/rt64)
- FSR's guidance on motion vectors and reactive masks (section 6) applies equally to TAA. Alpha-blended content without depth or motion vectors needs special handling. — [AMD FSR 3.1 docs](https://gpuopen.com/manuals/fidelityfx_sdk/techniques/super-resolution-upscaler/)

### Inferences
These are reasoned recommendations. I didn't verify them against a fetched primary source in this session:
- MSAA in Vulkan works well for opaque geometry. Use alpha-to-coverage for alpha-tested foliage and fences, because PS2 games use alpha test heavily. Full-screen effects that read the frame buffer need resolve-before-sample.
- FXAA/SMAA are cheap post AA. Apply them before the HUD so text isn't blurred.
- TAA adds ghosting on UV-scrolling and alpha surfaces.
- Mipmapping and anisotropic filtering: many PS2 games shipped without mips or with manual LOD (the GS uses K/L LOD parameters). Generate mips in the texture cache on upload, and enable anisotropy (VkSamplerCreateInfo `anisotropyEnable`/`maxAnisotropy`) only for 3D world textures. Never use them for HUD, fonts, palettized lookup textures, or render-target-as-texture reads. Render targets and clamped sprite atlases bleed with mips or anisotropy.
- Dithering: the PS2 applies a 4x4 ordered dither when writing 16-bit framebuffers. When rendering at 32-bit or higher, disable dither or do it at native scale. Scaled dither patterns look like screen-door noise.
- Color clamp: the GS has COLCLAMP and wrap modes for 8-bit color. Blending in float formats must reproduce the clamp or wrap, or additive effects will look different.
- Gamma: PS2 output was designed for CRT TV gamma, so expose a gamma/brightness slider.

### Gaps
- I didn't fetch PCSX2's documentation on its dithering options ("Unscaled", "Scaled", "Force 32-bit"), blending accuracy, or its texture filtering and trilinear options.
- I found no sourced comparison of MSAA, FXAA, SMAA, and TAA for retro content.

## 5. Post-processing, lighting upgrades, HDR, ray tracing: faithful vs. art-breaking

### Takeaway
HDR output in Vulkan on Windows is done by choosing an HDR10/PQ (`VK_COLOR_SPACE_HDR10_ST2084_EXT`) or scRGB (`VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT`) swapchain color space and doing tone mapping and the transfer function yourself. RT64 shows that ray-traced lighting is technically possible on fixed-function-era content, but it needs material and light authoring and isn't in its public repo yet. Treat it as a stretch goal, not a faithful mode.

### Cited Findings
- `VK_COLOR_SPACE_HDR10_ST2084_EXT` = BT.2020 primaries + SMPTE ST2084 PQ encoding. The extended sRGB color spaces map to scRGB. — [Khronos VkColorSpaceKHR](https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VkColorSpaceKHR.html)
- For non-sRGB color spaces the implementation doesn't apply the OETF. Application shaders must do the encoding. `VK_EXT_swapchain_colorspace` adds these color spaces, and supported formats are queried with `vkGetPhysicalDeviceSurfaceFormatsKHR`. — [Khronos VK_EXT_swapchain_colorspace](https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VK_EXT_swapchain_colorspace.html)
- `VK_EXT_hdr_metadata` passes mastering display metadata. — [VK_EXT_hdr_metadata reference](https://hackage-content-origin.haskell.org/package/vulkan-3.27/candidate/docs/Vulkan-Extensions-VK_EXT_hdr_metadata.html)
- RT64 advertises ray-traced lighting, object motion blur, DLSS, and widescreen. However, "Emulator support (Plugin) and Ray Tracing (RT) are not available in this repository yet", and path tracing and model replacement are still in development. — [RT64 GitHub](https://github.com/rt64/rt64); [Nintendo Life coverage](https://3ds.nintendolife.com/news/2022/06/new-n64-emulator-plugin-adds-ray-tracing-widescreen-60fps-and-more-to-classics-like-zelda-and-paper-mario)

### Inferences
- Faithful options (low risk): higher resolution, AF, MSAA or SMAA, HDR output that maps the original SDR image into HDR at a chosen paper white with optional highlight expansion only on bloom or additive passes, correct gamma, and widescreen.
- Art-direction-risky options (should be toggles, default off):
  - SSAO: PS2 scenes often have baked vertex lighting, so AO double-darkens.
  - New dynamic shadows: they conflict with blob or projected shadows and baked lighting.
  - Bloom replacement: changes the intended glow.
  - Ray tracing: needs light sources that don't exist in PS2 data.
- HDR pitfall: PS2 additive effects clamp at 1.0 (see the COLCLAMP note in section 4). Unclamping for HDR changes the look, so expose it as an option.
- Do UI compositing after tone mapping, at a fixed nits level.

### Gaps
- I found no sourced material on SSAO or shadow upgrades for PS2 ports specifically, and no Digital Foundry analyses (none fetched).
- I didn't fetch AMD or NVIDIA HDR best-practice docs.

## 6. Upscalers (FSR 2/3, DLSS, XeSS) and frame generation

### Takeaway
Temporal upscalers need per-pixel motion vectors, depth, and masks for transparency and particles. A PS2-derived renderer produces none of these natively. Adding motion vectors requires previous-frame transforms per draw, which the frame-interpolation snapshot system can supply. Frame generation additionally needs HUD-less color and UI buffers. For content this light, spatial upscaling isn't needed for performance, so temporal upscalers mostly serve as AA (DLAA- or native-AA-style) at best.

### Cited Findings
- FSR 3.1 needs current-frame 2D motion vectors in the range ±(width, height), with optional `motionVectorScale`. 16-bit precision is enough. The motion vector buffer is at render resolution unless the display-resolution flag is set. — [AMD FSR 3.1.4 Upscaler docs](https://gpuopen.com/manuals/fidelityfx_sdk/techniques/super-resolution-upscaler/)
- The reactive mask marks areas without a depth or motion vector footprint, "particles or alpha-blended objects which don't write depth or motion vectors". The Transparency & Composition mask is a softer alternative for things like animated textures. — [AMD FSR 3.1.4 Upscaler docs](https://gpuopen.com/manuals/fidelityfx_sdk/techniques/super-resolution-upscaler/); [FSR 3.1.5 docs](https://gpuopen.com/manuals/fsr_sdk/techniques/super-resolution-upscaler/)
- DLSS Frame Generation needs depth, motion vectors, HUD-less color, and UI color buffers via Streamline. Missing or incorrect inputs cause "blurry objects, broken characters, increased flickering, and instability in UI and HUD". — [NVIDIA: integrating DLSS 4 with Streamline](https://developer.nvidia.com/blog/how-to-integrate-nvidia-dlss-4-into-your-game-with-nvidia-streamline); [NVIDIA: integrating DLSS 3](https://developer.nvidia.com/blog/how-to-successfully-integrate-dlss-3)
- One search summary claimed "DirectX 12 is required for DLSS Frame Generation", while also noting Streamline 2.4.10 Vulkan fixes. Treat the DX12-only claim as possibly outdated. Check the current Streamline docs. — [NVIDIA Streamline SDK](https://developer.nvidia.com/rtx/streamline/get-started)
- Vulkan Reflex needs an extra Vulkan layer when run through DXVK-NVAPI-type paths. — [NVIDIA driver guide: DLSS/Reflex](https://docs.nvidia.com/datacenter/tesla/driver-installation-guide/gaming.html)
- RT64 supports DLSS. — [wccftech coverage](https://wccftech.com/n64-emulator-plugin-rt64-ray-traced-lighting-dlss-object-motion-blur-widescreen-60fps/amp/)

### Inferences
- PS2 content pitfalls for temporal upscalers and frame generation:
  - Heavy alpha-blended particles, UV-scrolling water or sky, and full-screen feedback effects (motion blur via previous-frame blend, which is very common on PS2) all lack proper motion vectors and will smear.
  - Render-to-texture tricks (screen distortion, reflections) need reactive or T&C masks.
- Frame generation is a poor fit compared with native interpolation (section 3). Native interpolation produces real frames without added latency or artifacts. Frame generation could still layer on top for 240 Hz displays.
- XeSS has similar input requirements to FSR. I didn't verify this in this session.

### Gaps
- I didn't fetch XeSS docs.
- I didn't verify whether the DLSS FG Vulkan path is production-ready as of 2026.

## 7. Shader compilation stutter and pipeline caching in Vulkan

### Takeaway
PS2-style renderers generate many state permutations: blend modes, alpha test, fog, and texture formats. Standard fixes:
1. A persisted `VkPipelineCache`.
2. Pre-creating known pipelines at load.
3. `VK_EXT_graphics_pipeline_library` and/or dynamic state to shrink permutations.
4. RT64's approach: an ubershader fallback, with specialized pipelines compiled in the background.

### Cited Findings
- Creating pipelines at draw time without a pipeline cache causes stutter. Serialize `VkPipelineCache` data, reload it on the next run, and create known pipelines early. — [Vulkan Samples: Pipeline Management](https://docs.vulkan.org/samples/latest/samples/performance/pipeline_cache/README.html)
- `VK_EXT_graphics_pipeline_library` allows partial pipeline compilation for apps with many materials or much dynamic state. Monolithic pipelines "either fail to eliminate hitching, or require precompiling so many state combinations that the size of the pipeline cache is nearly unmanageable". — [Khronos VK_EXT_graphics_pipeline_library proposal](https://docs.vulkan.org/features/latest/features/proposals/VK_EXT_graphics_pipeline_library.html)
- A pipeline cache can be made robust against driver and version mismatch by validating the header (vendor/device ID, cache UUID) before reuse. — [Roblox: Creating a robust pipeline cache with Vulkan](https://about.roblox.com/newsroom/2021/02/creating-a-robust-pipeline-cache-with-vulkan)
- `VK_EXT_pipeline_creation_cache_control` allows fail-on-compile-required flags, which supports a "use fallback if not cached" strategy. — [Khronos ref page](https://docs.vulkan.org/refpages/latest/refpages/source/VK_EXT_pipeline_creation_cache_control.html)
- RT64 "uses ubershaders to guarantee no stutters due to pipeline compilation". — [RT64 GitHub](https://github.com/rt64/rt64)
- Blender's Vulkan backend notes on pipeline management. — [Blender developer docs](https://developer.blender.org/docs/features/gpu/vulkan/pipelines/)

### Inferences
- For Kya: enumerate the finite PS2 state space (blend, alpha test, depth test/write, fog, texture/CLUT format), key pipelines by a compact hash, and warm them at level load from a recorded list of permutations.
- Persist `VkPipelineCache`.
- Use dynamic state (Vulkan 1.3 / `VK_EXT_extended_dynamic_state` 1–3) for depth, cull, and blend where supported, to collapse permutations.
- Optionally add an ubershader path that draws while the specialized pipeline compiles on a worker thread.

### Gaps
- I have no measured permutation counts for Kya.
- I didn't fetch current (2025–2026) driver vendor guidance on `VK_EXT_shader_object` versus graphics pipeline libraries.
