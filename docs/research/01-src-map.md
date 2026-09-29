# 01 — Map of `src/`: what is decompiled and what is still a stub

**TL;DR**
1. `src/` has about 790 files and 330k lines. `src/b-witch` holds 294k of them (494 files) and is the real game. `EdenLib` has 25k lines, and many of its files are empty placeholders. `edVideo`, `Rendering` and `src/port` have 3–4k lines each. `edC` and `PSX2` are mostly shells.
2. Unfinished code is marked with the `IMPLEMENTATION_GUARD*` macro family (`src/b-witch/Types.h:738-753`). There are 654 call sites. 397 of them wrap the leftover Ghidra pseudocode (about 5.1k lines) inside the macro argument. Only 152 of the roughly 7.4k function bodies are *pure* stubs; most guards sit on a single unhandled branch.
3. Decompilation is far along. The main loop, level loading, collision, navigation, sectors, save data and frontend are almost free of guards. The parts that are left are concentrated in `ed3D.cpp` (31 guarded functions), the fighter/hero/Wolfen combat code, `ActorNativ`, the pause/help menus, FX and lights.
4. The game starts from `port/Windows/Host/src/win_main.cpp:32` and goes `main_internal` → `LevelInit` / `GameLoop` / `LevelTerm` (`src/b-witch/kya.cpp:2230-2275`). Actors are created by `CActorFactory::Factory` (`src/b-witch/ActorFactory.cpp:87`) from the class IDs in the level bank.
5. Surprises:
   - 13 of the 72 factory-registered actor classes are stubs whose `Create` only calls `SkipToNextActor` and whose constructors assert; 3 more (Shocker, ShockerCmd, Blazer) assert in their constructors, and 3 enum IDs (Cinematic, Liana, DragoTermites) have no factory case.
   - About 110 `b-witch` files and about 125 `EdenLib` files are ≤5-line placeholders named after the original PS2 source files. 643 `// Should be in:` comments tie code to them.
   - The `edBank`/`edFile` submodules are not checked out.
   - CMake refers to two files with the wrong case, and `Rendering/DisplayList.cpp` is dead duplicate code.

---

## 0. Method

- Counts use `rg --files <dir> | xargs wc -l` for size and `rg -c` / `rg -o` for markers.
- A small Python brace-matcher found function bodies and sorted them into three kinds:
  - **pure stub**: the body is only a guard, maybe with a `return`
  - **partial**: the body contains a guard somewhere
  - **empty**: the body is `{}`
- The owning class came from the nearest `Class::Method(` before each body. This is approximate: macros, inline header bodies and K&R-style code can be misclassified by ±a few.
- Totals over all of `src/`: 7,393 function bodies, 446 contain a guard, 152 are pure guard stubs, 240 are empty (mostly default virtuals in headers).

## 1. How unfinished work is marked

| Marker | Definition | Effect | Count |
|---|---|---|---|
| `IMPLEMENTATION_GUARD(x)` | `src/b-witch/Types.h:750` (Win), `:752` (PS2) | Windows: `assert(false)`; PS2: logs "Hit an assert!". **`x` is discarded**, so the original Ghidra pseudocode is kept inside the argument as reference. | 561 |
| `IMPLEMENTATION_GUARD_LOG(x)` | `Types.h:745` | `assert(false)`. Mostly used in constructors of stub actors (see §4.3). | 27 |
| `IMPLEMENTATION_GUARD_FX / _LIGHT / _UI / _SHADOW / _EMOTION / _ACTOR / _HELP` | `Types.h:738-746` | `assert(false)` | 21 / 6 / 1 / 1 / … |
| `IMPLEMENTATION_GUARD_DLIST_PATCH` | `src/b-witch/DlistManager.h:8` | `assert(false)` | — |
| `IMPLEMENTATION_GUARD_WIND_FX` | `src/b-witch/ActorWind.h:10` | `assert(false)` | 2 |
| **Silent** guards: `IMPLEMENTATION_GUARD_PS2` (`Types.h:740`), `_PROFILE` (`src/b-witch/profile.h:6`), `_FIGHT` (`src/b-witch/ActorFighter.h:11`), `_LIP` (`src/b-witch/CinematicManager.h:18`), `_OBJECTIVE` (`src/b-witch/LevelScheduler.h:18`), `IMPLEMENTATION_GUARD_ASTRUCT_5` (`src/b-witch/ActorWolfen.cpp:13365`) | defined empty | The code is **skipped silently**, with no assert. Easy to miss when looking for missing behaviour, e.g. level objectives at `src/b-witch/LevelScheduler.cpp:3177`. | ~15 |
| Raw `assert(false)` | e.g. `src/b-witch/ActorHero_Private.cpp:5577`, `src/b-witch/CinematicManager.cpp:2778`, `src/EdenLib/edSys/sources/ps2/_edSystem.cpp:30`, `src/port/pointer_conv.cpp:136` | Unreachable or unknown paths | 31 |
| `SkipToNextActor(pByteCode)` as the whole `Create` | `src/b-witch/Actor.cpp:3565` | The actor's bytecode is consumed and the actor has no behaviour. This is the "stub actor" pattern. | 13 actors |
| `CActorSKIP_HACK` | `src/b-witch/ActorFactory.cpp:83` | Fallback class for unknown actor IDs | 1 |
| `// TODO` | `src/b-witch/Settings.cpp:91`, `src/EdenLib/edMusic/sources/edMusicPlay.cpp:205` | Almost never used | 2 |
| `NOT_IMPLEMENTED` / `UNIMPLEMENTED` | — | **Not used** in `src/` | 0 |
| `undefined*` / `field_0xNN` | Ghidra types and unnamed fields | Shows how much structure layout is still unnamed, not missing code | 9,083 / 32,737 occurrences |

Notes:
- Release builds (`NDEBUG`) compile the assert-type guards out, so execution falls through into partially decompiled code.
- 654 real call sites (673 regex hits minus about 19 `#define` lines). 257 have an empty argument and 397 carry pseudocode.
- The files with the most wrapped pseudocode:
  - `ActorNativ.cpp`: 901 lines
  - `ed3D.cpp`: 555 lines
  - `ActorHero_Private.cpp`: 393 lines
  - `edParticles.cpp`: 263 lines

## 2. Top-level directories under `src/`

| Dir | Files | Lines | Guards / pure stubs | Purpose |
|---|---|---|---|---|
| `b-witch/` | 494 | 294,567 | 565 / 141 | Decompiled game (project name "b-witch", matching the original `D:/Projects/b-witch/` paths). Also contains the Eden runtime pieces that were never moved out: `ed3D.cpp`, `edDlist.cpp`, `edMem.cpp`, `edStr.cpp`, `edText.cpp`. |
| `EdenLib/` | 241 | 25,465 | 79 / 10 | Eden engine libraries. Real code lives in edCollision, edParticles, edCinematic, edSound, edMusic, edDev, edSys and edText. ed3D, edDList, edEvent, edProfile, edDma and edFCamera are mostly ≤5-line placeholders. |
| `edVideo/` | 12 | 2,786 | 1 / 0 | Video mode, fades, viewport and camera stack |
| `Rendering/` | 17 | 2,829 | 3 / 0 | Text formatting/fonts, DMA "shell", and a duplicate `CGlobalDList` |
| `port/` | 15 | 4,133 | 19 / 1 | PS2-to-PC shims used by decompiled code: pointer conversion, VU1 emulation, input, ISPC kernels |
| `edC/` | 10 | 101 | 0 | Bank/filer glue. The `.cpp` files only hold includes and globals. |
| `PSX2/` | 1 | 4 | 0 | One placeholder file |

### 2.1 `src/b-witch`
Covered in detail in §3. Size facts:
- 482 top-level entries: `.cpp`, `.h`, three `.c` files, `Ps2CosineTable.inc`, `generate_cpp_files.py`, and a `PSX2/Libs/EdenLib/Include` subtree of 13 placeholder headers.
- Largest files:

| File | Lines |
|---|---|
| `ActorHero_Private.cpp` | 18,166 |
| `ActorWolfen.cpp` | 15,906 |
| `ed3D.cpp` | 15,782 |
| `ActorFighter.cpp` | 11,775 |
| `CinematicManager.cpp` | 7,163 |
| `ActorNativ.cpp` | 6,576 |
| `Actor.cpp` | 6,204 |

### 2.2 `src/EdenLib`

| Sub-lib | Files / lines | Key files | Status |
|---|---|---|---|
| edCollision | 6 / 5,703 | `sources/edCollisions.cpp` (4,036), `sources/OBBTree.cpp` (1,651) | Decompiled. 15 guards (OBBTree 11). Four other source files are empty placeholders and are not compiled. |
| edParticles | 8 / 3,891 | `sources/edParticles.cpp` | 22 guards and 8 pure stubs, e.g. `edPartGenSpherePosition` at `:76`, `edPartGenCylinderPosition` at `:81`, `edPartGenSphereSpeed` at `:116` |
| edCinematic | 6 / 3,053 | `Sources/CinScene.cpp` (1,829), `Sources/Cinematic.cpp` | Mostly done. 6 guards, plus `assert(false)` at `CinScene.cpp:357`. |
| edSound | 9 / 2,964 | `sources/edSoundPlay.cpp`, `sources/ps2/_edSoundPlay.cpp` | PS2 IOP paths guarded (`_edSoundPlay.cpp` 4, `_edSoundInit.cpp` 3) |
| edMusic | 7 / 857 | `sources/edMusicPlay.cpp`, `sources/ps2/_edMusicPlay.cpp` | 13 guards. The PS2 RPC music path is not ported (see the TODO at `edMusicPlay.cpp:205`). |
| edDev | 57 / 1,249 | `Sources/edDevInit.cpp`, `Sources/ps2/_edDevDualShock2.cpp` | Most of the 57 files were generated by `EdenLib/generate_cpp_files_2.py`. DualShock2 has 6 guards. |
| edSys | 19 / 709 | `sources/EdHandlers.cpp` | Small. Memory code actually lives in `b-witch/edMem.cpp`. |
| ed3D | 59 / 1,133 | `sources/ed3DG3D.cpp`, `sources/ed3DG2D.cpp` | About 45 of 59 files are empty `.c` placeholders. The real renderer is `b-witch/ed3D.cpp`. |
| edDList, edEvent, edProfile, edDma, edFCamera | 17 / 68, 5 / 20, 4 / 16, 1 / 4, 1 / 4 | — | **All placeholders** (not in CMake) |
| edText, edAnim | 7 / 337, 2 / 178 | `edText/sources/edTextResources.cpp`, `edAnim/AnmSkeleton.cpp` | Small, mostly done |
| Include | 32 / 5,230 | `Include/edFile/edFilePath.h` (1,050), `Include/edBank/edBankFiler.h` (686) | Public headers |
| edBank, edFile | 0 / 0 | — | **Git submodules, not initialised** in this checkout (`.gitmodules:20-24`, `git submodule status` shows `-`) |

### 2.3 `src/edVideo`
- `VideoA.cpp` covers fades and video mode (`edVideoSetFade` at `:65`, `SetVideoMode` at `:130`).
- `VideoB.cpp`, `VideoC.cpp` and `VideoD.cpp` cover the display environment and swap (`edVideoSwap` at `VideoD.cpp:71`).
- `Viewport.cpp` handles viewports (`edViewportNew` at `:17`).
- `CameraStack.cpp` holds the `CCameraStack` implementation (`CameraStack.h:13`).
- Only one guard, in `VideoD.cpp`.

### 2.4 `src/Rendering`
- `edCTextFormat.cpp` (1,386 lines; class at `edCTextFormat.h:72`) does text layout and draw.
- `edCTextStyle.cpp` and `edCTextFont.cpp` hold text styles and fonts.
- `CustomShell.h/.cpp` holds the PS2 DMA channel enum and `SYNC` shims (`CustomShell.h:6-20`).
- `DisplayList.cpp` (178 lines) is **not in CMake** and redefines `CGlobalDList` (`:110`), which is also implemented in `b-witch/DlistManager.cpp`. It is dead code.

### 2.5 `src/port`
- `pointer_conv.h/.cpp` defines the `STORE_POINTER` / `LOAD_POINTER` macros (`pointer_conv.h:37-48`) that map 32-bit PS2 pointer fields.
- `vu1_emu.cpp` (3,400 lines) is the VU1 microcode/VIF emulator (API at `vu1_emu.h:4-25`), with 16 guards and several "would need a refactor" asserts (`:242`, `:2442`).
- `input.cpp` has keyboard/mouse `edDev` handlers (`input.h:8-11`).
- `hardware_draw.cpp`, `debug_draw.cpp` and `random.cpp` are small helpers. `NativeProjection.h` builds the PC projection matrix.
- `ispc/` holds vertex kernels.

### 2.6 `src/edC` and `src/PSX2`
- `edC/*.cpp` files are 5–19 lines each, holding only includes and handler globals (e.g. `edC/edCBank.cpp:15-17`). The bank/filer logic lives in the edBank/edFile submodules.
- `PSX2/EdenLib/edPacket/edpacket.cpp` is a 4-line include guard.

## 3. `src/b-witch` grouped by subsystem

The numbers come from the scan script. "% guarded" means the share of function bodies that contain any guard. The Core group's guard count includes 11 `#define` lines in `Types.h`.

| Subsystem | Files | Lines | Funcs | Guards | Guarded funcs | Pure stubs | % guarded | `field_0x` |
|---|---|---|---|---|---|---|---|---|
| Enemies & NPC characters | 84 | 66,519 | 1,580 | 127 | 89 | 38 | 5.6 | 7,950 |
| Rendering (ed3D / DList / lights) | 22 | 26,542 | 513 | 93 | 57 | 9 | **11.1** | 908 |
| World / prop actors | 58 | 39,294 | 955 | 60 | 48 | 19 | 5.0 | 4,011 |
| Fighter / combat framework | 18 | 19,870 | 371 | 55 | 25 | 2 | 6.7 | 3,754 |
| Hero (Kya) | 19 | 28,778 | 453 | 43 | 24 | 2 | 5.3 | 2,956 |
| FX / particles | 25 | 6,408 | 237 | 32 | 32 | 27 | **13.5** | 542 |
| UI / frontend / text | 41 | 17,148 | 462 | 31 | 20 | 5 | 4.3 | 1,875 |
| Core utils / memory / math | 28 | 8,031 | 280 | 27 | 14 | 7 | 5.0 | 79 |
| Actor core & manager | 29 | 18,170 | 527 | 24 | 18 | 7 | 3.4 | 1,533 |
| Events / cinematics / video | 11 | 10,133 | 306 | 19 | 14 | 4 | 4.6 | 647 |
| Cameras | 26 | 12,275 | 222 | 16 | 10 | 0 | 4.5 | 2,667 |
| Sound / lipsync | 8 | 5,380 | 128 | 13 | 13 | 8 | 10.2 | 736 |
| Minigames / meta actors | 25 | 2,774 | 72 | 10 | 10 | 9 | **13.9** | 153 |
| Scene / level / sector / boot | 42 | 18,085 | 348 | 7 | 6 | 2 | 1.7 | 3,193 |
| Animation | 6 | 4,468 | 98 | 5 | 4 | 1 | 4.1 | 208 |
| Input / platform | 9 | 1,606 | 36 | 2 | 2 | 1 | 5.6 | 205 |
| Navigation / paths | 20 | 4,406 | 108 | 1 | 1 | 0 | 0.9 | 342 |
| Collision | 8 | 3,571 | 77 | 0 | 0 | 0 | **0.0** | 253 |

### 3.1 Actor core and manager
- **Files:** `Actor`, `Actor_Cinematic`, `ActorMovable(+Services)`, `ActorAutonomous(+Services)`, `ActorServices`, `ActorShadows`, `ActorFactory`, `ActorManager`, `ActorCmd`, `BehaviourInactive`, `SwitchBehaviour`, `Dynamic`, `Cluster`.
- **Class hierarchy:** `CActor : CObject` (`Actor.h:492`) → `CActorMovable` (`ActorMovable.h:140`) → `CActorAutonomous` (`ActorAutonomous.h:112`) → `CActorFighter` (`ActorFighter.h:514`).
- **Behaviours** are separate objects: `CBehaviour : CObject` (`Actor.h:210`).
- **Other classes:** `CActorManager : CObjectManager` (`ActorManager.h:85`), `CActorFactory` (`ActorFactory.h:18`), `CShadow` (`ActorShadows.h:36`), `CVibrationDyn` / `CScalarDyn` (`Dynamic.h`).
- **Missing:** `CActorManager::Level_PreReset` and `Level_Reset` are pure stubs (`ActorManager.cpp:451-458`).

### 3.2 Hero (Kya)
- **Files:** `ActorHero`, `ActorHero_Private` (18k lines), `_Boomy`, `_GripClimb`, `_Wind`, `_JamGut`, `_Inventory`, `BehaviourInventory`, `InventoryInfo`.
- **Hierarchy:** `CActorHero : CActorFighter` (`ActorHero.h:341`) → `CActorHeroPrivate` (`ActorHero_Private.h:187`). Also `CInventoryInterface` (`ActorHero_Inventory.h:27`).
- **Status:** 19 of 302 `CActorHeroPrivate` functions are guarded. Examples are `Manage` (`ActorHero_Private.cpp:2428`), `Draw` (`:2659`), `StateHeroRun` (`:8779`) and `_Execute_Hold` (`:16450`). These are mostly branch-level: the functions work until an unhandled state is reached.

### 3.3 Fighter / combat framework
- **Files:** `ActorFighter*` (Blow DB, Projected, Ride, Slave), `FireShot`, `ChessBoard`, `Squad`, `CameraFightData`.
- **Classes:**
  - `CActorFighter`: 12 of 191 functions guarded. Guarded areas include `Create` (`ActorFighter.cpp:476`), `_Execute_Std` (`:1215`) and `_InterpretCollisions` (`:3005`).
  - `CSquad` (`Squad.h:61`).
  - `CBehaviourFighter*`.
- **Silent skips:** the silent `IMPLEMENTATION_GUARD_FIGHT` skips code in `ActorFighter.cpp` without asserting.

### 3.4 Enemies and NPC characters
- **Wolfen family:**
  - `CActorWolfen : CActorFighter` (`ActorWolfen.h:1018`). The heaviest enemy: 15 of 189 functions guarded, 11 pure stubs inline in the header (e.g. `ActorWolfen.h:508`, `:546`, `:702`, `:1145`).
  - Subclasses `CActorBrazul` and `CActorBunch`.
- **Autonomous actors:** `CActorNativ` (`ActorNativ.h:345`, with 901 lines of guarded pseudocode), `CActorJamGut` (`ActorJamGut.h:101`), Micken, Woof, Hunter, Shocker, Shoot, Blazer, Aton, Companion, NoseMonster and Pattern.
- **Plain `CActor` subclasses:** `CActorCommander` (`ActorCommander.h:65`, 42 functions, **0 guards**), Stiller, Amortos, Electrolla, DCA and Ship.
- **Perception:** `CVision` (`Vision.h:21`).
- **Placeholders:** `ActorWolfen_*.cpp` (Fight, FireArm, Knowledge, Services, Std, Track), `ActorWolfenGhost.cpp` and `ActorJamGut_Std.cpp` are placeholders. Their code was merged into `ActorWolfen.cpp` / `ActorJamGut.cpp` with "Should be in" comments.

### 3.5 World and prop actors
- **Classes:**
  - Platforms and bonuses: `CActorMovingPlatform` (`ActorMovingPlatform.h:353`), `CActorBonus` (`ActorBonus.h:182`), `CActorSwitch` (`ActorSwitch.h:209`)
  - Weapons and projectiles: `CActorProjectile` (`ActorProjectile.h:153`), `CActorBoomy` (`ActorBoomy.h:51`)
  - Others: Bridge, Rope, Trap, Teleporter, Money, Box/BasicBox, Fruit, Rune, Wind, Weapon, Fx, FogManager, EventGenerator, CheckpointManager, Clusteriser, GravityAware, HelperSign, ExplosiveDistributor, Ambre
- **Pure-stub behaviours:**
  - `CBehaviourTeleportRandom`: 5 of 8 functions stubbed (`ActorMovingPlatform.cpp:4149-4169`)
  - `CBehaviourProjectilePortable` / `CBehaviourProjectileExcuse` (`ActorProjectile.cpp:2732-2808`)
  - `CActorBridge::CBhvWindAware` (`ActorBridge.cpp:1185-1195`)
  - `CBehaviourEventGen` camera/earthquake/draw functions (`ActorEventGenerator.cpp:1049-1423`)

### 3.6 Minigames and meta actors
- **Files:** `ActorMiniGame*` (7 classes), `ActorCredits`, `ActorE3DemoManager`, `ActorWantedZoo`, `ActorNativShop`, `ActorAddOnPurchase`, `ActorBombLauncher`.
- **Status:** all 7 minigame classes plus BombLauncher and WantedZoo are stub actors (§4.3). `ActorCredits.cpp` (522 lines) and `ActorNativShop` are real code.

### 3.7 Cameras
- **Files:** `camera`, `CameraGame`, `CameraViewManager`, `CameraBoomy`, `CameraCinematic`, `CameraFixe(Perso)`, `CameraIntView`, `CameraMouseQuake`, `CameraRail`, `CameraShadow`, `CameraFightData`.
- **Classes:** `CCamera : CObject` (`camera.h:39`), `CCameraGame : CCameraExt` (`CameraGame.h:18`), `CCameraManager` (`CameraViewManager.h:223`).
- **Missing:** `CCameraManager` has 5 guarded functions: `Level_Init` (`CameraViewManager.cpp:579`), `Manage_EarthQuake` (`:1002`), `AddCamera` (`:1587`), `PopCamera` (`:2086`) and `SetMainCamera` (`:2152`).

### 3.8 Scene, level, sector and boot
- **Files:** `kya.cpp`, `LargeObject` (implements `CScene`), `LevelScheduler`, `SectorManager`, `FileManager3D`, `BootData`, `IniFile`, `MemoryStream`, `SaveManagement`, `ScenaricCondition`, `Cheat`, `Settings`.
- **Classes:**
  - `CScene` (`LargeObject.h:121`) and `CObjectManager` (`LargeObject.h:10`), which is the manager base with `Game_Init`, `LevelLoading_*` and `Level_*` hooks
  - `CLevelScheduler` (`LevelScheduler.h:351`)
  - `CSectorManager` / `CSector` (`SectorManager.h:93`, `:67`)
  - `C3DFileManager` (`FileManager3D.h:26`)
  - `CSaveManagement` (`SaveManagement.h:40`)
- **Missing:** this is the most complete area (1.7% guarded). Known gaps:
  - `CScene::Level_Reset` (`LargeObject.cpp:875`)
  - `CLevelScheduler::Game_Term` (`LevelScheduler.cpp:2358`)
  - Silently skipped objectives code (`LevelScheduler.cpp:3177`)

### 3.9 UI, frontend and text
- **Files:** `Frontend*` / `FrontEnd*` (Disp, Bank, Widget, Life, Magic, Money, Inventory, Enemy), `Pause`, `Help`, `MapManager`, `MenuElements`, `MenuMessageBox`, `SpriteWidget`, `LocalizationManager`, `TranslatedTextData`, `edText`.
- **Classes:** `CFrontend` (`Frontend.h:126`), `CFrontendDisplay` (`FrontEndDisp.h:37`), `CWidget` (`FrontEndWidget.h:10`), `CPauseManager` (`Pause.h:193`), `CMapManager` (`MapManager.h:164`), `CHelpManager` (`Help.h:28`), `CLanguageManager` (`LocalizationManager.h:8`).
- **Missing:**
  - `Pause.cpp` has 19 guards: `DrawPauseMenu` (`:1316`), `CSimpleMenu::DrawMainMenu` (`:2245`), and the pure stubs `HelpEnter` / `HelpLeave` (`:3014`, `:3020`).
  - `CWorldMap::Draw` is a stub (`MapManager.cpp:4722`).

### 3.10 Sound and lipsync
- **Files:** `Audio`, `AudioWind`, `LipSync` / `Lipsync.h`.
- **Classes:** `CAudioManager` (`Audio.h:323`, 3 guarded functions) and `CLipTrackManager` (`Lipsync.h:107`).
- **Missing:** `Lipsync.h` has 8 pure-stub virtuals, e.g. `UpdateKFrame` at `:52`.

### 3.11 Collision
- **Files:** `CollisionManager`, `CollisionRay`.
- **Classes:** `CCollision` (`CollisionManager.h:40`), `CCollisionManager` (`CollisionManager.h:157`), `CCollisionRay`.
- **Status:** **no guards.** The low-level code is `EdenLib/edCollision`.

### 3.12 Animation
- **Files:** `Animation`, `AnmManager`, `AnimEffects` (placeholder).
- **Classes:** `CAnimationManager` (`AnmManager.h:88`), `edAnmStage`, `edAnmBinMetaAnimator`, `edAnmMacroAnimator`.
- **Missing:** 5 guards, plus an `assert(false)` at `Animation.cpp:1833`.

### 3.13 Navigation and paths
- **Files:** `Path*`, `NaviMesh`, `NaviCell`, `WayPoint`.
- **Classes:** `CPathManager` (`PathManager.h:10`), `CWayPointManager` (`WayPoint.h:19`), `CBasicNaviMesh` (`NaviMesh.h:63`), `CBasicPathFinder` (`PathFinder.h:48`), `CPathFollow`, `CPathDynamic`.
- **Status:** only 1 guard.

### 3.14 Events, cinematics and video
- **Files:** `EventManager`, `EventTrack`, `CinematicManager`; `Movie_PSS`, `playpss.c`, `video.cpp`, `ldimage.c` and `strfile.c` are placeholders.
- **Classes:**
  - `CEventManager` (`EventManager.h:136`) and `CTrackManager` (`EventTrack.h:64`)
  - `CCinematicManager` (`CinematicManager.h:910`), whose `Game_Term`, `Level_PreReset` and `Level_Reset` are stubs (`:916`, `:931-932`)
  - `CCinematic` (`CinematicManager.h:653`) and `CBWitchCin` / `CBWCin*`
- **Note:** `PLAY_INTRO_VIDEO` is 0 (`Types.h:736`).

### 3.15 FX and particles
- **Files:** `Fx`, `FxComposite`, `FxGroup`, `FxLightEmitter`, `FxLod`, `FxParticle`, `FxPath`, `FxRandom`, `FxSound`, `Fx_Spark`, `Fx_Tail`.
- **Classes:** `CNewFx` (`Fx.h:75`), `CFxManager` (`Fx.h:605`), `CFxParticleManager` (`FxParticle.h:70`).
- **Missing:** the highest stub density of any group. `FxLod.h` is 9 of 9 pure stubs (`:13-22`). `FxGroup`, `FxPath` and `FxLightEmitter` also have pure stubs.

### 3.16 Rendering (ed3D, display lists, lights)
- **Files:** `ed3D.cpp` (15.8k lines), `edDlist.cpp`, `DlistManager`, `light.cpp`, `LightManager`, `LightBase`, `StaticMeshComponent`, `ed3DScratchPadGlobalVar`.
- **Classes:** `CGlobalDListManager` (`DlistManager.h:232`), `CLightManager` (`LightManager.h:38`), `CLightSpot` / `CLightOmni` / `CLightDirectional` / `CLightTorch`.
- **Missing:**
  - `ed3D.cpp` is the **most-guarded file in `src/`**: 58 guards in 31 functions, e.g. `ed3DFlushStrip` (`:4110`), `ed3DFlushStripMultiTexture` (`:3643`) and `ed3DFlushList` (`:6821`).
  - Every light's `DoLighting` is a pure stub (`light.cpp:368`, `:454`, `:532`, `:650`).
  - `edDlist.cpp` has 3D line/sprite vertex stubs (`:1908`, `:2032`, `:2092`, `:2197`).

### 3.17 Input / platform and core utilities
- **Input:** `CPlayerInput` (`InputManager.h:123`), `CCompatibilityHandlingPS2` (`CompatibilityHandlingPS2.h:23`).
- **Core utilities:** `edMem.cpp` (Eden heap, 2 pure stubs at `:630` and `:841`), `edList`, `edStr`, `MathOps`, `edMathTCBSpline` (3 guarded functions), `TimeController` (`Timer`, `TimeController.h:8`), `profile.cpp` (5 of 6 functions are stubs), and `Types.h` (all PS2 types plus the guard macros).

## 4. Decompilation status ranking

### 4.1 Files ranked by stub markers (guard call sites)

| # | File | Guards | Guarded funcs / total | Pure stubs |
|---|---|---|---|---|
| 1 | `b-witch/ed3D.cpp` | 58 | 31 / 266 | 0 |
| 2 | `b-witch/ActorFighter.cpp` | 49 | 20 / 244 | 1 |
| 3 | `b-witch/ActorHero_Private.cpp` | 33 | 15 / 265 | 0 |
| 4 | `b-witch/ActorWolfen.cpp` | 29 | 20 / 419 | 4 |
| 5 | `b-witch/ActorNativ.cpp` | 26 | 11 / 165 | 0 |
| 6 | `EdenLib/edParticles/sources/edParticles.cpp` | 22 | 11 / 50 | 8 |
| 7 | `b-witch/Pause.cpp` | 19 | 11 / 96 | 2 |
| 8 | `b-witch/edDlist.cpp` | 17 | 12 / 96 | 4 |
| 9 | `port/vu1_emu.cpp` | 16 | 7 / 76 | 1 |
| 10 | `b-witch/ActorEventGenerator.cpp` | 16 | 8 / 31 | 0 |
| 11 | `b-witch/ActorJamGut.cpp` | 14 | 7 / 89 | 0 |
| 12 | `b-witch/CameraViewManager.cpp` | 12 | 6 / 60 | 0 |
| 13 | `EdenLib/edCollision/sources/OBBTree.cpp` | 11 | — | 0 |
| 14 | `b-witch/light.cpp` | 11 | 9 / 54 | 5 |
| 15 | `b-witch/CinematicManager.cpp` | 11 | 8 / 225 | 0 |
| 16 | `b-witch/ActorWolfen.h` | 11 | 11 / 33 | 11 |
| 17 | `b-witch/MapManager.cpp` | 10 | 7 / 59 | 2 |
| 18 | `b-witch/FxLod.h` | 9 | 9 / 9 | 9 |
| 19 | `b-witch/ActorPunchingBall.cpp` | 9 | 9 / 33 | 8 |
| 20 | `b-witch/Lipsync.h` | 8 | 8 / 11 | 8 |

(144 files contain at least one guard; the other 124 have fewer than 8.)

### 4.2 Most incomplete classes
- **By stub density** (share of the class that is pure stubs):
  - `CActorPunchingBall` with its three behaviours: 11 of about 25 functions are pure stubs (`ActorPunchingBall.cpp:144-685`)
  - `CBehaviourTeleportRandom`: 5 / 8
  - `CFxLod*` / `CFxLodScenaricData`: all stubs (`FxLod.h`, `FxLod.cpp`)
  - `CBhvWindAware`: 3 / 5
  - The `CLight*::DoLighting` family
  - `CLipTrack*` virtuals
  - `edParticles` generators
- **By absolute guard count** (big classes with many guarded branches):
  - `CActorHeroPrivate`: 19 guarded functions
  - `CActorWolfen`: 15
  - `CActorFighter`: 12
  - `CBehaviourEventGen`: 6 of 15
  - `CCameraManager`: 5 of 40
  - `CActor`: 5 of 157
  - `CActorJamGut`: 5 of 73
  - `CBnsInstance`: 5 of 11
- **By being entirely absent** (see §4.3): the 7 minigame actors, BombLauncher, WantedZoo, AmortosCmd, WoodMonster, Hedgehog, Electrolla. Liana and DragoTermites have no class at all.

### 4.3 Stub actors (factory-registered but empty)

Each of these has:
- a constructor containing `IMPLEMENTATION_GUARD_LOG()`, which asserts in debug builds, and
- a `Create()` that only calls `SkipToNextActor(pByteCode)`.

Classes: `CActorMiniGame`, `…TimeAttack`, `…BoxCounter`, `…Boomy`, `…Distance`, `CActorMiniGamesManager`, `…Organizer`, `CActorBombLauncher`, `CActorWantedZoo`, `CActorAmortosCmd`, `CActorWoodMonster`, `CActorHedgehog`, `CActorElectrolla`. Examples: `ActorMiniGameDistance.h:9-11`, `ActorMiniGameDistance.cpp:4-7`, `ActorWantedZoo.cpp:4-7`, `ActorHedgehog.cpp:4-6`, `ActorElectrolla.cpp:6-8`.

Constructors that assert without the skip-only `Create`: `CActorShocker` and `CActorShockerCmd` (`ActorShocker.h:11`, `ActorShockerCmd.h:16`) have real `Create` code but still assert in their constructors. `CActorBlazer` also asserts in its constructor (`ActorBlazer.h:48`).

Enum values with **no factory case**: `CINEMATIC` = 0x1 (built separately at `CinematicManager.cpp:667`), `LIANA` = 0x1a, `DRAGO_TERMITES` = 0x23 (`Types.h:165-241`). They fall into `default:`, which guards, logs "Unimplemented class" and allocates `CActorSKIP_HACK` (`ActorFactory.cpp:451-458`). `ActorLiana.*` and `ActorDragoTermit.*` are 4-line placeholders.

### 4.4 Most complete classes and areas (0 guards, substantial size)
- **Actors:** `CActorHero` (43 functions, `ActorHero.cpp`), `CActorCommander` (42), `CActorMovingPlatform` class proper (33; only its behaviours are stubbed), `CActorBrazul` (23), `CActorNoseMonster` (22), `CActorRope` (19), `CActorBunch`, `CActorBasicBox`, `CActorSwitch`, `CActorShip`, `CActorFruit`.
- **Collision:** `CCollision` (41) and `CCollisionManager` (15).
- **Level / sector / save:** `CSectorManager` (22) and `CSaveManagement` (19).
- **Frontend:** `CFrontendMagicGauge` (20), `CSprite` (20), `CFrontendDisplay` (18), `CInventoryInterface` (18), `CFrontendInventory` (16), `CWidget` (15).
- **Other:** `CVision` (15), `CBasicPathFinder` (13), `CIniFile` (10).
- **Whole subsystems:** Collision (0 guards), Navigation (1), Scene/Level (1.7%).

## 5. Entry points

### 5.1 Process start and main loop
1. `main()` in `port/Windows/Host/src/win_main.cpp:20-34` sets up the renderer, debug menu, texture/mesh libraries and gamepad, then calls `main_internal(argc, argv)` (`:32`).
2. `main_internal` (`src/b-witch/kya.cpp:2230-2275`; on PS2 this is `main`, `kya.h:17`) runs `MainInit` (`kya.cpp:1400`).
3. `MainInit` initialises edSys, edDebug, video, edDList, ed3D, edFile, edText, edDev, music, sound, edEvent, edBank and edVideo. It then calls `CScene::CreateScene()` (`kya.cpp:1529`) and `Game_Init()` (`kya.cpp:1530`, defined at `LargeObject.cpp:331`, which calls every manager's `Game_Init`).
4. The top-level loop is `do { LevelInit(); GameLoop(); LevelTerm(); } while ((GameFlags & 1) == 0);` (`kya.cpp:2269-2273`).
5. `LevelInit` (`kya.cpp:2202`) calls `LoadingLoop` (`:2033`), then `CScene::Level_Install` and `Level_Init`.
6. Each `GameLoop` frame (`:2078`) handles input, pause/help/map, `CScene::Level_Manage` (`LargeObject.cpp:881`), `CScene::Level_Draw` (`LargeObject.cpp:940`) and `edVideoFlip`. Soft reset is guarded (`kya.cpp:2089-2092`).

Placeholders: `main.cpp`, `Game.cpp`, `Level.cpp`, `LevelMap.cpp` and `scene.cpp` are 4-line placeholders. Their code lives in `kya.cpp` and `LargeObject.cpp`, marked `// Should be in: D:/Projects/b-witch/Game.cpp` and similar.

### 5.2 Scene and managers
- The `CScene` constructor (`LargeObject.cpp:152`) creates the `CActorFactory` (`:192`) and all managers into `CScene::ptable` (`LargeObject.h:64-90`, an array of 0x18 `CObjectManager*`). It starts with `CLevelScheduler` (`LargeObject.cpp:222`) and continues with Language, FrontendBank, FrontendDisplay, Help, Pause, Map, Camera, Sector, Light, 3DFile and others.
- Every scene phase then loops over `ptable.aManagers` and calls the matching virtual, e.g. `Level_Install` at `LargeObject.cpp:437-440` and `Level_Init` at `:542-545`.

### 5.3 Level loading
1. `CLevelScheduler::LevelLoading_Begin` (`LevelScheduler.cpp:2765`) loads `<levelPath>/<level>/LevelIOP.bnk`.
2. `LevelLoading_Manage` (`:2803`) is a 4-stage state machine: it waits for sound and music, then loads `Level.bnk` (`:2831-2843`).
3. `Level_Install` (`:2852`) installs the bank, which fires `TableBankCallback[24]` (`:2721-2745`). That table maps bank chunk types to installers: `BnkInstallScene`, `BnkInstallSceneCfg`, `BnkInstallG3D`/`G2D`, `BnkInstallCol`, `BnkInstallCameras`, `BnkInstallLights`, `BnkInstallCinematic` and so on.
4. `BnkInstallSceneCfg` (`:2386`) calls `CActorManager::Level_LoadClassesInfo` (`:2411`).
5. `BnkInstallScene` (`:2363`) feeds the scene bytecode to `Level_AddAll` on these managers in order: 3DFile, WayPoint, Path, Collision, **Actor**, Sector (`:2372-2378`).

### 5.4 Actor creation
1. **Per-class batch allocation:** `CActorManager::Level_LoadClassesInfo` (`ActorManager.cpp:973`) calls `CActorFactory::Factory(classId, count, &size, nullptr)` (`ActorManager.cpp:986`). `Factory` (`ActorFactory.cpp:87-464`) is a switch over `ACTOR_CLASS` (enum at `Types.h:165`, `ACTOR_NUM_CLASSES` = 0x57 at `Types.h:827`). It returns `NEW_ARRAY_POLYMORPHIC(CActorX, count)`, and freeing uses the same switch with `pAlloc != nullptr` (`ActorManager.cpp:83-86`).
2. **Per-instance setup:** `CActorManager::Level_AddAll` (`ActorManager.cpp:131`) reads the actor count. For each actor it reads `index` and `type`, takes the next slot from `aClassInfo[type].aActors`, and calls the virtual `pActor->Create(pMemoryStream)` (`:168`).
3. **Static class traits:** held in `CActorFactory::gClassProperties[ACTOR_NUM_CLASSES]` (`ActorFactory.cpp:465`).

## 6. Surprising findings

1. **Guards preserve pseudocode.** `IMPLEMENTATION_GUARD(...)` ignores its argument (`Types.h:750`), so about 5.1k lines of raw Ghidra output are embedded in macro arguments. They compile to nothing. A grep for missing logic should search the arguments, not only the call sites.
2. **Silent guards.** `_PS2`, `_PROFILE`, `_FIGHT`, `_LIP`, `_OBJECTIVE` and `_ASTRUCT_5` expand to nothing, so level objectives, some fighter logic and lipsync pieces are skipped without any assert (e.g. `LevelScheduler.cpp:3177`).
3. **Stub actors assert on construction.** Any level containing a minigame, WantedZoo, Hedgehog, Electrolla, Shocker or Blazer actor asserts in a debug build as soon as `Factory` allocates the array, before `Create` runs (§4.3).
4. **Gameplay paths that assert:**
   - Pressing Select calls `HelpEnter()` (`kya.cpp:2125`), which is a pure stub (`Pause.cpp:3014`).
   - Soft reset asserts (`kya.cpp:2090`).
   - Level and cinematic reset (`LargeObject.cpp:875`, `ActorManager.cpp:451-458`, `CinematicManager.h:931-932`) are stubs, which suggests checkpoint/level restart does not work.
5. **Placeholder file tree mirrors the original PS2 sources.**
   - About 110 `b-witch` files and about 125 `EdenLib` files are ≤5-line include guards.
   - They were generated by `b-witch/generate_cpp_files.py` and `EdenLib/generate_cpp_files_2.py`, from names recovered in debug strings.
   - The real code sits elsewhere, and 643 `// Should be in: D:/Projects/...` comments say where it belongs. For example, 39 functions in `ActorHero_Private.cpp` belong in `ActorHero_Std.cpp`.
   - The Eden 3D and display-list engines (`b-witch/ed3D.cpp`, `b-witch/edDlist.cpp`) live in `b-witch`, not in `EdenLib/ed3D` or `EdenLib/edDList`.
6. **Case-mismatched CMake paths.** `CMakeLists.txt:689` lists `ActorJamgut.cpp` / `.h` (the files are `ActorJamGut.*`), and `:372` lists `LipSync.h` (the file is `Lipsync.h`). This works only on case-insensitive file systems, so it would break a Linux or macOS host build.
7. **Dead duplicate.** `Rendering/DisplayList.cpp` is the only non-trivial source file missing from CMake, and it duplicates `CGlobalDList`.
8. **Submodules not checked out.** `src/EdenLib/edBank` and `src/EdenLib/edFile` are empty in this checkout (`git submodule status` shows `-`), so bank and filer code is not available here. Almost all other submodules are also uninitialised; only TextureUpload is present.
9. **Few TODO comments.** Only 2 `TODO` comments exist in all of `src/`, so the guard macros are the only reliable measure of progress.
