# What decompilation-based PC ports teach about scope and effort

Research for issue #41 (parent map #36, "Kya remaster feasibility"). Gathered 2026-09-30.

**Question.** Look at projects that turned a console decompilation into a modern PC port or remaster. For each one: team size, time from decomp to a playable port, which modern features shipped first, how they handle assets and legality, and what went wrong. Then work out lessons and a rough effort calibration for Kya.

**Method.** I used primary sources where they exist: the project repositories (creation dates, tags, release notes, READMEs, contributor lists via the GitHub API), the projects' own blogs, and the GitHub DMCA repository. Press articles are used only for dates that no primary source records, and they are marked *(press)*.

**How to read "team size".** GitHub contributor counts overstate the team, because many contributors send one small PR. Each row gives three numbers: total contributors, contributors with 10 or more commits, and contributors with 50 or more commits. The last is the closest proxy for the core team. Squash-merge workflows undercount commits per person, so read these as orders of magnitude.

---

## 1. Per-project findings

### 1.1 OpenGOAL: Jak & Daxter trilogy (PS2)

The closest precedent: the same console and era, and a game-code port rather than emulation.

| Item | Value | Source |
|---|---|---|
| Repo created / first commit | 2020-08-22 | `open-goal/jak-project` API, first commit "Initial commit" 2020-08-22 |
| First tagged build | v0.0.1, 2022-04-03 | GitHub releases |
| Jak 1 first public release (v0.1.0) | 2022-05-19 | tag `v0.1.0` commit date |
| Jak 2 release line (v0.2.0) | 2023-11-05 | GitHub releases |
| Jak 3 released in launcher | Q1 2026 (v0.3.0 tag 2026-04-01) | [Q1 2026 progress report](https://opengoal.dev/blog/progress-report-q1-2026), releases |
| Contributors | 72 total, 16 with ≥10 commits, 8 with ≥50 | contributors API |
| Core | water111 (990 commits), xTVaser (692), ManDude (434), Hat-Kid (211) | contributors API |
| Licence | ISC | repo |

**Approach.** Over 98% of the games are written in GOAL, Naughty Dog's in-house Lisp. The team wrote a decompiler to GOAL source, their own GOAL compiler targeting x86-64, an asset extractor and a repacker ([README](https://github.com/open-goal/jak-project)). The first progress report (Sept 2020) estimated about **500,000 lines of GOAL** to convert. It planned automatic decompilation plus manual porting of the inline-assembly sections, and noted that heavy debug info had shipped on the disc ([Sept 2020 report](https://opengoal.dev/blog/progress-report-sept-2020)).

**Renderer.** They did not emulate the GS/VU. Each PS2 render path was reimplemented as its own native OpenGL renderer: tfrag/tie/shrub ("background"), merc ("foreground"), sky, ocean, sprite, shadow, eye, depth-cue, texture animator, and a `DirectRenderer` fallback for raw GIF packets (`game/graphics/opengl_renderer/` in the repo). VU microcode that could not be decompiled lives in `game/mips2c`.

**Assets and legality.** "Do not use this decompilation project without the use of your own legally purchased copy of the game. OpenGOAL does not include any assets from the original games." Every retail PAL/NTSC/NTSC-J PS2 disc is supported. PS3/PS4/PS5 re-releases are not ([README](https://github.com/open-goal/jak-project)). Extraction runs from the user's ISO through a launcher.

**Features first.** Native x86-64 performance, then quality-of-life and PC features: high FPS, camera inversion (in v0.1.0 notes), sound (v0.1.0 notes "Initial Sound Implementation"), texture replacement, and localisation through Crowdin (May 2023). Mod tooling came later (custom actors for Jak 2/3 in the Jan 2025 report).

**What went wrong / was hard.**
- Each sequel took about as long as the first game, even with the whole toolchain already built: Jak 1 took about 21 months (Aug 2020 → May 2022), Jak 2 about 18 more (→ Nov 2023, and it is still labelled beta in the README), and Jak 3 about 29 more (→ 2026).
- Long-tail bugs: in one crash, "even though we knew about it for many months, it took us a very long time to finally figure it out". It was a use-after-free on navmesh unload during level transitions ([Nov 2023 report](https://opengoal.dev/blog/progress-report-nov-2023)).
- High-FPS side effects (camera speed, texture scroll, UI animation), Intel GPU driver performance, and audio sync all needed dedicated passes (same report).
- The pace depends on a handful of people. Later reports say development has "been a bit slower as of late" ([blog index](https://opengoal.dev/blog)).
- x86-64 only, and macOS runs through Rosetta, because their compiler emits x86 ([README](https://github.com/open-goal/jak-project)).

### 1.2 Ship of Harkinian / HarbourMasters: OoT, MM, Star Fox 64, Mario Kart 64 (N64)

| Item | Value | Source |
|---|---|---|
| OoT decomp reaches 100% (debug ROM) | ~Nov 2021, after "nearly two years" | [VGC, 2021-11-27](https://www.videogameschronicle.com/news/zelda-64-has-been-fully-decompiled-potentially-opening-the-door-for-mods-and-ports/) *(press)*; `zeldaret/oot` created 2020-03-17 |
| SoH repo created / 1.0.0 "Deckard Alfa" | 2022-03-22 | repo API; tag `1.0.0` commit date |
| 2.0.0 / 3.0.0 | 2022-05-13 / 2022-07-14 | tag commit dates |
| Latest | 9.2.3, 2026-04-14 | releases |
| SoH contributors | 176 total, 49 ≥10, 18 ≥50 | contributors API |
| 2Ship2Harkinian (MM) | repo 2023-11-15 → 1.0.0 on 2024-05-27 (~6 months); 135 contributors, 18 ≥50 | repo, releases, contributors API |
| Starship (SF64) | repo 2024-05-21 → v1.0.0 on 2024-12-22 (~7 months); 27 contributors, 2 ≥50 | same |
| SpaghettiKart (MK64) | repo 2024-04-07 → 0.9.9 on 2025-11-15 (~19 months) → 1.0.0 on 2026-02-25 | same |

**Approach.** Every game sits on **libultraship (LUS)**, a shared reimplementation of the N64 SDK (libultra) for modern hardware, plus the matching decomp's C source ([SoH README](https://github.com/HarbourMasters/Shipwright)). Rendering goes through a display-list interpreter with DX11, OpenGL and Metal backends. The shared library is why ports after the first one took 6–7 months.

**Assets and legality.** "The Ship does not include any copyrighted assets. You are required to provide a supported copy of the game." Assets are extracted from the user's ROM into an `.o2r`/`.otr` archive, and ROM hashes are validated ([README](https://github.com/HarbourMasters/Shipwright)). **1.0.0 accepted only one ROM**: the GameCube Master Quest *debug* build (`zelda@srd022j 03-02-21`), because that was the version the decomp matched (README at tag `1.0.0`). Retail-ROM support came later.

**Features first.** 1.0: "modern controls, widescreen, high-resolution, gyroscopy" (README at 1.0.0), an ImGui menubar with enhancements and cheats, and save states. Later: alternate/custom asset packs (`mods` folder, fast64 Blender plugin, `retro` OTR tool), a randomizer, and text-to-speech.

**What went wrong.** Shipping 1.0 tied to one unusual ROM turned the ROM question into a user-support issue. The README still points to a ROM-hash compatibility checker. Scope grew continuously (randomizer, many enhancements) and needed a large contributor base: 18 people with ≥50 commits. SpaghettiKart shows the same shared stack can still take 19+ months for a game with unusual code.

### 1.3 Super Mario 64: decomp → sm64-port → sm64ex → Render96 (N64)

| Item | Value | Source |
|---|---|---|
| SM64 decomp public | 2019-08-25 ("init2"), monthly "Refresh" drops until mid-2020 | `sm64-port` commit history (inherits decomp history) |
| Official `sm64-port` "Port initial commit" | 2020-06-18 | `sm64-port/sm64-port` commits |
| `sm64ex` fork created | 2020-05-07 | repo API |
| Render96ex created | 2020-08-06 | repo API |
| Team | decomp: pseudonymous "n64" drops, 5 GitHub contributors; sm64-port: ~11 contributors; sm64ex: 58 total, 2 ≥50 (fgsfdsfgs 240, vrmiguel 91) | contributors API |

**Approach.** The port is a thin platform layer: the decomp's C plus a display-list-to-OpenGL/DX translator (`gfx_glx.h`, `gfx_pc`), SDL input and audio. Because the decomp was matching C, the port layer was small, about **10 months from decomp to a working PC build**.

**Assets and legality.** "This repo does not include all assets necessary for compiling the game. A prior copy of the game is required to extract the assets." The user must supply `baserom.<ver>.z64` at *build* time ([README](https://github.com/sm64-port/sm64-port)).

**Features first.** Widescreen, 60 FPS interpolation and free camera (sm64ex). Render96 then added a model-replacement system (DynOS) and HD texture packs, maintained in separate repos ([Render96ex README](https://github.com/Render96/Render96ex)).

**What went wrong.**
- Prebuilt executables with embedded assets circulated widely, and Nintendo targeted them (and YouTube videos) in 2020 *(press; no GitHub DMCA notice for the source repos turned up in `github/dmca`)*. The source-only, bring-your-ROM repos stayed up.
- Fork fragmentation: sm64-port → sm64ex → Render96ex and dozens of others, each with its own features and no stable upstream.
- Render96's "remaster" layer (models, textures) lives outside the code and progressed slowly and unevenly. Asset remastering is a separate project in its own right.

### 1.4 Perfect Dark: decomp → fgsfdsfgs port (N64)

| Item | Value | Source |
|---|---|---|
| Decomp | GitLab `ryandwyer/perfect-dark`, GitHub mirror created 2020-02-17; NTSC versions "fully decompiled" (~97.4% byte-matching, rest functionally equal) by Oct 2022 | repo API; [VGC 2022-10-17](https://www.videogameschronicle.com/news/perfect-dark-has-been-fully-decompiled-making-pc-ports-and-mods-possible/) *(press, quoting Dwyer)* |
| Decomp team | essentially **one person** (Ryan Dwyer) | VGC; GitHub mirror shows 2 contributors |
| Port first commits | "port: make it link and load" 2023-07-29 | `fgsfdsfgs/perfect_dark` commit history |
| Port CI build published | 2024-05-14 (`ci-dev-build`) | releases |
| Port team | 33 contributors; **1 with ≥50** (fgsfdsfgs, 724 commits); next is 42 | contributors API |
| Licence | MIT | repo |

**Features first** ([README](https://github.com/fgsfdsfgs/perfect_dark)): mouselook, dual analog, widescreen, configurable FOV, 60 FPS with framerate-bug fixes, and fixes for original crashes. Then basic mod support (custom levels), a larger heap, experimental uncapped tickrate up to 240 FPS ("issues running faster than ~165 FPS"), and Transfer Pak emulation. Platforms: Windows, Linux, macOS x86_64 and arm64, and Switch.

**Assets and legality.** A ROM with a specific md5 is required at *run* time in `data/`. Only NTSC-final is recommended, and PAL/JPN need separate executables.

**What went wrong.** Still self-described as "mostly functional ... minor graphics- and gameplay-related issues, and possibly occasional crashes" nearly three years in. There is no stable release, only a rolling CI build. One maintainer carries it.

### 1.5 For comparison: static recompilation (Zelda64Recomp, N64Recomp, PS2Recomp)

| Item | Value | Source |
|---|---|---|
| N64Recomp repo | 2022-11-16 | repo API |
| Zelda64Recomp (MM) | repo 2023-10-23 → v1.0.0 on 2024-05-10 (~7 months); 22 contributors, 2 ≥50 | repo, releases, contributors API |
| PS2Recomp | repo 2025-04-12, "Playstation 2 Static Recompiler & Runtime Tool to make native PC ports" | `ran-j/PS2Recomp` API |

This route skips the decompilation: it translates the machine code straight to C and pairs it with a high-accuracy renderer (RT64). It shipped high framerate, ultrawide, gyro, mod support, autosave, low input lag and instant loads, while keeping "all graphical effects ... exactly as they did originally" ([README](https://github.com/Zelda64Recomp/Zelda64Recomp)). The user supplies the ROM in-app. No extraction step is needed. The README states a blanket ban on generative-AI contributions. That policy position is relevant to Kya's plan to use AI for textures and audio (see the legal and community note below).

### 1.6 Counter-example: re3 / reVC (GTA III / Vice City, PC)

A reverse-engineered source port, not a decompilation of a console game, but it is the clearest legal failure in the space. Take-Two filed a DMCA notice against `GTAmodding/re3` and its forks on 2021-02-19 ([github/dmca 2021-02-19-take-two.md](https://github.com/github/dmca/blob/master/2021/02/2021-02-19-take-two.md)). Counter-notices followed (April–June 2021), and a further Take-Two notice in 2025 still lists re3 forks ([2025-04-25-take-two.md](https://github.com/github/dmca/blob/master/2025/04/2025-04-25-take-two.md)). The publisher was active, the games were still on sale, and the port competed with an upcoming remaster ("Definitive Edition", Nov 2021). Bring-your-own-assets did not protect it.

---

## 2. Cross-project patterns

| Project | Core team (≥50 commits) | Decomp → first playable port | Port → "complete" | Asset rule |
|---|---|---|---|---|
| OpenGOAL Jak 1 | 4 (8 over whole trilogy) | ~21 mo (toolchain + decomp + port together) | — | own PS2 disc, extract via launcher |
| OpenGOAL Jak 2 / 3 | same team | +18 mo / +29 mo | Jak 2 still "beta" | same |
| SoH (OoT) | many (18 today) | ~4 mo after 100% decomp | continuous since | own ROM → `.otr` (debug ROM only at 1.0) |
| 2Ship / Starship | 18 / 2 | ~6–7 mo (with LUS reused) | — | own ROM |
| SpaghettiKart | — | ~19 mo to 0.9.9 | 1.0 at ~23 mo | own ROM |
| SM64 port | ~2 | ~10 mo | forks never converge | own ROM at build |
| Perfect Dark port | **1** | ~9 mo after decomp to "mostly functional" (Jul 2023 → May 2024) | still WIP at ~3 yrs | own ROM at run |
| Zelda64Recomp | 2 | n/a (no decomp), ~7 mo | — | own ROM in-app |

1. **Porting is cheap once the decomp is complete. The decomp is the real cost.** Every N64 port that started from a finished decomp reached playable in 4–10 months, even with a single maintainer (Perfect Dark). The decompilations took about 2 years each (OoT, PD). OpenGOAL, which had to build decomp, compiler and port together, took about 21 months for its first game.
2. **A shared platform layer compounds.** HarbourMasters' second and third games took 6–7 months because libultraship was reused. OpenGOAL's sequels did not get much faster, because every game added new engine code to decompile. Reuse only helps with the platform layer, not with game code.
3. **Every project that lasted used the same legal model:** no assets in the repo or release, the user supplies the disc/ROM, the tool validates hashes, and the port extracts to its own archive. Distributing binaries with embedded assets (early SM64 builds) and competing with a publisher's own commercial remaster (re3) drew the takedowns.
4. **The first release is always an enhanced port, never a remaster.** The first features to ship are the same across projects: widescreen and resolution, high framerate (with framerate-bug fixes), modern controls (mouse, dual analog, gyro, rebinding), an in-game settings menu, and fixes for original crashes. Asset upgrades (Render96 models, HD textures, SoH custom assets) come later, are usually community-made, and live in separate packs.
5. **High framerate is a project of its own.** OpenGOAL, Perfect Dark and SM64 all needed dedicated work on camera speed, texture scroll, UI animation and logic tied to ticks. PD still breaks above ~165 FPS.
6. **Supporting one version first is normal and costs support effort later.** SoH shipped with only the debug ROM, PD recommends only NTSC-final, and OpenGOAL chose to support every PS2 retail region.
7. **The long tail lasts for years.** OpenGOAL Jak 2 is still beta about 3 years after release. PD is still "mostly functional". Months-long hunts for single crashes are normal.
8. **Bus factor.** The Perfect Dark port and sm64ex each rest on one or two people. OpenGOAL's pace fell as its core team's time dropped.

---

## 3. Lessons for Kya

Where Kya is (issue #36, 2026-09-30): one main contributor (477 of 490 commits on `main` since 2023-01-28), a non-matching C++ decomp with a Vulkan renderer already running the 3D world and lighting on Windows and macOS arm64, several levels blocked on undecompiled actors, and a `CFxSpark::Init` crash on 0xB–0xD.

1. **Kya is structurally closest to OpenGOAL**, not to the N64 ports. It is PS2, its renderer paths are reimplemented natively rather than emulated, and the decomp is not finished, so decomp and port run in parallel. N64 "4–10 months after decomp" figures do not apply until the decomp gap closes.
2. **Kya has already paid the port-layer cost** that N64 projects spend their first months on: the platform shims in `src/port/` and the Vulkan renderer in `port/Windows/Renderer/`. So the effort that remains is mostly **decompilation of missing actors and FX plus long-tail bugs**, which matches the note in #36 that the remaining decomp is probably the biggest cost item.
3. **Ship an enhanced port before any remaster.** Follow the common first-feature set (widescreen/resolution, high FPS with tick-rate fixes, controller and mouse rebinding, settings menu, crash fixes, skippable cutscenes and QoL saves). Treat HD assets as a later, separable pack, the Render96 and SoH `mods` model, so the code release never waits on art.
4. **Plan high FPS as its own work item.** Budget for an audit of every system tied to the frame (camera, animation, physics, particles, UI) rather than just uncapping the frame limiter.
5. **Adopt the proven legal model without deviation.** Keep assets out of the repo and releases, require the user's own disc, validate the ISO hash, and extract to a port-owned archive (as OpenGOAL's launcher and SoH's `.o2r` do). Never publish prebuilt binaries with assets. Watch the rights holder: re3 shows that an active publisher with a competing product can take down even a bring-your-own-assets project. (Kya-specific rights are #37.)
6. **Pick one disc version first**, and say clearly which one is supported (hash plus region), as SoH and PD did. Adding more regions later is a known and bounded cost.
7. **Treat a shared engine layer like libultraship as a design goal, for the sequel.** #36 says the remaster should be the base the sequel runs on. The HarbourMasters data shows the payoff comes when the platform and renderer layer is a clean library that game code sits on.
8. **Bus factor is the top schedule risk.** Every one-maintainer precedent (PD port, sm64ex) stays in rolling WIP. Recruiting even two or three regular contributors (the OpenGOAL core size) changes the calibration more than any technical choice.
9. **AI assets carry reputation risk as well as quality risk.** A major precedent (Zelda64Recomp) bans generative-AI contributions outright. If Kya uses AI for textures and audio, keep those packs optional and separate, and label them.

## 4. Effort calibration

These are rough ranges drawn from the precedents above. They are not a plan.

| Phase | Precedent anchor | Kya estimate (1 main dev, part-time-ish) | Kya estimate (3–4 regular devs) |
|---|---|---|---|
| Close the decomp gap to "every level loads and completes" | OoT/PD decomps ~2 yr with dozens / 1 dev; OpenGOAL Jak 1 ~21 mo with 4 core devs | **the dominant cost; size it from #38 (undecompiled, level by level)** (likely 1–2+ years solo) | roughly ÷2–3 |
| Enhanced-port feature set (widescreen, high FPS, input, menus, QoL) | PD port ~9 mo solo; SoH ~4 mo team; 2Ship/Starship 6–7 mo | **4–9 months**, of which high FPS is the biggest single item | 2–4 months |
| Long-tail stabilisation to "no known crashes on a full playthrough" | OpenGOAL Jak 2 still beta after ~3 yrs; PD "mostly functional" at ~3 yrs | **open-ended; plan ≥6 months** after feature-complete, then ongoing | ≥3 months, ongoing |
| Remaster asset layer (HD textures, audio cleanup, optional models) | Render96, SoH custom assets: community-driven, separate repos, multi-year | separate track, starts after the enhanced port ships; gated on tooling (`port/KyaTexture`, `port/KyaMesh`) | same, scales with artists rather than programmers |

**Headline calibration.** For a single-maintainer project like Kya, precedents put a *playable enhanced port* at about 1–2 years after the decomp gap is closed enough for every level to load. That gap is the variable to measure, and it is likely larger than the port work itself. A *remaster* is a second, asset-driven project on top, and none of the precedents finished one as a single coherent release: they all shipped it as optional packs.

---

## Sources

Primary (repositories, via GitHub API and READMEs, queried 2026-09-30):
- https://github.com/open-goal/jak-project and https://opengoal.dev/blog (reports: [Sept 2020](https://opengoal.dev/blog/progress-report-sept-2020), [Nov 2023](https://opengoal.dev/blog/progress-report-nov-2023), [Q1 2026](https://opengoal.dev/blog/progress-report-q1-2026))
- https://github.com/HarbourMasters/Shipwright (README at tag `1.0.0` and `develop`), https://github.com/HarbourMasters/2ship2harkinian, https://github.com/HarbourMasters/Starship, https://github.com/HarbourMasters/SpaghettiKart
- https://github.com/zeldaret/oot, https://github.com/zeldaret/mm
- https://github.com/n64decomp/sm64, https://github.com/sm64-port/sm64-port, https://github.com/sm64pc/sm64ex, https://github.com/Render96/Render96ex
- https://github.com/n64decomp/perfect_dark, https://github.com/fgsfdsfgs/perfect_dark
- https://github.com/N64Recomp/N64Recomp, https://github.com/Zelda64Recomp/Zelda64Recomp, https://github.com/ran-j/PS2Recomp
- https://github.com/github/dmca: `2021/02/2021-02-19-take-two.md`, `2021/04/2021-04-08-take-two-counternotice.md`, `2025/04/2025-04-25-take-two.md`

Secondary (dates only, marked *(press)*):
- VGC, "Zelda 64 has been fully decompiled…", 2021-11-27
- VGC, "Perfect Dark has been fully decompiled…", 2022-10-17

Not verified from a primary source: the exact scope of Nintendo's 2020 takedowns of SM64 PC port binaries and videos (press reports only), and team headcounts outside GitHub (Discord, testers, artists).
