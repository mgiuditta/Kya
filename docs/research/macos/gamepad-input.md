# Which input API replaces WinRT gamepad input on macOS

Research for mgiuditta/Kya#18 (map #8, "Kya on macOS"). Checked 2026-09-29. Planning only: no port code was changed.

**Answer: use GLFW's gamepad API (`glfwGetGamepadState`) in a new `port/` source file on macOS, and keep `port/Windows/Input/gamepad.cpp` (WinRT) unchanged on Windows.** GLFW is already linked, the pinned version (3.5-dev) has the API and bundled macOS mappings for DualShock 4, DualSense and Xbox pads, and its 15 buttons + 6 axes cover everything the current WinRT backend reads. What GLFW lacks (rumble, button pressure) is either not used by the port today (rumble) or not exposed by WinRT either (pressure). If rumble becomes a requirement later, add an Apple GameController haptics file on top. SDL would be a new dependency for no gain today.

## 1. Current input path and its seam

Layers, from the game down to the OS:

| Layer | What it does | Where |
|---|---|---|
| Game | `CInputManager` reads each route's analog and click value from edDev | `src/b-witch/InputManager.cpp:553-591` |
| edDev (EdenLib) | `edDevReadUpdate` sends `EVENT_POLL_INPUT` (`0x90000006`) to each port's event function every frame; it sends `EVENT_SEND_DATA` (`0x90000005`) only if `field_0x2c` is set | `src/EdenLib/edDev/Sources/edDevReadUpdate.cpp:28-34`, event IDs `src/EdenLib/edDev/Sources/edDev.h:47-54` |
| Port selection | On PS2 the port uses `_edDevDualShock2`. On PC (`#else`) it uses `Input::_edDevKeyboard` for the pad port and `Input::_edDevMouse` for the mouse | `src/EdenLib/edDev/Sources/edDevInit.cpp:219-238`, `:149` |
| PC pad handler (in `src/port/`) | `EVENT_POLL_INPUT` calls `PollKeyboard` and then `PollGamepad`. Each one fills the 24 (`0x18`) `Pad_d` slots from `Input::gInputFunctions`. `PollGamepad` overwrites the keyboard values only when `controllerAnyPressed()` returns true | `src/port/input.cpp:12-33`, `:148-152` |
| **Seam** | `Input::InputFunctions`: a table of `std::function`s (`key*`, `mouse*`, `controllerPressed/Released/Analog/AnyPressed`) indexed by `ROUTE_*` IDs `0x00-0x17` | `port/include/input_functions.h:6-55`, storage `port/src/input_functions.cpp:5` |
| Gamepad backend (Windows) | `KyaGamepad::AddGamepadSupport()` installs the four `controller*` functions, implemented with WinRT `Windows::Gaming::Input::Gamepad::Gamepads().GetAt(0).GetCurrentReading()` | `port/Windows/Input/gamepad.cpp:1-241` (install at `:236-242`); interface `port/include/gamepad.h:5` |
| Keyboard/mouse backend | `DebugMenu::AddKeyboardMouseSupport()` installs the `key*`/`mouse*` functions through ImGui key queries. ImGui gets its keys from `imgui_impl_glfw`, so this is already platform-neutral | `port/DebugMenu/src/DebugMenuInput.cpp:13-49`; `port/Windows/Renderer/ext/imgui/CMakeLists.txt:14` |
| Wiring | `main` calls `DebugMenu::AddKeyboardMouseSupport()` and then `KyaGamepad::AddGamepadSupport()` | `port/Windows/Host/src/win_main.cpp:30-31` |
| Build | `gamepad.cpp` is listed unconditionally in the port sources | `port/CMakeLists.txt:42` |
| Replay/test override | The hero-replay tool swaps the whole `gInputFunctions` table, which shows the seam already works as a swappable backend | `port/DebugMenu/src/DebugHeroReplay.cpp:96-139` |

The seam is `KyaGamepad::AddGamepadSupport()` plus the `controller*` members of `Input::gInputFunctions`. A macOS backend only needs to provide another definition of `AddGamepadSupport()` and have CMake pick that file. Nothing in `src/` has to change (`src/port/input.cpp` only calls through the table).

### What the game actually consumes

- **Routes:** 24 per pad (`ROUTE_L2` … `ROUTE_L_ANALOG_DOWN`, `port/include/input_functions.h:6-34`): 16 buttons/d-pad, then each stick split into 4 half-axes (left/right/up/down), each positive in `[0,1]`.
- **Analog values on PS2:** the original handler stores `analogValue = pressure/255` for the 16 buttons (DualShock 2 pressure mode) and turns each stick byte into two half-axes around 0.5 (`src/EdenLib/edDev/Sources/ps2/_edDevDualShock2.cpp:138-160`, `:170-210`).
- **Analog values on PC today:** WinRT has no button pressure, so `GetGamepadAnalog` returns 0/1 for buttons, `LeftTrigger/RightTrigger` (0..1) for L2/R2, and signed stick values for the half-axes (`port/Windows/Input/gamepad.cpp:142-216`). Button pressure is already lost on Windows.
- **Rumble:** `EVENT_SET_DATA`/`EVENT_SEND_DATA` (the DualShock 2 actuator bytes) are still `IMPLEMENTATION_GUARD`ed in both the PS2 handler (`_edDevDualShock2.cpp:99-114`) and the PC handler (`src/port/input.cpp:132-147`). No decompiled caller issues `EVENT_SET_DATA`. (The `vibrationDyn`/`VibrationParam` hits in `src/b-witch/ActorBasicBox.cpp` and `ActorPunchingBall.cpp` are object shake physics, not pad rumble.) **No rumble reaches any backend today**, WinRT included.

So a macOS backend has to deliver exactly what the WinRT one does: 14 digital buttons + d-pad, 2 sticks, 2 analog triggers, a "first connected pad" lookup, and an "any input" check.

## 2. Candidates

### GLFW gamepad API (already a dependency)

- **Version:** the submodule `port/Windows/Renderer/ext/glfw` is pinned at `e7ea71be` (2025-01-13, "Update changelog and add credit"). That is 23 commits after the 3.4 tag, and `glfw3.h` reports `3.5` (dev) ([glfw3.h@e7ea71b L287-294](https://github.com/glfw/glfw/blob/e7ea71be039836da3a98cea55ae5569cb5eb885c/include/GLFW/glfw3.h)). It is built from source (`port/Windows/Renderer/CMakeLists.txt:7`) and linked into Renderer, imgui and tests (`:186`, `ext/imgui/CMakeLists.txt:23`, `port/Test/CMakeLists.txt:26`). Note: the submodule is not initialized in this checkout, so the GLFW facts here come from upstream at that exact commit.
- **API:** `glfwGetGamepadState(jid, &state)` (since 3.3) fills `GLFWgamepadstate { unsigned char buttons[15]; float axes[6]; }`. The buttons are A/B/X/Y (with `CROSS/CIRCLE/SQUARE/TRIANGLE` aliases), bumpers, back, start, guide, thumbs and d-pad. The axes are left/right X/Y and left/right trigger, all in −1..1 ([docs/input.md "Gamepad input"](https://github.com/glfw/glfw/blob/e7ea71be039836da3a98cea55ae5569cb5eb885c/docs/input.md), [glfw3.h `GLFWgamepadstate`](https://github.com/glfw/glfw/blob/e7ea71be039836da3a98cea55ae5569cb5eb885c/include/GLFW/glfw3.h)). This maps 1:1 onto `GamepadReading` as the port uses it.
- **Mapping to PS2 state:** the sticks and triggers are analog. **Mapping differences a port must handle:** triggers rest at −1 (WinRT: 0), so convert with `(v+1)/2`. Y axes follow the SDL_GameControllerDB/Xbox convention where up is negative, so the sign must be flipped against WinRT's `LeftThumbstickY` (verify on hardware). Buttons are digital only, the same as WinRT today.
- **Coverage on macOS:** the Cocoa backend enumerates IOKit HID devices with usage Joystick/GamePad/MultiAxisController ([cocoa_joystick.m@e7ea71b L313-318](https://github.com/glfw/glfw/blob/e7ea71be039836da3a98cea55ae5569cb5eb885c/src/cocoa_joystick.m)). The bundled `mappings.h` (a copy of SDL_GameControllerDB) has 169 `platform:Mac OS X` entries. These include "PS4 Controller" (`054c:05c4`, `09cc`), "PS5 Controller"/"Sony DualSense" (`054c:0ce6`, both USB `03` and Bluetooth `05` bus prefixes), Xbox One wired, Xbox Wireless, Xbox Series and Elite Series 2 ([mappings.h@e7ea71b](https://github.com/glfw/glfw/blob/e7ea71be039836da3a98cea55ae5569cb5eb885c/src/mappings.h)). Unknown pads can be added at runtime with `glfwUpdateGamepadMappings` and a `gamecontrollerdb.txt`.
- **Rumble:** not supported. The GLFW request "Support for haptic feedback (vibration, rumble)" has been open since 2013 ([glfw/glfw#57](https://github.com/glfw/glfw/issues/57)).
- **Threading:** `glfwGetGamepadState` "must only be called from the main thread" (glfw3.h doc comment). The port already calls `glfwInit`/`glfwPollEvents` on the thread that runs `main_internal` (`port/Windows/Renderer/Vulkan/src/VulkanRenderer.cpp:433`, `:930`), which is also where edDev polls. This needs a check if the macOS host moves the game loop off the main thread.
- **Cost:** plain C++, no new dependency, no Objective-C++.

### Apple GameController framework

- **API/coverage:** `GCExtendedGamepad` (macOS 10.9+) plus device profiles `GCDualShockGamepad` (DualShock 4, macOS 11.0), `GCDualSenseGamepad` (macOS 11.3, adds adaptive triggers) and `GCXboxGamepad` (macOS 11.0) ([GCExtendedGamepad](https://developer.apple.com/documentation/gamecontroller/gcextendedgamepad), [GCDualShockGamepad](https://developer.apple.com/documentation/gamecontroller/gcdualshockgamepad), [GCDualSenseGamepad](https://developer.apple.com/documentation/gamecontroller/gcdualsensegamepad), [GCXboxGamepad](https://developer.apple.com/documentation/gamecontroller/gcxboxgamepad)). This is Apple's own driver path, so coverage of these three families is the best of the options.
- **Pressure:** `GCControllerButtonInput.value` is "the level of pressure the user is applying to the button", and `GCControllerElement.isAnalog` reports whether an element gives analog data ([value](https://developer.apple.com/documentation/gamecontroller/gccontrollerbuttoninput/value), [isAnalog](https://developer.apple.com/documentation/gamecontroller/gccontrollerelement/isanalog)). In practice only triggers are analog on DS4/DualSense/Xbox pads, so face-button pressure is not recovered.
- **Rumble:** `GCController.haptics` → `GCDeviceHaptics.createEngine(withLocality:)` gives a Core Haptics engine (macOS 11.0+) ([haptics](https://developer.apple.com/documentation/gamecontroller/gccontroller/haptics), [createEngine](https://developer.apple.com/documentation/gamecontroller/gcdevicehaptics/createengine(withlocality:))). This is the only way to get rumble on macOS without a new dependency.
- **Keyboard:** `GCKeyboard` exists (macOS 11.0), but it is not needed because ImGui/GLFW already covers the keyboard.
- **Background:** `shouldMonitorBackgroundEvents` (macOS 11.3) is needed if input should keep working while the app is unfocused ([docs](https://developer.apple.com/documentation/gamecontroller/gccontroller/shouldmonitorbackgroundevents)).
- **Cost:** system framework, so no new third-party dependency. It does need an Objective-C++ (`.mm`) file and `-framework GameController` (plus `CoreHaptics` for rumble). The code has no reuse outside Apple.

### SDL3

- **API:** `SDL_Gamepad` with `SDL_RumbleGamepad(gamepad, low, high, duration_ms)`, and trigger rumble on supported pads ([SDL3/SDL_RumbleGamepad](https://wiki.libsdl.org/SDL3/SDL_RumbleGamepad)). On Apple platforms it drives pads through GameController (`src/joystick/apple/SDL_mfijoystick.m` imports `<GameController/GameController.h>` and has DualSense detection) as well as its own HIDAPI drivers ([SDL_mfijoystick.m](https://github.com/libsdl-org/SDL/blob/main/src/joystick/apple/SDL_mfijoystick.m)). Latest release: `release-3.4.16`, 2026-09-02.
- **Coverage/features:** the broadest option: rumble, LEDs, gyro, and the same SDL_GameControllerDB mappings.
- **Cost:** a **new dependency** (submodule + CMake), and it overlaps GLFW, which already owns the window and event loop. That goes against the map #8 rule "avoid new deps if an existing one suffices", and nothing in the port needs SDL's extras today.

## 3. Comparison

| | GLFW (pinned 3.5-dev) | GameController | SDL3 |
|---|---|---|---|
| Sticks + analog triggers | Yes (−1..1; triggers need rescale, Y sign flip) | Yes | Yes |
| Button pressure (PS2 pressure mode) | No | Only where `isAnalog` (triggers on modern pads) | No (standard gamepad) |
| Rumble | No ([#57](https://github.com/glfw/glfw/issues/57)) | Yes, Core Haptics | Yes |
| DualShock 4 / DualSense / Xbox One/Series | Yes, via bundled mappings (IOKit HID) | Yes, first-party profiles | Yes |
| Keyboard fallback | Existing ImGui/GLFW path, unchanged | Existing path, unchanged | Existing path, unchanged |
| New dependency | None | None (system framework, `.mm` file) | Yes |
| Covers what the port consumes today | Fully | Fully | Fully |

## 4. Recommendation and shape of the change (for the implementation ticket)

1. **Windows stays on WinRT:** leave `port/Windows/Input/gamepad.cpp` byte-for-byte. Guard its line in `port/CMakeLists.txt:42` with `if(WIN32)` (the renderer/host macOS work will split that list anyway).
2. **Add a macOS/GLFW backend**, e.g. `port/macOS/Input/gamepad_glfw.cpp` (or a platform-neutral `port/Input/gamepad_glfw.cpp`). It defines `KyaGamepad::AddGamepadSupport()` with the same four functions. Each call finds the first `jid` where `glfwJoystickIsGamepad` is true, then calls `glfwGetGamepadState`. Mapping: `CROSS/CIRCLE/SQUARE/TRIANGLE` → the same routes as WinRT A/B/X/Y, `BACK`→`ROUTE_SELECT`, `START`→`ROUTE_START`, bumpers → L1/R1, thumbs → L3/R3, trigger axes → L2/R2 via `(v+1)/2`, sticks with the Y sign flipped. Keep the existing pressed/released edge logic. `port/include/gamepad.h` and `win_main.cpp:31` do not change.
3. **No `#ifdef` in `src/`:** `src/port/input.cpp` only goes through `gInputFunctions`.
4. **Keyboard fallback:** no work. `DebugMenuInput.cpp` goes through ImGui's GLFW backend on both OSes.
5. **Deferred:** rumble. Once `EVENT_SET_DATA`/`EVENT_SEND_DATA` are decompiled and a `controllerRumble` entry is added to `InputFunctions`, implement it on macOS with a small `.mm` using `GCController.haptics`, and on Windows with WinRT `Gamepad.Vibration`. Button pressure is not recoverable on any modern pad or API, so it stays digital, as on Windows today.

Parity notes found while mapping, which are not blockers: the WinRT backend's opposite-direction half-axes return negative values (e.g. `ROUTE_L_ANALOG_DOWN` = −Y, `gamepad.cpp:163-165`), while the PS2 handler clamps them to 0. It also keeps separate `previousButtonState` arrays for pressed and released (`gamepad.cpp:115`, `:130`). A GLFW backend should copy this behaviour for parity, or fix both backends together.
