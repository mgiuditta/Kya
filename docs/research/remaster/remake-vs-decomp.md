# What a modern-engine remake would cost compared with the decomp route

Research for [#44](https://github.com/mgiuditta/Kya/issues/44), part of the remaster feasibility map [#36](https://github.com/mgiuditta/Kya/issues/36).
Researched 2026-09-30. Code references are against `main` @ `2dfa7f0`, disc data in the main checkout's `bin/MAC/CDEURO`.
Inputs: [#38 decomp gap](https://github.com/mgiuditta/Kya/blob/research/remaster-decomp-gap/docs/research/remaster/decomp-gap.md), [#41 port precedents](https://github.com/mgiuditta/Kya/blob/research/remaster-precedents/docs/research/remaster/port-precedents.md), [#40 asset upscaling](https://github.com/mgiuditta/Kya/blob/research/remaster-assets/docs/research/remaster/asset-upscaling.md), [#37 rights](https://github.com/mgiuditta/Kya/blob/research/remaster-rights/docs/research/remaster/rights-and-risk.md).

Tags: **[code]** read from this repo, **[tool]** read from a public tool's source, **[primary]** official page or repo, **[press]** secondary source, used only where no primary one was found. Estimates are rough ranges, not a plan.

## Answer in short

- **A faithful remake costs more than the decomp route, not less, and it does not remove the decomp work.** The gameplay is about 262k lines of decompiled C++ in `src/b-witch` (119 actor classes; the hero alone is about 26.6k lines) **[code]**. A remake has to rebuild all of that. Without the original code you rebuild it by watching the game, and the result drifts. With the decomp as a reference you still need the decomp, including the third that is not done yet (#38).
- **Asset conversion is partly solved and mostly buildable.** KyaBank already turns G2D into PNG and G3D *object* meshes into glTF **[tool]**. Missing: level geometry (clusters), skeletons and animations, collision, level layout (actor placement and per-class parameters), cameras, paths, lights, particles and cinematics. Each of these can be exported by reusing the decomp's own parsers, but the level-layout exporter needs the per-class `Create` parsers, which are exactly the parts #38 lists as stubbed.
- **Precedents:** professional remakes of PS2 games took a funded studio about 1.5–2.5 years (Destroy All Humans!: 60 people; SpongeBob Rehydrated: from Jan 2018 to Jun 2020). Solo or small-team fan remakes of 3D games mostly ship demos or single levels, run for a decade, or stop (CryZENx's OoT in Unreal: 10 years, stopped; Sonic P-06: solo since 2014, demos only; Rayman 2/3 UE5 fan remakes: one level each).
- **Legal difference is large.** The decomp route ships no game data and needs the player's disc (the model every surviving project uses, #37/#41). A remake with new art distributes a derivative work of Atari's characters, levels and story, so the disc requirement no longer protects it. AM2R, the best-known finished fan remake, was taken down. A remake that converts the disc at install time keeps the disc model but then shows the original low-poly assets, which defeats the reason for a remake.
- **Side-by-side (one main developer):** decomp route to a full enhanced port about **2–3.5 years**, with remaster packs later as a separate track. Faithful remake about **5–10 years**, and likely unfinished. With a team of 4–6 including artists, about 1–2 years vs about 3–5 years. **Recommendation: keep the remake as a comparison only.** It makes sense only as a licensed, funded project (the Atari/Nightdive route in #37), and even then the decomp is the best specification it could have.

## 1. Scale of what a remake has to rebuild [code]

Counts from `src/b-witch` on `main`:

| Part | Size in the decomp | What it covers |
|---|---|---|
| Whole game layer | 262,414 lines in `*.cpp`, 119 `Actor*.cpp` files | every actor, manager and rule |
| Hero (Kya) | 26,645 lines across `ActorHero*.cpp` (`ActorHero_Private.cpp` 18,166) | movement states, climbing (`ActorHero_GripClimb.cpp`), wind walls, toboggan, riding |
| Fighting | `ActorFighter.cpp` 11,775 | combat state machine shared by hero and enemies |
| Boomy | `ActorBoomy.cpp` 1,717 + `CameraBoomy.cpp` 387 | boomerang targeting, flight, aim camera |
| Enemies / NPCs | `ActorWolfen.cpp` 15,906, `ActorNativ.cpp` 6,576, `ActorJamGut.cpp` 3,994, and more | AI, escape/track, arenas, riding |
| Cameras | 11,096 lines in `Camera*.cpp` | game, fixed, rail, fight, cinematic cameras |
| Cinematics | 8,005 lines in `Cinematic*.cpp` + `Actor_Cinematic.cpp`, plus `edCinematic` | cutscene playback, audio sync, lipsync |
| Content | 14 levels + credits, about 10,000 placed actors (0x7 alone has 1,574) | from #38's per-level table [data] |

A remake needs a design-level equivalent of every row. The movement and combat "feel" lives in these state machines and in tuning values read from the scene data, so it cannot be copied from video alone.

## 2. Asset reuse: what can be converted and with which tools

### What is on the disc [code]

`TableBankCallback` in `src/b-witch/LevelScheduler.cpp` lists every bank entry type the game installs: animations and anim macros, sound config, samples, songs, sound banks, scene and scene cfg, G3D meshes, G2D textures, static and dynamic collision, events, tracks, cameras, A* paths, lights, liptracks, FX config, cinematics and particle managers. Each level folder has `LEVEL.BNK`, `SECT*.BNK` (streamed sectors), `MAP*.BNK`, `CINE/`, `STREAM/` and `STREAMCH/`.

### Existing tools

| Tool | What it does | Licence | Source |
|---|---|---|---|
| **KyaBank** (Icey1717) | `list`/`extract` BNK; `texconvert` G2D → PNG; `meshconvert` G3D → glTF/glb | GPL-3.0 (the LICENSE file; the README says MIT) | [repo](https://github.com/Icey1717/KyaBank) **[tool]** |
| `port/KyaTexture` | decodes G2D (with palettes) into RGBA8; shared with KyaBank | submodule | `port/KyaTexture/src/Texture.cpp` **[code]** |
| `port/KyaMesh` | walks G3D hierarchies → LODs → objects → strips into `SimpleMesh` vertex buffers | submodule | `port/KyaMesh/src/Mesh.h` **[code]** |
| edBank / edFile decomps | bank and file readers | — | [edBank](https://github.com/Icey1717/edBank), [edFile](https://github.com/Icey1717/edFile) |
| Port runtime | every other format is parsed by decompiled engine code (edAnim, edCollision, edCinematic, particles, music synth) | GPL-3.0 (this repo) | `src/EdenLib/*`, `port/Audio` **[code]** |

What KyaBank's mesh converter does **[tool]** (`src/Mesh/ConvertMesh.cpp`): it runs the engine's own `ed3DInstallG3D`, then writes one `.gltf` per hierarchy and LOD (`<name>_H<n>_L<n>.gltf`) with `POSITION`, `NORMAL`, `TEXCOORD_0` and `COLOR_0` and one opaque, double-sided PBR material per texture. It does **not** export the cluster (level geometry), skeletons, skin data, animations or alpha modes.

### Per-asset status

| Asset | Can it be converted? | Tool today | Work left | Size |
|---|---|---|---|---|
| Textures (G2D) | Yes | KyaBank `texconvert`; port decode | Keep PS2 alpha 0–128 → rescale to 0–255 for a modern engine; one image per palette variant (#40) | S |
| Character / prop meshes (G3D objects) | Yes | KyaBank `meshconvert` | Material alpha modes, LOD grouping into one asset | S |
| Level geometry (G3D clusters, `CSTA`) | Yes, same parser | none (KyaMesh parses clusters, KyaBank does not write them) | Export cluster strips per sector; stitch sectors | M |
| Skeletons and skinning | Yes | none | Skinning is **rigid**: each vertex carries one bone-matrix index (`native.vert.glsl`, `animFlags` → `animMatrix[...]`) **[code]**, so a glTF skin with weight 1.0 per vertex is enough. Bone hierarchy from `edANM_SKELETON` | M |
| Animations | Yes, by baking | none | Sample the decompiled evaluator (`edAnmTransformCtrl::GetValue`, `src/b-witch/Animation.cpp`) per bone per frame into glTF channels. Anim macros / layered blends (`edAnmMetaAnimator`) must be re-authored in the engine's animation graph | M/L |
| Collision (static + dynamic) | Yes | none | Export `edCollision` triangle sets and OBB trees (`OBBTree.cpp`, 11 live guards) as mesh colliders; material/flag mapping to engine physics layers | M |
| Level layout (actor placement) | Partly | none | `INST` chunks give class and index **[#38]**, but each actor's transform and parameters are read by its class's `Create`. 10 classes are stubs and several `Create`s parse but drop fields (#38). So a full layout export needs the same decomp work the decomp route needs | L (and blocked by #38) |
| Cameras, paths (A*), tracks, lights, fog | Yes, parsers exist | none | Export to engine splines / nav data / light actors; the camera *behaviour* is code (§1), not data | M |
| Particles / FX | Poorly | none | Eden's particle system (`edParticles`, 22 guards) does not map onto Niagara / VFX Graph / GPUParticles. Rebuild by eye | L |
| Cinematics (`.cin`) | Partly | none | Camera and actor tracks can be baked like animations; events, lipsync (about 15/39 functions present) and audio sync need rebuilding | M/L |
| Music | Awkward | port synth (`edMusicSynth`) | Music is sequenced Sony data played by a synth (#40). A modern engine needs either the port's synth as a plugin or songs rendered to audio, which loses interactive transitions | M |
| Voice / SFX streams | Yes | vgmstream, port decoder (#40) | Batch decode VAG / PS-ADPCM to WAV | S |

**Result:** about 6–12 months of tool work for one developer to get geometry, textures, skeletons, baked animations, collision, cameras and audio into Unreal, Unity or Godot, *provided* the tools reuse this repo's parsers. Level layout is the exception: it depends on finishing the stubbed actor classes, which is decomp-route work.

This matches the professional precedent: Purple Lamp used Industrial Park, a fan tool for Heavy Iron's archives, "to aid in extracting levels as reference material" for SpongeBob Rehydrated ([Wikipedia](https://en.wikipedia.org/wiki/SpongeBob_SquarePants:_Battle_for_Bikini_Bottom_%E2%80%93_Rehydrated) **[press]**; [IndustrialPark repo](https://github.com/igorseabra4/IndustrialPark), GPL-3.0 **[primary]**). Extracted data served as reference; the art was rebuilt.

## 3. Rebuilding gameplay without the original code

| System | Without the decomp | Using the decomp as a spec |
|---|---|---|
| Movement (run, jump, glide, climb, wind walls, toboggan) | Tuned by feel against video/emulator. Many states, each with its own constants | Port constants and state graphs; the hero is mostly decompiled |
| Combat (`CBehaviourFighter`) | Rebuild combos, hit reactions, arena rules by observation | 47 guards in `ActorFighter.cpp` are still open (#38) |
| Boomy | Small and well bounded; rebuildable | Straightforward |
| Enemy AI (Wolfen, Nativ, JamGut, Hedgehog, Shocker, Electrolla, Blazer, WoodMonster) | Each needs behaviour design from observation | Four of these are stubs today (#38); same cost as the decomp route |
| Cameras | High drift risk: camera feel is the most noticed difference in platformer remakes | 11k lines; port as logic |
| Cinematics | Re-author in Sequencer / Timeline / AnimationPlayer from baked tracks | Audio sync and skip are guarded (#38) |
| Menus, saves, map, shops, mini-games | Rebuild; no fidelity constraint beyond look | Nativ City mini-games are XL stubs (#38) |

**Using the decomp as a spec is the only realistic way to get the "faithful" result #36 requires**, but it has two costs. First, the missing third of the decomp (#38's ~330 functions to load every level, then the guards hit in play) has to be done anyway, or those parts are designed from scratch. Second, porting 262k lines of C++ behaviour into an engine's actor model is a rewrite, not a copy: Eden's actors run on their own scheduler, collision, animation and camera stacks, so each class is re-expressed against Unreal's `AActor`/Chaos, Unity's `MonoBehaviour`/PhysX, or Godot's nodes/Jolt. That is where the "reimplement every level" cost goes. Keeping Eden's systems and only swapping the renderer is what the decomp route already does with the Vulkan backend.

### Fidelity risk

- **High** for movement, camera and combat feel: these are the parts players compare first, and the engine's physics and character controller have different defaults (step heights, slope handling, ground snapping, fixed vs variable timestep). The decomp route keeps the original tick (0.02 s, #39) and its behaviour.
- **Medium** for enemy AI and level scripting: event generators (`CActorEventGenerator`, used on 11 levels, 16 guards) and triggers must be re-implemented one by one.
- **Low** for static art: converted meshes and textures are the original data.
- **Art-style risk** if art is reworked: #36 requires the original style, and a remake's reason to exist is new art. Professional remakes still needed dedicated art-direction work to stay recognisable (Destroy All Humans! rebuilt every asset with a 60-person team; Rehydrated worked with Nickelodeon on the style) **[press]**.

## 4. Precedents: remakes of PS2-era and 3D games

| Project | Engine | Team | Duration | Outcome | Source |
|---|---|---|---|---|---|
| Destroy All Humans! (2020), PS2 original | UE4 | Black Forest Games, **60 people** | announced Jun 2019, released 28 Jul 2020 | Shipped; kept original dialogue and audio (improved), rebuilt every asset, new mocap | [Wikipedia](https://en.wikipedia.org/wiki/Destroy_All_Humans!_(2020_video_game)) **[press]** |
| SpongeBob BfBB Rehydrated, PS2/GC/Xbox original | UE4 | Purple Lamp Studios (size not published) | Jan 2018 → 23 Jun 2020 (~2.5 years) | Shipped; levels extracted with a fan tool as reference | [Wikipedia](https://en.wikipedia.org/wiki/SpongeBob_SquarePants:_Battle_for_Bikini_Bottom_%E2%80%93_Rehydrated) **[press]** |
| CryZENx, Ocarina of Time fan remake | UE4 / UE5 | essentially 1 person | 2016 → stopped 2026 (~10 years) | Never finished; stopped voluntarily when Nintendo announced an official remake | [Zelda Universe, 2026-07-11](https://zeldauniverse.net/2026/07/11/cryzenx-halts-ocarina-of-time-fan-remake-project-after-10-years/) **[press]** |
| Sonic P-06, Sonic '06 fan remake | Unity (since 2014) | 1 main developer (Ian "ChaosX" Moris) + helpers | first demo Apr 2019, demos through 2023 and later | Ongoing demos; animations, textures and shaders largely remade | [Wikipedia](https://en.wikipedia.org/wiki/Sonic_P-06) **[press]** |
| AM2R, Metroid II fan remake (2D) | GameMaker | 1 main developer | ~2006 → Aug 2016 (~10 years) | Released, then DMCA'd by Nintendo; development ended Sep 2016 | [Wikipedia](https://en.wikipedia.org/wiki/AM2R) **[press]** |
| Rayman 2: Redreamed | UE 5.6 | "a team of artists" | demo 7 Sep 2025 | One level (Fairy Glade) + move-set course | [GameJolt](https://gamejolt.com/games/rayman-2-redreamed/1004947), [DSOGaming](https://www.dsogaming.com/news/rayman-2-fan-remake-demo-available-for-download/) **[press]** |
| Rayman 3: Havoc Reignited | UE5 | small (one artist credited) | announced Apr 2024, demo 6 Sep 2025 | First world only | [DSOGaming](https://www.dsogaming.com/pc-performance-analyses/rayman-3-unreal-engine-5-fan-remake-demo-released/) **[press]** |
| Project Reimagine (Rayman 2 level) | UE5 | 1 student | 10 weeks | One level, as coursework | [itch.io](https://nathan3197.itch.io/project-reimagine) **[primary]** |

Pattern: a single level with good-looking art is a 10-week to 1-year job; a full 3D game with faithful gameplay is a funded studio's 1.5–2.5 years, or a solo fan's decade. None of the 3D fan remakes above has shipped a complete game. The decomp ports in #41 (OpenGOAL, SoH, Perfect Dark) did ship complete games with 1–4 core developers.

## 5. Engine choice and its licence

| Engine | Licence | Fit with this repo (GPL-3.0) | Runtime import of disc data |
|---|---|---|---|
| **Godot 4** | MIT; "licence terms … do not apply to the content you create" ([godotengine.org/license](https://godotengine.org/license/)) **[primary]** | Compatible | Loads glTF 2.0, PNG, WAV/Ogg at runtime in exported games ([docs](https://docs.godotengine.org/en/stable/tutorials/io/runtime_file_loading_and_saving.html)) **[primary]** |
| **Unreal Engine 5** | Proprietary EULA; 5% royalty on lifetime gross above $1M per product ([unrealengine.com/license](https://www.unrealengine.com/license)) **[primary]** | Engine source cannot be redistributed publicly; linking GPL-3.0 code into a proprietary engine is the usual GPL incompatibility (Medium, not legal advice) | Needs a plugin such as glTFRuntime (MIT, [repo](https://github.com/rdeioris/glTFRuntime)) |
| **Unity 6** | Proprietary; Personal is free under $200K revenue/funding in the prior 12 months ([unity.com/pricing](https://unity.com/pricing)) **[primary]** | Same GPL concern as Unreal | Needs a runtime glTF package |

A free fan project owes no royalties on any of the three. Godot is the only one whose licence sits cleanly next to this repo's GPL-3.0 code and the GPL-3.0 KyaBank/IndustrialPark-style tools.

## 6. Legal difference

| | Decomp route (enhanced port → remaster) | Remake |
|---|---|---|
| What is distributed | Code only; player supplies the disc, assets are read or extracted locally (#37, #41) | Either new art (derivative of Atari's characters, levels, story, music) or the converted original assets |
| Does the disc requirement protect it? | Yes, the model every surviving decomp project uses | Only if every asset is converted from the disc at install time. Any reworked model, texture or level shipped with the remake is distributed derivative content |
| Visibility | Low: a port of an obscure title | High: remakes circulate as trailers and screenshots |
| Precedents | OpenGOAL, SoH, SM64 source repos still up; takedowns hit prebuilt binaries with assets and re3 (#41) | AM2R taken down after release; CryZENx stopped himself when an official remake appeared; Rayman fan remakes up so far |
| Commercial path | Pitch the port to Atari / Nightdive as a remaster base (#37) | Same licence need; a studio would likely rebuild the art itself |

The install-time conversion variant (remake engine + disc → glTF/PNG/WAV on the player's machine) is technically possible (§2, §5) and keeps the legal model, but then the remake renders the original PS2 assets through a different engine. That gives the look the decomp route already delivers, with the fidelity risks of §3 added.

## 7. Side-by-side estimate

Sizes build on #38 (decomp gap), #41 (calibration: enhanced port about 1–2 years after every level loads, one main developer) and §2–§4 above. "Solo" means one main developer part-time-ish, as in #41.

| Work item | Decomp route | Remake route (faithful, Godot/UE/Unity) |
|---|---|---|
| Asset access | Done (engine reads the banks) | 6–12 months of converter work (§2); level layout blocked on the same stubs as the decomp |
| Remaining decompilation | ~330 functions to load every level, then guards hit in play (#38): **~1–2 years solo** | Still needed as a spec for faithful behaviour, or replaced by design-from-observation (slower and less faithful) |
| Gameplay systems | Already there (the decomp) | Rebuild movement, combat, Boomy, AI, cameras, triggers: **2–4 years solo** even with the decomp as reference (262k lines to re-express) |
| Content assembly | None (original levels load) | Rebuild 14 levels + ~10,000 actors, the cinematics (563 bank files across the levels' `CINE/` folders on the disc), menus, mini-games: **1–2 years solo** |
| Renderer / modern features | Vulkan backend exists; widescreen, high FPS, input, menus: 4–9 months (#39, #41) | Engine provides; tuning and look-matching still needed: 3–6 months |
| New art (remake's point) | Optional HD packs later (#40), artist-scaled | Required, artist-scaled; the dominant cost if models are reworked |
| Long tail / stabilisation | ≥6 months (#41) | ≥6 months, plus fidelity bug reports ("it doesn't feel like Kya") |
| **Total, solo** | **~2–3.5 years** to a full enhanced port | **~5–10 years**; precedents say likely unfinished |
| **Total, 4–6 people** | **~1–2 years** | **~3–5 years** with at least 2 artists (a 60-person studio did a PS2 remake in ~1.5–2 years) |
| Fidelity | Original behaviour by construction | High risk on movement, camera and combat |
| Legal | Disc-required, no data shipped (low-to-moderate risk, #37) | Ships derivative content unless install-time conversion; higher visibility |
| Serves the sequel base (#36) | Yes: sequel runs on the same engine and modding pipeline | No: the sequel's pipeline (#28 loose-file override, actor tool) targets the Eden engine |

## 8. Conclusion

- The remake does not skip the biggest cost item. The decomp gap stays on its critical path (as a spec, or as level-layout data), and a full gameplay rewrite comes on top.
- The asset converters are worth building anyway: extend KyaBank with cluster geometry, skeletons + rigid skins, baked animations and collision. They serve modding, the remaster's review pipeline and any future licensed remake, at M-sized cost each.
- A remake only makes sense as a licensed, funded studio project after the decomp route has produced a working port, which is the Nightdive/Doom 64 pattern in #37. For this fan project, it stays a comparison.

## Open checks

1. Confirm the anim bank format decodes fully outside the runtime (a KyaBank `animconvert` prototype) and that all skinned strips really use one matrix per vertex.
2. Measure how many scene parameters each actor class reads in `Create`, to size the level-layout exporter.
3. Team size for Purple Lamp on Rehydrated (the Unreal developer interview was not reachable, HTTP 403).
4. Primary sources for the fan remakes (creators' own posts) instead of press.

## Sources

- This repo: `src/b-witch/LevelScheduler.cpp` (`TableBankCallback`), `src/b-witch/Animation.cpp`, `src/b-witch/ActorHero*.cpp`, `ActorFighter.cpp`, `ActorBoomy.cpp`, `Camera*.cpp`, `CinematicManager.cpp`, `src/EdenLib/edCollision/sources/OBBTree.cpp`, `port/KyaMesh/src/Mesh.h`, `port/KyaTexture/src/Texture.cpp`, `port/Windows/Renderer/Shaders/src/native.vert.glsl`.
- Tools: https://github.com/Icey1717/KyaBank (`README.md`, `LICENSE.txt`, `src/Mesh/ConvertMesh.cpp`), https://github.com/Icey1717/edBank, https://github.com/Icey1717/edFile, https://github.com/igorseabra4/IndustrialPark, https://github.com/rdeioris/glTFRuntime.
- Engines: https://godotengine.org/license/, https://docs.godotengine.org/en/stable/tutorials/io/runtime_file_loading_and_saving.html, https://www.unrealengine.com/license, https://unity.com/pricing.
- Remakes (press): https://en.wikipedia.org/wiki/Destroy_All_Humans!_(2020_video_game), https://en.wikipedia.org/wiki/SpongeBob_SquarePants:_Battle_for_Bikini_Bottom_%E2%80%93_Rehydrated, https://zeldauniverse.net/2026/07/11/cryzenx-halts-ocarina-of-time-fan-remake-project-after-10-years/, https://en.wikipedia.org/wiki/Sonic_P-06, https://en.wikipedia.org/wiki/AM2R, https://www.dsogaming.com/news/rayman-2-fan-remake-demo-available-for-download/, https://www.dsogaming.com/pc-performance-analyses/rayman-3-unreal-engine-5-fan-remake-demo-released/, https://nathan3197.itch.io/project-reimagine.
- Sibling research: #37, #38, #40, #41 (linked at the top).
