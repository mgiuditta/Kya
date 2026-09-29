# Replacing dynamic blend / color-write state with pipeline variants on macOS

Research for mgiuditta/Kya#16 (map #8, "Kya on macOS"). Follows up on
[moltenvk-support.md](https://github.com/mgiuditta/Kya/blob/research/moltenvk-support/docs/research/macos/moltenvk-support.md)
(#10), which found that MoltenVK 1.4.2 and KosmicKrisp report
`extendedDynamicState3ColorBlendEnable`, `extendedDynamicState3ColorBlendEquation` and
`extendedDynamicState3ColorWriteMask` as unsupported, and that neither exposes `VK_EXT_color_write_enable`.

Code references are to `docs/research` @ `028d4d4`. Paths are relative to
`port/Windows/Renderer/Vulkan/src/` unless they start with `port/` or `src/`.

## TL;DR

- **The blend equation already uses pipeline variants.** The #10 note assumed `Blending.cpp` switches blend state
  per draw. It doesn't anymore: the native renderer resolves the GS `ALPHA` register to a
  `VkPipelineColorBlendAttachmentState` and binds a **lazily created, cached** pipeline per `(blendIndex, ABE)`
  (`Native/NativeRendererSetup.cpp:136-261`). The display-list renderer does the same (`Native/NativeDisplayList.cpp:375-402`).
  No pipeline in the tree declares `VK_DYNAMIC_STATE_COLOR_BLEND_ENABLE_EXT` or `..._COLOR_BLEND_EQUATION_EXT`.
- **The only caller of the EDS3 blend commands is dead weight.** `Blending.cpp:200-234` (`SetBlendingDynamicState`) is called only from
  the debug mesh viewer (`port/DebugMenu/src/DebugMeshViewerVulkan.cpp:333`), whose pipeline has static blend state. Per the spec
  that call is invalid usage, and it has no effect. Delete it and drop the two EDS3 blend features from the required list.
- **What actually has to move into the pipeline key is color-write state:** `VK_DYNAMIC_STATE_COLOR_WRITE_ENABLE_EXT` and
  `VK_DYNAMIC_STATE_COLOR_WRITE_MASK_EXT` on the native pipelines. The code produces only **3 write states**: RGBA, RGB (no alpha)
  and "no color write".
- **Variant count:** 81 blend indices plus "blend off" give 82 keys, which collapse to **47 distinct blend attachments**.
  Adding the 3 write states gives at most 47×2+1 = **95 distinct native pipelines**. When writes are off, blend state doesn't
  matter, so all "off" variants are one pipeline. That's if the four main render stages share variants, which Vulkan's render-pass
  compatibility allows. The source code itself hard-codes only 5 `ALPHA` values; the rest come from material data, so expect a few dozen
  in real play.
- **Keep lazy creation plus a cache,** as today. Add the write state to the existing key, share the map across compatible stages,
  and add a `VkPipelineCache` (none exists today). That cache matters more on MoltenVK, which uses it to skip SPIR-V→MSL conversion.
- **Keep Windows unchanged with a runtime feature check,** not `#ifdef __APPLE__`: one `bDynamicColorWrite` flag, set from the queried
  features. When it's true, the write bits of the key are forced to "RGBA" and the dynamic states stay in the pipeline. A debug override
  lets the static path be tested on Windows.

## 1. How pipelines are created and keyed today

### Native renderer (the 3D path)

- Render stages: `NativeRendererState::renderPass` is an `unordered_map<RenderPassKey, RenderStage>` (`Native/NativeRendererInternal.h:298`),
  keyed by `clearMode | kind << 8` (`NativeRendererInternal.h:114-150`). `Setup()` creates 6 stages: Main × {None, Depth, ColorDepth, Color},
  ShadowMask and ShadowReceiver (`Native/NativeRendererSetup.cpp:412-428`). Each has its own `VkRenderPass`
  (`NativeRendererSetup.cpp:73-129`).
- Base pipeline: `RenderStage::CreatePipeline()` builds one pipeline per stage through `Native::CreatePipeline`
  (`Native/NativeRenderer.cpp:26-52`, `NativeRendererSetup.cpp:263-383`) and stores it as blend key 0 (`NativeRenderer.cpp:52`).
- Blend variants: `GetBlendPipeline(key, alpha, ABE)` (`NativeRendererSetup.cpp:247-261`):
  1. `ResolveBlendState` (`Native/Blending.cpp:158-198`) maps `ALPHA.{A,B,C,D}` to `blendIndex = ((A*3+B)*3+C)*3+D` and looks it up in
     the PCSX2-derived 81-entry `m_blendMap` (`Blending.cpp:46-129`).
  2. Key = `blendIndex | (blendEnable ? 0x100 : 0)` (`NativeRendererSetup.cpp:136-139`).
  3. On a miss, `CreateBlendPipeline` (`NativeRendererSetup.cpp:141-245`) builds a full pipeline with that attachment state baked in and
     stores it in `RenderStage::gBlendPipelines` (`unordered_map<uint16_t, VkPipeline>`, `NativeRendererInternal.h:189`).
- Recording binds the variant only when the prim, ABE or effective `ALPHA` changes (`Native/NativeRendererRecording.cpp:329-341`).
  The preview renderer does the same (`Native/NativePreviewRenderer.cpp:117`).
- Dynamic states on every native pipeline (`NativeRendererSetup.cpp:201-208` and `341-348`): viewport, scissor,
  depth write enable, depth compare op (core 1.3, supported by MoltenVK), **`COLOR_WRITE_ENABLE_EXT`** and
  **`COLOR_WRITE_MASK_EXT`**.

### Display-list renderer (2D/HUD/particles)

- `DisplayListPipelineState::blendPipelines` (`unordered_map<uint8_t, VkPipeline>`, `Native/NativeDisplayList.cpp:43`) is keyed by
  `blendIndex` only, because blending is always on (`NativeDisplayList.cpp:375-402`). The only dynamic states are viewport and scissor
  (`NativeDisplayList.cpp:308-311`), so for #16 it's **already Metal-compatible**. Its geometry shader is a separate problem (#10).

### Other pipelines

Post-processing, shadow blur, debug shapes and the final framebuffer blit all use fixed blend state and only viewport/scissor as
dynamic state (`Native/PostProcessing.cpp:118-167`, `Native/NativeShadow.cpp:165-192`, `Native/NativeDebugShapes.cpp:420-472, 534-586`,
`Objects/FrameBuffer.cpp:225-261`). None of them are affected.

The PCSX2-derived `Objects/Pipeline.cpp` already bakes blend and `wrgba` into its `PipelineSelector` key
(`Pipeline.cpp:42-46, 145-170, 267-284`). But `PS2::GetPipeline` has **no callers** in the tree; only `GetPipelines().clear()` is used
(`VulkanPS2.cpp:75, 496-499`). It's dead, and it doesn't need to change.

### Pipeline cache

**There is none.** Every `vkCreateGraphicsPipelines` passes `VK_NULL_HANDLE` as the cache
(`NativeRendererSetup.cpp:239, 378`, `NativeDisplayList.cpp:341`, and the others above). The PCSX2 builder's cache argument is commented out
(`Objects/Pipeline.cpp:178`). Also, `CreateBlendPipeline` reloads and re-reflects both SPIR-V files on every miss
(`NativeRendererSetup.cpp:146-147`) instead of reusing the stage's modules.

## 2. Where the dynamic color state is set

| Call site | State | Values produced |
|---|---|---|
| `Native/NativeRendererRecording.cpp:177-211` `SetColorDepthDynamicState` (main + shadow-mask draws) | color write enable, write mask | enable = `FALSE` only for the duplicated `AFAIL_ZB_ONLY && ATST_NEVER` z-only draw (`NativeRendererSubmission.cpp:345-348`); mask = RGB when `TEST.AFAIL == AFAIL_RGB_ONLY` and not a framebuffer material, else RGBA |
| `NativeRendererRecording.cpp:343-349` (shadow receiver) | enable, mask | always TRUE / RGBA |
| `Native/NativePreviewRenderer.cpp:121-125` | enable, mask | enable = `!bIsAfailZOnly`; mask RGBA |
| `port/DebugMenu/src/DebugMeshViewerVulkan.cpp:308-322` | enable, mask | TRUE / RGBA |
| `port/DebugMenu/src/DebugMeshViewerVulkan.cpp:333` → `Blending.cpp:200-234` | **EDS3 blend enable + equation** | see below |

**`FBMSK` is not used by the native renderer.** It's stored (`port/Windows/Renderer/include/GSState.h:72`,
`include/GIFReg.h:231`) but never read into a write mask. The only `FBMSK`-like path is the dead PCSX2 `wrgba`. If `FBMSK` is emulated later,
only per-byte all-or-nothing masks (0x00/0xFF per channel) map to `VkColorComponentFlags` (4 bits, 16 values). Partial-bit masks would need
shader-side read-modify-write in any case.

### The EDS3 blend call is invalid and has no effect

`DebugMeshViewerVulkan.cpp:306` binds a pipeline created by `Native::CreatePipeline` with static blend state
(`NativeRendererSetup.cpp:321-329`; its dynamic list has no blend states). It then calls `vkCmdSetColorBlendEnableEXT` /
`vkCmdSetColorBlendEquationEXT` (`Blending.cpp:213, 224/229`). The Vulkan spec, *Dynamic State*: "If the state is not specified as dynamic
in the new pipeline object ... Before any draw or dispatch call with this pipeline there must: not have been any calls to any of the
corresponding dynamic state setting commands after this pipeline was bound."
([pipelines.adoc, `dynamic-state-current-value`](https://github.com/KhronosGroup/Vulkan-Docs/blob/main/chapters/pipelines.adoc#pipelines-dynamic-state)).
So the call is a validation error, and the mesh viewer draws with the pipeline's static "blend off" anyway.
`SetBlendingDynamicState` should be deleted (or the mesh viewer switched to `GetBlendPipeline`-style variants). After that,
`extendedDynamicState3ColorBlendEnable`/`Equation` don't need to be required anywhere
(`VulkanRenderer.cpp:136-137, 189-190, 716-717`).

## 3. How many variants

### Blend attachments

`m_blendMap` has 3⁴ = 81 entries. Resolving them the way `ResolveBlendState` does (factors/op from the table; `BLEND_HW_CLR1` forces
`dst = SRC1_COLOR`; the alpha channel is always `ONE, ZERO, ADD`) gives **46 distinct enabled attachment states**:

- `Cs` (ONE, ZERO, ADD): 12 indices (`0000, 0010, 0020, 0200, 0210, 0220, 1100, 1110, 1120, 2200, 2210, 2220`)
- `Cd` (ZERO, ONE, ADD): 9 indices (`xx01` degenerate cases)
- `0` (ZERO, ZERO, ADD): 9 indices (`xx02` degenerate cases)
- `Cd*(1+X)` via HW_CLR1 (DST_COLOR, SRC1_COLOR, ADD): 3 indices (`1201, 1211, 1221`)
- six pairs where `A=0` and `A=2` resolve alike (`0100/2100`, `0110/2110`, `0120/2120`, `1001/2001`, `1011/2011`, `1021/2021`)
- 34 unique ones

Plus "ABE off", which is 1 more (blendIndex is forced to 0, `Blending.cpp:170-172`). **Total: 47 distinct blend states for 82 key values.**
(An `ALPHA` field value of 3, which is reserved on the GS, would index past the 81-entry table in `GetBlend`. It never happens in practice,
but a variant key built from raw fields should mask or assert.)

### Write state

From §2: `{RGBA, RGB, none}` = 3. With no color writes, blend state is irrelevant, so every "none" variant is the same pipeline.
Metal also lets `writeMask = 0` stand in for write-enable, since both are fields of the same PSO color attachment descriptor
([`MTLRenderPipelineColorAttachmentDescriptor`](https://developer.apple.com/documentation/metal/mtlrenderpipelinecolorattachmentdescriptor):
`writeMask`, `isBlendingEnabled`, blend factors/operations).

### Per stage and total

| Pipeline family | Key today | Key on macOS | Distinct upper bound |
|---|---|---|---|
| Native main (4 clear-mode stages + preview on `Empty`) | `blendIndex \| ABE<<8` per stage (≤82 each, ≤328 total) | + 2-bit write state | 47×2 + 1 = **95** if shared across the 4 stages; 4× that if not |
| Native shadow mask | base pipeline, blend off | + write state (`SetColorDepthDynamicState` also runs there) | **3** |
| Native shadow receiver | base pipeline | fixed RGBA, no key change | **1** |
| Display list | `blendIndex` (≤81) | unchanged | **46** distinct (81 keys) |

Sharing across the 4 main stages is legal. Their render passes differ only in load ops and initial layouts
(`NativeRendererSetup.cpp:73-95`, same formats and same `mainDependency`), and Vulkan's *Render Pass Compatibility* ignores
"Initial and final image layout" and "Load and store operations"
([renderpass.adoc, `renderpass-compatibility`](https://github.com/KhronosGroup/Vulkan-Docs/blob/main/chapters/renderpass.adoc)).
So a variant created against `CM None` can be bound inside `CM ColorDepth`.

### What is actually hit

`ALPHA` values hard-coded in the decompiled code:

| ALPHA (ABCD) | Meaning | Source |
|---|---|---|
| `0101` | `Cs*As + Cd*(1-As)` (normal alpha) | `src/b-witch/ed3D.cpp:2743, 2837, 5568`, `src/b-witch/edDlist.cpp:991` (`edDListBlendFuncNormal`), `src/edVideo/VideoB.cpp:119` |
| `0201` | `Cs*As + Cd` (additive) | `edDlist.cpp:1002-1006` (`edDListBlendFunc_002ca830`, raw `0x48`) |
| `1210` | `Cs + Cd*Ad` | `edDlist.cpp:794` (`edDListBlendFunc50`) |
| `1201` | `Cd*(1+As)` (HW_CLR1) | `src/b-witch/Pause.cpp:654` via `edDListAlphaBlend` |
| `2220` | `Cs` (blend no-op) | `ed3D.cpp:5579, 6749, 7300, 11880` |

`ed3D.cpp:7137` uses runtime globals, and most 3D draws take `ALPHA` from the material's texture registers
(`NativeRendererRecording.cpp:323-327`), so the full set depends on the data. The existing `DrawTrace` already records the effective
alpha per draw (`NativeRendererRecording.cpp:236-237`). Logging `gBlendPipelines.size()` per stage after a level playthrough would pin
down the real number. Given the handful of GS blend modes PS2 games normally use, the working set should be around 5-15 blend states × ≤3
write states.

## 4. All at startup, lazily, or cached?

**Lazy + in-memory map + `VkPipelineCache`, i.e. extend what exists.**

- *All at startup:* 95 native + 46 display-list pipelines (4× the native count without stage sharing) is affordable, but most of them
  are never used, and on MoltenVK each one is a Metal PSO compile. It would also slow startup on Windows for no benefit.
- *Lazy (today):* the first use of a new blend mode stalls the recording thread for one pipeline compile. That's acceptable, and it's
  what PCSX2's Vulkan backend (which this code derives from) does.
- *Cached:* add one `VkPipelineCache`, pass it to every `vkCreateGraphicsPipelines`, and save/load it with `vkGetPipelineCacheData`
  across runs. The spec defines the cache for reuse "between pipelines and between runs of an application"
  ([pipelines.adoc `pipelines-cache`](https://github.com/KhronosGroup/Vulkan-Docs/blob/main/chapters/pipelines.adoc)). MoltenVK uses the
  serialized cache to skip SPIR-V→MSL conversion on later runs
  ([MoltenVK Runtime User Guide, "Shader Loading Time"](https://github.com/KhronosGroup/MoltenVK/blob/main/Docs/MoltenVK_Runtime_UserGuide.md#shader_load_time)).
  Optionally warm the few known modes (`0101, 0201, 2220`, blend-off) at stage creation.
- Cheap win either way: reuse the stage's shader modules in `CreateBlendPipeline` instead of reloading and reflecting SPIR-V on every
  miss (`NativeRendererSetup.cpp:146-147`).

## 5. Keeping the Windows dynamic path: runtime check vs compile-time guard

**Use a runtime feature check.**

- The information is already queried: `ValidateRequiredDeviceFeatures` reads `VkPhysicalDeviceColorWriteEnableFeaturesEXT` and
  `VkPhysicalDeviceExtendedDynamicState3FeaturesEXT` (`VulkanRenderer.cpp:169-191`). Change it so these are optional:
  - move `VK_EXT_extended_dynamic_state3` and `VK_EXT_color_write_enable` from `deviceExtensions` (`VulkanRenderer.cpp:47-53`) to
    `optionalDeviceExtensions`
  - drop the two blend-EDS3 requirements (§2)
  - set `bDynamicColorWrite = colorWriteEnable && extendedDynamicState3ColorWriteMask`
  - chain the feature structs at device creation only when supported (`VulkanRenderer.cpp:712-729`)
- `bDynamicColorWrite == true` (Windows, today's behavior): keep the two dynamic states in the pipeline, force the write-state bits of
  the variant key to "RGBA" so the variant count doesn't grow, and keep calling `vkCmdSetColorWrite*EXT`. Nothing changes.
- `bDynamicColorWrite == false` (MoltenVK/KosmicKrisp): omit the two dynamic states, bake `colorWriteMask` (0 for "no write") into
  the attachment, and have `SetColorDepthDynamicState` return the write state for `GetBlendPipeline` instead of calling the EXT
  functions. Depth write/compare stay dynamic, since they're core 1.3 and MoltenVK supports them.
- Why not `#ifdef __APPLE__`: the deciding fact is a driver feature, not the OS. A runtime flag lets the static path be exercised on
  Windows (debug-menu or env override, or `KyaPortTest`), and it picks up any future driver that gains the features. It also keeps a
  single code path in the recorder. PCSX2 handles optional Vulkan features the same way, with runtime feature flags.
- Blend *constants* are unaffected: `VK_DYNAMIC_STATE_BLEND_CONSTANTS` is core 1.0 and maps to Metal's per-encoder
  [`setBlendColor`](https://developer.apple.com/documentation/metal/mtlrendercommandencoder/setblendcolor(red:green:blue:alpha:)).
  Side finding: the native and display-list pipelines don't make blend constants dynamic and never call `vkCmdSetBlendConstants`
  (static constants are `0`, `NativeRendererSetup.cpp:194-197`). So every `C = FIX` mode (`CONST_COLOR` factors) currently blends with
  F = 0 instead of `ALPHA.FIX`. Worth a separate ticket, and it doesn't add variants.

## Sources

- Vulkan spec, *Dynamic State* / *Pipeline Cache*: [Vulkan-Docs `chapters/pipelines.adoc`](https://github.com/KhronosGroup/Vulkan-Docs/blob/main/chapters/pipelines.adoc) (`[[pipelines-dynamic-state]]`, `[[dynamic-state-current-value]]`, `[[pipelines-cache]]`)
- Vulkan spec, *Render Pass Compatibility*: [Vulkan-Docs `chapters/renderpass.adoc`](https://github.com/KhronosGroup/Vulkan-Docs/blob/main/chapters/renderpass.adoc) (`[[renderpass-compatibility]]`)
- MoltenVK v1.4.2 extension table: [`MVKExtensions.def`](https://github.com/KhronosGroup/MoltenVK/blob/v1.4.2/MoltenVK/MoltenVK/Layers/MVKExtensions.def) (EDS3 exposed at L142, no `EXT_color_write_enable`); EDS3 sub-features per #10.
- MoltenVK Runtime User Guide, [Shader Loading Time](https://github.com/KhronosGroup/MoltenVK/blob/main/Docs/MoltenVK_Runtime_UserGuide.md#shader_load_time)
- Apple Metal: [`MTLRenderPipelineColorAttachmentDescriptor`](https://developer.apple.com/documentation/metal/mtlrenderpipelinecolorattachmentdescriptor), [`setBlendColor`](https://developer.apple.com/documentation/metal/mtlrendercommandencoder/setblendcolor(red:green:blue:alpha:))
- Prior finding: [moltenvk-support.md](https://github.com/mgiuditta/Kya/blob/research/moltenvk-support/docs/research/macos/moltenvk-support.md) (#10)
- Repo code as cited inline. Variant counts were computed by parsing `m_blendMap` in `Native/Blending.cpp` and applying `ResolveBlendState`'s rules.
