# Running the Windows build under CrossOver / Wine / GPTK on Apple Silicon

Research ticket: mgiuditta/Kya#12 (map: mgiuditta/Kya#8, "Wayfinder: Kya on macOS").
Researched 2026-09-29. This was desk research only: nothing was installed or run.

## Verdict

**The current Windows build will not run under any of the three layers. It stops during Vulkan device selection.**
Every route from Vulkan to Metal on macOS goes through MoltenVK or, optionally, Mesa's KosmicKrisp.
Neither one exposes `geometryShader`, the three EDS3 color-blend dynamic states, or `VK_EXT_color_write_enable`.
Kya requires all of them (`port/Windows/Renderer/Vulkan/src/VulkanRenderer.cpp:47-53,129-139`), and `pickPhysicalDevice` throws `failed to find a suitable GPU` (`VulkanRenderer.cpp:674-687`).
GPTK/D3DMetal does not help, because it only translates Direct3D.

Audio (XAudio2 goes to FAudio) and the gamepad (Windows.Gaming.Input) do have working Wine implementations.
They are not the blockers.

A compatibility layer is only a viable dev loop if the renderer first gets fallbacks for those four features.
That is the same renderer work a native macOS/MoltenVK port needs, so the layer saves no renderer effort.
What it can do is let you check audio, input and game logic on a Mac before any native platform code exists.
Rosetta is also being retired (see below), which makes this a short-term option at best.

## 1. How Vulkan reaches Metal on each layer

### Wine (upstream, `winemac.drv`)

- `winevulkan` forwards the app's Vulkan calls to a host Vulkan library that `win32u` loads with `dlopen(SONAME_LIBVULKAN)`.
  See [`dlls/win32u/vulkan.c`](https://gitlab.winehq.org/wine/wine/-/blob/master/dlls/win32u/vulkan.c) (Wine 11.18, current master).
- On macOS, `configure.ac` first looks for `libvulkan`, which is the Khronos loader and can load any ICD.
  If it isn't found, `SONAME_LIBVULKAN` is set to **`libMoltenVK`** directly.
  See [`configure.ac`, "Check for Vulkan"](https://gitlab.winehq.org/wine/wine/-/blob/master/configure.ac).
- `winemac.drv` maps `VK_KHR_win32_surface` to `VK_EXT_metal_surface`, or to `VK_MVK_macos_surface` as a fallback, on the host.
  See [`dlls/winemac.drv/vulkan.c`](https://gitlab.winehq.org/wine/wine/-/blob/master/dlls/winemac.drv/vulkan.c).
- Wine does not translate anything itself. The Windows app sees the host driver's features and extensions and nothing more.
  So Kya gets exactly what MoltenVK or KosmicKrisp report.

### CrossOver

- CrossOver 26.0 (2026-02-10) is based on Wine 11.0, and the current release is 26.3.0 (2026-07-21).
  The macOS changelog lists MoltenVK updates (CrossOver 24: 1.2.5, CrossOver 25: 1.2.10), D3DMetal 3.0, and DXMT v0.72.
  None of these entries mention geometry shaders.
  See the [CrossOver changelog](https://www.codeweavers.com/crossover/changelog).
- Vulkan apps therefore take the same path as in Wine: winevulkan to MoltenVK to Metal.
  DXMT and D3DMetal only handle D3D10/11/12.
- CrossOver is an x86_64 app that runs through Rosetta 2 on Apple Silicon.

### Apple Game Porting Toolkit (GPTK / D3DMetal)

- D3DMetal converts DXIL/DXBC Direct3D (11/12) to Metal. Apple's GPTK pages do not mention Vulkan.
  GPTK 4 (WWDC 2026) adds D3DMetal 4 for DX12 plus agent tooling.
  See [developer.apple.com/games/game-porting-toolkit](https://developer.apple.com/games/game-porting-toolkit/) and [WWDC26 session 357](https://developer.apple.com/videos/play/wwdc2026/357/).
- Apple's Homebrew formula for the GPTK Wine is built from CrossOver 22.1.1 sources.
  It has no `molten-vk` dependency, and it passes `--without-vulkan --disable-winevulkan` for the 32-bit half.
  See [`apple/homebrew-apple/Formula/game-porting-toolkit.rb`](https://github.com/apple/homebrew-apple/blob/main/Formula/game-porting-toolkit.rb).
  A Vulkan app in the GPTK evaluation environment would at best land on MoltenVK, if one happens to be installed, and at worst find no Vulkan at all.
  The environment is also for evaluation, not for shipping.

### Feature check against Kya's requirements

These are Kya's hard requirements (`VulkanRenderer.cpp:47-53` extensions, `129-139` `RequiredDeviceFeatures`, `607` `VK_API_VERSION_1_3`).
Driver support comes from each driver's current source.

| Kya requires | MoltenVK `main` (2026-09-27, latest release v1.4.2) | KosmicKrisp (Mesa `main`, 2026-09-28) |
|---|---|---|
| Vulkan 1.3 | yes (1.4) | yes (1.4) |
| `geometryShader` (`displaylist.geom.glsl`) | **no**: `mvkClear(&_features)` and never set | **no**: not in `kk_get_device_features` |
| `VK_EXT_extended_dynamic_state3` | extension yes | extension yes |
| `extendedDynamicState3ColorBlendEnable` | **false** (`MVKDevice.mm` ~l.707) | **not set** |
| `extendedDynamicState3ColorBlendEquation` | **false** | **not set** |
| `extendedDynamicState3ColorWriteMask` | **false** | **not set** |
| `VK_EXT_color_write_enable` / `colorWriteEnable` | **no**: not in `MVKExtensions.def` | **no** |
| `dualSrcBlend` | yes | yes |
| `fillModeNonSolid` | yes | **no** |
| `synchronization2`, `samplerAnisotropy` | yes | yes |

Sources: [MoltenVK `MVKDevice.mm`](https://github.com/KhronosGroup/MoltenVK/blob/main/MoltenVK/MoltenVK/GPUObjects/MVKDevice.mm), [`MVKExtensions.def`](https://github.com/KhronosGroup/MoltenVK/blob/main/MoltenVK/MoltenVK/Layers/MVKExtensions.def), [MoltenVK Runtime User Guide](https://github.com/KhronosGroup/MoltenVK/blob/main/Docs/MoltenVK_Runtime_UserGuide.md), [KosmicKrisp `kk_physical_device.c`](https://gitlab.freedesktop.org/mesa/mesa/-/blob/main/src/kosmickrisp/vulkan/kk_physical_device.c).
LunarG's Feb-2026 roadmap still lists geometry/tessellation as "in investigation" ([Phoronix summary](https://www.phoronix.com/news/KosmicKrisp-2026)).

**Result:** the limits are the same ones a native MoltenVK port would hit.
The compatibility layers add nothing on top of MoltenVK and take nothing away.

What it would take to get past device selection (this is a scope note, not a design):
- The GS in `displaylist.geom.glsl` only expands a line primitive into a sprite quad. Vertex-shader or instanced expansion can replace it.
- The EDS3 blend/write-mask calls live in `port/Windows/Renderer/Vulkan/src/Native/{Blending,NativeRendererRecording,NativeRendererSetup,NativeRendererSubmission,NativePreviewRenderer}.cpp` and `port/DebugMenu/src/DebugMeshViewerVulkan.cpp`.
  They would need a static-pipeline-variant fallback.

## 2. Audio: XAudio2 through FAudio

- Kya calls `CoInitializeEx(COINIT_MULTITHREADED)` and then `XAudio2Create` (`port/Audio/edSoundStreamService.cpp:69-79`), and links `XAudio2 ole32` (`port/Audio/CMakeLists.txt:23`).
  With the Windows 10+ SDK this resolves to `xaudio2_9.dll`.
- Wine ships `xaudio2_9.dll` (and 2_0 through 2_8), built on a bundled FAudio (`libs/faudio`, currently FAudio 26.09).
  It also covers X3DAudio and XAPO/XAPOFX.
  See [`dlls/xaudio2_9/Makefile.in`](https://gitlab.winehq.org/wine/wine/-/blob/master/dlls/xaudio2_9/Makefile.in) and [`libs/faudio/include/FAudio.h`](https://gitlab.winehq.org/wine/wine/-/blob/master/libs/faudio/include/FAudio.h).
  Audio output on macOS goes through `winecoreaudio.drv`.
- Apple's GPTK formula also depends on `faudio`.
- **Expectation:** this should work. FAudio is the same XAudio2 reimplementation that Proton ships.
  The remaining risk is small behavioural differences in streaming voices and callbacks. You can only find those by running the game.

## 3. Gamepad: WinRT Windows.Gaming.Input

- Kya polls `Windows::Gaming::Input::Gamepad::Gamepads()` and `GetCurrentReading()` every frame through C++/WinRT (`port/Windows/Input/gamepad.cpp:4-19`).
  It does not use events or vibration.
- Wine implements `windows.gaming.input.dll`, with Gamepad, RawGameController, RacingWheel and ForceFeedback, on top of dinput8/HID and combase activation.
  See [`dlls/windows.gaming.input/`](https://gitlab.winehq.org/wine/wine/-/tree/master/dlls/windows.gaming.input).
- Wine only reports a device as a **`Gamepad`** when its HID path carries the XInput-compatible `&XI_` marker ([`provider.c`](https://gitlab.winehq.org/wine/wine/-/blob/master/dlls/windows.gaming.input/provider.c) ~l.180).
  Everything else shows up as a Joystick or RacingWheel, and `Gamepad::Gamepads()` will not include it.
  On macOS, devices come from `winebus.sys` through its IOHID or SDL backends ([`dlls/winebus.sys`](https://gitlab.winehq.org/wine/wine/-/tree/master/dlls/winebus.sys)).
  Xbox and SDL-recognised controllers get the XInput mapping.
- **Expectation:** this should work with common Xbox/PlayStation pads and may fail with unusual HID devices.
  The keyboard path through GLFW is unaffected.

## 4. Is this a realistic dev loop?

### Option A: build on Windows, run on the Mac

This works mechanically: copy `bin/WIN/*` and the game data into a CrossOver bottle.
It is blocked by section 1, though. Until the renderer has the fallbacks, the game throws at startup.
The bigger problem is that each iteration means building on a Windows machine and then copying the output across.

### Option B: cross-compile on the Mac

- The toolchain exists. [xwin](https://github.com/Jake-Shadle/xwin) downloads the MSVC CRT and Windows SDK, including the cppwinrt headers, onto macOS (with prebuilt aarch64 binaries).
  It works with `clang-cl`, `lld-link` and `/winsysroot`.
  [msvc-wine](https://github.com/mstorsjo/msvc-wine) documents a pure clang-cl + LLD mode that never runs MSVC under Wine, but it targets Linux.
- Kya's CMake needs changes before this can work:
  - `CMakeLists.txt:34-35` hard-codes `CMAKE_C/CXX_COMPILER "clang"`, which should come from a toolchain file.
  - `port/Windows/Renderer/Shaders/CMakeLists.txt:6-11` and `port/Windows/Renderer/CMakeLists.txt:197` hard-code `$ENV{VULKAN_SDK}/Bin/glslangValidator.exe` and `dxc.exe`. The macOS Vulkan SDK ships native tools under different paths.
  - `src/port/ispc/ispc.cmake` compiles with `--arch=x86-64 --target=sse2` but no `--target-os=windows`. On a Mac host it would emit Mach-O objects.
  - `find_package(Vulkan)` has to find the Windows import library, not the host SDK.
  - Tracy, ONNX Runtime and ncnn `FetchContent` deps (`port/Windows/Renderer/CMakeLists.txt:115-151`) would also have to cross-build or be disabled.
- The x64 `.exe` then runs through Wine and Rosetta 2. It is not debuggable with Visual Studio, and Wine's own debugging tools are weak compared with a native build.

### Rosetta sunset

Apple says **macOS 27 is the last release with general Rosetta support**.
Only "older, unmaintained gaming titles that rely on Intel-based frameworks" keep Rosetta after that ([Apple developer news, 2026-09-01](https://developer.apple.com/news/?id=w5ngl9k2)).
x86_64 Wine and CrossOver on Apple Silicon depend on Rosetta.
Whether they fall under that exception from macOS 28 on has not been announced, so any Wine-based loop may stop working about a year from now.

### Assessment

Option B is possible but costs more setup than a native arm64 macOS target would.
A native target needs the same CMake cleanup plus the MoltenVK fallbacks, and it gets a real debugger.
Recommendation: do not invest in a Wine/CrossOver dev loop.
If a quick check is wanted after the renderer fallbacks land, use Option A in CrossOver as a one-off smoke test of audio and input. Do not treat it as the day-to-day loop.

## Open questions (need hands-on testing)

- Does CrossOver ship a patched MoltenVK fork with anything beyond upstream? The changelog doesn't say. Checking would mean inspecting the CrossOver source tarball or running `vulkaninfo` in a bottle.
- Does FAudio behave the same as Windows for Kya's streaming/sample voices (`edSoundStreamService`, `edSoundSampleService`)?
- Does C++/WinRT activation work under Wine for this binary? Wine implements it, but it has not been tested with Kya's `winrt::` usage and apartment setup.
