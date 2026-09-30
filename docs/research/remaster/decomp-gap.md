# Decompilation gap for a full playthrough

Answers [#38](https://github.com/mgiuditta/Kya/issues/38) (parent map: #36). Static analysis only: code on `macos/arm64-port` at `0aa81c2`, and disc data in `bin/MAC/CDEURO`. I did not run the game. Every claim below is tagged **[data]** (read from disc banks), **[code]** (read from `src/`), or **[runtime]** (already observed on the Mac and recorded in the #27 comments).

## Short answer

- Four levels load today because none of their actor classes are stubbed: 0x1 The Roots, 0x4 and 0x6 (Nativ City parts), 0x9 (Forgotten Island cave). **[data+code]**, matches **[runtime]**.
- Everything else is blocked at load by **10 actor classes that are still stubs**, plus **one crash bug in a class that is otherwise decompiled** (Brazul). No level uses a class that is missing from the factory. **[data+code]**
- The load blockers, in order of payoff:
  1. The Brazul spark fix (S) unblocks 0xB, 0xC and 0xD, which include **The Fortress**.
  2. Electrolla, Hedgehog and Shocker/ShockerCmd (L each) together unblock 0x2, 0x3, 0x5, 0x7 and 0xA.
  3. Blazer (M) is still needed for 0x8 Forgotten Island.
  4. WoodMonster and AmortosCmd (M) are still needed for 0x2.
  5. The Nativ City mini-game family (L to XL) is needed for 0x0, the hub.
- Once a level loads, what remains is 617 live `IMPLEMENTATION_GUARD` sites, which assert on the `PLATFORM_WIN` builds, macOS included. Some functions are also only partly decompiled. The worst of these for a playthrough are in combat, cutscenes, level restart and the pause/map menus.

## How this was measured

### Which classes each level instantiates [data]

- `LEVEL/<folder>/LEVEL.BNK` is an `edCBankFileHeader` bank. Its layout is defined in `src/EdenLib/Include/edBank/edBankFile.h`, and the parsing is in `edBankFile.cpp` (`get_entry`, `get_entry_typepair`).
- Bank entry type/subtype 6/2 goes to `BnkInstallSceneCfg` and 6/1 goes to `BnkInstallScene`. Both are wired in `src/b-witch/LevelScheduler.cpp:2729-2730`.
- `BnkInstallSceneCfg` ends with `CActorManager::Level_LoadClassesInfo`, which reads `count` followed by `(classId, totalCount)` pairs and calls `CActorFactory::Factory` for each class (`LevelScheduler.cpp:2411`, `ActorManager.cpp:973`).
- `CActorManager::Level_AddAll` then reads one `TSNI` ("INST") chunk per actor, holding `actorIndex` and `classId` (`ActorManager.cpp:131-168`).
- A throwaway Python script found the class list in every scene cfg. It then cross-checked the list against the `INST` records in the scene file. For all 14 levels and CREDITS, the per-class counts matched exactly. PREINTRO (0xE, the start menu) has no actors.

### Status of each class [code]

- **STUB**: the class constructor contains `IMPLEMENTATION_GUARD_LOG()` (which is `assert(false)`, `Types.h:749`), and/or `Create` is only `SkipToNextActor`.
  - `Factory` allocates classes in class-id order, so a level asserts on its **lowest-id stub class**. This is exactly what #27 recorded: Electrolla on 0x2/0x3/0x8, Hedgehog on 0x5, Shocker on 0x7/0xA.
  - `IMPLEMENTATION_GUARD` is `assert(false)` whenever `PLATFORM_WIN` is defined, and the root `CMakeLists.txt:870` defines it for every non-PS2 build, macOS included.
- **Function coverage**: `python/FunctionToFilename.txt` lists 7,450 functions with their original source file. It was dumped from a symbolized ELF whose header reads `SLUS_204.40_e3`. I matched `Class::Method` names case-insensitively against definitions in `src/` (plus the `EdenLib` submodules from the main checkout, which are not populated in worktrees).
  - Overall, 4,889 of 7,450 (66%) are matched by name.
  - This is a rough measure. It misses renamed functions and doesn't care whether a function body is complete.
  - That ELF has no symbols for the retail-only classes Brazul, BombLauncher, WantedZoo, MiniGameDistance, MiniGameBoomy and Credits, so their size is an estimate.
- **Guard counts**: invocations of the `IMPLEMENTATION_GUARD*` macros that assert, in `src/b-witch`, `src/port` and `src/EdenLib`. The no-op variants are excluded: `_PS2`, `_LIP`, `_OBJECTIVE`, `_PROFILE`, `_FIGHT`.

## Per level

Actor and class counts come from the disc **[data]**. "Blocks load" means the class is a stub **[code]**. "Partial" means under 60% of that class's original functions are present **[code]**. The first assert is **[code]**-predicted and agrees with **[runtime]** wherever #27 recorded one.

| id | level | actors | classes | blocks load (stub classes, instance count) | partial classes in use (functions present / original) | first assert |
|---|---|---|---|---|---|---|
| 0x0 | Nativ City | 1159 | 41 | HEDGEHOG x16, MINI_GAMES_MANAGER x1, MINI_GAMES_ORGANIZER x11, MINI_GAME_TIME_ATTACK x8, MINI_GAME_BOOMY x2, BOMB_LAUNCHER x4, WANTED_ZOO x1, MINI_GAME_DISTANCE x1 | COMPANION (12/23), ARAIGNOS (8/14), NATIV_CMD (8/14), ADD_ON_PURCHASE x23 (5/9) | Hedgehog |
| 0x1 | The Roots | 616 | 36 | - | COMPANION, ARAIGNOS x39, NATIV_CMD, SHOOT x3 (7/21) | none (loads) |
| 0x2 | Flying Forest | 746 | 43 | ELECTROLLA x19, HEDGEHOG x21, WOOD_MONSTER x12, AMORTOS_CMD x1, SHOCKER x4 | COMPANION, ARAIGNOS x44, EVENT_GENERATOR x5 (6/19), SHOOT x3 | Electrolla |
| 0x3 | Hunter's Domain | 1180 | 44 | ELECTROLLA x1, HEDGEHOG x50, SHOCKER x3 | COMPANION, ARAIGNOS x15, SHIP x1 (8/15), CARE_BOY x5 (5/9), EVENT_GENERATOR, SHOOT x10 | Electrolla |
| 0x4 | Nativ City part | 110 | 25 | - | EVENT_GENERATOR, ADD_ON_PURCHASE x9 | none (loads) |
| 0x5 | The Quarry | 1381 | 40 | HEDGEHOG x17 | COMPANION, ARAIGNOS x13, EVENT_GENERATOR x7, SHOOT x5 | Hedgehog |
| 0x6 | Nativ City part | 80 | 15 | - | COMPANION, EVENT_GENERATOR | none (loads) |
| 0x7 | The Air Post | 1574 | 39 | SHOCKER x11, SHOCKER_CMD x3 | COMPANION, ARAIGNOS x21, EVENT_GENERATOR, SHOOT x14 | Shocker |
| 0x8 | Forgotten Island | 812 | 38 | ELECTROLLA x9, BLAZER x28 | COMPANION, ARAIGNOS x3, EVENT_GENERATOR, SHOOT x10 | Electrolla |
| 0x9 | Forgotten Island (cave) | 102 | 18 | - | COMPANION, EVENT_GENERATOR | none (loads) |
| 0xA | Wolfun City | 576 | 40 | SHOCKER x4, SHOCKER_CMD x2 | COMPANION, EVENT_GENERATOR x2, SHOOT x7, ADD_ON_PURCHASE x1 | Shocker |
| 0xB | The Quarry (Brazul part) | 56 | 18 | - (BRAZUL crash, see below) | COMPANION | CFxSpark::Init segfault |
| 0xC | The Fortress | 187 | 26 | - (BRAZUL crash) | EVENT_GENERATOR x4 | CFxSpark::Init segfault |
| 0xD | Level Test | 120 | 23 | - (BRAZUL crash) | EVENT_GENERATOR x10 | CFxSpark::Init segfault |
| 0xE | Start Menu | 0 | 0 | - | - | - |
| 0xF | Credits | 5 | 4 (MICKEN, NATIV, WOOF, CREDITS) | - | - | - |

The factory never builds CINEMATIC (0x1), LIANA (0x1a) or DRAGO_TERMITES (0x23); those ids fall to `default: IMPLEMENTATION_GUARD()` in `ActorFactory.cpp:451`. **No retail level instantiates them [data]**, so that gap can be ignored. The complete per-level class lists are in the appendix.

### The Brazul crash [code]

- `CActorBrazul`'s spark behaviour holds a plain `CFxSpark field_0x40` followed by two buffers that are too small. The header even flags it: `// THIS IS LIKELY AN FX_SPARK_NO_ALLOC` (`ActorBrazul.h:39-42`).
- `CFxSpark::Create` sets `field_0xe4 = nullptr` (`Fx_Spark.cpp:114`), and `Init` then writes through it (`Fx_Spark.cpp:133-139`).
- Every other caller sets `field_0xe4 = aUnknown` right after `Create`: see `ActorProjectile.cpp:2327` and `ActorAmbre.cpp:50`. In Brazul that line is commented out (`ActorBrazul.cpp:1087`).
- The fix is to make the member a `CFxSparkNoAlloc<3, 12>`, matching `Create(3, 0xc, ...)` at line 1086, and to restore the assignment. Size **S**.

## Global guard count by subsystem [code]

617 live guard sites (another 42 are no-op variants).

| subsystem | live guards | biggest files |
|---|---|---|
| Renderer / 3D / display lists (engine and port) | 120 | ed3D.cpp 56, edDlist.cpp 17, vu1_emu.cpp 16, light.cpp 11, FxLod.h 9 |
| Enemies/NPCs (Wolfen, Nativ, JamGut, ...) | 112 | ActorWolfen.cpp 27 (+11 in .h), ActorNativ.cpp 26, ActorJamGut.cpp 14, ActorPunchingBall.cpp 9 |
| Other actors (platforms, triggers, cinematic behaviour, ...) | 107 | ActorEventGenerator.cpp 16, Actor.cpp 7, Actor_Cinematic.cpp 6, ActorMovingPlatform/Bridge/Boomy 5 each |
| Hero (Kya) and fighting | 88 | ActorFighter.cpp 47 (mostly `CBehaviourFighter::InitState/TermState`), ActorHero_Private.cpp 32 |
| Particles / FX | 43 | edParticles.cpp 22, Fx.cpp 4 |
| Menus / HUD / map / pause | 32 | Pause.cpp 19, MapManager.cpp 10 |
| Cinematics and lipsync | 29 | CinematicManager.cpp 11, Lipsync.h 8, CinematicManager.h 4, edCinematic 6 |
| Misc (math, memory, animation) | 19 | edMathTCBSpline.cpp 4, Animation.cpp 4 |
| Camera | 16 | CameraViewManager.cpp 12 |
| Collision | 15 | OBBTree.cpp 11 |
| Save / memory card / file | 12 | `_edFileFilerMCard.cpp` 7 (PS2 filer), edBank 4 |
| Input | 11 | `_edDevDualShock2.cpp` 6 |
| Level flow / scene / events | 7 | EventManager.cpp 3, LevelScheduler.cpp 2 |
| Audio | 6 | Audio.cpp 5 |

### Subsystems a playthrough needs [code]

- **Level restart**: `CScene::Level_Reset` (`LargeObject.cpp:875`), `CActorManager::Level_PreReset`/`Level_Reset` (`ActorManager.cpp:451-459`) and `CCinematicManager` `Level_PreReset`/`Level_Reset` (`CinematicManager.h:931-932`) are all bare guards.
  - The scene enters this path through `SCENE_STATE_RESET` (`LargeObject.cpp:668-673`).
  - The checkpoint path (`Level_CheckpointReset`) is implemented, so dying at a checkpoint should work, but a full level restart asserts.
- **Cinematics**: several things are guarded:
  - audio-synced cutscene timing (`CCinematic::ManageState_Playing`, `CinematicManager.cpp:2180/2194`, also `:485`)
  - the cutscene skip branch (`:2194`)
  - `CCinematicManagerB::Level_ManagePaused` (`:5163`)
  - `DrawBandsAndSubtitle` (`:5841`)
  - `CBehaviourCinematic::Manage`/`InterpreteCinMessage` (`Actor_Cinematic.cpp:637-815`)
  - all 7 lipsync keyframe `Create`s (`Lipsync.h:52-88`)

  LipSync is about 15 of 39 functions present.
- **Pause and menus**: guarded code includes the save-menu help text (`CSimpleMenu::DrawInitialSaveMenuHelp`), option pages (`draw_option_type_page`), `CSimpleMenuPause::set_mode` branches, `CPauseManager::Level_Draw` and `LevelLoading_Manage`, plus the world/level map draw (`CLevelMap::Draw`, `CWorldMap::Draw`, `CMapManager::Level_Manage`, `LoadMarkerPositionsForLevel`). LevelMap.cpp is about 5 of 22 functions present.
- **Save**: `SaveManagement.cpp` has no guards (13 of 19 functions matched by name). Memory card saving on macOS was fixed on this branch (commits `346838a`, `2b4a8dc`). The remaining guards are in the PS2 memory-card filer, which the PC port does not use.
- **Audio**: guards sit on `CMusicManager::IsActive`, `CAudioManager::WillLoadFileFromBank`, `ReceiveEvent` and `ActivateCheckpoint`, the last of which runs on checkpoint trigger (`Audio.cpp:259-3126`). Audio.cpp is 35 of 56 functions present.
- **Intro FMV**: `PLAY_INTRO_VIDEO 0` (`Types.h:740`, `kya.cpp:2042`). Cosmetic, not a blocker.
- **Mid-play guards in core classes**: these are not load blockers, but they fire on specific moves or states:
  - `CBehaviourFighter` Init/TermState (combat)
  - hero states such as `StateHeroRun`, `StateHeroToboggan`, `StateHeroWindWallMove` and `StateEvaluate`
  - Wolfen escape/track states
  - the Nativ Akasa arena/combo tutorial (`CBehaviourNativAkasa`)
  - JamGut riding

  Nobody can list which ones a playthrough hits without playing it. The guard counts above are the size of that pool.

## Ranked work list for a full playthrough

Each item's rank is set by how many levels and story beats it unblocks, weighed against its size. Sizes: **S** is under a day or a few functions, **M** is about 10-25 functions, **L** is 25-60 functions, and **XL** is over 60. Function counts are from the symbolized ELF (`original / present`).

| # | item | size | unblocks | evidence |
|---|---|---|---|---|
| 1 | Fix `CActorBrazul` spark: make it `CFxSparkNoAlloc<3,12>` and set `field_0xe4` | **S** (2-3 lines + header layout) | load of 0xB, 0xC Fortress, 0xD | `ActorBrazul.h:39`, `ActorBrazul.cpp:1087`, `Fx_Spark.cpp:114-139` |
| 2 | `CActorElectrolla` | **M/L** (24 fns, 3 present) | load of 0x2, 0x3, 0x8 (29 instances) | ctor guard `ActorElectrolla.h:11`, `Create` skips |
| 3 | `CActorHedgehog` | **L** (43 fns, 1 present; 28 state configs already there) | load of 0x0 hub, 0x2, 0x3, 0x5 (104 instances) | ctor guard `ActorHedgehog.h:12` |
| 4 | `CActorShocker` + `CActorShockerCmd` | **L** (39 + 19 fns, 2 + 1 present; `Create` parses but drops fields) | load of 0x2, 0x3, 0x7 Air Post, 0xA Wolfun City | ctor guard `ActorShocker.h:12` |
| 5 | Finish `CActorBlazer` (remove ctor guard, missing functions) | **M** (26 fns, 20 present) | load of 0x8 Forgotten Island (28 instances) | ctor guard; 1 live guard |
| 6 | `CActorWoodMonster`, `CActorAmortosCmd` | **M + M** (20 + 19 fns, 1 each present) | load of 0x2 Flying Forest | `Create` skips |
| 7 | Nativ City mini-games: `CActorMiniGamesManager` (20), `MiniGamesOrganizer` (62), `MiniGame` (49 base), `MiniGameTimeAttack` (32), `MiniGameBoxCounter` (33, not placed on disc), plus retail-only `MiniGameBoomy`, `MiniGameDistance`, `BombLauncher`, `WantedZoo` | **XL** (about 200+ fns in the ELF, plus 4 unsymbolized classes) | load of 0x0 hub | all `Create` skip + ctor guard. **Shortcut (S):** drop the ctor guards and keep `SkipToNextActor` to get the hub loading without mini-games, if the story does not gate on them (unverified) |
| 8 | Level restart path: `CScene::Level_Reset`, `CActorManager::Level_PreReset/Level_Reset`, `CCinematicManager::Level_PreReset/Level_Reset` | **M** | "restart level" / game-over flow | bare guards, listed above |
| 9 | Partial classes used on almost every level: `CActorEventGenerator` (31/14, 16 guards, 11 levels), `CActorShoot` + `ActorShootService` (42/19 + 25/0, 7 levels), `CActorCompanion` (33/19, 11 levels), `CActorNativCmd` (31/11), `CActorAraignos` (14/8), `AddOnPurchase`, `CareBoy`, `Ship` | **L** in total (about 110 fns) | correct behaviour after load (triggers, turrets, the companion, shops) | function coverage |
| 10 | Cinematics: audio-synced timing, skip, paused state, subtitles/bands, `CBehaviourCinematic` messages, lipsync `Create`s | **M/L** (about 30 guards, LipSync about 25 fns) | story cutscenes playing through without asserts | `CinematicManager.cpp`, `Actor_Cinematic.cpp`, `Lipsync.h` |
| 11 | Combat and hero states: `CBehaviourFighter` Init/TermState, hero state guards, Wolfen/Nativ/JamGut behaviour guards | **L** (about 200 guard sites across Hero, Fighter and enemies) | fights and boss/arena sequences (Akasa arena, JamGut riding) | guard table |
| 12 | Pause, save menu and map: `Pause.cpp` (19 guards), `CLevelMap`/`CWorldMap` draw, `MapManager` marker loading | **M** | using the pause menu, saving from the menu, the map screen | guard table |
| 13 | Audio leftovers: `ActivateCheckpoint`, `ReceiveEvent`, `WillLoadFileFromBank`, `IsActive` | **S/M** | checkpoint music/state, bank streaming edge cases | `Audio.cpp` |
| 14 | Engine/renderer guards (ed3D, edDlist, vu1_emu, particles, OBBTree) | **L**, mostly port-side | visual correctness; some may fire only in specific scenes | guard table |
| 15 | Intro FMV (`PLAY_INTRO_VIDEO`) | **S/M** | cosmetic | `Types.h:740` |

Critical path to a start-to-finish run: 1 → 3 → (7 or its shortcut) → 2 → 4 → 6 → 5, then 8, 10, 11 and 12 as they are hit while playing. By the ELF counts, items 2-7 come to about 330 functions, most of them self-contained actor classes.

## Caveats

- Function coverage matches names only. A present function can still contain guards or commented-out code: Shocker's `Create`, for example, parses its fields but stores none of them.
- The symbolized ELF is not the retail PAL build. Retail-only classes are unsized, and method sets may differ slightly.
- Which mid-play guards a playthrough actually hits cannot be known statically. Items 9-14 need a play session per level once they load.

## Appendix: classes per level [data]

Format: `CLASS instances`. Ids and names are from `ACTOR_CLASS` in `src/b-witch/Types.h:169`.

- 0x0: ACTOR 1, BOOMY 1, ACTOR_HERO_PRIVATE 1, MICKEN 10, NATIV 42, MOVING_PLATFORM 615, BONUS 8, ROPE 2, GRAVITY_AWARE 2, SWITCH 24, WIND 25, AMBER 1, FRUIT 5, BRIDGE 2, CLUSTERISER 3, COMPANION 1, PROJECTILE 21, CHECKPOINT_MANAGER 2, ARAIGNOS 8, HEDGEHOG 16, TELEPORTER 10, AMORTOS 8, HELPER_SIGN 87, TRAP 1, FX 34, PUNCHING_BALL 1, MONEY 5, NATIV_SHOP 9, BOX 11, BASIC_BOX 69, ACCELERATOS 74, NATIV_CMD 8, FOG_MANAGER 1, MINI_GAMES_MANAGER 1, MINI_GAMES_ORGANIZER 11, MINI_GAME_TIME_ATTACK 8, MINI_GAME_BOOMY 2, ADD_ON_PURCHASE 23, BOMB_LAUNCHER 4, WANTED_ZOO 1, MINI_GAME_DISTANCE 1
- 0x1: BOOMY 1, ACTOR_HERO_PRIVATE 1, MICKEN 11, WOLFEN 22, NATIV 7, MOVING_PLATFORM 166, BONUS 16, ROPE 1, GRAVITY_AWARE 1, SWITCH 17, WIND 25, COMMANDER 8, AMBER 2, FRUIT 9, BRIDGE 1, CLUSTERISER 1, COMPANION 1, DCA 4, PROJECTILE 26, CHECKPOINT_MANAGER 2, WEAPON 18, ARAIGNOS 39, TELEPORTER 5, AMORTOS 6, TRAP 24, FX 65, MONEY 3, BOX 4, ATON 1, BASIC_BOX 118, NATIV_CMD 1, FOG_MANAGER 3, SHOOT 3, STILLER 2, RUNE 1, BONUS_FOUNTAIN 1
- 0x2: ACTOR 3, BOOMY 1, ACTOR_HERO_PRIVATE 1, MICKEN 7, WOLFEN 37, NATIV 5, ELECTROLLA 19, MOVING_PLATFORM 193, BONUS 76, ROPE 5, GRAVITY_AWARE 5, SWITCH 27, WIND 32, COMMANDER 16, AMBER 19, FRUIT 8, BRIDGE 2, CLUSTERISER 7, COMPANION 1, DCA 2, PROJECTILE 40, EXPLOSIVE_DISTRIBUTOR 7, CHECKPOINT_MANAGER 2, WEAPON 21, ARAIGNOS 44, HEDGEHOG 21, TELEPORTER 4, NOSE_MONSTER 1, WOOD_MONSTER 12, AMORTOS 10, HELPER_SIGN 9, TRAP 1, FX 25, EGG 1, AMORTOS_CMD 1, MONEY 9, BOX 32, BASIC_BOX 26, EVENT_GENERATOR 5, FOG_MANAGER 1, SHOOT 3, SHOCKER 4, RUNE 1
- 0x3: ACTOR 1, BOOMY 1, ACTOR_HERO_PRIVATE 1, WOLFEN 48, NATIV 7, ELECTROLLA 1, MOVING_PLATFORM 382, BONUS 74, ROPE 14, GRAVITY_AWARE 11, SWITCH 46, WIND 51, COMMANDER 25, AMBER 13, FRUIT 3, BRIDGE 1, CLUSTERISER 5, COMPANION 1, DCA 1, PROJECTILE 50, EXPLOSIVE_DISTRIBUTOR 5, CHECKPOINT_MANAGER 1, WEAPON 26, ARAIGNOS 15, HEDGEHOG 50, TELEPORTER 6, AMORTOS 7, WOOF 15, HELPER_SIGN 24, TRAP 1, FX 42, MONEY 9, JAMGUT 4, BOX 82, SHIP 1, CARE_BOY 5, BASIC_BOX 129, EVENT_GENERATOR 1, SHOOT 10, HUNTER 1, SHOCKER 3, STILLER 4, RUNE 1, BONUS_FOUNTAIN 2
- 0x4: BOOMY 1, ACTOR_HERO_PRIVATE 1, WOLFEN 1, NATIV 11, MOVING_PLATFORM 17, BONUS 4, SWITCH 3, WIND 6, COMMANDER 1, FRUIT 3, CLUSTERISER 1, PROJECTILE 3, CHECKPOINT_MANAGER 1, TELEPORTER 2, AMORTOS 2, HELPER_SIGN 24, TRAP 1, FX 7, PUNCHING_BALL 1, MONEY 3, NATIV_SHOP 5, ATON 1, EVENT_GENERATOR 1, FOG_MANAGER 1, ADD_ON_PURCHASE 9
- 0x5: BOOMY 1, ACTOR_HERO_PRIVATE 1, MICKEN 5, WOLFEN 45, NATIV 6, MOVING_PLATFORM 731, BONUS 93, SWITCH 63, WIND 22, COMMANDER 22, AMBER 17, FRUIT 8, BRIDGE 1, CLUSTERISER 6, COMPANION 1, DCA 3, PROJECTILE 32, EXPLOSIVE_DISTRIBUTOR 6, CHECKPOINT_MANAGER 1, WEAPON 16, ARAIGNOS 13, HEDGEHOG 17, TELEPORTER 3, AMORTOS 14, WOOF 10, HELPER_SIGN 8, TRAP 4, MONEY 3, JAMGUT 1, BOX 60, ATON 1, BASIC_BOX 116, ACCELERATOS 15, EVENT_GENERATOR 7, SHOOT 5, PATTERN_CON 2, PATTERN_CMD 2, STILLER 16, RUNE 1, BONUS_FOUNTAIN 3
- 0x6: BOOMY 1, ACTOR_HERO_PRIVATE 1, WOLFEN 1, NATIV 5, MOVING_PLATFORM 29, BONUS 1, SWITCH 2, COMMANDER 1, COMPANION 1, DCA 1, PROJECTILE 12, EXPLOSIVE_DISTRIBUTOR 8, FX 15, JAMGUT 1, EVENT_GENERATOR 1
- 0x7: BOOMY 1, ACTOR_HERO_PRIVATE 1, MICKEN 11, WOLFEN 61, NATIV 8, MOVING_PLATFORM 481, BONUS 91, ROPE 2, GRAVITY_AWARE 2, SWITCH 85, WIND 44, COMMANDER 32, AMBER 18, FRUIT 8, BRIDGE 1, CLUSTERISER 9, COMPANION 1, DCA 5, PROJECTILE 106, EXPLOSIVE_DISTRIBUTOR 10, CHECKPOINT_MANAGER 1, WEAPON 52, ARAIGNOS 21, TELEPORTER 4, AMORTOS 7, WOOF 21, HELPER_SIGN 42, TRAP 10, FX 102, MONEY 3, BOX 61, BASIC_BOX 224, EVENT_GENERATOR 1, SHOOT 14, SHOCKER 11, STILLER 17, SHOCKER_CMD 3, RUNE 1, BONUS_FOUNTAIN 2
- 0x8: BOOMY 1, ACTOR_HERO_PRIVATE 1, MICKEN 1, WOLFEN 8, NATIV 4, ELECTROLLA 9, MOVING_PLATFORM 195, BONUS 37, SWITCH 15, WIND 24, COMMANDER 5, AMBER 11, FRUIT 5, CLUSTERISER 2, COMPANION 1, DCA 8, PROJECTILE 190, EXPLOSIVE_DISTRIBUTOR 2, CHECKPOINT_MANAGER 1, WEAPON 3, ARAIGNOS 3, TELEPORTER 4, AMORTOS 6, WOOF 1, HELPER_SIGN 2, TRAP 1, FX 30, MONEY 3, JAMGUT 1, BOX 10, BASIC_BOX 127, ACCELERATOS 58, EVENT_GENERATOR 1, SHOOT 10, STILLER 2, BLAZER 28, RUNE 1, BONUS_FOUNTAIN 1
- 0x9: ACTOR 1, BOOMY 1, ACTOR_HERO_PRIVATE 1, MOVING_PLATFORM 29, BONUS 31, SWITCH 1, WIND 2, COMMANDER 1, FRUIT 3, COMPANION 1, PROJECTILE 2, CHECKPOINT_MANAGER 1, TELEPORTER 1, TRAP 1, MONEY 3, BASIC_BOX 21, EVENT_GENERATOR 1, BUNCH 1
- 0xA: ACTOR 1, BOOMY 1, ACTOR_HERO_PRIVATE 1, MICKEN 4, WOLFEN 40, NATIV 2, MOVING_PLATFORM 243, BONUS 18, ROPE 2, GRAVITY_AWARE 3, SWITCH 36, WIND 12, COMMANDER 21, AMBER 7, FRUIT 6, BRIDGE 5, CLUSTERISER 1, COMPANION 1, DCA 8, PROJECTILE 64, EXPLOSIVE_DISTRIBUTOR 3, CHECKPOINT_MANAGER 1, WEAPON 21, TELEPORTER 4, AMORTOS 2, WOOF 3, HELPER_SIGN 5, TRAP 1, FX 3, MONEY 3, JAMGUT 1, BOX 5, BASIC_BOX 23, EVENT_GENERATOR 2, SHOOT 7, ADD_ON_PURCHASE 1, SHOCKER 4, STILLER 6, SHOCKER_CMD 2, BONUS_FOUNTAIN 3
- 0xB: BOOMY 1, ACTOR_HERO_PRIVATE 1, MOVING_PLATFORM 29, BONUS 4, SWITCH 1, WIND 2, COMMANDER 1, AMBER 2, FRUIT 3, COMPANION 1, PROJECTILE 2, CHECKPOINT_MANAGER 1, TELEPORTER 1, AMORTOS 1, TRAP 1, MONEY 3, ATON 1, BRAZUL 1
- 0xC: ACTOR 1, BOOMY 1, ACTOR_HERO_PRIVATE 1, WOLFEN 14, NATIV 12, MOVING_PLATFORM 49, BONUS 4, SWITCH 6, WIND 8, COMMANDER 3, AMBER 3, FRUIT 3, PROJECTILE 11, CHECKPOINT_MANAGER 2, WEAPON 5, TELEPORTER 1, AMORTOS 1, TRAP 1, FX 9, MONEY 3, BASIC_BOX 1, ACCELERATOS 37, EVENT_GENERATOR 4, STILLER 4, BRAZUL 1, BONUS_FOUNTAIN 2
- 0xD: BOOMY 2, ACTOR_HERO_PRIVATE 3, MICKEN 1, WOLFEN 7, MOVING_PLATFORM 28, BONUS 2, SWITCH 15, COMMANDER 9, CLUSTERISER 1, PROJECTILE 3, CHECKPOINT_MANAGER 1, WEAPON 3, TELEPORTER 6, AMORTOS 2, WOOF 4, TRAP 4, FX 1, MONEY 1, BOX 6, BASIC_BOX 9, EVENT_GENERATOR 10, BUNCH 1, BRAZUL 1
- 0xF (CREDITS): MICKEN 1, NATIV 2, WOOF 1, CREDITS 1
