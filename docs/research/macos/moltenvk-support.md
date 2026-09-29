# What MoltenVK (and KosmicKrisp) support for Kya's Vulkan requirements

Research for mgiuditta/Kya#10 (map #8, "Kya on macOS"). Checked 2026-09-29 against:

- **MoltenVK v1.4.2** (latest release, 2026-07-24): <https://github.com/KhronosGroup/MoltenVK/releases/tag/v1.4.2>
- **KosmicKrisp** (LunarG's Mesa Vulkan-on-Metal driver, Apple Silicon only), Mesa `main` as of 2026-09-28 (commit `27b5db37`, "kk: Update conformance version to 1.4.6.2").

The sources are the drivers' own feature and extension tables, which are exactly what `vkGetPhysicalDeviceFeatures2` and `vkEnumerateDeviceExtensionProperties` report:

- MoltenVK features: [`MVKDevice.mm` @ v1.4.2](https://github.com/KhronosGroup/MoltenVK/blob/v1.4.2/MoltenVK/MoltenVK/GPUObjects/MVKDevice.mm) (`initFeatures()` ~L2760, EDS3 struct ~L697-730, geometry limits ~L3110)
- MoltenVK extensions: [`MVKExtensions.def` @ v1.4.2](https://github.com/KhronosGroup/MoltenVK/blob/v1.4.2/MoltenVK/MoltenVK/Layers/MVKExtensions.def)
- MoltenVK loader/portability notes: [Runtime User Guide](https://github.com/KhronosGroup/MoltenVK/blob/main/Docs/MoltenVK_Runtime_UserGuide.md)
- KosmicKrisp: [`src/kosmickrisp/vulkan/kk_physical_device.c`](https://gitlab.freedesktop.org/mesa/mesa/-/blob/main/src/kosmickrisp/vulkan/kk_physical_device.c)

## What Kya requires today

`port/Windows/Renderer/Vulkan/src/VulkanRenderer.cpp`: the required device extensions (L47-53), `RequiredDeviceFeatures` (L130-139, checked at L184-191), `apiVersion = VK_API_VERSION_1_3` (L607), and the features enabled at device creation (L714-729). If any of them is missing, device selection rejects the GPU.

Where the renderer actually uses them:

- `geometryShader`: only `Shaders/src/displaylist.geom.glsl`, which expands a line (2 vertices) into a sprite quad (4 vertices). It's loaded by `Native/NativeDisplayList.cpp:419` and `Objects/Pipeline.cpp:28`.
- EDS3 `vkCmdSetColorBlendEnableEXT` / `vkCmdSetColorBlendEquationEXT`: `Native/Blending.cpp:204-229` (GS blend emulation).
- `VK_DYNAMIC_STATE_COLOR_WRITE_ENABLE_EXT` + `COLOR_WRITE_MASK_EXT`: `Native/NativeRendererSetup.cpp:207-208, 347-348, 409-411`, `Native/NativePreviewRenderer.cpp`, `DebugMenu/src/DebugMeshViewerVulkan.cpp:312-322`.
- `dualSrcBlend`: `index = 1` fragment outputs in `native.frag`, `displaylist*.frag`, and the `SRC1_*` factors in `Blending.cpp`/`Pipeline.cpp`.
- `fillModeNonSolid`: `VK_POLYGON_MODE_LINE`, only when `bWireframe` is set (debug), in `NativeRendererSetup.cpp:178, 310`.
- `VK_EXT_extended_dynamic_state` (1): depth write/compare dynamic states. These are core in Vulkan 1.3.
- `VK_EXT_extended_dynamic_state2`: required, but none of its states are set.

## Support matrix

| Requirement | MoltenVK 1.4.2 | KosmicKrisp (Mesa main) | Workaround | Source |
|---|---|---|---|---|
| Vulkan 1.3 `apiVersion` (+ `synchronization2`) | **Yes.** Advertises Vulkan 1.4; `synchronization2 = true` | **Yes.** `VK_MAKE_VERSION(1,4,…)`, conformance 1.4.6.2; `synchronization2 = true` | None needed | [UG](https://github.com/KhronosGroup/MoltenVK/blob/main/Docs/MoltenVK_Runtime_UserGuide.md), [MVKDevice.mm](https://github.com/KhronosGroup/MoltenVK/blob/v1.4.2/MoltenVK/MoltenVK/GPUObjects/MVKDevice.mm), [kk_physical_device.c](https://gitlab.freedesktop.org/mesa/mesa/-/blob/main/src/kosmickrisp/vulkan/kk_physical_device.c) |
| `geometryShader` | **No.** Feature not set; every `maxGeometry*` limit is 0. Tracked in open issue #1524 | **No.** Feature not set (it has placeholder `maxGeometry*` limits, but that doesn't enable the feature). Tessellation/geometry work is on the roadmap | Remove the GS. Expand each sprite line to a quad on the CPU (emit 4 verts/6 indices), or in the VS by vertex pulling (`gl_VertexIndex/4` into a storage buffer), which is how PCSX2 handles sprites on Metal | [MVKDevice.mm](https://github.com/KhronosGroup/MoltenVK/blob/v1.4.2/MoltenVK/MoltenVK/GPUObjects/MVKDevice.mm), [MoltenVK#1524](https://github.com/KhronosGroup/MoltenVK/issues/1524), [kk_physical_device.c](https://gitlab.freedesktop.org/mesa/mesa/-/blob/main/src/kosmickrisp/vulkan/kk_physical_device.c) |
| `VK_EXT_extended_dynamic_state` (1) | **Yes** | **Yes** | Core in 1.3 anyway | [MVKExtensions.def](https://github.com/KhronosGroup/MoltenVK/blob/v1.4.2/MoltenVK/MoltenVK/Layers/MVKExtensions.def), [kk_physical_device.c](https://gitlab.freedesktop.org/mesa/mesa/-/blob/main/src/kosmickrisp/vulkan/kk_physical_device.c) |
| `VK_EXT_extended_dynamic_state2` | **Yes** (extension exposed; `LogicOp` sub-feature false) | **Yes** | Kya uses none of it, so it could just be dropped from the required list | same |
| `VK_EXT_extended_dynamic_state3` (extension) | **Yes, exposed**, but only DepthClamp, PolygonMode, DepthClip, SampleLocations, LineRasterizationMode (+ ProvokingVertex with private API) | **Yes, exposed**, with a similar subset (DepthClamp, DepthClip, NegOneToOne, LineRasterizationMode, ProvokingVertex, SampleLocations, TessDomainOrigin) | See the three rows below | same |
| `extendedDynamicState3ColorBlendEnable` (`vkCmdSetColorBlendEnableEXT`) | **No** (`= false`) | **No** (not set) | Make blend enable static pipeline state and add it to the pipeline key. The PCSX2-derived code (`Pipeline.cpp`) already builds pipelines per blend factor, so this means more pipeline variants plus a small cache | same |
| `extendedDynamicState3ColorBlendEquation` (`vkCmdSetColorBlendEquationEXT`) | **No** (`= false`) | **No** | Same: bake `VkPipelineColorBlendAttachmentState` from `Blending.cpp`'s table into the pipeline key | same |
| `extendedDynamicState3ColorWriteMask` | **No** (`= false`) | **No** | Make the write mask static pipeline state (key bit) | same |
| `VK_EXT_color_write_enable` (`colorWriteEnable`) | **No.** Not in `MVKExtensions.def` | **No.** Not in the extension table | Replace it with a static `colorWriteMask = 0` pipeline variant (write disabled means mask 0) | same |
| `dualSrcBlend` | **Yes** (`= true`, `maxFragmentDualSrcAttachments = 1`) | **Yes** (`= true`, `maxFragmentDualSrcAttachments = 1`) | None needed | same |
| `fillModeNonSolid` | **Yes** (`= true`). `LINE` maps to Metal's triangle fill mode "lines"; `POINT` polygons are not available (portability subset) | **No.** Not set in the feature table | Only the debug wireframe uses it: make it an optional feature and hide the wireframe toggle when absent | same |
| `samplerAnisotropy` | **Yes** | **Yes** | None needed | same |
| Portability enumeration | **Required.** With the Vulkan SDK loader, MoltenVK devices are only enumerated if the instance enables `VK_KHR_portability_enumeration` and sets `VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR`; the device exposes `VK_KHR_portability_subset`, which must then be enabled | Probably not required, because KosmicKrisp reports a conformance version; setting the flag is harmless | Kya doesn't do this today: add the instance extension and flag on Apple, and enable `VK_KHR_portability_subset` when it's advertised. Small code change | [UG, "portability" paragraph](https://github.com/KhronosGroup/MoltenVK/blob/main/Docs/MoltenVK_Runtime_UserGuide.md), [MVKExtensions.def](https://github.com/KhronosGroup/MoltenVK/blob/v1.4.2/MoltenVK/MoltenVK/Layers/MVKExtensions.def) |
| `VK_KHR_external_memory` (optional) | Yes | n/a (optional in Kya) | Already optional | [MVKExtensions.def](https://github.com/KhronosGroup/MoltenVK/blob/v1.4.2/MoltenVK/MoltenVK/Layers/MVKExtensions.def) |

## Conclusion

Neither Vulkan-on-Metal driver can run Kya's renderer unchanged today. As written, device selection rejects the GPU on:

1. **`geometryShader`**. This is the one requirement that can't be met by toggling features or adding pipeline variants. Neither MoltenVK (open issue #1524) nor KosmicKrisp implements geometry shaders, and there's no release date. It is still small in Kya: one GS (`displaylist.geom.glsl`) that turns sprite lines into quads, which a CPU or vertex-shader expansion can replace.
2. **EDS3 ColorBlendEnable / ColorBlendEquation / ColorWriteMask and `VK_EXT_color_write_enable`**. Both drivers report all four as unsupported, and Metal bakes blend state into the pipeline state object, so they're unlikely to ever be supported. The workaround is to move them into the pipeline key (more pipeline variants plus a cache). This is the largest refactor, because `Blending.cpp` currently switches blend state per draw.
3. **Portability enumeration** isn't a blocker, only a missing instance flag and extension.

`dualSrcBlend`, Vulkan 1.3/`synchronization2`, EDS1/EDS2 and anisotropy work on both drivers. `fillModeNonSolid` works on MoltenVK only, and Kya uses it just for the debug wireframe.

**Recommendation:** target MoltenVK first, since it is the one that ships in the Vulkan SDK and also supports Intel/AMD Macs. Then make three code changes: GS removal, static blend/write-mask pipeline variants, and portability flags. Treat none of the gaps as a hard blocker that needs driver work.
