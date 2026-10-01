# Case Studies: PC Ports/Remasters Built on Decompilations or Static Recompilation

Scope note: researched 2026-10-01 for the Kya: Dark Lineage PS2 decomp/PC port. The most relevant precedents for Kya, as PS2 titles, are OpenGOAL (Jak), Sly Cooper/Project Cane, the Ratchet & Clank decomp and PS2Recomp. The N64/X360 projects (HarbourMasters/libultraship, sm64ex, Perfect Dark, N64Recomp/RT64, UnleashedRecomp) are the best-documented sources of technique.

## Q1. How did each project handle architecture, rendering, input/audio, mods and tooling?

### Takeaway
Two families exist. (a) **Decomp-based source ports** (sm64ex, Ship of Harkinian/libultraship, Perfect Dark, OpenGOAL) compile decompiled code natively and replace the console's graphics layer with a translation layer or rewritten renderers. (b) **Static recompilers** (N64Recomp, XenonRecomp, PS2Recomp) translate machine code to C/C++ one instruction at a time and run it on a runtime that reimplements the hardware. For a PS2 game the hard part is always the VU1/GS pipeline. OpenGOAL handled it with slow, exact "test" renderers first and then PC-native renderers fed by offline preprocessing. PS2Recomp has not solved it yet.

### Cited Findings

**OpenGOAL (Jak & Daxter, Jak II, Jak 3; PS2), the closest analogue to Kya**
- Strategy: "decompile the original game code into human-readable GOAL code" and "develop our own compiler for GOAL and recompile the game code for x86-64". GOAL (Naughty Dog's Lisp dialect) is over 98% of the original game code. Compiler performance target is "around the same as unoptimized C". — [OpenGOAL GitHub README](https://github.com/open-goal/jak-project)
- The decompiler is assisted by hand: devs "manually specify function types and locations where we believe the original code had type casts" so the output compiles directly. — [OpenGOAL README](https://github.com/open-goal/jak-project)
- Supports live modification of code while the game runs, as the original GOAL toolchain did. — [OpenGOAL README](https://github.com/open-goal/jak-project)
- Platforms: x86-64 Windows, Linux, macOS (Rosetta). Jak 1 complete, Jak 2 beta, Jak 3 in progress per the README. The blog's Q2 2026 post mentions "Tons of bugfixes post-Jak 3 release", so Jak 3 has now shipped (the README may lag). — [README](https://github.com/open-goal/jak-project); [OpenGOAL blog](https://opengoal.dev/blog)
- Renderer methodology (tfrag, non-instanced background geometry): "I made two different 'test' renderers that were slow but did things exactly the same way as in the PS2 version", then "a custom PC version called tfrag3 ... there is an offline preprocessing step that reads the level data and outputs data in a good format for PC." — [OpenGOAL docs: porting tfrag](https://opengoal.dev/docs/porting-info/drawable_and_tfrag/porting_tfrag)
- Work moved from VU1 to the GPU: "all clipping/scissoring and transformation is done on the GPU". Draw-call count is the main performance lever: "Keeping the number of OpenGL draw calls down is probably the best thing we can do for performance." Time-of-day lighting (palette color interpolation) is recomputed in C++ for tfrag3. — [OpenGOAL docs: porting tfrag](https://opengoal.dev/docs/porting-info/drawable_and_tfrag/porting_tfrag)
- The project runs natively with no PS2 emulation and has an OpenGL renderer. Press reported 4K/60fps. — [MegaVisions](https://www.megavisions.net/check-out-this-unofficial-jak-and-daxter-pc-port/); [Yahoo/PC Gamer syndication](https://sg.news.yahoo.com/fun-challenge-team-ported-jak-221244628.html)
- Docs cover texture replacements and modding of OpenGOAL code. The blog lists "new features for mods, and a fresh look for the launcher" (Q2 2026) and "custom actor support" (Jan 2025). — [opengoal.dev docs intro](https://opengoal.dev/docs/intro); [OpenGOAL blog](https://opengoal.dev/blog)

**Sly Cooper decomp / Project Cane (PS2)**
- The sly1 decomp is a WIP decompilation that aims "to better understand the game engine". The repo has no game assets or code from the executable and needs your own copy of the game. — [theonlyzac/sly1](https://github.com/theonlyzac/sly1) (via search summary)
- Project Cane is a separate fan effort toward a native PC Sly Cooper. It "uses original game data while replacing or recreating the systems needed to make that content work outside the PS2". It showed "A Stealthy Approach" gameplay in Aug 2026 and has no release date. — [Generation Amiga, 2026-08-31](https://www.generationamiga.com/2026/08/31/sly-cooper-pc-port-is-taking-shape-as-project-cane-shows-new-gameplay/)

**Ratchet & Clank (2002) decomp (PS2)**
- A matching decompilation of NTSC R&C on Codeberg. It is "currently mostly empty, but builds a byte-for-byte matching binary" and is mostly built using the Sly1 decomp as reference. — [Codeberg bordplate/RC1](https://codeberg.org/bordplate/RC1) (via search summary)

**PS2Recomp (static recompiler, PS2)**
- Converts PS2 ELF MIPS R5900 to C++, e.g. `addiu $r4,$r4,0x20` → `ctx->r4 = ADD32(ctx->r4, 0X20);`. Includes "PS2-specific MMI and VU0 macro support". — [ran-j/PS2Recomp](https://github.com/ran-j/PS2Recomp)
- Runtime (`ps2xRuntime`) has a guest memory model, a function dispatch table and syscall dispatch. `ps2xIOP` does "R3000A IRX execution, a virtual IOP kernel, and generic HLE fallbacks". TOML config sets `general.input` / `general.output` / `general.stubs` (incl. `handler@0xADDRESS` for stripped binaries). — [PS2Recomp README](https://github.com/ran-j/PS2Recomp)
- Limits: "Performance is very bad for VU and GS" and "Hardware emulation is partial and many paths are stubbed". No real renderer and no shipped game ports. — [PS2Recomp README](https://github.com/ran-j/PS2Recomp); press: [Time Extension, Jan 2026](https://www.timeextension.com/news/2026/01/native-pc-ports-of-ps2-games-could-be-on-the-way-thanks-to-new-recompilation-experiment), [Gigazine](https://www.gigazine.net/gsc_news/en/20260130-ps2recomp)

**Ship of Harkinian (OoT) / 2Ship2Harkinian (MM) / libultraship (LUS)**
- Built on the zeldaret OoT decomp plus libultraship, "a custom library designed to replicate the original N64 SDK's libultra functionality on modern systems". Render backends: DirectX 11 (Windows default), OpenGL, Metal. Switchable in Settings or `shipofharkinian.json`. — [Shipwright README](https://github.com/HarbourMasters/Shipwright)
- Assets are extracted from the user ROM into a `.o2r` archive. Custom assets ship as `.otr` in a mods folder. Tooling: **retro** (OTR generator) and **fast64** (Blender plugin). — [Shipwright README](https://github.com/HarbourMasters/Shipwright)
- Timeline: decomp finished Nov 2021, first Windows release 22 Mar 2022 with widescreen, Linux plus 60fps and save states in May 2022, custom textures/models and multiplayer in Apr 2023. LUS was then reused for 2Ship2Harkinian (26 May 2024), Star Fox 64, Mario Kart 64, SM64, Banjo-Kazooie, Paper Mario. GameCube expansion (Twilight Princess, "Courage Reborn") announced Jun 2026. — [Wikipedia: Ship of Harkinian](https://en.wikipedia.org/wiki/Ship_of_Harkinian)
- Starship (Star Fox 64, late 2024) offers higher frame rates and ultrawide. SpaghettiKart (Mario Kart 64) offers ultrawide, 4K, AA, up to 120fps and a track editor. Users generate an O2R from their US ROM with the bundled tool. — [Retro Handhelds](https://retrohandhelds.gg/you-havent-played-mario-kart-64-like-this-before/); [tbreak](https://tbreak.com/fan-made-mario-kart-64-pc-port-released-with-track-editor-and-ultrawide-support/); [GamingOnLinux Feb 2026](https://gamingonlinux.com/2026/02/spaghettikart-the-mario-kart-64-fan-made-pc-port-gets-a-big-upgrade)

**SM64 PC port (sm64-port → sm64ex)**
- Renderers: OpenGL 1.3, GL 2.1, D3D11, D3D12. Widescreen, configurable draw distance, Puppycam analog/mouse camera, remappable controls, cheats menu, and "Optional external data loading (so far only textures and assembled soundbanks), providing support for custom texture packs". — [sm64ex README](https://github.com/sm64pc/sm64ex)
- Render96 (HD model/texture packs on sm64ex) was not researched directly in this session (see Gaps).

**Perfect Dark port (fgsfdsfgs)**
- Uses "the libultraship version of fast3d" taken from Ship of Harkinian and needs OpenGL 3.0/ES3.0. Audio code comes from sm64-port, and mouselook fixes from Mouse Injector/1964GEPD authors. — [fgsfdsfgs/perfect_dark](https://github.com/fgsfdsfgs/perfect_dark)
- Features: mouselook, dual analog, widescreen, FOV, 60 FPS with framerate-bug fixes, "basic mod support, currently enough to load a few custom levels". Targets Windows, Linux, macOS and Switch. — [perfect_dark README](https://github.com/fgsfdsfgs/perfect_dark); [Time Extension](https://timeextension.com/news/2023/11/perfect-dark-has-got-a-fanmade-pc-port-which-adds-a-bunch-of-great-qol-features)

**N64Recomp / Zelda64Recomp / RT64**
- N64Recomp emits C per MIPS instruction. Jumps become function calls, and branches become switches where possible. It supports "statically linked and relocatable overlays" (`LOOKUP_FUNC`, `RELOC_HI16`) and needs an ELF with symbol/function metadata. A single-file output mode supports patched functions, which is the basis for mods. Runtime is N64ModernRuntime. — [N64Recomp README](https://github.com/N64Recomp/N64Recomp)
- RT64 is a D3D12/Vulkan/Metal N64 renderer. Interpolation does "automatic matching of draw calls between frames based on their rendering parameters". Transforms "are decomposed into their constituent components ... interpolated separately and recomposed ... to prevent interpolation artifacts". It also has arbitrary-aspect widescreen/ultrawide, DDS texture packs with async streaming (Rice-compatible names), and an "extended GBI" so ports can pass rendering hints. — [rt64/rt64](https://github.com/rt64/rt64)
- Zelda64Recomp (Majora's Mask): "all graphical effects were rendered exactly as they did originally on the N64". Objects, terrain, texture scrolling, screen effects and most HUD render at high framerate without changing gameplay. Mods install by drag-and-drop or an in-game menu, with a Thunderstore catalog. — [Zelda64Recomp README](https://github.com/Zelda64Recomp/Zelda64Recomp)

**XenonRecomp / Unleashed Recompiled (Sonic Unleashed, Xbox 360)**
- XenonRecomp translates PPC to C++ and XenosRecomp translates Xenos shaders to HLSL, "directly inspired by N64: Recompiled". The renderer runs on **Plume** (a D3D12/Vulkan abstraction) with bindless textures, shader specialization and parallel transfer queues. Pipelines compile asynchronously during loading, not during gameplay. — [UnleashedRecomp README](https://github.com/hedge-dev/UnleashedRecomp)
- Mods: Hedge Mod Manager (Generations mod format). Code mods are not available yet. — [UnleashedRecomp README](https://github.com/hedge-dev/UnleashedRecomp)

### Inferences
- Kya's current approach (decomp in `src/`, Vulkan renderer in `port/Windows/Renderer` with PCSX2-derived helpers) sits between OpenGOAL and a GS emulator. OpenGOAL's sequence is a proven way to move from GS emulation toward native rendering: exact reference renderer → offline-preprocessed, PC-native renderer per draw subsystem, with transform and clipping moved to the GPU.
- The libultraship lesson: one shared, reusable port layer (renderer abstraction + archive format + config/UI/controller) let one team ship more than five games. Kya's `src/port` shims and `port/` layer could be shaped the same way.
- RT64's draw-call matching plus decomposed-transform interpolation is a renderer-side way to get high-fps visuals without touching game tick logic. It is applicable when the renderer sees per-object matrices.

### Gaps
- No primary source fetched for OpenGOAL's high-fps (e.g. 150fps) implementation, widescreen hacks, or the merc/tie renderers. Only tfrag docs were read.
- Render96, Banjo (BanjoRecomp/LUS Banjo port) and Spaghetti Kart internals were not fetched from primary sources. The Banjo-Kazooie LUS port is mentioned only via Wikipedia.
- Input/audio architecture details for most projects were not found in the READMEs read (besides Perfect Dark's reuse of sm64-port audio).
- Renderer of Project Cane is not reported.

## Q2. How do they avoid distributing copyrighted assets?

### Takeaway
The standard pattern is a code-only repo plus user-supplied game image. The image is hash-validated, then either extracted at build time (sm64ex), converted to an archive at first launch (SoH `.o2r`, SpaghettiKart), loaded directly at runtime (Zelda64Recomp), or installed via a verifying installer (UnleashedRecomp). OpenGOAL does the same for PS2 ISOs, which maps directly onto Kya.

### Cited Findings
- SoH: user provides own ROM, validated by SHA1 (checker at ship.equipment). Assets become a `.o2r` archive. — [Shipwright README](https://github.com/HarbourMasters/Shipwright)
- Press summary: SoH "doesn't involve any of the game's original code; instead, it reads the code from a separate ROM file and extracts it". — [search summary citing GameSpot/others](https://gamespot.com/articles/legend-of-zelda-ocarina-of-time-fan-made-pc-port-is-out/1100-6501836/) (aggregator phrasing. The decomp source itself is derived from Nintendo's code, so "no original code" is a simplification.)
- Zelda64Recomp: user selects their NA ROM in the main menu. The app "automatically load[s] assets from the provided copy, so there is no need to go through a separate extraction step". — [Zelda64Recomp README](https://github.com/Zelda64Recomp/Zelda64Recomp)
- UnleashedRecomp: files come from a legally acquired Xbox 360 copy via an installer with integrity checks. "This project does not include any game assets." — [UnleashedRecomp README](https://github.com/hedge-dev/UnleashedRecomp)
- OpenGOAL: users extract from their own ISO into `iso_data/<game>`. Version detection is via JSON config. PAL/NTSC/NTSC-J retail are supported, while PS3/PS4/PS5 releases are explicitly excluded. "Do not use this decompilation project without the use of your own legally purchased copy of the game." — [OpenGOAL README](https://github.com/open-goal/jak-project)
- sm64ex: assets are extracted at build time, and "there must be **no upload of any copyrighted asset**". `./extract_assets.py --clean` strips them. — [sm64ex README](https://github.com/sm64pc/sm64ex)
- Perfect Dark: ROM is placed in `data/` with a region-specific name (`pd.ntsc-final.z64`). — [perfect_dark README](https://github.com/fgsfdsfgs/perfect_dark)
- Sly1 decomp: repo contains no assets or code from the executable. — [theonlyzac/sly1](https://github.com/theonlyzac/sly1)

### Inferences
- For Kya: a first-launch ISO picker that checks the hash per region, then extracts or caches assets into a local archive (OpenGOAL/O2R style), is the established practice. Supporting multiple retail regions and explicitly rejecting re-releases is also common.
- Matching decomps (sm64, OoT) and recompilations (Zelda64Recomp) both still ship code derived from the original binary. The asset split reduces risk but does not remove it.

### Gaps
- No court ruling found that validates the "bring your own ROM" model.

## Q3. What went well / known pitfalls (frame-rate-dependent logic, cutscenes, UI scaling)?

### Takeaway
High framerate is the main recurring pitfall. Games with variable or fixed-tick logic break above about 60fps. The robust fix is to keep the game tick and interpolate rendering (RT64), or to fix bugs one by one and tell users to cap the rate. Ultrawide breaks cutscene framing, so projects lock cutscenes to the original aspect ratio by default.

### Cited Findings
- UnleashedRecomp: "Sonic Unleashed is not a game that runs at a fixed rate on any of its target platforms". Many HFR fixes are included, but some glitches remain above 60 FPS, and users should "try temporarily limiting the frame rate". — [UnleashedRecomp README](https://github.com/hedge-dev/UnleashedRecomp)
- UnleashedRecomp ultrawide: "cutscenes are locked to their original aspect ratio to prevent presentation issues" by default (configurable), with UI alignment options. — [UnleashedRecomp README](https://github.com/hedge-dev/UnleashedRecomp)
- Perfect Dark: "Uncap Tickrate" up to 240 FPS, but "the game will have issues running faster than ~165 FPS, so use VSync or `Video.FramerateLimit`". — [perfect_dark README](https://github.com/fgsfdsfgs/perfect_dark)
- Zelda64Recomp: high framerate is achieved without affecting gameplay. External frame limiters cause stutter, so users should use the in-game limiter. Cutscenes at very wide aspect show animation quirks at screen edges. Overlays (MSI Afterburner, Wallpaper Engine) cause performance issues. — [Zelda64Recomp README](https://github.com/Zelda64Recomp/Zelda64Recomp)
- RT64 interpolates decomposed transform components separately "to prevent interpolation artifacts". — [rt64](https://github.com/rt64/rt64)
- UnleashedRecomp avoids shader-compile stutter by compiling pipelines asynchronously during loads. — [UnleashedRecomp README](https://github.com/hedge-dev/UnleashedRecomp)
- OpenGOAL: build slow, exact renderers first and optimize later. Draw-call count dominates performance. — [OpenGOAL tfrag docs](https://opengoal.dev/docs/porting-info/drawable_and_tfrag/porting_tfrag)
- PS2Recomp shows that naive VU/GS handling is very slow ("Performance is very bad for VU and GS"). — [PS2Recomp](https://github.com/ran-j/PS2Recomp)
- What went well: reuse across projects. Perfect Dark took LUS fast3d and sm64-port audio, the R&C decomp is based on Sly1's setup, and LUS was reused across about 6 N64 ports. — [perfect_dark](https://github.com/fgsfdsfgs/perfect_dark); [RC1](https://codeberg.org/bordplate/RC1); [Wikipedia SoH](https://en.wikipedia.org/wiki/Ship_of_Harkinian)

### Inferences
- For Kya (a PS2 action game probably locked to a 50/60Hz vblank tick): keep the original tick and add render interpolation, ideally in the Vulkan backend by matching draws/matrices as RT64 does. This is safer than raising the logic rate. Ship the cap and VSync options from day one.
- Default cutscenes to 4:3 or 16:9 pillarboxing, and give HUD elements anchoring options.
- Precompile or warm the Vulkan pipeline cache during loading screens.

### Gaps
- No primary write-up was found on cutscene timing desyncs or PAL 50Hz vs NTSC 60Hz handling in OpenGOAL. This is worth a follow-up.
- Mr-Wiseguy's blog posts on RT64 interpolation internals were not fetched.

## Q4. Release/distribution practices and publisher reactions

### Takeaway
Successful projects ship source plus binaries on GitHub Releases, with CI nightlies, AppImage/Flatpak on Linux, and sometimes their own launcher (OpenGOAL). They warn against third-party mirrors. Nintendo has gone after compiled binaries containing assets (SM64 2020) but has not taken down asset-free LUS/Recomp projects. Take-Two sued re3/reVC. No Sony action against OpenGOAL or the Sly projects was found.

### Cited Findings
- SoH: Windows exe, Linux AppImage, macOS bundle, nightly builds via GitHub Actions. — [Shipwright README](https://github.com/HarbourMasters/Shipwright)
- Zelda64Recomp offers a Flatpak (incl. Steam Deck). — [Zelda64Recomp README](https://github.com/Zelda64Recomp/Zelda64Recomp)
- UnleashedRecomp ships only via official GitHub releases: "We will never distribute builds on other websites, via Discord servers or via third-party update tools." — [UnleashedRecomp README](https://github.com/hedge-dev/UnleashedRecomp)
- OpenGOAL has its own launcher (redesigned in Q2 2026). — [OpenGOAL blog](https://opengoal.dev/blog)
- OpenGOAL docs describe trademark use as "nominative fair use". — [opengoal.dev docs](https://opengoal.dev/docs/intro)
- SM64 PC port (May 2020): Nintendo's representatives (Wildwood Law Group) DMCA'd hosts of the compiled executable and at least one video, calling it "an unauthorized derivative work". The effort focused on hosted binaries, not the creator. — [VGC](https://www.videogameschronicle.com/news/nintendo-takes-action-against-mario-64-pc-port/); [Nintendo Life](https://www.nintendolife.com/news/2020/05/nintendo_cracks_down_on_the_super_mario_64_pc_port); [PCGamesN](https://www.pcgamesn.com/super-mario-64/pc-port-copyright)
- SoH: Wikipedia documents no takedowns as of 2026. Outlets flagged the risk ("notoriously copyright-lawsuit-happy", GameSpot). — [Wikipedia SoH](https://en.wikipedia.org/wiki/Ship_of_Harkinian)
- re3/reVC (GTA III/VC): Take-Two's DMCA was countered and the repos were restored. Take-Two then sued 14 developers in California, GitHub removed the repos again, and the developers chose to fight. — [VGC](https://videogameschronicle.com/news/take-two-is-suing-the-creators-of-a-gta-3-and-vice-city-reverse-engineering-projects); [VGC](https://videogameschronicle.com/news/grand-theft-auto-modders-have-decided-to-fight-take-twos-lawsuit); [GamingOnLinux](https://www.gamingonlinux.com/2021/11/gta-modders-behind-re3-and-revc-fire-back-in-court/page=1/)
- Project Cane is not affiliated with Sony. No Sony response was reported. — [Generation Amiga](https://www.generationamiga.com/2026/08/31/sly-cooper-pc-port-is-taking-shape-as-project-cane-shows-new-gameplay/)
- Sep 2026: Harbour Masters faced criticism after disclosing undisclosed generative-AI use in development. This is a community-trust risk. — [Wikipedia SoH](https://en.wikipedia.org/wiki/Ship_of_Harkinian)

### Inferences
- For Kya: binaries on GitHub Releases with CI, no assets, hash-checked ISO import, and an explicit "official channels only" notice. Kya's IP holder (Eden Games/Atari lineage) has no known record for or against fan ports. Any such reaction would be speculative.

### Gaps
- Final outcome of the Take-Two v. re3 lawsuit was not found.
- No reports were found of Sony action against OpenGOAL, or of any publisher action against PS2 decomp ports specifically.
- No data was found on the IP holder of Kya (Atari/Eden) and its stance on fan projects.
