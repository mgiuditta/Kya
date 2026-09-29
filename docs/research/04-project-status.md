# 04 - Project Status Briefing (for new contributors)

Snapshot taken 2026-09-29 on branch `fix/texture-upload-submodule` (HEAD `ae34f9c`). Nothing was built or run for this report; everything below comes from reading files and git history.

## TL;DR

1. Kya is essentially a single-maintainer project. Icey1717 has made about 477 of the 493 commits, from 2023-01-28 to now. The code is a decompilation of the PS2 game plus a Windows/Vulkan port. `a28f0f1` (2026-07-25) is titled "Complete playable!!!", which suggests the game can be finished start to end on PC.
2. Since late June 2026, work has gone into audio (the whole edSound/edMusic/CAudioManager stack), FX and renderer fidelity (emitters, heat FX, trails, cinematic lights, full-screen passes), save tooling, and ASan cleanup. September 2026 was the busiest month on record (57 commits).
3. `BugList.md` has only 3 entries. All are level-specific visual or cutscene bugs and none is a crash. The most self-contained one is the "elevator tree door always visible" bug, but you need a save at the right point to reproduce it.
4. `MemoryErrors.txt` is a Windows ASan log: 1 heap-buffer-overflow and 24 heap-use-after-free reports. They trace back to three root causes: audio stream grouping, FX-vs-actor teardown order, and a stale `CActorHero::_gThis`. All three appear fixed in `95bdfe5`, `470fa23` and `5c6186a` (2026-09-25), so the file is a historical record, not a to-do list.
5. The README does not say how to get game data. The build expects an extracted PAL disc (`assets/CDEURO/...`, gitignored), which gets mirrored into `bin/WIN/`. Ask the maintainer about this first. It is also a good small docs contribution.

---

## 1. What the README says

Source: `README.md`.

- **Goal:** reverse-engineer the original C++ source of *Kya: Dark Lineage* (Eden Games / Atari, PS2, 2003) "facilitating a deeper understanding of its mechanics, enabling modding, and preserving its legacy" (README.md:3-4). `src/` holds the decompiled code and `port/` holds the PC rendering/runtime (README.md:17-18).
- **Related projects:** KyaBank (a `.BNK` extractor), edBank and edFile (decompiled libraries, which are also submodules under `src/EdenLib/`), and the external docs site https://kyadlfiles.github.io/ (README.md:25-31).
- **Status disclaimer:** "work in progress, currently needs fixing as it may not run successfully once built. It also currently only compiles for Windows." (README.md:37). This line predates the "Complete playable" milestone and is probably out of date.
- **Requirements:** Visual Studio 2022, the Clang tools for VS2022, and the Vulkan SDK with GLM headers (README.md:39-42). AGENTS.md adds `cmake --preset x64-debug` / `cmake --build out/build/x64-debug` and a smoke test, `bin/WIN/KyaPortTest.exe` or `ctest` (AGENTS.md "Build And Test").
- **How to run:** clone with `--recursive`, open in VS, pick *x64 Debug* and the target *Kya_Debug.exe*, and optionally add `-DENABLE_ADDRESS_SANITIZER=ON` (there is also a `x64-debug-asan` preset in `CMakePresets.json`). Step 4, "Run and Test", is empty (README.md:46-71).
- **Game files: not documented.** Here is what the build actually does:
  - On Windows, the `sync_files` target runs `DoRobo.bat assets/ bin/WIN/`, a robocopy of `assets/` into the executable folder (CMakeLists.txt:803-811, `DoRobo.bat`).
  - `assets/` is gitignored (`.gitignore`). Tests and loaders expect paths like `assets/CDEURO/LEVEL` and `CDEURO/Frontend/kyatitle.g2d` (`port/Test/src/audio_music_tests.cpp:336`, `port/Test/src/tests.cpp:424`), and `cdrom0:` paths are remapped to host paths (`port/Audio/edSoundStreamService.cpp:214`).
  - So you need the extracted file tree of your own copy of the **European (CDEURO) PS2 disc** in `assets/`. This is inferred from the code, so confirm it with the maintainer.
- **License:** GPL-3.0 (README.md:88, `LICENSE.txt`).

## 2. BugList.md

Source: `BugList.md` (3 lines). It was added in `8227a10` (2025-12-03, "Up to wolfen factory") with 4 items. `d19327d` (2026-03-28) removed "There are no shop item buy cutscenes", which implies that one was fixed.

| # | Bug (verbatim) | Category | Likely code area | Good first issue? |
|---|---|---|---|---|
| 1 | "Upon retrieving the receptacle, the cutscene after throws vulkan validation warnings" (BugList.md:1) | Rendering / Vulkan correctness in a cinematic | Receptacle pickup message: `CINEMATIC_MESSAGE_RECEPTACLE_CHANGED` (`src/b-witch/CinematicManager.h:83`, handled in `src/b-witch/Actor_Cinematic.cpp:822-825`). Warnings are printed by `ValidationErrorCallback` (`port/Windows/Renderer/Vulkan/src/VulkanRenderer.cpp:1213-1219`, severity mask at :639). Cinematic lights (`8110fa8`, `70d81f1`, `e440f43`) and full-screen/framebuffer passes (`ef98dfd`, `9e5f805`) are recent renderer changes that may be involved. | **Medium.** The fix is probably small (a barrier, layout, or descriptor issue), and the validation message tells you what is wrong. The hard part is getting there: you need a save near the receptacle. The debug backup-save loader (`50f7cea`, `cb14bc1`) and the level teleport debug (`6231837`) help. It is well scoped if you are comfortable with Vulkan. |
| 2 | "Map geometry is showing up in the factory section in the flying forest, after the climb section" (BugList.md:2) | Rendering / sector visibility (gameplay-adjacent) | Sector streaming and culling: `src/b-witch/SectorManager.cpp`, `src/b-witch/Cluster.cpp`, `src/b-witch/ActorClusteriser.cpp`, and the ed3D cluster/hierarchy code in `src/EdenLib/ed3D`. The new Draw Inspector (`docs/draw-inspector.md`, `e09fbac`) is built for this kind of question ("why is `SECTn.g3d_*` drawn?"). | **No.** The cause could be in sector-switch logic, cluster culling, or the renderer, so you have to diagnose before you can fix. Worth doing once you know the pipeline. |
| 3 | "The door to the evelator tree shows up all the time" (BugList.md:3) | Gameplay-state / visibility (actor or switch logic) | Probably the hub level (level 0) elevator/teleporter logic: `CActorSwitch::ManageNativElevators` (`src/b-witch/ActorSwitch.cpp:463-494`) switches its targets only when `CActorTeleporter::LevelHasTeleporters()` finds an open destination subsector (`src/b-witch/ActorTeleporter.cpp:700-730`, the `(SStack48.flags & 1)` check). Scenario/visibility state is inspectable via `port/DebugMenu/src/DebugScenario.cpp`. This mapping is a hypothesis: "door" and "elevator tree" never appear together in the code. | **Yes, best candidate.** It probably comes down to one wrong condition, flag, or unimplemented switch target in decompiled code. You can compare it against Ghidra function by function, it needs no renderer knowledge, and you can check the fix visually. Confirm with the maintainer which actor the "door" is. |

**Overall:** 0 crashes, 0 audio bugs, and 0 memory bugs are listed. 2 are rendering bugs and 1 is gameplay/visibility. The list is short, and GitHub issues are nearly empty too: upstream has only #4, "Broken TextureUpload submodule commit" (open, 2026-08-17), which PR #5 from this fork addresses. So the real backlog lives in the maintainer's head. See the open questions below.

## 3. MemoryErrors.txt

**What it is:** a raw AddressSanitizer log (2192 lines, one process `==18596==`) from a Windows Clang+ASan debug run (`G:\repos\Kya\...` paths). It was committed and rewritten in `95bdfe5` and `470fa23` (2026-09-25), in the same commits that fixed the errors it reports.

**Error classes** (counted from `ERROR: AddressSanitizer` lines):

| Class | Count | Location | Root cause | Status |
|---|---|---|---|---|
| heap-buffer-overflow | 1 | `CAudioManager::AddSoundStreams` (`src/b-witch/Audio.cpp:2256`, MemoryErrors.txt:1-28) | The PS2 stream-grouping loop reads one byte before the `__s` array when the last group has no successor | Fixed: a Windows-only rewrite under `#ifdef PLATFORM_WIN` (`src/b-witch/Audio.cpp` ~2211-2250, `470fa23`) |
| heap-use-after-free | 1 | `CFxHandle::IsValid` ← `CActorFx::CBhvSingle::Term` (MemoryErrors.txt:77-100) | FX pools (`CFxManager::Level_Term`) were freed before actors released their FX handles | Fixed: on Windows, actors are terminated first in `CScene::Level_Term` (`src/b-witch/LargeObject.cpp` ~803-821, `470fa23`) |
| heap-use-after-free | 23 | `CActorHero::TestState_*` (via `CCameraStack::GetCurHeroState` during the next level's `LevelLoading_Begin`), plus DebugMenu readers: `Debug::Hero::ShowMenu`, `GetActorBehaviourName`, `GetActorStateName`, `DebugHelpers::ImGui::TextVector4`, ImGui `InputFloat` | All freed by `DeleteArrayPolymorphic<CActorHeroPrivate>` (`Types.h:786`) ← `CActorFactory::Factory`, so the stale pointer is the global `CActorHero::_gThis` across a level transition | Fixed: `_gThis` is cleared after actor teardown (`src/b-witch/ActorManager.cpp`, `470fa23`). `DebugHero.cpp:320-322` already null-checks it. `5c6186a` also passes the real count to the polymorphic array delete so every destructor runs. |

**Subsystems involved:** audio (level sound-bank install), FX/particles teardown, the hero actor and camera stack across level transitions, and the port DebugMenu. Other Windows-only bound-check fixes in the same batch cover `C3DFileManager::GetMaterialFromId` (`FileManager3D.cpp`), `CFxEmitterPool::Draw` (`ActorWind.cpp`), `OBBTree.cpp`, `ActorBonus`, and `ActorMoney` (`95bdfe5`). The pattern is that Ghidra's `&&` ordering reads out of bounds before the bound check. The fix keeps the PS2 behaviour behind `#else`.

**For newcomers:** rerun with the `x64-debug-asan` preset (`ASAN_OPTIONS=halt_on_error=0`) to get a fresh log. The committed file probably no longer matches the current code.

## 4. Commit history

**Totals:** 493 commits. First commit `6d0f12e` / `41a7563` on 2023-01-28.

**Contributors** (`git shortlog -sne --all`):
- Icey1717 (two emails): 295 + 182 = **477**, plus 1 as "Joseph Griggs" (`9bfef13`)
- KyaDLFiles ("The Kya: Dark Lineage Files"): 12, all in May 2025. These made the repo buildable from GitHub (upstream PR #2), added settings (PR #3), and updated the README.
- mgiuditta (this fork): 3, all on 2026-09-29 (`961dddc`, `8d270b6`, `ae34f9c`)

**Commits per month:**

```
2023: Jan 4  Mar 3  Apr 1  May 1  Jun 6  Jul 10 Aug 8  Sep 8  Oct 6  Nov 4  Dec 8
2024: Jan 6  Feb 2  Mar 6  Apr 13 May 11 Jun 14 Jul 3  Aug 6  Sep 7  Oct 12 Nov 18 Dec 11
2025: Jan 24 Feb 4  Mar 4  Apr 2  May 46 Jun 24 Jul 21 Aug 10 Sep 4  Oct 8  Nov 4  Dec 4
2026: Jan 1  Feb 0  Mar 52 Apr 16 May 13 Jun 19 Jul 6  Aug 6  Sep 57
```

Activity comes in bursts: long quiet stretches (for example Jan-Feb 2026) followed by very dense months. Commits are often large. Since 2026-06-29 there are 69 commits touching about 1,122 file-changes (+59k / -8k lines). Part of that is tooling (`tools/local_renamer`, `tools/ghidra-module`), and `a28f0f1` even committed Gradle build outputs.

### Progress timeline (how far the game runs)

| Date | Commit | Milestone |
|---|---|---|
| 2024-06 | `ddfcf45`, `e2e3b5d`, `041d701` | "Native renderer mostly working", toggleable, alpha blending |
| 2024-12 / 2025-01 | `22f6b6b`, `1adb711`, `f857f2b`, `6ba4d48` | Nativ City intro cutscenes, "Level 2 loading" |
| 2025-01-04 | `6231837` | Debug teleport-to-level |
| 2025-05 | `a41a589`, `11e33b6` | Speaking to Nativs, shop, episode scheduler |
| 2025-06-08 | `eff7d7f` | "Load Flying Forest" |
| 2025-06/07 | `50da9f2`, `c7752cc` | Wolfen fights |
| 2025-12-03 | `8227a10` | "Up to wolfen factory" (BugList created) |
| 2026-04/05 | `b6c27ef`, `2612427` | Wolfen escape, "Add brazul fight in" (Brazul is the final boss) |
| 2026-07-25 | `a28f0f1` | **"Complete playable!!!"**: ActorBrazul, ActorCredits, FireShot, fog. The game appears completable through the credits. |
| 2026-08/09 | `9a9ae8e` … `b8bc517` | Audio: from stub to "Audio impl finished" (2026-09-15) and music playback (`fce8cc5`) |
| 2026-09-26 | `2dfa7f0` | Audio in level transitions fixed |

So according to commit messages, every level is reachable and the game can be finished. Remaining work is fidelity (rendering, FX, audio polish), correctness (ASan, validation), and the level-specific bugs in BugList. "Playable" has not been independently verified for this report.

### Areas worked on in the last ~3 months (since 2026-06-29)

Most-touched paths by file-change count: `src/EdenLib/ed3D` (56), `tools/local_renamer` (52), `port/Windows/Renderer` (47), `src/EdenLib/edSound` (43), `src/EdenLib/Include` (41), `tools/ghidra-module` (34), `port/DebugMenu/src` (30), `port/Test/src` (25), `edMusic`/`edDList` (17 each), `src/b-witch/Audio.cpp` (16).

By theme:
- **Audio** (Aug-Sep): `adde976`, `9a9ae8e`, `dd272c5`, `96a6ef8`, `bd07c1a`, `dd97825` … `b8bc517`, `2dfa7f0`. Includes a new `port/Audio/` backend and GoogleTest audio tests.
- **FX / renderer fidelity** (Sep): emitters (`6810ea8`, `5fad1a1`), PS2 full-screen passes and heat FX (`ef98dfd`, `c5a6e06`, `1bff358`), sparks, trails, cluster sprites and static mesh refactor (`11d14e8`, `c26ccd2`, `ec823c7`, `dff6f59`, `efc71d6`), shadow pass (`37dd254`), cinematic lights (`8110fa8`).
- **Debug tooling:** Draw Inspector (`e09fbac`), backup-save browser and direct load (`50f7cea`, `127ee1b` … `cb14bc1`, tested in `port/Test/src/windows_save_tests.cpp`), auto-saving (`e35c68e`).
- **Gameplay completion:** sign actor, frontend inventory (`6bbe3f9`, `a3ab5b7`, `8df9476`), Brazul/credits (`a28f0f1`).
- **Correctness:** gauss fix that made actors inactive (`42c2b64`), ASan fixes (`95bdfe5`, `470fa23`, `5c6186a`).
- **Fork housekeeping:** TextureUpload submodule pointer (`961dddc`, upstream PR #5 / issue #4), duplicate json submodule (`8d270b6`), agent config (`ae34f9c`).

Several recent commits follow an AI-agent workflow ("superpowers" spec → plan → TDD commits on 2026-09-24; see `docs/superpowers/`).

## 5. Existing docs (`docs/`)

- `docs/debug-drawing-reference.md`: API reference for `Renderer::Native::DebugShapes`, used to queue world-space debug lines and shapes each frame.
- `docs/draw-inspector.md`: how to use the DebugMenu Draw Inspector (capture a frame, filter draws by mesh/texture, open mesh/texture viewers).
- `docs/particles-draw.md`: walkthrough of `edPartDrawShaper`, the single particle-to-display-list function.
- `docs/sound-instance-generation-handles.md`: layout of 32-bit generation+slot handles for `ed_sound_instance`.
- `docs/agents/domain.md`: tells agent skills to read `CONTEXT.md` and `docs/adr/`. Neither exists yet.
- `docs/agents/issue-tracker.md`: issues live on GitHub and are handled with `gh`.
- `docs/agents/triage-labels.md`: maps canonical triage roles to label names.
- `docs/plans/local-variable-renaming-tool.md`: plan for `tools/local_renamer`, a web app that gets LLM-suggested local-variable names with human review.
- `docs/plans/free-model-comparison.md`: plan for an evaluator ranking free OpenRouter models for that renamer.
- `docs/renderer-refactor/README.md`: index of 10 renderer architecture findings, with checkboxes.
  - `01-...service-locator.md`: global `ImageRendererApp` state exposed through free functions.
  - `02-...resource-ownership.md`: inconsistent RAII vs. manual `Destroy()` for Vulkan objects.
  - `03-mixed-failure-handling.md`: exceptions, asserts, and bool returns mixed together.
  - `04-native-renderer-inl-files.md`: `NativeRenderer.cpp` split across `.inl` includes.
  - `05-native-renderer-global-state.md`: mutable namespace-global renderer state.
  - `06-cmake-and-module-boundaries.md`: flat source list and over-broad include dirs.
  - `07-native-frame-resources.md`: per-frame resources living on `NativeRendererState`.
  - `08-native-draw-granularity.md`: 846 tiny draws per frame in the `AirPostBaseLine` capture.
  - `09-native-redundant-state-binds.md`: low state variety but a rebind on every draw.
  - `10-renderdoc-python-capture-analysis.md`: using RenderDoc's Python API for capture analysis.
  - `AirPostBaseLine_gpu_timing_summary.md`, `AirPostBaseLine_gpu_durations.csv`, `analyze_renderdoc_gpu_timing.py`: GPU timing data and script for that capture.
- `docs/superpowers/specs/2026-09-24-backup-save-direct-load-design.md` and `docs/superpowers/plans/2026-09-24-backup-save-direct-load.md`: design and implementation plan for loading a debug backup save without restoring it. Already merged in `cb14bc1`.

## 6. Good first contributions

Ranked by scope, grounded in the files above.

1. **README "Getting game files" and "Run" sections.** Document the `assets/CDEURO` layout, `DoRobo.bat`, `bin/WIN`, the ASan preset, and `ctest`. It is small and useful, and there is nothing in the code to break.
2. **BugList #3 (elevator-tree door).** Likely a localised decompilation logic bug in `ActorSwitch.cpp` / `ActorTeleporter.cpp`, and you can compare it against Ghidra.
3. **Refresh `MemoryErrors.txt`.** Run the `x64-debug-asan` preset through a few level transitions, then triage or fix any new reports. Recent fixes follow a repeatable pattern: `#ifdef PLATFORM_WIN` guard plus an explanatory comment, with the original kept in `#else`.
4. **Renderer-refactor findings** that are still unchecked in `docs/renderer-refactor/README.md`. Some are mechanical (06 CMake boundaries, 04 `.inl` split), but check with the maintainer first because the renderer is under active change.
5. **BugList #1 (Vulkan validation in receptacle cutscene).** Only if you know Vulkan. The validation message pinpoints the problem.

## 7. Open questions for the upstream maintainer (Icey1717)

1. **Game data:** which disc/region is officially supported (PAL "CDEURO" only?), and what is the exact expected `assets/` layout? Is there an extraction script?
2. **"Complete playable":** does that mean start-to-credits with no debug teleports? Which levels or sequences are known to be broken or skipped?
3. **Where does the backlog live?** Is `BugList.md` current and complete, or is there a private list? Would you accept GitHub issues and labels (`docs/agents/triage-labels.md` suggests yes)?
4. **BugList #3:** which actor is the "door to the elevator tree", and in which level/sector? Can you share a save or checkpoint for #1 and #2?
5. **`MemoryErrors.txt`:** is it meant to be a living log (regenerate and commit) or a one-off? Should ASan runs be part of PR validation?
6. **Decompilation fidelity policy:** is `#ifdef PLATFORM_WIN` divergence (as in `470fa23`) the preferred way to fix PS2-UB bugs? Is matching or byte-exact decompilation a goal, or only behavioural equivalence?
7. **Branch/PR workflow:** do you merge external PRs to `main` directly? Should submodule changes (`port/KyaMesh`, `port/KyaTexture`, TextureUpload) be coordinated first (see issue #4 / PR #5)?
8. **Non-Windows plans:** is a Linux/macOS port wanted (Vulkan/MoltenVK), or is Windows-only intentional for now?
9. **Renderer refactor:** is `docs/renderer-refactor/` an active roadmap open to contributors, or personal notes?
10. **Tooling artifacts:** should build outputs under `tools/ghidra-module/totala/build/` (committed in `a28f0f1`) be removed and gitignored?
