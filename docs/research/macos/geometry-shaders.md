# Geometry shaders in the Kya port, and what can replace them on macOS

Research for [mgiuditta/Kya#11](https://github.com/mgiuditta/Kya/issues/11), part of the macOS map (#8).
Code references are to branch `docs/research` at `028d4d4`. External sources were checked on 2026-09-29.

## TL;DR

- **Only one geometry shader runs at runtime:** `displaylist.geom.glsl`. It turns a PS2 GS **SPRITE** (two corner vertices sent as a line) into a screen-aligned quad for the 2D/HUD display list (`NativeDisplayList.cpp:414-420`).
- **The `ps2.hlsl` `gs_main` variants are dead code at runtime.** They belong to the PCSX2-derived `PS2::GetPipeline` path, and nothing calls that function (see §2).
- MoltenVK does not expose `geometryShader`, and neither does Metal. The renderer currently **requires** the feature and stops at device creation without it (`VulkanRenderer.cpp:131`, `:184`, `:727`).
- **Recommendation: expand sprites on the CPU in `NativeDisplayList.cpp`,** giving 4 vertices and 6 indices per sprite drawn with the existing triangle-list pipeline. Also make `geometryShader` optional. This is about 40-60 lines, costs next to nothing (only HUD sprites go through it), and produces the same triangles as the GS. It can be limited to macOS (runtime feature check or `__APPLE__`) so Windows keeps its current path. It is also simple enough to use everywhere.
- Vertex-shader expansion (PCSX2's approach) is the second choice. Compute pre-passes and mesh shaders are overkill here. MoltenVK has no GS emulation to fall back on.

## 1. What each geometry shader does

### 1.1 `displaylist.geom.glsl`: sprite to quad (live)

`port/Windows/Renderer/Shaders/src/displaylist.geom.glsl`

- Input `layout(lines)`, output `triangle_strip, max_vertices = 4` (`:3-4`).
- Input vertex 0 is the top-left corner (LT) and vertex 1 is the bottom-right corner (RB), already in clip space (`:17-23`; the vertex shader passes `inXYZ` straight through, `displaylist.vert.glsl:12`).
- **Flat depth:** `lt.z = rb.z` (`:26`).
- **Flat colour:** all 4 output vertices use RB's colour (`:29`, `:41`, `:47`, `:53`, `:59`). This matches PS2 sprite behaviour, where the last vertex's colour applies.
- Builds LB = (lt.x, rb.y) and RT = (rb.x, lt.y) with texcoords swapped to match (`:32-36`, `:48`, `:54`).
- Emits the strip LT, LB, RT, RB (`:38-62`). That rasterises as triangles (LT, LB, RT) and (LB, RT, RB).

This is **primitive expansion plus per-primitive attributes** (flat Z and colour taken from the last vertex). The same thing done in a VS or on the CPU gives the same result.

**Where it is used:**

| Step | Location |
|---|---|
| Decompiled game code opens a sprite batch | `src/b-witch/SpriteWidget.cpp:275`, `:766`; `src/b-witch/Pause.cpp:2881` (`edDListBegin(..., DISPLAY_LIST_DATA_TYPE_SPRITE /*6*/, ...)`); generic dispatch in `src/b-witch/edDlist.cpp:2381-2382` |
| Flows into the port | `edDlist.cpp:1436` → `DISPLAY_LIST_2D_BEGIN` (`port/Windows/Renderer/include/displaylist.h:24`) → `Renderer::DisplayList::Begin2D` (`NativeDisplayList.cpp:507`), which sets `GSState.PRIM.PRIM = mode` |
| Vertices | `SetVertex` (`NativeDisplayList.cpp:571-596`) converts to NDC on the CPU and calls the shared `KickVertex` template. For `GS_SPRITE` that template emits **2 indices** per sprite (`renderer.h:544-549`) |
| Pipeline choice | `GetPipelineKey` picks `topologyLineList` when `PRIM == 6` (`NativeDisplayList.cpp:52`) |
| Pipeline creation | `CreatePipelines` makes three pipelines. Only the textured line-list one has a GS: `"shaders/displaylist.geom.spv"` (`NativeDisplayList.cpp:404-420`). The GS stage is attached at `:232-242` and its bindings at `:356-367`, with topology `VK_PRIMITIVE_TOPOLOGY_LINE_LIST` at `:259` |
| Draw | `FinalizeDraw` → `vkCmdDrawIndexed` (`NativeDisplayList.cpp:462-495`) |
| Build | `Shaders/CMakeLists.txt:19-23` globs `*.geom.glsl` and compiles it with glslangValidator (`:37-46`) |

### 1.2 `ps2.hlsl` `gs_main`: PCSX2 TFX geometry shaders (not reached at runtime)

`port/Windows/Renderer/Shaders/src/ps2/ps2.hlsl:905-1064`. The variants are selected by `GS_PRIM`, `GS_POINT`, `GS_LINE` and `GS_IIP` (defaults at `:19-23`). They are copied from an older PCSX2 `tfx` shader:

| Macro combination | Lines | What it does |
|---|---|---|
| `GS_PRIM==0 && GS_POINT==0` | `:908-914` | Pass-through point |
| `GS_PRIM==0 && GS_POINT==1` | `:916-951` | Point → `PointSize` quad (6 verts) |
| `GS_PRIM==1 && GS_LINE==0` | `:953-964` | Pass-through line; flat colour when `!IIP` |
| `GS_PRIM==1 && GS_LINE==1` | `:966-1012` | Line → thick quad (normal × `PointSize`) |
| `GS_PRIM==2` | `:1014-1027` | Pass-through triangle; flat colour when `!IIP` |
| `GS_PRIM==3` | `:1029-1062` | **Sprite → quad**, the same logic as `displaylist.geom.glsl` but also with `t`/`ti` (fog, Q, integer UVs) |

**Where it is wired in, and why it doesn't run:**

- `Shader::PS2::CompileShaders` compiles a `gs_6_0`/`gs_main` module for every key (`Objects/VulkanShader.cpp:146-158`).
- `PS2_Internal::CreateGraphicsPipelinePCSX2` always loads and attaches it: `Pipeline.cpp:28` (load), `:53` (bindings), `:87` (`SetGeometryShader`).
- That function is only called from `PS2::GetPipeline` (`Pipeline.cpp:267-279`). **No code calls `PS2::GetPipeline`.** `rg "GetPipeline\("` finds only its declaration (`Pipeline.h:132`), its definition, and unrelated `GetPipeline()` members in the Native renderer and DebugMenu. `VulkanPS2.cpp:75` and `:496-499` only clear or destroy the (empty) map.
- The standalone `ShaderCompiler` tool (`Shaders/compiler/src/ShaderCompiler.cpp:257-260`) also builds the GS permutations into `Shaders_PS2.pack`, but the runtime archive load is commented out (`VulkanShader.cpp:12-15`).
- The main shader build does not compile `ps2.hlsl` at all. The HLSL glob only matches `*.frag|vert|geom.hlsl` (`Shaders/CMakeLists.txt:27-31`), and no such files exist.

The live game renders through the Native renderer. Its pipelines (`NativeRendererSetup.cpp`, `NativeShadow.cpp`, `PostProcessing.cpp`, `NativeDebugShapes.cpp`, the DebugMenu mesh viewer) pass an empty `geomShaderFilename`, and `ReflectedModule` returns a null module for an empty name (`VulkanShader.cpp:71-78`). **So the only GS that has to be replaced for macOS is the display-list one.** The `ps2.hlsl` GS only needs to stay out of the macOS build, or be dropped from `CreateGraphicsPipelinePCSX2` if that path ever comes back (see §4).

### 1.3 Other ways the port depends on the geometry-shader feature

- `RequiredDeviceFeatures::geometryShader = true` (`VulkanRenderer.cpp:131`) makes device selection fail with "physical device is missing required Vulkan features: geometryShader" (`:184`, `:196-201`). The feature is also enabled unconditionally at `:727`. **This check has to be relaxed on macOS whatever replacement is chosen.**
- No shader uses `gl_PrimitiveID`/`SV_PrimitiveID` or `gl_Layer`. PCSX2 needs `geometryShader` for exactly that reason ("gl_PrimitiveID is part of the Geometry SPIR-V Execution Model", `GSDeviceVK.cpp:2794-2795`). Kya's fragment shaders have no such hidden dependency.

## 2. Platform facts

- **Metal has no geometry-shader stage.** Its programmable stages are vertex, fragment, tile, object/mesh and compute kernels ([`MTLMeshRenderPipelineDescriptor`](https://developer.apple.com/documentation/metal/mtlmeshrenderpipelinedescriptor), [WWDC22 "Transform your geometry with Metal mesh shaders"](https://developer.apple.com/videos/play/wwdc2022/10162/)).
- **MoltenVK never reports `geometryShader`.** `MVKPhysicalDevice::initFeatures()` starts with `mvkClear(&_features)` and never sets `geometryShader` (MoltenVK `main` @ `2c8ec93`, [`MVKDevice.mm`](https://github.com/KhronosGroup/MoltenVK/blob/main/MoltenVK/MoltenVK/GPUObjects/MVKDevice.mm), `initFeatures`). It sets `tessellationShader` and emulates it with compute, but has **no GS emulation**. MoltenVK also does not list `VK_EXT_mesh_shader` ([`MVKExtensions.def`](https://github.com/KhronosGroup/MoltenVK/blob/main/MoltenVK/MoltenVK/Layers/MVKExtensions.def)), so Vulkan mesh shaders are not an option through MoltenVK either.
- **KosmicKrisp** (LunarG's Mesa Vulkan-on-Metal driver, Vulkan 1.3 conformant) also lacks geometry shaders today. Blender's notes say "Geometry shaders are not supported" ([Blender dev docs: KosmicKrisp](https://developer.blender.org/docs/features/gpu/vulkan/kosmic_krisp/)). LunarG lists tessellation/geometry as future work ([XDC 2025 overview](https://www.lunarg.com/lunarg-at-xdc-2025-kosmickrisp-overview/)). Don't count on it.
- **PCSX2 dropped geometry shaders for expansion.** Its Vulkan `tfx.glsl` now expands points, lines and sprites in the vertex shader (`VS_EXPAND_POINT/LINE/SPRITE`, [`tfx.glsl`](https://github.com/PCSX2/pcsx2/blob/master/bin/resources/shaders/vulkan/tfx.glsl) lines ~10-16, 296-360). It pulls vertices from a storage buffer (`vertex_buffer[BaseVertex + index]`, ~`:122-150`) and uses a static expansion index buffer (`GSDevice::GenerateExpansionIndexBuffer`: `base+0,1,2, 1,2,3` per quad, [`GSDevice.cpp`](https://github.com/PCSX2/pcsx2/blob/master/pcsx2/GS/Renderers/Common/GSDevice.cpp) ~`:329`). This is on by default (`m_features.vs_expand = !GSConfig.DisableVertexShaderExpand`, `GSDeviceVK.cpp:2799`). The Metal backend does the same thing with `vs_main_expand` and `m_expand_index_buffer` ([`GSDeviceMTL.mm`](https://github.com/PCSX2/pcsx2/blob/master/pcsx2/GS/Renderers/Metal/GSDeviceMTL.mm)). The port's `Selectors.h:126-154` already has PCSX2's `VSExpand` enum, but `ps2.hlsl` is older than that code and doesn't implement it.

## 3. Options

Scope: replace `displaylist.geom.glsl`, a handful of HUD/menu sprites per frame.

| Option | How | Effort | Perf | macOS-only possible? | Notes |
|---|---|---|---|---|---|
| **A. CPU expansion** (recommended) | In the display list, when `PRIM == GS_SPRITE`, hold the LT vertex. When RB arrives, write 4 `DisplayListVertex` (LT, LB, RT, RB with RB's Z and colour and swapped ST) plus 6 indices `0,1,2, 1,2,3`, then draw with the existing **triangle-list** pipeline | **S**: roughly 40-60 lines in `NativeDisplayList.cpp` (a local sprite branch in `SetVertex` or a `DisplayListVertex` helper; don't touch the shared `KickVertex` in `renderer.h:409-556`, which `VulkanPS2.cpp:225` also uses). Map `GetPipelineKey` (`:52`) to triangle-list. Relax `VulkanRenderer.cpp:131/727` | Negligible: 2× vertex data for a few dozen sprites; no extra draws, passes or pipelines | Yes: `#ifdef __APPLE__` or a runtime `!features.geometryShader` check chooses between the GS pipeline and CPU expansion | The same triangles and attributes as the GS strip, so output should match pixel for pixel. **It also removes a latent gap:** the line-list pipeline only exists for *textured* sprites (`:414-420`), while `Pause.cpp:2876-2883` draws an untextured sprite after `edDListUseMaterial(nullptr)` → `BindTexture(nullptr)` (`edDlist.cpp:1160-1161`). That looks up the key {lineList, noTex}, which doesn't exist (`GetPipelineState` asserts, `:57-62`). Found by reading the code; not checked at runtime |
| B. VS expansion / vertex pulling (PCSX2 style) | Bind the vertex buffer as an SSBO. Draw `6 × nSprites` indices from a static expansion index buffer. The VS reads `vertex[(gl_VertexIndex>>2)*2 + {0,1}]` and chooses the corner from the low bits, as in PCSX2 `VS_EXPAND_SPRITE` | **M**: new `displaylist_expand.vert.glsl`, SSBO descriptor and binding, static index buffer, index-count rewriting, a 16-byte-aligned vertex layout (`DisplayListVertex` is `alignas(32)`, `renderer.h:157-163`) | GPU-side and very cheap. Better than A only at large sprite counts, which Kya doesn't have | Yes, same gating | Worth it only if the `ps2.hlsl` TFX path comes back and needs point/line expansion too. In that case, port PCSX2's current `tfx.glsl` instead of the GS |
| C. Compute pre-pass | A compute shader writes expanded vertices into a buffer, then a normal triangle draw | **M-L**: extra pipeline, barrier, buffer management | An extra dispatch and barrier per display-list flush; worse than A/B at this scale | Yes | This is how MoltenVK emulates tessellation. Pointless for 2-vertex → 4-vertex quads |
| D. Metal mesh shaders | Object/mesh stage emits the quads | **L**: needs a native Metal backend or raw MSL, because MoltenVK doesn't expose `VK_EXT_mesh_shader`; Metal 3 GPUs only ([WWDC22 10162](https://developer.apple.com/videos/play/wwdc2022/10162/)) | Fine, but no gain over A/B | macOS-only by nature | Doesn't fit a Vulkan-via-MoltenVK plan at all |
| E. MoltenVK / driver GS emulation | — | — | — | — | **Doesn't exist.** MoltenVK leaves `geometryShader` false and has no emulation; KosmicKrisp has none today (§2) |

## 4. Recommendation

1. **Option A, CPU sprite expansion in `NativeDisplayList.cpp`.** It is the smallest change and keeps the vertex format, descriptor layout and triangle-list pipeline the game already uses for non-sprite 2D. Its output should be identical to today's GS output.
2. **Gate it with a runtime check, not only `#ifdef`:** `useGsSprites = physicalDeviceFeatures.geometryShader`. Windows GPUs keep the current GS pipeline without change. macOS (MoltenVK, and any Vulkan driver without GS) takes the CPU path. Once A has been checked on Windows, deleting the GS path completely is a reasonable follow-up, since it is less code with the same result. That decision belongs to the maintainer, not to this ticket.
3. **Make `geometryShader` optional:** remove it from `RequiredDeviceFeatures` (`VulkanRenderer.cpp:131`, `:184`) and enable it at `:727` only when supported.
4. **Keep `ps2.hlsl`/`CreateGraphicsPipelinePCSX2` out of the macOS build, or leave them untouched.** They are unreachable. If that path is ever revived, replace its GS with PCSX2's current VS-expand shaders (option B) rather than porting the GS.

Follow-on work for the implementation ticket (#14 depends on this one): the unconditional `SetGeometryShader` at `Pipeline.cpp:87` would make `vkCreateGraphicsPipelines` invalid on MoltenVK if that path were ever reached. It is dead today, so guarding or deleting it is housekeeping, not a blocker.
